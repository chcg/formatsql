#define NOMINMAX
#include "formatter.h"
#include "settings.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>

static const int KW = 6;  // "select" width — reference for right-aligning keywords

// ─── char / string helpers ────────────────────────────────────────────────────

static char lc(char c)  { return (char)std::tolower((unsigned char)c); }
static char uc_(char c) { return (char)std::toupper((unsigned char)c); }

static std::string to_lower(const std::string& s) {
    std::string r = s; for (char& c : r) c = lc(c); return r;
}

static bool word_boundary(char c) {
    return !c || c == ' ' || c == '\t' || c == '(' || c == ')' ||
           c == '\n' || c == '\r' || c == ',' || c == ';' || c == '.';
}

static bool starts_word(const std::string& sl, const char* w) {
    size_t n = strlen(w);
    if (sl.size() < n) return false;
    if (sl.compare(0, n, w) != 0) return false;
    return sl.size() == n || word_boundary(sl[n]);
}

static std::vector<std::string> split_lines(const std::string& s) {
    std::vector<std::string> out;
    std::string line;
    for (char c : s) {
        if (c == '\r') continue;
        if (c == '\n') { out.push_back(line); line.clear(); }
        else line += c;
    }
    out.push_back(line);
    return out;
}

static std::string join_lines(const std::vector<std::string>& v) {
    std::string r;
    for (size_t i = 0; i < v.size(); ++i) {
        r += v[i];
        if (i + 1 < v.size()) r += '\n';
    }
    return r;
}

static std::string rtrim(const std::string& s) {
    size_t e = s.size();
    while (e > 0 && (s[e-1] == ' ' || s[e-1] == '\t')) --e;
    return s.substr(0, e);
}

