#include "settings_io.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <map>

// ─── small helpers (no WinAPI) ─────────────────────────────────────────────────

static std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (unsigned char)s[a] <= ' ') ++a;
    while (b > a && (unsigned char)s[b - 1] <= ' ') --b;
    return s.substr(a, b - a);
}

// Strips \r and \n so a free-text value (e.g. fqdn_db) can never break the
// single-line key=value format.
static std::string sanitize_value(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) if (c != '\r' && c != '\n') out += c;
    return out;
}

static const char* dialect_to_str(Dialect d) {
    switch (d) {
    case Dialect::Snowflake:  return "snowflake";
    case Dialect::PostgreSQL: return "postgresql";
    case Dialect::MSSQL:      return "mssql";
    case Dialect::MySQL:      return "mysql";
    case Dialect::SQLite:     return "sqlite";
    case Dialect::Databricks: return "databricks";
    default:                  return "ansi";
    }
}
static Dialect dialect_from_str(const std::string& s) {
    if (s == "snowflake")  return Dialect::Snowflake;
    if (s == "postgresql") return Dialect::PostgreSQL;
    if (s == "mssql")      return Dialect::MSSQL;
    if (s == "mysql")      return Dialect::MySQL;
    if (s == "sqlite")     return Dialect::SQLite;
    if (s == "databricks") return Dialect::Databricks;
    return Dialect::ANSI;
}

// ─── writer ─────────────────────────────────────────────────────────────────

std::string settings_to_ini(const FormatSettings& s) {
    std::ostringstream o;

    o << "[General]\n";
    o << "Dialect=" << dialect_to_str(s.dialect) << "\n";
    o << "\n";

    o << "[Casing]\n";
    o << "Keywords="    << (s.kw_case == KeywordCase::Preserve ? "preserve" : s.kw_case == KeywordCase::Upper ? "upper" : "lower") << "\n";
    o << "Functions="   << (s.fn_case == KeywordCase::Preserve ? "preserve" : s.fn_case == KeywordCase::Upper ? "upper" : "lower") << "\n";
    o << "Identifiers=" << (s.id_case == IdentCase::Preserve  ? "preserve" : s.id_case == IdentCase::Upper  ? "upper" : "lower") << "\n";
    o << "\n";

    o << "[Columns]\n";
    o << "SplitCols="    << (s.split_cols ? "1" : "0") << "\n";
    o << "CommaPos="     << (s.comma_pos == CommaPos::After ? "after" : "before") << "\n";
    o << "AliasOp="      << (s.alias_op == AliasOp::Add ? "add" : s.alias_op == AliasOp::Remove ? "remove" : "leave") << "\n";
    o << "AliasCase="    << (s.alias_case == AliasCase::Upper ? "upper" : "lower") << "\n";
    o << "AlignAliases=" << (s.align_aliases ? "1" : "0") << "\n";
    o << "\n";

    o << "[Alignment]\n";
    o << "Keywords=" << (s.align_keywords ? "1" : "0") << "\n";
    o << "JoinOn="   << (s.align_join_on  ? "1" : "0") << "\n";
    o << "\n";

    o << "[Structure]\n";
    o << "CaseStmt="      << (s.case_stmt == CaseStmt::ExpandJoined ? "expand_joined" : s.case_stmt == CaseStmt::Inline ? "inline" : s.case_stmt == CaseStmt::LeaveAsIs ? "leave" : "expand") << "\n";
    o << "CteNl="         << (s.cte_nl ? "1" : "0") << "\n";
    o << "CteIndent="     << (s.cte_indent ? "1" : "0") << "\n";
    o << "SubIndent="     << (s.sub_indent ? "1" : "0") << "\n";
    o << "SemiAdd="       << (s.semi_add ? "1" : "0") << "\n";
    o << "Semicolons="    << (s.semi_pos == SemicolonPos::Preserve ? "preserve" : s.semi_pos == SemicolonPos::OwnLine ? "own" : s.semi_pos == SemicolonPos::Remove ? "remove" : "same") << "\n";
    o << "AlignCaseWhen=" << (s.align_case_when ? "1" : "0") << "\n";
    o << "\n";

    o << "[Spacing]\n";
    o << "Operators="    << (s.spc_operators == SpacesOpt::Add ? "add" : s.spc_operators == SpacesOpt::Remove ? "remove" : "leave") << "\n";
    o << "Functions="    << (s.spc_functions == SpacesOpt::Add ? "add" : s.spc_functions == SpacesOpt::Remove ? "remove" : "leave") << "\n";
    o << "BlankBetween=" << (s.blank_between ? "1" : "0") << "\n";
    o << "BlankLines="   << (s.blank_lines == BlankLines::Preserve ? "preserve" : s.blank_lines == BlankLines::Remove ? "remove" : "merge") << "\n";
    o << "MaxLength="    << s.max_length << "\n";
    o << "InListWrap="   << s.in_list_wrap << "\n";
    o << "\n";

    o << "[Joins]\n";
    o << "StripInnerOuter=" << (s.strip_inner_outer ? "1" : "0") << "\n";
    o << "\n";

    o << "[FQDN]\n";
    o << "Database=" << sanitize_value(s.fqdn_db) << "\n";
    o << "Schema="   << sanitize_value(s.fqdn_schema) << "\n";

    return o.str();
}

