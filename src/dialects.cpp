#include "dialects.h"
#include "formatter.h"   // verbatim_run_len / copy_verbatim_run
#include <cctype>
#include <cstring>

// ─── token rewriter ──────────────────────────────────────────────────────────
// Case-insensitive replace of `from` with `to`, skipping comments and quoted
// literals. `kw` selects the boundary rule:
//   kw == false : function call — `from` ends in '(', only the left side needs
//                 a word boundary (SUBSTR( must not match inside FOOSUBSTR().
//   kw == true  : bare keyword / NOW()-style token — both sides need a
//                 boundary so CURRENT_TIMESTAMP doesn't match a longer word.
static std::string replace_token(const std::string& sql,
                                 const char* from, const char* to, bool kw) {
    std::string out;
    out.reserve(sql.size());
    const size_t n = std::strlen(from);

    for (size_t i = 0; i < sql.size(); ) {
        if (copy_verbatim_run(sql, i, out)) continue;

        if (i + n <= sql.size()) {
            bool match = true;
            for (size_t k = 0; k < n && match; ++k)
                match = std::tolower((unsigned char)sql[i + k]) ==
                        std::tolower((unsigned char)from[k]);
            if (match) {
                bool wb_before = (i == 0) ||
                    (!std::isalnum((unsigned char)sql[i - 1]) && sql[i - 1] != '_');
                bool wb_after = !kw || (i + n >= sql.size()) ||
                    (!std::isalnum((unsigned char)sql[i + n]) && sql[i + n] != '_');
                if (wb_before && wb_after) {
                    out += to;
                    i += n;
                    continue;
                }
            }
        }
        out += sql[i++];
    }
    return out;
}

// ─── per-dialect rule tables ─────────────────────────────────────────────────
// native  : the spelling this dialect uses
// canon   : the canonical/hub spelling
// kw      : boundary rule (see replace_token)
// to_hub_only : normalise native ▶ canon, but never rewrite canon ▶ native
//               (used when several native spellings collapse onto one canon,
//               so the reverse direction picks a single canonical native form)
struct Rule {
    const char* native;
    const char* canon;
    bool kw;
    bool to_hub_only;
};

static const Rule RULES_ANSI[] = {
    { "CHARACTER_LENGTH(", "CHAR_LENGTH(", false, true  },
};
static const Rule RULES_SNOWFLAKE[] = {
    { "SUBSTR(",  "SUBSTRING(",   false, false },
    { "LENGTH(",  "CHAR_LENGTH(", false, false },
    { "NVL(",     "COALESCE(",    false, false },
    { "IFNULL(",  "COALESCE(",    false, true  },
};
static const Rule RULES_POSTGRESQL[] = {
    { "SUBSTR(", "SUBSTRING(",        false, false },
    { "LENGTH(", "CHAR_LENGTH(",      false, false },
    { "NOW()",   "CURRENT_TIMESTAMP", true,  false },
};
static const Rule RULES_MSSQL[] = {
    { "LEN(",      "CHAR_LENGTH(",      false, false },
    { "ISNULL(",   "COALESCE(",         false, false },
    { "IIF(",      "IFF(",              false, false },
    { "GETDATE()", "CURRENT_TIMESTAMP", true,  false },
};
static const Rule RULES_MYSQL[] = {
    { "IFNULL(", "COALESCE(",         false, false },
    { "IF(",     "IFF(",              false, false },
    { "NOW()",   "CURRENT_TIMESTAMP", true,  false },
};
static const Rule RULES_SQLITE[] = {
    { "IFNULL(", "COALESCE(",   false, false },
    { "SUBSTR(", "SUBSTRING(",  false, false },
    { "LENGTH(", "CHAR_LENGTH(", false, false },
    { "IIF(",    "IFF(",        false, false },
};
static const Rule RULES_DATABRICKS[] = {
    { "COLLECT_LIST(",  "ARRAY_AGG(",    false, false },
    { "COLLECT_SET(",   "ARRAY_AGG(",    false, true  },
    { "DATE_FORMAT(",   "TO_CHAR(",      false, false },
    { "FROM_UNIXTIME(", "TO_TIMESTAMP(", false, false },
};

struct Table { const Rule* rules; size_t count; };

static Table table_for(Dialect d) {
    switch (d) {
    case Dialect::Snowflake:  return { RULES_SNOWFLAKE,  sizeof RULES_SNOWFLAKE  / sizeof(Rule) };
    case Dialect::PostgreSQL: return { RULES_POSTGRESQL, sizeof RULES_POSTGRESQL / sizeof(Rule) };
    case Dialect::MSSQL:      return { RULES_MSSQL,      sizeof RULES_MSSQL      / sizeof(Rule) };
    case Dialect::MySQL:      return { RULES_MYSQL,      sizeof RULES_MYSQL      / sizeof(Rule) };
    case Dialect::SQLite:     return { RULES_SQLITE,     sizeof RULES_SQLITE     / sizeof(Rule) };
    case Dialect::Databricks: return { RULES_DATABRICKS, sizeof RULES_DATABRICKS / sizeof(Rule) };
    case Dialect::ANSI:       return { RULES_ANSI,       sizeof RULES_ANSI       / sizeof(Rule) };
    }
    return { nullptr, 0 };
}

static std::string to_hub(const std::string& sql, Dialect d) {
    Table t = table_for(d);
    std::string s = sql;
    for (size_t i = 0; i < t.count; ++i)
        s = replace_token(s, t.rules[i].native, t.rules[i].canon, t.rules[i].kw);
    return s;
}

static std::string from_hub(const std::string& sql, Dialect d) {
    // ANSI is the hub itself — converting to ANSI is pure normalisation, so it
    // must not rewrite a perfectly valid standard spelling into a vendor one.
    if (d == Dialect::ANSI) return sql;
    Table t = table_for(d);
    std::string s = sql;
    for (size_t i = 0; i < t.count; ++i)
        if (!t.rules[i].to_hub_only)
            s = replace_token(s, t.rules[i].canon, t.rules[i].native, t.rules[i].kw);
    return s;
}

std::string convert_dialect(const std::string& sql, Dialect from, Dialect to) {
    if (from == to) return sql;
    return from_hub(to_hub(sql, from), to);
}

const wchar_t* dialect_name(Dialect d) {
    switch (d) {
    case Dialect::Snowflake:  return L"Snowflake";
    case Dialect::PostgreSQL: return L"PostgreSQL";
    case Dialect::MSSQL:      return L"MS SQL";
    case Dialect::MySQL:      return L"MySQL";
    case Dialect::SQLite:     return L"SQLite";
    case Dialect::Databricks: return L"Databricks";
    case Dialect::ANSI:       return L"ANSI";
    }
    return L"ANSI";
}