static std::string ltrim(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// ─── verbatim runs: comments and quoted literals ──────────────────────────────
// A single shared primitive for the pattern repeated throughout this file:
// "skip over block comments, line comments, and quoted literals without
// looking inside them". Quoted literals cover '...', "...", and `...`
// (backtick-quoted identifiers, e.g. MySQL/Databricks `order`) so that a
// reserved word used as a quoted identifier is never mistaken for the
// keyword itself by a later pass. All three quote kinds use the same
// doubled-quote escape ('' / "" / ``) for a literal quote character inside.
//
// Returns the length of the run starting at sql[i], or 0 if none applies.
// Line comments stop right before the newline so callers still see it.
// Declared in formatter.h so the dialect converter can share it.
size_t verbatim_run_len(const std::string& sql, size_t i) {
    if (i + 1 < sql.size() && sql[i] == '/' && sql[i + 1] == '*') {
        size_t j = i + 2;
        while (j + 1 < sql.size() && !(sql[j] == '*' && sql[j + 1] == '/')) ++j;
        return std::min(j + 2, sql.size()) - i;
    }
    if (i + 1 < sql.size() && sql[i] == '-' && sql[i + 1] == '-') {
        size_t j = sql.find('\n', i);
        return (j == std::string::npos ? sql.size() : j) - i;
    }
    if (sql[i] == '\'' || sql[i] == '"' || sql[i] == '`') {
        char q = sql[i];
        size_t j = i + 1;
        while (j < sql.size()) {
            if (sql[j] == q) {
                if (j + 1 < sql.size() && sql[j + 1] == q) { j += 2; continue; }
                ++j; break;
            }
            ++j;
        }
        return j - i;
    }
    return 0;
}

// Appends the verbatim run at sql[i] to `out` and advances `i` past it.
// Returns false (no-op) if sql[i] doesn't start a comment/quoted literal.
bool copy_verbatim_run(const std::string& sql, size_t& i, std::string& out) {
    size_t n = verbatim_run_len(sql, i);
    if (!n) return false;
    out.append(sql, i, n);
    i += n;
    return true;
}

// Same as copy_verbatim_run but discards the text — for scans that only
// need to skip past a run without building an output string.
bool skip_verbatim_run(const std::string& sql, size_t& i) {
    size_t n = verbatim_run_len(sql, i);
    if (!n) return false;
    i += n;
    return true;
}

// ─── keyword table ────────────────────────────────────────────────────────────

static const char* SQL_KEYWORDS[] = {
    "select","from","where","join","left","right","inner","outer","full","cross",
    "on","and","or","not","in","between","like","ilike","is","null","group","by",
    "order","having","qualify","limit","union","all","intersect","except","with",
    "as","case","when","then","else","end","create","replace","view","table",
    "insert","into","values","update","set","delete","alter","drop","distinct",
    "exists","over","partition","rows","range","preceding","following","current",
    "row","asc","desc","nulls","first","last","lateral","flatten","using","top",
    "natural",
    "recursive","materialized","temporary","temp","if","begin","commit","rollback",
    "primary","foreign","key","references","unique","index","constraint","default",
    nullptr
};

static bool is_sql_keyword(const char* p, size_t len) {
    for (const char** k = SQL_KEYWORDS; *k; ++k) {
        if (strlen(*k) != len) continue;
        bool ok = true;
        for (size_t i = 0; i < len; ++i)
            if (lc(p[i]) != (*k)[i]) { ok = false; break; }
        if (ok) return true;
    }
    return false;
}

// ─── pass 1: normalize to lowercase ──────────────────────────────────────────
// All subsequent passes operate on lowercase SQL keywords.
// The final case pass converts to the user's desired case at the very end.

static std::string normalize(const std::string& sql) {
    std::string out;
    out.reserve(sql.size());
    size_t i = 0;

    while (i < sql.size()) {
        char c = sql[i];
        if (copy_verbatim_run(sql, i, out)) continue;
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < sql.size() && (std::isalnum((unsigned char)sql[i]) || sql[i] == '_')) ++i;
            size_t len = i - start;
            if (is_sql_keyword(sql.c_str() + start, len))
                for (size_t j = start; j < start+len; ++j) out += lc(sql[j]);
            else
                out.append(sql, start, len);
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── pass 1b: tidy interior whitespace ──────────────────────────────────────
// Runs on normalized (still single-ish) text before any alignment happens:
//   - collapses runs of spaces/tabs to one space (leading indentation is
//     dropped and rebuilt by the alignment passes; one blank line between
//     statements is kept)
//   - removes the space between a function name and its '('  (count (*) -> count(*))
//   - guarantees one space after every comma                 (nullif(a,b) -> nullif(a, b))
// Comments and quoted literals are copied verbatim.

static std::string tidy_spacing(const std::string& sql) {
    std::string out;
    out.reserve(sql.size());
    size_t i = 0;

    // Tail of `out` is an identifier that is NOT a SQL keyword (i.e. a function
    // name), so a following '(' should be glued to it.
    auto tail_is_fn_name = [&]() -> bool {
        size_t e = out.size();
        if (e == 0 || !(std::isalnum((unsigned char)out[e-1]) || out[e-1] == '_')) return false;
        size_t s = e;
        while (s > 0 && (std::isalnum((unsigned char)out[s-1]) || out[s-1] == '_')) --s;
        return !is_sql_keyword(out.c_str() + s, e - s);
    };

    while (i < sql.size()) {
        if (copy_verbatim_run(sql, i, out)) continue;
        char c = sql[i];

        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            int nls = 0;
            while (i < sql.size() &&
                   (sql[i] == ' ' || sql[i] == '\t' || sql[i] == '\n' || sql[i] == '\r')) {
                if (sql[i] == '\n') ++nls;
                ++i;
            }
            if (nls >= 2)      out += "\n\n";
            else if (nls == 1) out += '\n';
            else if (i < sql.size() && sql[i] == '(' && tail_is_fn_name()) {
                /* drop the space: function call */
            } else if (!out.empty() && out.back() != '\n') {
                out += ' ';
            }
            continue;
        }

        if (c == ',') {
            bool prev_digit = !out.empty() && std::isdigit((unsigned char)out.back());
            out += ',';
            ++i;
            while (i < sql.size() && (sql[i] == ' ' || sql[i] == '\t')) ++i;
            // Skip a decimal/grouping comma between digits (NL number 1.000,00).
            bool next_digit = i < sql.size() && std::isdigit((unsigned char)sql[i]);
            if (i < sql.size() && sql[i] != ')' && sql[i] != '\n' && sql[i] != '\r'
                && !(prev_digit && next_digit))
                out += ' ';
            continue;
        }

        out += c;
        ++i;
    }
    return out;
}

// ─── pass 2: CASE statement formatting ───────────────────────────────────────

// Returns position after the "end" that closes the CASE starting at `pos`
// (where pos points just after "case").
static size_t find_case_end(const std::string& s, size_t pos) {
    int depth = 1;
    while (pos < s.size() && depth > 0) {
        if (skip_verbatim_run(s, pos)) continue;
        bool pb = (pos == 0) || word_boundary(s[pos-1]);
        if (pb) {
            auto cmp4 = [&](const char* w) {
                size_t n = strlen(w);
                if (pos + n > s.size()) return false;
                for (size_t j = 0; j < n; ++j) if (lc(s[pos+j]) != w[j]) return false;
                return pos+n == s.size() || word_boundary(s[pos+n]);
            };
            if (cmp4("case")) { depth++; pos += 4; continue; }
            if (cmp4("end"))  { --depth; if (!depth) return pos + 3; pos += 3; continue; }
        }
        ++pos;
    }
    return pos;
}

static std::string collapse_ws(const std::string& s) {
    std::string r; bool sp = false;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (c == ' ') { if (!sp) r += ' '; sp = true; }
        else { r += c; sp = false; }
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r;
}

// Expand a single CASE...END block that's been collapsed to one line.
// `case_col` = column position of `case` in the output (for indentation).
// Layout: "case when <cond>" on first line, then THEN/WHEN/ELSE indented to
// the WHEN position (case_col + 5), END back at case_col.
static std::string expand_case(const std::string& block, int case_col) {
    std::string out;
    std::string base_pad(case_col, ' ');       // "end" aligns under "case"
    std::string when_pad(case_col + 5, ' ');  // aligns under "when" after "case "
    size_t i = 0;
    bool first_when = true;

    out += "case";
    i = 4;
    if (i < block.size() && block[i] == ' ') ++i;

    int depth = 0;
    int nested = 0;
    bool in_str = false; char str_d = 0;

    while (i < block.size()) {
        char c = block[i];
        // Note: block has already been through collapse_ws(), so newlines are
        // gone — only quote protection makes sense here, not comment-skipping
        // (a line comment would have no terminator left to stop at).
        if (in_str) {
            out += c;
            if (c == str_d && !(i+1 < block.size() && block[i+1] == str_d)) in_str = false;
            ++i; continue;
        }
        if (c == '\'' || c == '"' || c == '`') { in_str = true; str_d = c; out += c; ++i; continue; }
        if (c == '(') { depth++; out += c; ++i; continue; }
        if (c == ')') { depth--; out += c; ++i; continue; }

        bool pb = (i == 0) || word_boundary(block[i-1]);
        if (pb && depth == 0) {
            auto cmp = [&](const char* w) {
                size_t n = strlen(w);
                if (i+n > block.size()) return false;
                for (size_t j = 0; j < n; ++j) if (lc(block[i+j]) != w[j]) return false;
                return i+n == block.size() || word_boundary(block[i+n]);
            };
            if (cmp("case")) { nested++; out += "case"; i += 4; if (i<block.size()&&block[i]==' ') { out+=' '; ++i; } continue; }
            if (cmp("end")) {
                if (nested > 0) { nested--; out += "end"; i += 3; continue; }
                while (!out.empty() && out.back() == ' ') out.pop_back();
                out += '\n' + base_pad + "end";
                i += 3;
                while (i < block.size()) out += block[i++];
                return out;
            }
            if (nested == 0) {
                auto strip_trail = [&]() { while (!out.empty() && out.back() == ' ') out.pop_back(); };
                if (cmp("when")) {
                    strip_trail();
                    if (first_when) {
                        first_when = false;
                        out += " when";  // first WHEN stays on the same line as CASE
                    } else {
                        out += '\n' + when_pad + "when";
                    }
                    i += 4; if (i<block.size()&&block[i]==' ') { out+=' '; ++i; } continue;
                }
                if (cmp("then")) {
                    strip_trail();
                    if (g_settings.case_stmt == CaseStmt::ExpandJoined)
                        out += " then";    // stay on same line as WHEN
                    else
                        out += '\n' + when_pad + "then";
                    i += 4; if (i<block.size()&&block[i]==' ') { out+=' '; ++i; } continue;
                }
                if (cmp("else")) { strip_trail(); out += '\n' + when_pad + "else"; i += 4; if (i<block.size()&&block[i]==' ') { out+=' '; ++i; } continue; }
            }
        }
        out += c; ++i;
    }
    return out;
}

static std::string format_case_stmts(const std::string& sql) {
    if (g_settings.case_stmt == CaseStmt::LeaveAsIs) return sql;

    std::string out;
    size_t i = 0;

    while (i < sql.size()) {
        char c = sql[i];
        if (copy_verbatim_run(sql, i, out)) continue;
        bool pb = (i == 0) || word_boundary(sql[i-1]);
        if (pb && i+4 <= sql.size() &&
            lc(sql[i])=='c' && lc(sql[i+1])=='a' && lc(sql[i+2])=='s' && lc(sql[i+3])=='e' &&
            (i+4 == sql.size() || word_boundary(sql[i+4]))) {
            size_t end_pos = find_case_end(sql, i+4);
            std::string block = sql.substr(i, end_pos - i);
            // figure out the column position of this CASE keyword
            size_t last_nl = out.rfind('\n');
            int case_col = (int)(last_nl == std::string::npos ? out.size() : out.size() - last_nl - 1);
            if (g_settings.case_stmt == CaseStmt::Inline)
                out += collapse_ws(block);
            else
                out += expand_case(collapse_ws(block), case_col);
            i = end_pos;
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── pass 3: operator spacing ─────────────────────────────────────────────────

static std::string apply_op_spacing(const std::string& sql) {
    SpacesOpt opt = g_settings.spc_operators;
    if (opt == SpacesOpt::LeaveAsIs) return sql;

    // Ordered: longer operators first so >= is matched before >
    struct Op { const char* s; int n; };
    static const Op OPS[] = {{"!=",2},{"<>",2},{"<=",2},{">=",2},{"<",1},{">",1},{"=",1},{nullptr,0}};

    std::string out;
    out.reserve(sql.size() + 64);
    size_t i = 0;

    while (i < sql.size()) {
        char c = sql[i];
        if (copy_verbatim_run(sql, i, out)) continue;
        // Pass '->' and '->>' (Postgres JSON) through untouched so neither the
        // comparison loop nor the arithmetic block below can split them.
        if (c == '-' && i + 1 < sql.size() && sql[i+1] == '>') {
            out += "->"; i += 2;
            if (i < sql.size() && sql[i] == '>') { out += '>'; ++i; }
            continue;
        }

        const Op* found = nullptr;
        for (const Op* op = OPS; op->s; ++op) {
            if ((int)sql.size() - (int)i >= op->n && sql.compare(i, op->n, op->s) == 0) {
                // Single-char <, >, = that is really part of a compound operator
                // we don't format (>>, <<, @>, <@, #>, =>, := ...): leave it alone.
                static const char* PREV_BLOCK = "-<>@#|&~:=";
                static const char* NEXT_BLOCK = "<>@#|&~";
                if (op->n == 1 && i > 0 && strchr(PREV_BLOCK, sql[i-1])) break;
                if (op->n == 1 && i + 1 < sql.size() && strchr(NEXT_BLOCK, sql[i+1])) break;
                found = op; break;
            }
        }
        if (found) {
            if (opt == SpacesOpt::Add) {
                if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
                out.append(found->s, found->n);
                i += found->n;
                if (i < sql.size() && sql[i] != ' ' && sql[i] != '\n') out += ' ';
            } else { // Remove
                while (!out.empty() && out.back() == ' ') out.pop_back();
                out.append(found->s, found->n);
                i += found->n;
                while (i < sql.size() && sql[i] == ' ') ++i;
            }
            continue;
        }

        // Arithmetic / concat operators: || + - * / %
        // Add mode only (Remove could turn "1 - -1" into the comment "1--1").
        // Only spaced when clearly binary: an operand ends on the left and
        // starts on the right. '*' after SELECT/DISTINCT and '->' are skipped.
        if (opt == SpacesOpt::Add && (c=='|'||c=='+'||c=='-'||c=='*'||c=='/'||c=='%')) {
            int oplen = (c == '|' && i+1 < sql.size() && sql[i+1] == '|') ? 2
                      : (c == '|') ? 0 : 1;
            if (oplen) {
                auto op_end   = [](char x){ return std::isalnum((unsigned char)x) || x=='_' ||
                                                   x==')' || x=='\'' || x=='"' || x=='`'; };
                auto op_start = [](char x){ return std::isalnum((unsigned char)x) || x=='_' ||
                                                   x=='(' || x=='\'' || x=='"' || x=='`' ||
                                                   x=='+' || x=='-'; };
                char prevc = 0;
                for (size_t k = out.size(); k-- > 0; ) { if (out[k] != ' ') { prevc = out[k]; break; } }
                size_t na = i + oplen;
                while (na < sql.size() && sql[na] == ' ') ++na;
                char nextc = na < sql.size() ? sql[na] : 0;

                // Preceding word in `out` (for the keyword check below).
                size_t we = out.size();
                while (we > 0 && out[we-1] == ' ') --we;
                size_t ws = we;
                while (ws > 0 && (std::isalnum((unsigned char)out[ws-1]) || out[ws-1]=='_')) --ws;
                bool prev_is_kw = ws < we && is_sql_keyword(out.c_str() + ws, we - ws);

                bool binary;
                if (oplen == 2) {                       // ||  — always binary
                    binary = prevc && nextc;
                } else {
                    // "-1"/"+1" right after a keyword (select -1, then -1, …) is unary.
                    binary = op_end(prevc) && op_start(nextc) && !prev_is_kw;
                }
                if (binary) {
                    if (!out.empty() && out.back() != ' ' && out.back() != '\n') out += ' ';
                    out.append(sql, i, oplen);
                    i += oplen;
                    if (i < sql.size() && sql[i] != ' ' && sql[i] != '\n') out += ' ';
                    continue;
                }
            }
        }
        out += c; ++i;
    }
    return out;
}

// ─── pass 4: clause splitting ─────────────────────────────────────────────────

static std::string split_clauses(const std::string& sql) {
    struct Def { const char* kw; bool is_select; };
    static const Def SPLITS[] = {
        {"natural left join",false},{"natural right join",false},
        {"natural full join",false},{"natural inner join",false},
        {"natural join",   false},
        {"left outer join",false},{"right outer join",false},
        {"full outer join",false},{"cross join",      false},
        {"inner join",     false},{"left join",       false},
        {"right join",     false},{"full join",       false},
        {"group by",       false},{"order by",        false},
        {"union all",      false},{"join",            false},
        {"from",           false},{"where",           false},
        {"having",         false},{"qualify",         false},
        {"limit",          false},{"union",           false},
        {"intersect",      false},{"except",          false},
        {"select",         true },
        {"on",             false},
        {"using",          false},
        {"and",            false},
        {"or",             false},
        {"insert",         false},
        {"update",         false},
        {"values",         false},
        {"set",            false},
        {"create or replace", false},  // before "or" so "or" in SPLITS doesn't split it
        {"create",         false},
        {"drop",           false},
        {"delete from",    false},     // before standalone "delete" for longer-match priority
        {"delete",         false},
        {nullptr,          false}
    };

    // Paren types:
    //   Func      – function call paren: no keyword splitting inside
    //   SubqInline – subquery in FROM/IN/EXISTS: SELECT goes inline with '(', splits keywords
    //   CteBody   – CTE body (preceded by AS): SELECT goes on new line, splits keywords,
    //               closing ')' goes on its own line
    enum class PT { Func, SubqInline, CteBody };

    std::string out;
    out.reserve(sql.size() * 2);
    size_t i = 0;
    int  paren = 0;
    bool in_sel = false;
    std::vector<PT> paren_stack;
    bool just_opened_inline = false;  // suppress ensure_nl for first SELECT after '('
    bool skip_next_and = false;       // the AND that belongs to BETWEEN ... AND

    auto ensure_nl = [&]() {
        if (just_opened_inline) { just_opened_inline = false; return; }
        while (!out.empty() && out.back() == ' ') out.pop_back();
        if (!out.empty() && out.back() != '\n') out += '\n';
    };
    auto allow_split = [&]() {
        for (PT p : paren_stack) if (p == PT::Func) return false;
        return true;
    };
    // True if the last non-space word in 'out' (on the current line) is "as"
    auto last_word_is_as = [&]() {
        size_t e = out.size();
        while (e > 0 && out[e-1] == ' ') --e;
        if (e < 2) return false;
        if (lc(out[e-1]) != 's' || lc(out[e-2]) != 'a') return false;
        return e < 3 || !(std::isalnum((unsigned char)out[e-3]) || out[e-3] == '_');
    };

    while (i < sql.size()) {
        char c = sql[i];
        if (copy_verbatim_run(sql, i, out)) continue;
        if (c == '(') {
            size_t j = i + 1;
            while (j < sql.size() && sql[j] == ' ') ++j;
            auto starts = [&](const char* w, size_t n) {
                return j + n <= sql.size()
                    && sql.compare(j, n, w) == 0
                    && (j + n >= sql.size() || word_boundary(sql[j + n]));
            };
            bool is_subq = starts("select", 6) || starts("with", 4);
            PT pt = PT::Func;
            if (is_subq) pt = last_word_is_as() ? PT::CteBody : PT::SubqInline;
            paren_stack.push_back(pt);
            ++paren; out += c; ++i;
            if (pt == PT::SubqInline) {
                while (i < sql.size() && sql[i] == ' ') ++i;  // skip space between '(' and 'select'
                just_opened_inline = true;
            }
            continue;
        }
        if (c == ')') {
            PT was = paren_stack.empty() ? PT::Func : paren_stack.back();
            if (!paren_stack.empty()) paren_stack.pop_back();
            if (was == PT::CteBody) ensure_nl();  // put closing ')' on its own line
            --paren; out += c; ++i; continue;
        }

        // Comma: split SELECT columns only when split_cols is enabled
        if (c == ',' && allow_split() && in_sel && g_settings.split_cols) {
            if (g_settings.comma_pos == CommaPos::Before) {
                ensure_nl();
                out += ',';
                ++i;
                while (i < sql.size() && (sql[i] == ' ' || sql[i] == '\t')) ++i;
                out += ' ';
            } else { // After
                out += ',';
                ++i;
                while (i < sql.size() && (sql[i] == ' ' || sql[i] == '\t')) ++i;
                ensure_nl();
                out += ' ';
            }
            continue;
        }

        // Clause keyword — at top level or inside subquery/CTE parens
        bool prev_word = i > 0 && (std::isalnum((unsigned char)sql[i-1]) || sql[i-1] == '_');

        // "select distinct on (...)" — keep the whole DISTINCT ON (...) qualifier
        // on the SELECT line so its "on" is not split off as a JOIN clause.
        if (!prev_word && c == 's' && allow_split()
            && sql.compare(i, 6, "select") == 0
            && (i + 6 >= sql.size() || word_boundary(sql[i+6]))) {
            size_t k = i + 6;
            while (k < sql.size() && sql[k] == ' ') ++k;
            if (sql.compare(k, 8, "distinct") == 0
                && k + 8 < sql.size() && word_boundary(sql[k+8])) {
                size_t m = k + 8;
                while (m < sql.size() && sql[m] == ' ') ++m;
                if (sql.compare(m, 2, "on") == 0 && m + 2 < sql.size()
                    && (sql[m+2] == ' ' || sql[m+2] == '(')) {
                    size_t p = m + 2;
                    while (p < sql.size() && sql[p] == ' ') ++p;
                    if (p < sql.size() && sql[p] == '(') {
                        int d = 0; size_t q = p;
                        for (; q < sql.size(); ++q) {
                            if (sql[q] == '(') ++d;
                            else if (sql[q] == ')') { if (--d == 0) { ++q; break; } }
                        }
                        ensure_nl();
                        out.append(sql, i, q - i);   // "select distinct on (...)"
                        i = q;
                        in_sel = true;
                        continue;
                    }
                }
            }
        }

        // "between" — pass through inline and remember that the next AND is
        // part of the range, not a WHERE conjunction.
        if (!prev_word && c == 'b' && i + 7 <= sql.size()
            && sql.compare(i, 7, "between") == 0
            && (i + 7 == sql.size() || word_boundary(sql[i+7]))) {
            out.append(sql, i, 7);
            i += 7;
            skip_next_and = true;
            continue;
        }

        if (!prev_word && std::isalpha((unsigned char)c) && allow_split()) {
            bool kw_matched = false;
            for (const Def* d = SPLITS; d->kw; ++d) {
                size_t n = strlen(d->kw);
                if (i + n > sql.size()) continue;
                bool match = true;
                for (size_t j = 0; j < n; ++j)
                    if (sql[i+j] != d->kw[j]) { match = false; break; }
                if (!match) continue;
                char nx = (i+n < sql.size()) ? sql[i+n] : '\0';
                if (!word_boundary(nx)) continue;
                if (n == 3 && d->kw[0] == 'a' && d->kw[1] == 'n' && skip_next_and) {
                    out.append(sql, i, n);   // BETWEEN's AND: keep inline
                    i += n;
                    skip_next_and = false;
                    kw_matched = true;
                    break;
                }
                ensure_nl();
                out.append(sql.c_str() + i, n);
                i += n;
                in_sel = d->is_select;
                kw_matched = true;
                break;
            }
            if (kw_matched) continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── clause matching helpers ──────────────────────────────────────────────────

struct KwInfo { size_t len; bool is_join; };

static KwInfo match_clause(const std::string& sl) {
    struct Def { const char* kw; bool is_join; };
    static const Def DEFS[] = {
        {"natural left join",true},{"natural right join",true},
        {"natural full join",true},{"natural inner join",true},
        {"natural join",   true},
        {"left outer join",true},{"right outer join",true},
        {"full outer join",true},{"cross join",      true},
        {"inner join",     true},{"left join",       true},
        {"right join",     true},{"full join",       true},
        {"group by",      false},{"order by",        false},
        {"union all",     false},{"join",            true},
        {"from",          false},{"where",           false},
        {"having",        false},{"qualify",         false},
        {"limit",         false},{"union",           false},
        {"intersect",     false},{"except",          false},
        {"insert",        false},{"update",          false},
        {"values",        false},{"set",             false},
        {"create",        false},{"drop",            false},
        {"delete",        false},
        {nullptr,         false}
    };
    for (const Def* d = DEFS; d->kw; ++d) {
        size_t n = strlen(d->kw);
        if (sl.size() >= n && sl.compare(0, n, d->kw) == 0) {
            if (sl.size() == n || word_boundary(sl[n]))
                return {n, d->is_join};
        }
    }
    return {0, false};
}

static bool is_on_and(const std::string& sl) {
    return starts_word(sl, "on") || starts_word(sl, "and") || starts_word(sl, "or")
        || starts_word(sl, "using");
}

static bool is_select_kw(const std::string& sl) { return starts_word(sl, "select"); }

static std::string pad_kw(const std::string& kw) {
    size_t p = kw.find(' ');
    int first = (int)(p == std::string::npos ? kw.size() : p);
    return std::string(std::max(0, KW - first), ' ');
}

static void split_kw(const std::string& s, size_t n, std::string& kw, std::string& rest) {
    kw = s.substr(0, n);
    size_t rs = n;
    while (rs < s.size() && (s[rs]==' '||s[rs]=='\t')) ++rs;
    rest = s.substr(rs);
}

// ─── alias processing (for a single column's content) ────────────────────────
// `col` is the column text WITHOUT any leading ", " prefix.
// Input is lowercased (since we normalize first).

static std::string process_alias(const std::string& col) {
    AliasOp   op = g_settings.alias_op;
    AliasCase ac = g_settings.alias_case;
    if (op == AliasOp::LeaveAsIs) return col;

    // Find the LAST " as " at paren depth 0, outside strings
    int depth = 0; bool in_str = false; char str_d = 0;
    size_t as_pos = std::string::npos;
    for (size_t i = 0; i + 4 <= col.size(); ++i) {
        char c = col[i];
        if (in_str) { if (c == str_d) in_str = false; continue; }
        if (c=='\''||c=='"') { in_str=true; str_d=c; continue; }
        if (c=='(') { ++depth; continue; }
        if (c==')') { --depth; continue; }
        if (depth == 0 && col.compare(i, 4, " as ") == 0)
            as_pos = i;
    }

    if (as_pos != std::string::npos) {
        std::string expr   = col.substr(0, as_pos);
        std::string alias  = col.substr(as_pos + 4);
        if (op == AliasOp::Remove)
            return expr + " " + alias;
        // Add or Preserve-with-AS: apply alias_case to the AS keyword
        std::string as_kw = (ac == AliasCase::Upper) ? "AS" : "as";
        return expr + " " + as_kw + " " + alias;
    }

    // No explicit AS. In Add mode, promote an implicit trailing alias
    // ("sum(x) total" -> "sum(x) as total", "id x" -> "id as x").
    if (op == AliasOp::Add && col.find("--") == std::string::npos
        && col.find("/*") == std::string::npos) {
        std::string t = rtrim(col);
        size_t e = t.size(), b = e;
        while (b > 0 && t[b-1] != ' ' && t[b-1] != '\t') --b;
        std::string alias = t.substr(b);
        std::string expr  = rtrim(t.substr(0, b));

        bool alias_ident = !alias.empty() &&
            (std::isalpha((unsigned char)alias[0]) || alias[0] == '_');
        for (char ch : alias)
            if (!(std::isalnum((unsigned char)ch) || ch == '_')) alias_ident = false;

        // Last whitespace-separated token of expr (to reject "a or b", "x is null").
        size_t xe = expr.size(), xb = xe;
        while (xb > 0 && expr[xb-1] != ' ' && expr[xb-1] != '\t') --xb;
        std::string last_tok = to_lower(expr.substr(xb));
        char lastc = expr.empty() ? 0 : expr.back();
        bool expr_complete = std::isalnum((unsigned char)lastc) || lastc == '_' ||
                             lastc == ')' || lastc == '\'' || lastc == '"' || lastc == '`';
        std::string el = to_lower(expr);
        bool expr_ok = !expr.empty() && el != "distinct" && el != "all" && expr_complete &&
                       (last_tok == "end" || !is_sql_keyword(last_tok.c_str(), last_tok.size()));

        if (alias_ident && expr_ok &&
            !is_sql_keyword(alias.c_str(), alias.size())) {
            std::string as_kw = (ac == AliasCase::Upper) ? "AS" : "as";
            return expr + " " + as_kw + " " + alias;
        }
    }
    return col;
}

// Normalize the alias on a plain "<name> [as] <alias>" table reference to match
// alias_op. Conservative: only touches the exact 2- or 3-token shape, leaving
// anything with commas, extra keywords, parens or subqueries alone.
static std::string normalize_table_ref(const std::string& ref) {
    AliasOp op = g_settings.alias_op;
    if (op == AliasOp::LeaveAsIs) return ref;

    std::string r = rtrim(ltrim(ref));
    std::vector<std::string> tok;
    { std::string cur;
      for (char c : r) {
          if (c == ' ' || c == '\t') { if (!cur.empty()) { tok.push_back(cur); cur.clear(); } }
          else cur += c;
      }
      if (!cur.empty()) tok.push_back(cur);
    }

    auto is_name  = [](const std::string& s) {
        if (s.empty()) return false;
        for (char c : s) if (!(std::isalnum((unsigned char)c) || c == '_' || c == '.')) return false;
        return !std::isdigit((unsigned char)s[0]);
    };
    auto is_alias = [](const std::string& s) {
        if (s.empty() || !(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
        for (char c : s) if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
        return !is_sql_keyword(s.c_str(), s.size());
    };
    std::string as_kw = (g_settings.alias_case == AliasCase::Upper) ? "AS" : "as";

    if (tok.size() == 2 && is_name(tok[0]) && !is_sql_keyword(tok[0].c_str(), tok[0].size())
        && is_alias(tok[1])) {
        return op == AliasOp::Remove ? tok[0] + " " + tok[1]
                                     : tok[0] + " " + as_kw + " " + tok[1];
    }
    if (tok.size() == 3 && to_lower(tok[1]) == "as"
        && is_name(tok[0]) && !is_sql_keyword(tok[0].c_str(), tok[0].size())
        && is_alias(tok[2])) {
        return op == AliasOp::Remove ? tok[0] + " " + tok[2]
                                     : tok[0] + " " + as_kw + " " + tok[2];
    }
    return ref;
}

// Split s by ',' at paren/string depth 0
static std::vector<std::string> split_comma_aware(const std::string& s) {
    std::vector<std::string> parts;
    std::string cur;
    bool in_s = false; char sd = 0; int d = 0;
    for (char c : s) {
        if (in_s) { cur += c; if (c == sd) in_s = false; continue; }
        if (c == '\'' || c == '"') { in_s = true; sd = c; cur += c; continue; }
        if (c == '(') { d++; cur += c; continue; }
        if (c == ')') { d--; cur += c; continue; }
        if (c == ',' && d == 0) { parts.push_back(cur); cur.clear(); continue; }
        cur += c;
    }
    parts.push_back(cur);
    return parts;
}

// ─── pass 5: line-by-line postprocessing ──────────────────────────────────────

static std::string postprocess(const std::string& text) {
    auto lines = split_lines(text);
    std::vector<std::string> out;
    size_t i = 0;
    int join_end = -1;

    while (i < lines.size()) {
        std::string raw = rtrim(lines[i]);
        int base = 0;
        while (base < (int)raw.size() && (raw[base]==' '||raw[base]=='\t')) ++base;
        std::string s  = raw.substr(base);
        std::string sl = to_lower(s);

        if (s.empty()) { out.push_back(""); join_end = -1; ++i; continue; }

        // ── SELECT ─────────────────────────────────────────────────────────
        if (is_select_kw(sl)) {
            // First column is on the SELECT line
            std::string col1;
            if (sl.size() > 7) { col1 = s.substr(7); ++i; }
            else if (sl.size() == 7 && s[6] == ' ') { col1 = ""; ++i; }
            else {
                ++i;
                while (i < lines.size()) {
                    std::string nxt = lines[i];
                    size_t ns = nxt.find_first_not_of(" \t");
                    if (ns != std::string::npos) { col1 = nxt.substr(ns); break; }
                    ++i;
                }
                ++i;
            }
            col1 = rtrim(col1);
            // Peel off a leading DISTINCT ON (...) / DISTINCT / ALL quantifier so
            // process_alias sees only the first real column expression.
            std::string qual;
            {
                std::string ql = to_lower(col1);
                if (ql.rfind("distinct on", 0) == 0) {
                    size_t p = 11;
                    while (p < col1.size() && col1[p] == ' ') ++p;
                    if (p < col1.size() && col1[p] == '(') {
                        int d = 0; size_t q = p;
                        for (; q < col1.size(); ++q) {
                            if (col1[q] == '(') ++d;
                            else if (col1[q] == ')') { if (--d == 0) { ++q; break; } }
                        }
                        qual = rtrim(col1.substr(0, q));
                        col1 = ltrim(col1.substr(q));
                    }
                } else if (starts_word(ql, "distinct")) {
                    qual = col1.substr(0, 8); col1 = ltrim(col1.substr(8));
                } else if (starts_word(ql, "all")) {
                    qual = col1.substr(0, 3); col1 = ltrim(col1.substr(3));
                }
            }
            col1 = process_alias(col1);
            if (!qual.empty()) col1 = col1.empty() ? qual : qual + " " + col1;
            out.push_back(std::string(base, ' ') + "select " + col1);

            if (g_settings.split_cols) {
                // Collect column lines until next clause keyword
                bool comma_after = (g_settings.comma_pos == CommaPos::After);
                // col_pfx: spaces that align comma/content under first column of SELECT
                //   For comma_before: "     , " = 5 spaces + ", " (len 7)
                //   For comma_after:  "       " = 7 spaces
                std::string col_pfx(base + KW - 1, ' ');

                std::vector<std::string> col_lines;
                while (i < lines.size()) {
                    std::string cr = rtrim(lines[i]);
                    size_t cs = cr.find_first_not_of(" \t");
                    if (cs == std::string::npos) break;
                    std::string cs_ = cr.substr(cs);
                    std::string csl = to_lower(cs_);
                    if (match_clause(csl).len > 0 || is_select_kw(csl)) break;
                    cr = rtrim(cr);
                    // Strip leading ", " or " " (from comma_after splits)
                    std::string col_content;
                    if (!cs_.empty() && cs_[0] == ',')
                        col_content = (cs_.size() > 2 && cs_[1] == ' ') ? cs_.substr(2) : cs_.substr(1);
                    else
                        col_content = cs_;
                    col_content = rtrim(col_content);
                    col_lines.push_back(col_content);
                    ++i;
                }

                // Output columns
                for (size_t ci = 0; ci < col_lines.size(); ++ci) {
                    std::string cc = process_alias(col_lines[ci]);
                    if (comma_after) {
                        // comma at end of previous line
                        if (!out.empty()) out.back() += ',';
                        out.push_back(col_pfx + " " + cc);
                    } else {
                        out.push_back(col_pfx + ", " + cc);
                    }
                }
            }
            join_end = -1;
            continue;
        }

        // Helper: extract inner content of the '(...)' starting at paren_pos in s,
        // returning (inner, j) where j points past the closing ')'.
        auto extract_paren = [&](size_t paren_pos) -> std::pair<std::string, size_t> {
            std::string inner; size_t j = paren_pos + 1; int d2 = 1;
            bool in_s2 = false; char sd2 = 0;
            while (j < s.size() && d2 > 0) {
                char c2 = s[j];
                if (in_s2) { inner += c2; if (c2 == sd2) in_s2 = false; ++j; continue; }
                if (c2 == '\'' || c2 == '"') { in_s2 = true; sd2 = c2; inner += c2; ++j; continue; }
                if (c2 == '(') { d2++; inner += c2; ++j; continue; }
                if (c2 == ')') { --d2; if (d2 > 0) inner += c2; ++j; continue; }
                inner += c2; ++j;
            }
            return {inner, j};
        };

        // Helper: first '(' position, scanning from offset, skipping spaces only.
        auto find_paren = [&](size_t from) -> size_t {
            bool in_s2 = false; char sd2 = 0;
            for (size_t j = from; j < s.size(); ++j) {
                char c2 = s[j];
                if (in_s2) { if (c2 == sd2) in_s2 = false; continue; }
                if (c2 == '\'' || c2 == '"') { in_s2 = true; sd2 = c2; continue; }
                if (c2 == '(') return j;
                if (c2 != ' ') return std::string::npos; // non-space non-paren: stop
            }
            return std::string::npos;
        };

        // ── INSERT INTO ─────────────────────────────────────────────────────
        // Format: insert into table (first_col
        //                          , second_col
        //                          , last_col)
        // The '(' stays on the insert line; columns align under first_col;
        // closing ')' goes directly after the last column.
        if (starts_word(sl, "insert")) {
            std::string pd = g_settings.align_keywords ? pad_kw("insert") : "";
            // Full scan for '(' — find_paren stops at non-spaces which is wrong here
            size_t pp = std::string::npos;
            { bool in_s2 = false; char sd2 = 0;
              for (size_t j = 0; j < s.size(); ++j) {
                  char c2 = s[j];
                  if (in_s2) { if (c2 == sd2) in_s2 = false; continue; }
                  if (c2 == '\'' || c2 == '"') { in_s2 = true; sd2 = c2; continue; }
                  if (c2 == '(') { pp = j; break; }
              }
            }
            if (pp == std::string::npos) {
                out.push_back(std::string(base, ' ') + pd + s);
            } else {
                std::string header = rtrim(s.substr(0, pp));
                auto pk = extract_paren(pp);
                auto cols = split_comma_aware(pk.first);
                // Column position of '(' in output = base + pd + header + " ("
                int paren_col = (int)(base + pd.size() + header.size() + 1);
                // Continuation prefix: comma sits one left of '('
                std::string cont_pfx(paren_col - 1, ' ');
                if (cols.size() <= 1) {
                    std::string col = cols.empty() ? "" : ltrim(rtrim(cols[0]));
                    out.push_back(std::string(base, ' ') + pd + header + " (" + col + ")");
                } else {
                    out.push_back(std::string(base, ' ') + pd + header + " (" + ltrim(rtrim(cols[0])));
                    for (size_t ci = 1; ci < cols.size(); ++ci) {
                        std::string col = ltrim(rtrim(cols[ci]));
                        bool last = (ci == cols.size() - 1);
                        out.push_back(cont_pfx + ", " + col + (last ? ")" : ""));
                    }
                }
            }
            join_end = -1; ++i; continue;
        }

        // ── VALUES ──────────────────────────────────────────────────────────
        if (starts_word(sl, "values")) {
            std::string pd = g_settings.align_keywords ? pad_kw("values") : "";
            size_t pp = find_paren(6); // scan from after "values"
            if (pp == std::string::npos) {
                out.push_back(std::string(base, ' ') + pd + s);
            } else {
                auto pk = extract_paren(pp);
                auto vals = split_comma_aware(pk.first);
                std::string pfx(base + KW + 1, ' ');
                for (size_t vi = 0; vi < vals.size(); ++vi) {
                    if (vi == 0) out.push_back(std::string(base, ' ') + pd + "values ( " + ltrim(rtrim(vals[vi])));
                    else         out.push_back(pfx + ", " + ltrim(rtrim(vals[vi])));
                }
                out.push_back(pfx + ")");
            }
            join_end = -1; ++i; continue;
        }

        // ── UPDATE SET ──────────────────────────────────────────────────────
        if (starts_word(sl, "set") && (s.size() == 3 || s[3] == ' ')) {
            std::string rest = s.size() > 4 ? s.substr(4) : "";
            std::string pd = g_settings.align_keywords ? pad_kw("set") : "";
            auto items = split_comma_aware(rest);
            std::string cpfx(base + KW - 1, ' '); // 5 spaces for comma-before alignment
            bool any = false;
            for (auto& it : items) {
                std::string v = ltrim(rtrim(it));
                if (v.empty()) continue;
                if (!any) { out.push_back(std::string(base, ' ') + pd + "set " + v); any = true; }
                else       out.push_back(cpfx + ", " + v);
            }
            if (!any) out.push_back(std::string(base, ' ') + pd + s);
            join_end = -1; ++i; continue;
        }

        // ── CREATE TABLE/VIEW/INDEX etc. ────────────────────────────────────
        // If a '(' is found, expand the definition list (same pattern as INSERT).
        // If no '(', pass through — e.g. "create view v as" before a SELECT.
        if (starts_word(sl, "create")) {
            size_t pp = std::string::npos;
            { bool in_s2 = false; char sd2 = 0;
              for (size_t j = 0; j < s.size(); ++j) {
                  char c2 = s[j];
                  if (in_s2) { if (c2 == sd2) in_s2 = false; continue; }
                  if (c2 == '\'' || c2 == '"') { in_s2 = true; sd2 = c2; continue; }
                  if (c2 == '(') { pp = j; break; }
              }
            }
            if (pp == std::string::npos) {
                out.push_back(std::string(base, ' ') + s);
            } else {
                std::string header = rtrim(s.substr(0, pp));
                auto pk = extract_paren(pp);
                auto cols = split_comma_aware(pk.first);
                int paren_col = (int)(base + header.size() + 1);
                std::string cont_pfx(paren_col - 1, ' ');
                if (cols.size() <= 1) {
                    std::string col = cols.empty() ? "" : ltrim(rtrim(cols[0]));
                    out.push_back(std::string(base, ' ') + header + " (" + col + ")");
                } else {
                    out.push_back(std::string(base, ' ') + header + " (" + ltrim(rtrim(cols[0])));
                    for (size_t ci = 1; ci < cols.size(); ++ci) {
                        std::string col = ltrim(rtrim(cols[ci]));
                        bool last = (ci == cols.size() - 1);
                        out.push_back(cont_pfx + ", " + col + (last ? ")" : ""));
                    }
                }
            }
            join_end = -1; ++i; continue;
        }

        // ── DROP TABLE/VIEW/INDEX etc. ───────────────────────────────────────
        // Pass through without keyword alignment (drop = 4 chars, pad_kw adds 2 spaces
        // which looks odd for a statement-level keyword).
        if (starts_word(sl, "drop")) {
            out.push_back(std::string(base, ' ') + s);
            join_end = -1; ++i; continue;
        }

        // ── JOIN keywords ───────────────────────────────────────────────────
        KwInfo ki = match_clause(sl);
        if (ki.len > 0 && ki.is_join) {
            std::string kw, rest;
            split_kw(s, ki.len, kw, rest);
            // Strip INNER / OUTER if requested (comparisons are on lowercase)
            if (g_settings.strip_inner_outer) {
                if (kw == "inner join")      kw = "join";
                else if (kw == "left outer join")  kw = "left join";
                else if (kw == "right outer join") kw = "right join";
                else if (kw == "full outer join")  kw = "full join";
            }
            std::string prefix = std::string(base, ' ') + pad_kw(kw) + kw;
            join_end = (int)prefix.size();
            rest = normalize_table_ref(rest);
            out.push_back(prefix + (rest.empty() ? "" : " " + rest));
            ++i; continue;
        }

        // ── ON / AND / OR inside JOIN ────────────────────────────────────────
        if (join_end >= 0 && is_on_and(sl)) {
            size_t sp = s.find(' ');
            std::string kw   = sp == std::string::npos ? s : s.substr(0, sp);
            std::string rest = sp == std::string::npos ? "" : s.substr(sp+1);
            int pad = std::max(0, join_end - base - (int)kw.size());
            if (!g_settings.align_join_on) pad = 0;
            out.push_back(std::string(base, ' ') + std::string(pad, ' ') + kw
                          + (rest.empty() ? "" : " " + rest));
            ++i; continue;
        }

        // ── Other clause keywords ────────────────────────────────────────────
        if (ki.len > 0) {
            std::string kw, rest;
            split_kw(s, ki.len, kw, rest);
            if (kw == "from") rest = normalize_table_ref(rest);
            std::string pd = g_settings.align_keywords ? pad_kw(kw) : "";
            out.push_back(std::string(base, ' ') + pd + kw
                          + (rest.empty() ? "" : " " + rest));
            join_end = -1;
            ++i; continue;
        }

        // ── AND / OR in WHERE (not after JOIN) ───────────────────────────────
        if (join_end < 0 && is_on_and(sl)) {
            size_t sp = s.find(' ');
            std::string kw   = sp == std::string::npos ? s : s.substr(0, sp);
            std::string rest = sp == std::string::npos ? "" : s.substr(sp+1);
            std::string pd = g_settings.align_keywords ? pad_kw(kw) : "";
            out.push_back(std::string(base, ' ') + pd + kw
                          + (rest.empty() ? "" : " " + rest));
            ++i; continue;
        }

        // ── pass-through ─────────────────────────────────────────────────────
        out.push_back(raw);
        ++i;
    }

    // Semicolon positioning
    std::string result = join_lines(out);
    switch (g_settings.semi_pos) {
    case SemicolonPos::Preserve:
        break; // leave as-is
    case SemicolonPos::SameLine: {
        // Move standalone semicolons onto previous line
        std::string tmp; tmp.reserve(result.size());
        for (size_t pos = 0; pos < result.size(); ++pos) {
            if (result[pos] == '\n') {
                size_t j = pos + 1;
                while (j < result.size() && (result[j]==' '||result[j]=='\t')) ++j;
                if (j < result.size() && result[j] == ';') {
                    tmp += ';'; pos = j; continue; // skip the newline, consume ';'
                }
            }
            tmp += result[pos];
        }
        result = tmp;
        break;
    }
    case SemicolonPos::OwnLine: {
        // Move inline semicolons onto their own line
        std::string tmp; tmp.reserve(result.size());
        for (size_t pos = 0; pos < result.size(); ++pos) {
            if (result[pos] == ';') {
                // Remove spaces before the semicolon on this line, put semicolon on new line
                while (!tmp.empty() && tmp.back() == ' ') tmp.pop_back();
                tmp += '\n';
                tmp += ';';
                // Skip any spaces after the semicolon (but keep newline)
                while (pos+1 < result.size() && result[pos+1] == ' ') ++pos;
            } else {
                tmp += result[pos];
            }
        }
        result = tmp;
        break;
    }
    case SemicolonPos::Remove: {
        std::string tmp; tmp.reserve(result.size());
        for (size_t pos = 0; pos < result.size(); ++pos) {
            if (result[pos] == ';') {
                while (!tmp.empty() && tmp.back() == ' ') tmp.pop_back();
                while (pos+1 < result.size() && result[pos+1] == ' ') ++pos;
            } else {
                tmp += result[pos];
            }
        }
        result = tmp;
        break;
    }
    }

    // Blank lines between clause groups
    if (g_settings.blank_between) {
        auto rlines = split_lines(result);
        std::vector<std::string> blines;
        for (size_t ri = 0; ri < rlines.size(); ++ri) {
            std::string sl2 = to_lower(rtrim(rlines[ri]));
            size_t ns = sl2.find_first_not_of(" \t");
            std::string sl2t = ns == std::string::npos ? "" : sl2.substr(ns);
            bool is_clause = is_select_kw(sl2t) || match_clause(sl2t).len > 0;
            if (is_clause && ri > 0 && !blines.empty() && !blines.back().empty())
                blines.push_back("");
            blines.push_back(rlines[ri]);
        }
        result = join_lines(blines);
    }

    // Collapse or remove consecutive blank lines
    if (g_settings.blank_lines != BlankLines::Preserve) {
        int max_bl = (g_settings.blank_lines == BlankLines::Remove) ? 0 : 1;
        std::string tmp; tmp.reserve(result.size());
        int nl = 0;
        for (char c : result) {
            if (c == '\n') { if (++nl <= max_bl + 1) tmp += c; }
            else { nl = 0; tmp += c; }
        }
        result = tmp;
    }

    return result;
}

// ─── pass 6: apply final case (kw_case / fn_case) ────────────────────────────
// At this point the text has lowercase keywords. We apply the user's case choice.
// Special rule for `as`: if followed by an identifier (non-keyword), use alias_case.
// Otherwise (followed by keyword or `(`), use kw_case.

static std::string apply_final_case(const std::string& sql) {
    KeywordCase kc = g_settings.kw_case;
    KeywordCase fc = g_settings.fn_case;
    if (kc == KeywordCase::Preserve && fc == KeywordCase::Preserve) return sql;

    std::string out;
    out.reserve(sql.size());
    size_t i = 0;

    while (i < sql.size()) {
        char c = sql[i];
        if (copy_verbatim_run(sql, i, out)) continue;
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t start = i;
            while (i < sql.size() && (std::isalnum((unsigned char)sql[i]) || sql[i] == '_')) ++i;
            size_t len = i - start;
            std::string word = sql.substr(start, len);

            // Check if followed by `(` (function call)
            size_t j = i;
            while (j < sql.size() && (sql[j] == ' ' || sql[j] == '\t')) ++j;
            bool is_fn_call = (j < sql.size() && sql[j] == '(');

            bool is_kw = is_sql_keyword(word.c_str(), len);

            if (is_fn_call && !is_kw) {
                for (char& ch : word) ch = (fc == KeywordCase::Lower) ? lc(ch) :
                                           (fc == KeywordCase::Upper)  ? uc_(ch) : ch;
                out += word;
            } else if (is_kw) {
                // For `as`: check what follows to decide alias_case vs kw_case
                if (word == "as" && g_settings.alias_op != AliasOp::LeaveAsIs) {
                    // Skip to next non-space
                    size_t nxt = i;
                    while (nxt < sql.size() && (sql[nxt]==' '||sql[nxt]=='\t')) ++nxt;
                    bool next_is_kw = false;
                    bool next_is_paren = (nxt < sql.size() && sql[nxt] == '(');
                    if (nxt < sql.size() && std::isalpha((unsigned char)sql[nxt])) {
                        size_t ws = nxt;
                        while (ws < sql.size() && (std::isalnum((unsigned char)sql[ws]) || sql[ws]=='_')) ++ws;
                        next_is_kw = is_sql_keyword(sql.c_str()+nxt, ws-nxt);
                    }
                    KeywordCase as_case = (next_is_kw || next_is_paren) ? kc :
                        (g_settings.alias_case == AliasCase::Upper ? KeywordCase::Upper : KeywordCase::Lower);
                    for (char& ch : word) ch = (as_case==KeywordCase::Upper) ? uc_(ch) : lc(ch);
                    out += word;
                } else {
                    for (char& ch : word) ch = (kc==KeywordCase::Lower) ? lc(ch) :
                                               (kc==KeywordCase::Upper)  ? uc_(ch) : ch;
                    out += word;
                }
            } else {
                // Plain identifier: apply id_case
                IdentCase ic = g_settings.id_case;
                for (char& ch : word) ch = (ic == IdentCase::Lower) ? lc(ch) :
                                           (ic == IdentCase::Upper)  ? uc_(ch) : ch;
                out += word;
            }
            continue;
        }
        out += c; ++i;
    }
    return out;
}

// ─── pass 7: re-indent inline subquery content ───────────────────────────────
// After postprocess the outer query is aligned, but subquery lines (which appear
// on lines after "(select ...") still have outer-level indentation. This pass
// finds "(select" on a line, determines the column of 'select', and re-indents
// every subsequent line in that subquery block accordingly.

static std::string format_subqueries(const std::string& text) {
    auto lines = split_lines(text);
    std::vector<std::string> out;
    size_t i = 0;

    while (i < lines.size()) {
        const std::string& line = lines[i];

        // Find '(select' on this line (outside strings)
        size_t paren_pos = std::string::npos;
        {
            bool in_str2 = false; char sd2 = 0;
            for (size_t k = 0; k < line.size(); ++k) {
                char c = line[k];
                if (in_str2) { if (c == sd2 && !(k+1<line.size() && line[k+1]==sd2)) in_str2=false; continue; }
                if (c=='\''||c=='"') { in_str2=true; sd2=c; continue; }
                if (c == '(') {
                    size_t j = k + 1;
                    while (j < line.size() && line[j] == ' ') ++j;
                    if (j+6 <= line.size() && to_lower(line.substr(j,6)) == "select"
                        && (j+6 >= line.size() || word_boundary(line[j+6]))) {
                        paren_pos = k; break;
                    }
                }
            }
        }

        if (paren_pos == std::string::npos) { out.push_back(line); ++i; continue; }

        int subq_col = (int)paren_pos + 1;  // column where 'select' starts

        // Find the matching ')' scanning forward through lines
        size_t close_line = std::string::npos, close_pos2 = std::string::npos;
        {
            int depth = 1;
            bool in_str2 = false; char sd2 = 0;
            for (size_t j = i; j < lines.size() && close_line == std::string::npos; ++j) {
                size_t ks = (j == i) ? paren_pos + 1 : 0;
                for (size_t k = ks; k < lines[j].size(); ++k) {
                    char c = lines[j][k];
                    if (in_str2) { if (c==sd2&&!(k+1<lines[j].size()&&lines[j][k+1]==sd2)) in_str2=false; continue; }
                    if (c=='\''||c=='"') { in_str2=true; sd2=c; continue; }
                    if (c=='(') depth++;
                    else if (c==')') { if (--depth==0) { close_line=j; close_pos2=k; break; } }
                }
            }
        }

        if (close_line == std::string::npos) { out.push_back(line); ++i; continue; }

        // Re-indent helper: take a raw line and apply subquery keyword alignment.
        auto reindent_line = [&](const std::string& raw, const std::string& suffix) -> std::string {
            size_t ns = raw.find_first_not_of(" \t");
            if (ns == std::string::npos) return suffix;
            std::string content = rtrim(raw.substr(ns));
            std::string cl = to_lower(content);

            if (!content.empty() && content[0] == ',') {
                std::string col = (content.size()>2 && content[1]==' ') ? content.substr(2) : content.substr(1);
                col = process_alias(rtrim(col));
                return std::string(subq_col + KW - 1, ' ') + ", " + col + suffix;
            }
            KwInfo ki = match_clause(cl);
            if (ki.len > 0) {
                std::string kw, rest; split_kw(content, ki.len, kw, rest);
                if (kw == "from" || ki.is_join) rest = normalize_table_ref(rest);
                std::string pd = g_settings.align_keywords ? pad_kw(kw) : "";
                return std::string(subq_col, ' ') + pd + kw + (rest.empty() ? "" : " " + rest) + suffix;
            }
            if (is_on_and(cl)) {
                size_t sp = content.find(' ');
                std::string kw = sp==std::string::npos ? content : content.substr(0, sp);
                std::string rest = sp==std::string::npos ? "" : content.substr(sp+1);
                std::string pd = g_settings.align_keywords ? pad_kw(kw) : "";
                return std::string(subq_col, ' ') + pd + kw + (rest.empty() ? "" : " " + rest) + suffix;
            }
            return std::string(subq_col, ' ') + content + suffix;
        };

        // The opening line stays as-is (outer clause + '(select ...' already aligned)
        out.push_back(line);

        // Re-indent middle lines
        for (size_t j = i + 1; j < close_line; ++j)
            out.push_back(reindent_line(lines[j], ""));

        // Re-indent the close line: content before ')' + ')' + tail
        if (close_line > i) {
            std::string cl = lines[close_line];
            std::string before = rtrim(cl.substr(0, close_pos2));
            std::string from_close = cl.substr(close_pos2);  // ')' + tail
            if (rtrim(before).empty())
                out.back() += from_close;  // no content before ')': attach to previous line
            else
                out.push_back(reindent_line(before, from_close));
        }

        i = close_line + 1;
    }

    return join_lines(out);
}

// ─── pass 8: window function formatting ──────────────────────────────────────
// Expands OVER (PARTITION BY ... ORDER BY ...) inside SELECT column lines.
// Only runs when split_cols is enabled (columns are on separate lines).

static bool parse_window_spec(const std::string& inner,
                               std::string& part_str,
                               std::string& order_str,
                               std::string& frame_str) {
    part_str.clear(); order_str.clear(); frame_str.clear();
    struct Mark { size_t pos; size_t kw_len; int type; }; // 1=partition by, 2=order by, 3=frame
    std::vector<Mark> marks;

    std::string sl = to_lower(inner);
    bool in_s = false; char sd = 0; int d = 0;
    for (size_t i = 0; i < sl.size(); ) {
        char c = sl[i];
        if (in_s) { if (c == sd) in_s = false; ++i; continue; }
        if (c == '\'' || c == '"') { in_s = true; sd = c; ++i; continue; }
        if (c == '(') { d++; ++i; continue; }
        if (c == ')') { d--; ++i; continue; }
        if (d != 0) { ++i; continue; }
        bool wb = (i == 0) || word_boundary(sl[i-1]);
        if (wb) {
            auto mat = [&](const char* kw) -> bool {
                size_t n = strlen(kw);
                return i + n <= sl.size() && sl.compare(i, n, kw) == 0 &&
                       (i + n >= sl.size() || word_boundary(sl[i + n]));
            };
            if (mat("partition by")) { marks.push_back({i, 12, 1}); i += 12; continue; }
            if (mat("order by"))     { marks.push_back({i, 8,  2}); i += 8;  continue; }
            if (mat("rows"))         { marks.push_back({i, 4,  3}); i += 4;  continue; }
            if (mat("range"))        { marks.push_back({i, 5,  3}); i += 5;  continue; }
        }
        ++i;
    }

    bool has_pb = false, has_ob = false;
    for (auto& m : marks) { if (m.type == 1) has_pb = true; if (m.type == 2) has_ob = true; }
    if (!has_pb && !has_ob) return false;

    for (size_t mi = 0; mi < marks.size(); ++mi) {
        if (marks[mi].type == 3) {
            frame_str = rtrim(inner.substr(marks[mi].pos));
            break;
        }
        size_t cs = marks[mi].pos + marks[mi].kw_len;
        while (cs < inner.size() && inner[cs] == ' ') ++cs;
        size_t ce = (mi + 1 < marks.size()) ? marks[mi + 1].pos : inner.size();
        std::string content = rtrim(inner.substr(cs, ce - cs));
        if      (marks[mi].type == 1) part_str  = content;
        else if (marks[mi].type == 2) order_str = content;
    }
    return true;
}

static std::string format_window_fns(const std::string& text) {
    if (!g_settings.split_cols) return text;

    auto lines = split_lines(text);
    std::vector<std::string> out;

    for (const auto& line : lines) {
        bool in_s = false; char sd = 0;
        size_t over_pos = std::string::npos;
        size_t paren_open = std::string::npos;

        for (size_t i = 0; i < line.size(); ) {
            char c = line[i];
            if (!in_s && c == '-' && i+1 < line.size() && line[i+1] == '-') break;
            if (in_s) { if (c == sd) in_s = false; ++i; continue; }
            if (c == '\'' || c == '"') { in_s = true; sd = c; ++i; continue; }
            bool wb = (i == 0) || word_boundary(line[i-1]);
            if (wb && i + 4 <= line.size() &&
                lc(line[i])=='o' && lc(line[i+1])=='v' &&
                lc(line[i+2])=='e' && lc(line[i+3])=='r' &&
                (i + 4 == line.size() || word_boundary(line[i+4]))) {
                size_t j = i + 4;
                while (j < line.size() && line[j] == ' ') ++j;
                if (j < line.size() && line[j] == '(') {
                    over_pos = i; paren_open = j; break;
                }
            }
            ++i;
        }

        if (over_pos == std::string::npos) { out.push_back(line); continue; }

        // Extract inner content of OVER(...)
        std::string inner;
        size_t j = paren_open + 1; int d = 1;
        bool in_s2 = false; char sd2 = 0;
        while (j < line.size() && d > 0) {
            char c2 = line[j];
            if (in_s2) { inner += c2; if (c2 == sd2) in_s2 = false; ++j; continue; }
            if (c2 == '\'' || c2 == '"') { in_s2 = true; sd2 = c2; inner += c2; ++j; continue; }
            if (c2 == '(') { d++; inner += c2; ++j; continue; }
            if (c2 == ')') { --d; if (d > 0) inner += c2; ++j; continue; }
            inner += c2; ++j;
        }
        size_t after_close = j;

        std::string part_str, order_str, frame_str;
        if (!parse_window_spec(ltrim(rtrim(inner)), part_str, order_str, frame_str)) {
            out.push_back(line); continue;
        }

        int paren_col   = (int)paren_open;  // column of '('
        int content_col = paren_col + 1;    // column of content right after '(' (no space)

        std::string prefix = line.substr(0, over_pos);
        std::string suffix = after_close < line.size() ? line.substr(after_close) : "";

        // Partition BY section
        if (!part_str.empty()) {
            auto cols = split_comma_aware(part_str);
            out.push_back(prefix + "over (partition by " + ltrim(rtrim(cols[0])));
            // Continuation cols: comma 2 chars left of first_part_col = content_col + 13 - 2
            std::string cont(content_col + 11, ' ');
            for (size_t ci = 1; ci < cols.size(); ++ci)
                out.push_back(cont + ", " + ltrim(rtrim(cols[ci])));
        }

        // ORDER BY section
        if (!order_str.empty()) {
            auto cols = split_comma_aware(order_str);
            if (part_str.empty()) {
                out.push_back(prefix + "over (order by " + ltrim(rtrim(cols[0])));
            } else {
                out.push_back(std::string(content_col, ' ') + "order by " + ltrim(rtrim(cols[0])));
            }
            // Continuation cols: comma 2 chars left of first_order_col = content_col + 9 - 2
            std::string cont(content_col + 7, ' ');
            for (size_t ci = 1; ci < cols.size(); ++ci)
                out.push_back(cont + ", " + ltrim(rtrim(cols[ci])));
        }

        // Frame clause (rows/range between ...)
        if (!frame_str.empty())
            out.push_back(std::string(content_col, ' ') + frame_str);

        // Closing ')': attach to the end of the last line, no leading space,
        // followed by whatever came after OVER(...) (" as rn" etc.).
        out.back() += ")";
        out.back() += suffix;
    }

    return join_lines(out);
}

// ─── minify: collapse SQL to a single line ───────────────────────────────────
// Strips all newlines and extra whitespace; drops line and block comments;
// preserves string literals verbatim.

std::string minify_sql(const std::string& sql) {
    std::string out;
    out.reserve(sql.size());
    bool sp = false; // deferred space

    size_t i = 0;
    while (i < sql.size()) {
        char c = sql[i];

        size_t n = verbatim_run_len(sql, i);
        if (n) {
            bool is_comment = (c == '/' && sql[i+1] == '*') || (c == '-' && sql[i+1] == '-');
            if (is_comment) { i += n; continue; }  // comments: drop entirely
            // Quoted literal ('/"/`): copy verbatim, same as elsewhere in the pipeline.
            if (sp && !out.empty()) { out += ' '; sp = false; }
            out.append(sql, i, n);
            i += n;
            continue;
        }

        // Whitespace → deferred single space
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!out.empty()) sp = true;
            ++i; continue;
        }

        // Regular character
        if (sp) { out += ' '; sp = false; }
        out += c; ++i;
    }

    return out;
}

// ─── FQDN qualification ───────────────────────────────────────────────────────

static std::string qualify_object_names(const std::string& sql) {
    std::string prefix;
    if (!g_settings.fqdn_db.empty())     prefix  = g_settings.fqdn_db + ".";
    if (!g_settings.fqdn_schema.empty()) prefix += g_settings.fqdn_schema + ".";
    if (prefix.empty()) return sql;

    // Collect CTE names to avoid qualifying them
    std::vector<std::string> ctes;
    {
        std::string sl = to_lower(sql);
        size_t pos = 0;
        while ((pos = sl.find(" as (", pos)) != std::string::npos) {
            size_t end = pos;
            while (end > 0 && sl[end - 1] == ' ') --end;
            size_t start = end;
            while (start > 0 && (std::isalnum((unsigned char)sl[start - 1]) || sl[start - 1] == '_')) --start;
            if (start < end) ctes.push_back(sl.substr(start, end - start));
            ++pos;
        }
    }

    auto is_cte = [&](const std::string& name) {
        std::string nl = to_lower(name);
        for (const auto& c : ctes) if (c == nl) return true;
        return false;
    };
    auto is_id = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };

    auto lines = split_lines(sql);
    for (auto& line : lines) {
        std::string sl = to_lower(line);
        for (const char* kw : {"from ", "join ", "into ", "update "}) {
            size_t klen = strlen(kw);
            size_t pos = sl.find(kw);
            if (pos == std::string::npos) continue;
            if (pos > 0 && is_id(sl[pos - 1])) continue;  // word boundary

            size_t istart = pos + klen;
            while (istart < sl.size() && sl[istart] == ' ') ++istart;
            if (istart >= sl.size() || sl[istart] == '(' || sl[istart] == '-') continue;

            size_t iend = istart;
            while (iend < sl.size() && (is_id(sl[iend]) || sl[iend] == '.')) ++iend;

            if (iend == istart) continue;
            std::string id = sl.substr(istart, iend - istart);
            if (id.find('.') != std::string::npos) continue;  // already qualified
            if (is_cte(id)) continue;

            line = line.substr(0, istart) + prefix + line.substr(istart);
            break;
        }
    }
    return join_lines(lines);
}

// ─── add missing semicolons ───────────────────────────────────────────────────

static std::string add_semicolons(const std::string& sql) {
    if (!g_settings.semi_add) return sql;
    if (g_settings.semi_pos == SemicolonPos::Remove) return sql;

    static const char* STMT_KWS[] = {
        "select", "insert", "update", "delete", "create", "drop",
        "with", "merge", "truncate", "alter", "begin", "commit", "rollback",
        nullptr
    };

    auto ends_with_semi = [](const std::string& line) {
        for (int i = (int)line.size() - 1; i >= 0; --i)
            if (line[i] != ' ' && line[i] != '\t') return line[i] == ';';
        return false;
    };

    auto is_stmt_start = [&](const std::string& line) -> bool {
        size_t sp = 0;
        while (sp < line.size() && line[sp] == ' ') ++sp;
        if (sp > (size_t)KW) return false;
        if (sp + 1 < line.size() && line[sp] == '-' && line[sp + 1] == '-') return false;
        std::string sl = to_lower(line.substr(sp));
        for (const char** kw = STMT_KWS; *kw; ++kw)
            if (starts_word(sl, *kw)) return true;
        return false;
    };

    auto line_type = [](const std::string& line) -> int {
        // 0=empty, 1=comment, 2=content
        for (size_t j = 0; j < line.size(); ++j) {
            if (line[j] == ' ' || line[j] == '\t') continue;
            if (j + 1 < line.size() && line[j] == '-' && line[j + 1] == '-') return 1;
            return 2;
        }
        return 0;
    };

    auto lines = split_lines(sql);
    std::vector<std::string> out;
    out.reserve(lines.size() + 4);

    size_t last_content = std::string::npos;
    bool   in_stmt      = false;

    auto flush_semi = [&]() {
        if (!in_stmt || last_content == std::string::npos) return;
        if (ends_with_semi(out[last_content])) { in_stmt = false; return; }
        if (g_settings.semi_pos == SemicolonPos::OwnLine) {
            out.insert(out.begin() + (ptrdiff_t)last_content + 1, ";");
        } else {
            std::string& l = out[last_content];
            while (!l.empty() && (l.back() == ' ' || l.back() == '\t')) l.pop_back();
            l += ';';
        }
        last_content = std::string::npos;
        in_stmt = false;
    };

    for (const auto& line : lines) {
        if (is_stmt_start(line)) {
            flush_semi();
            in_stmt = true;
            out.push_back(line);
            last_content = out.size() - 1;
        } else {
            out.push_back(line);
            if (in_stmt && line_type(line) == 2)
                last_content = out.size() - 1;
        }
    }
    flush_semi();

    return join_lines(out);
}

// ─── pass: align AS keywords in SELECT lists ──────────────────────────────────
// Only applies to comma-before style; expression start is always column KW+1=7.

static std::string align_select_aliases(const std::string& sql) {
    if (!g_settings.align_aliases) return sql;
    if (!g_settings.split_cols || g_settings.comma_pos != CommaPos::Before) return sql;

    static const size_t EXPR_COL = KW + 1;  // 7: "select " or "     , "

    // Return column index of 'a' in ' as ' at depth-0, or npos.
    auto find_as_col = [](const std::string& line) -> size_t {
        bool in_s = false; char sd = 0; int depth = 0;
        std::string sl = to_lower(line);
        for (size_t i = EXPR_COL; i + 4 <= sl.size(); ++i) {
            char c = sl[i];
            if (in_s) { if (c == sd) in_s = false; continue; }
            if (c == '\'' || c == '"') { in_s = true; sd = c; continue; }
            if (c == '(') { ++depth; continue; }
            if (c == ')') { --depth; continue; }
            if (depth == 0 && c == ' ' && sl.compare(i + 1, 3, "as ") == 0)
                return i + 1;  // position of 'a' in 'as'
        }
        return std::string::npos;
    };

    auto lines = split_lines(sql);
    bool in_block = false;
    size_t block_start = 0, block_max_as = 0;

    auto process_block = [&](size_t end) {
        if (!in_block || block_max_as == 0) { in_block = false; block_max_as = 0; return; }
        for (size_t li = block_start; li < end; ++li) {
            std::string& line = lines[li];
            if (line.size() < EXPR_COL) continue;
            bool is_first = (li == block_start);
            bool is_cont  = (line.substr(0, EXPR_COL) == "     , ");
            if (!is_first && !is_cont) continue;
            size_t ac = find_as_col(line);
            if (ac == std::string::npos || ac >= block_max_as) continue;
            size_t extra = block_max_as - ac;
            if (ac > 0 && line[ac - 1] == ' ')
                line.insert(ac - 1, extra, ' ');
        }
        in_block = false; block_max_as = 0;
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        if (line.size() < EXPR_COL) { process_block(i); continue; }
        std::string sl = to_lower(line);
        bool is_select = starts_word(sl, "select");
        bool is_cont   = (line.substr(0, EXPR_COL) == "     , ");

        if (is_select) {
            process_block(i);
            in_block = true; block_start = i;
            size_t ac = find_as_col(line);
            if (ac != std::string::npos && ac > block_max_as) block_max_as = ac;
        } else if (in_block && is_cont) {
            size_t ac = find_as_col(line);
            if (ac != std::string::npos && ac > block_max_as) block_max_as = ac;
        } else {
            process_block(i);
        }
    }
    process_block(lines.size());

    return join_lines(lines);
}

// ─── pass: align THEN columns in expanded CASE blocks ────────────────────────

static std::string align_case_when_cols(const std::string& sql) {
    if (!g_settings.align_case_when) return sql;
    if (g_settings.case_stmt != CaseStmt::ExpandJoined) return sql;
    // expand_case already puts WHEN...THEN on one line; just align THEN keywords.

    auto find_then_col = [](const std::string& line) -> size_t {
        bool in_s = false; char sd = 0; int depth = 0;
        std::string sl = to_lower(line);
        for (size_t j = 0; j + 5 <= sl.size(); ++j) {
            char c = sl[j];
            if (in_s) { if (c == sd) in_s = false; continue; }
            if (c == '\'' || c == '"') { in_s = true; sd = c; continue; }
            if (c == '(') { ++depth; continue; }
            if (c == ')') { --depth; continue; }
            if (depth == 0 && c == ' ' && sl.compare(j + 1, 5, "then ") == 0)
                return j + 1;
        }
        return std::string::npos;
    };

    auto find_case_kw = [](const std::string& line) -> size_t {
        bool in_s = false; char sd = 0;
        std::string sl = to_lower(line);
        for (size_t k = 0; k + 4 <= sl.size(); ++k) {
            char c = sl[k];
            if (in_s) { if (c == sd) in_s = false; continue; }
            if (c == '\'' || c == '"') { in_s = true; sd = c; continue; }
            bool pre  = (k == 0) || (!std::isalnum((unsigned char)sl[k-1]) && sl[k-1] != '_');
            bool post = (k+4 >= sl.size()) || (!std::isalnum((unsigned char)sl[k+4]) && sl[k+4] != '_');
            if (pre && post && sl.compare(k, 4, "case") == 0) return k;
        }
        return std::string::npos;
    };

    auto lines = split_lines(sql);
    size_t i = 0;
    while (i < lines.size()) {
        if (find_case_kw(lines[i]) == std::string::npos) { ++i; continue; }
        size_t case_line = i;
        ++i;

        std::vector<size_t> when_lines;
        std::vector<size_t> else_lines;

        if (to_lower(lines[case_line]).find(" when ") != std::string::npos)
            when_lines.push_back(case_line);

        while (i < lines.size()) {
            std::string lsl = to_lower(ltrim(lines[i]));
            if (starts_word(lsl, "end"))       { ++i; break; }
            if (starts_word(lsl, "when"))      when_lines.push_back(i);
            else if (starts_word(lsl, "else")) else_lines.push_back(i);
            ++i;
        }
        if (when_lines.empty()) continue;

        size_t max_then = 0;
        for (size_t wi : when_lines) {
            size_t p = find_then_col(lines[wi]);
            if (p != std::string::npos && p > max_then) max_then = p;
        }
        if (max_then == 0) continue;

        for (size_t wi : when_lines) {
            size_t p = find_then_col(lines[wi]);
            if (p == std::string::npos || p >= max_then) continue;
            if (p > 0 && lines[wi][p - 1] == ' ')
                lines[wi].insert(p - 1, max_then - p, ' ');
        }

        size_t val_col = max_then + 5;
        for (size_t ei : else_lines) {
            std::string& el = lines[ei];
            std::string esl = to_lower(el);
            size_t ep = esl.find("else");
            if (ep == std::string::npos) continue;
            size_t vs = ep + 4;
            while (vs < el.size() && el[vs] == ' ') ++vs;
            if (vs >= val_col) continue;
            el.insert(vs, val_col - vs, ' ');
        }
    }
    return join_lines(lines);
}

// ─── pass: wrap long IN (...) lists onto multiple lines ──────────────────────

static std::string wrap_in_lists(const std::string& sql) {
    if (g_settings.in_list_wrap <= 0) return sql;
    int threshold = g_settings.in_list_wrap;

    auto lines = split_lines(sql);
    std::vector<std::string> out;

    for (const auto& line : lines) {
        if ((int)line.size() <= threshold) { out.push_back(line); continue; }

        std::string sl = to_lower(line);
        size_t in_pos = sl.find(" in (");
        if (in_pos == std::string::npos) { out.push_back(line); continue; }

        // Verify not already inside a string
        bool in_str = false; char sd2 = 0;
        for (size_t k = 0; k < in_pos; ++k) {
            char c = line[k];
            if (in_str) { if (c == sd2) in_str = false; continue; }
            if (c == '\'' || c == '"') { in_str = true; sd2 = c; }
        }
        if (in_str) { out.push_back(line); continue; }

        size_t open_p = in_pos + 4;  // position of '('
        size_t close_p = std::string::npos;
        {
            int d = 1; in_str = false; sd2 = 0;
            for (size_t k = open_p + 1; k < line.size() && d > 0; ++k) {
                char c = line[k];
                if (in_str) { if (c == sd2) in_str = false; continue; }
                if (c == '\'' || c == '"') { in_str = true; sd2 = c; continue; }
                if (c == '(') ++d;
                else if (c == ')') { --d; if (!d) { close_p = k; break; } }
            }
        }
        if (close_p == std::string::npos) { out.push_back(line); continue; }

        // Skip subqueries
        std::string inner = line.substr(open_p + 1, close_p - open_p - 1);
        std::string il = to_lower(ltrim(inner));
        if (starts_word(il, "select")) { out.push_back(line); continue; }

        auto values = split_comma_aware(inner);
        if ((int)values.size() <= 1) { out.push_back(line); continue; }

        // Indent continuation lines so their values align under the first value.
        // First value starts at open_p+1; `, ` prefix is 2 chars → indent = open_p-1.
        std::string cont_indent(open_p > 1 ? open_p - 1 : 0, ' ');
        out.push_back(line.substr(0, open_p + 1) + ltrim(rtrim(values[0])));
        for (size_t vi = 1; vi < values.size(); ++vi)
            out.push_back(cont_indent + ", " + ltrim(rtrim(values[vi])));
        out.push_back(std::string(open_p, ' ') + ")" + line.substr(close_p + 1));
    }
    return join_lines(out);
}

// ─── top-level ────────────────────────────────────────────────────────────────

std::string format_sql(const std::string& sql) {
    bool crlf = sql.find("\r\n") != std::string::npos;

    // ── Jinja/dbt protection ─────────────────────────────────────────────────
    // Inline {{ expr }} and {# comment #} tokens are swapped for 'string' placeholders
    // so the pipeline doesn't corrupt them.  {% block %} lines pass through as-is.
    std::vector<std::string> jinja_toks;

    auto extract_jinja = [&](const std::string& s) -> std::string {
        if (s.find('{') == std::string::npos) return s;
        std::string o; o.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            if (s[i] == '{' && i + 1 < s.size()) {
                char t2 = s[i + 1];
                if (t2 == '{' || t2 == '#') {
                    char cl = (t2 == '{') ? '}' : '#';
                    size_t j = i + 2;
                    while (j + 1 < s.size() && !(s[j] == cl && s[j + 1] == '}')) ++j;
                    if (j + 1 < s.size()) {
                        jinja_toks.push_back(s.substr(i, j + 2 - i));
                        o += "'__" + std::to_string(jinja_toks.size() - 1) + "__'";
                        i = j + 2; continue;
                    }
                }
            }
            o += s[i++];
        }
        return o;
    };

    auto restore_jinja = [&](std::string s) -> std::string {
        for (size_t k = 0; k < jinja_toks.size(); ++k) {
            std::string ph = "'__" + std::to_string(k) + "__'";
            size_t pos;
            while ((pos = s.find(ph)) != std::string::npos)
                s.replace(pos, ph.size(), jinja_toks[k]);
        }
        return s;
    };

    auto is_jinja_block = [](const std::string& l) -> bool {
        size_t i = l.find_first_not_of(" \t");
        return i != std::string::npos && l.size() >= i + 2 && l[i] == '{' && l[i + 1] == '%';
    };

    bool has_jinja   = sql.find('{') != std::string::npos;
    std::string sql_work = has_jinja ? extract_jinja(sql) : sql;

    auto run_pipeline = [](const std::string& s) -> std::string {
        std::string t = normalize(s);
        t = tidy_spacing(t);
        t = apply_op_spacing(t);
        t = split_clauses(t);
        t = postprocess(t);
        t = format_subqueries(t);
        t = format_case_stmts(t);
        t = format_window_fns(t);
        t = apply_final_case(t);
        t = align_select_aliases(t);
        t = align_case_when_cols(t);
        t = wrap_in_lists(t);
        return t;
    };

    // ── No-format pragma + Jinja block lines ─────────────────────────────────
    bool has_pragma      = sql_work.find("@formatter:") != std::string::npos;
    bool needs_line_mode = has_pragma || has_jinja;

    std::string result;
    if (!needs_line_mode) {
        result = run_pipeline(sql_work);
    } else {
        auto lines = split_lines(sql_work);
        std::vector<std::string> out;
        std::vector<std::string> pending;
        bool fmt_on = true;

        auto is_pragma = [](const std::string& l, const char* kw) {
            size_t i = l.find_first_not_of(" \t\r");
            if (i == std::string::npos) return false;
            size_t n = strlen(kw);
            return l.size() >= i + n && l.compare(i, n, kw) == 0;
        };

        auto flush = [&]() {
            if (pending.empty()) return;
            std::string chunk = join_lines(pending);
            pending.clear();
            std::string fmt = run_pipeline(chunk);
            while (!fmt.empty() && fmt.back() == '\n') fmt.pop_back();
            auto fls = split_lines(fmt);
            out.insert(out.end(), fls.begin(), fls.end());
        };

        for (const auto& line : lines) {
            if (fmt_on && is_pragma(line, "-- @formatter:off")) {
                flush(); fmt_on = false; out.push_back(line);
            } else if (!fmt_on && is_pragma(line, "-- @formatter:on")) {
                out.push_back(line); fmt_on = true;
            } else if (fmt_on && has_jinja && is_jinja_block(line)) {
                flush(); out.push_back(line);
            } else if (fmt_on) {
                pending.push_back(line);
            } else {
                out.push_back(line);
            }
        }
        flush();
        result = join_lines(out);
    }

    result = add_semicolons(result);
    result = qualify_object_names(result);

    if (has_jinja) result = restore_jinja(result);

    if (crlf) {
        std::string tmp; tmp.reserve(result.size() + result.size() / 20);
        for (char c : result) { if (c == '\n') tmp += '\r'; tmp += c; }
        result = tmp;
    }
    return result;
}