// ─── reader ─────────────────────────────────────────────────────────────────

FormatSettings settings_from_ini(const std::string& ini_text) {
    FormatSettings s; // start from struct defaults; unknown/missing keys keep them

    // section -> (key -> value), lowercased section/key for lookup, matching
    // the case-sensitive names we write but tolerant of hand-edited casing.
    std::map<std::string, std::map<std::string, std::string>> data;
    std::string section;

    std::istringstream in(ini_text);
    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') continue;
        if (t.front() == '[' && t.back() == ']') {
            section = trim(t.substr(1, t.size() - 2));
            continue;
        }
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue; // malformed line: skip, never crash
        std::string key = trim(t.substr(0, eq));
        std::string val = trim(t.substr(eq + 1));
        data[section][key] = val;
    }

    auto get = [&](const char* sec, const char* key, const std::string& def) -> std::string {
        auto si = data.find(sec);
        if (si == data.end()) return def;
        auto ki = si->second.find(key);
        return ki == si->second.end() ? def : ki->second;
    };
    auto getb = [&](const char* sec, const char* key, bool def) -> bool {
        std::string v = get(sec, key, def ? "1" : "0");
        return v == "1" || v == "true";
    };
    auto geti = [&](const char* sec, const char* key, int def) -> int {
        std::string v = get(sec, key, "");
        if (v.empty()) return def;
        char* end = nullptr;
        long n = std::strtol(v.c_str(), &end, 10);
        return (end && end != v.c_str()) ? (int)n : def;
    };

    s.dialect = dialect_from_str(get("General", "Dialect", "ansi"));

    std::string kw = get("Casing", "Keywords", "lower");
    s.kw_case = kw == "preserve" ? KeywordCase::Preserve : kw == "upper" ? KeywordCase::Upper : KeywordCase::Lower;
    std::string fn = get("Casing", "Functions", "lower");
    s.fn_case = fn == "preserve" ? KeywordCase::Preserve : fn == "upper" ? KeywordCase::Upper : KeywordCase::Lower;
    std::string id = get("Casing", "Identifiers", "lower");
    s.id_case = id == "preserve" ? IdentCase::Preserve : id == "upper" ? IdentCase::Upper : IdentCase::Lower;

    s.split_cols = getb("Columns", "SplitCols", true);
    s.comma_pos  = get("Columns", "CommaPos", "before") == "after" ? CommaPos::After : CommaPos::Before;
    std::string ao = get("Columns", "AliasOp", "add");
    s.alias_op   = ao == "remove" ? AliasOp::Remove : ao == "leave" ? AliasOp::LeaveAsIs : AliasOp::Add;
    s.alias_case = get("Columns", "AliasCase", "lower") == "upper" ? AliasCase::Upper : AliasCase::Lower;
    s.align_aliases = getb("Columns", "AlignAliases", false);

    s.align_keywords = getb("Alignment", "Keywords", true);
    s.align_join_on  = getb("Alignment", "JoinOn", true);

    std::string cs = get("Structure", "CaseStmt", "expand");
    s.case_stmt = cs == "expand_joined" ? CaseStmt::ExpandJoined :
                  cs == "inline"        ? CaseStmt::Inline        :
                  cs == "leave"         ? CaseStmt::LeaveAsIs     : CaseStmt::Expand;
    s.cte_nl     = getb("Structure", "CteNl", true);
    s.cte_indent = getb("Structure", "CteIndent", true);
    s.sub_indent = getb("Structure", "SubIndent", true);
    s.semi_add   = getb("Structure", "SemiAdd", false);
    std::string sp = get("Structure", "Semicolons", "same");
    s.semi_pos = sp == "preserve" ? SemicolonPos::Preserve :
                 sp == "own"      ? SemicolonPos::OwnLine  :
                 sp == "remove"   ? SemicolonPos::Remove   : SemicolonPos::SameLine;
    s.align_case_when = getb("Structure", "AlignCaseWhen", false);

    std::string sop = get("Spacing", "Operators", "add");
    s.spc_operators = sop == "add" ? SpacesOpt::Add : sop == "remove" ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    std::string sfn = get("Spacing", "Functions", "leave");
    s.spc_functions = sfn == "add" ? SpacesOpt::Add : sfn == "remove" ? SpacesOpt::Remove : SpacesOpt::LeaveAsIs;
    s.blank_between = getb("Spacing", "BlankBetween", false);
    std::string bl = get("Spacing", "BlankLines", "merge");
    s.blank_lines = bl == "preserve" ? BlankLines::Preserve : bl == "remove" ? BlankLines::Remove : BlankLines::Merge;
    s.max_length   = geti("Spacing", "MaxLength", 0);
    s.in_list_wrap = geti("Spacing", "InListWrap", 0);

    s.strip_inner_outer = getb("Joins", "StripInnerOuter", true);

    s.fqdn_db     = get("FQDN", "Database", "");
    s.fqdn_schema = get("FQDN", "Schema", "");

    return s;
}
