// Standalone console harness for the hub-and-spoke dialect converter.
// Not part of the shipped plugin. Built by build_harness.cmd, run by test.cmd.
#include "dialects.h"
#include "settings_core.h"
#include <iostream>
#include <string>

// formatter.cpp (linked in for the shared verbatim_run_len helper) references this.
FormatSettings g_settings;

struct Case {
    const char* name;
    Dialect     from;
    Dialect     to;
    const char* in;
    const char* want;
};

static const Case CASES[] = {
    // ── core rewrites, both directions ──────────────────────────────────────
    { "sf->mssql",
      Dialect::Snowflake, Dialect::MSSQL,
      "SELECT IFF(x, a, b), NVL(y, 0), LENGTH(s), SUBSTR(s, 1, 2)",
      "SELECT IIF(x, a, b), ISNULL(y, 0), LEN(s), SUBSTRING(s, 1, 2)" },

    { "mssql->sf",
      Dialect::MSSQL, Dialect::Snowflake,
      "SELECT IIF(x,a,b), ISNULL(y,0), LEN(s), GETDATE()",
      "SELECT IFF(x,a,b), NVL(y,0), LENGTH(s), CURRENT_TIMESTAMP" },

    { "pg->mssql",
      Dialect::PostgreSQL, Dialect::MSSQL,
      "SELECT LENGTH(s), NOW() FROM t",
      "SELECT LEN(s), GETDATE() FROM t" },

    { "mysql->sf",
      Dialect::MySQL, Dialect::Snowflake,
      "SELECT IF(x,a,b), IFNULL(y,0), NOW()",
      "SELECT IFF(x,a,b), NVL(y,0), CURRENT_TIMESTAMP" },

    { "mssql->mysql",
      Dialect::MSSQL, Dialect::MySQL,
      "SELECT ISNULL(a,b), IIF(x,y,z), GETDATE()",
      "SELECT IFNULL(a,b), IF(x,y,z), NOW()" },

    { "databricks->sf",
      Dialect::Databricks, Dialect::Snowflake,
      "SELECT COLLECT_LIST(x), COLLECT_SET(x), DATE_FORMAT(d,'y'), FROM_UNIXTIME(t)",
      "SELECT ARRAY_AGG(x), ARRAY_AGG(x), TO_CHAR(d,'y'), TO_TIMESTAMP(t)" },

    { "sf->databricks",
      Dialect::Snowflake, Dialect::Databricks,
      "SELECT ARRAY_AGG(x), TO_CHAR(d), TO_TIMESTAMP(t)",
      "SELECT COLLECT_LIST(x), DATE_FORMAT(d), FROM_UNIXTIME(t)" },

    // ── converting to ANSI is pure normalisation (no vendor spellings) ──────
    { "mssql->ansi",
      Dialect::MSSQL, Dialect::ANSI,
      "SELECT LEN(s), ISNULL(a,b), GETDATE()",
      "SELECT CHAR_LENGTH(s), COALESCE(a,b), CURRENT_TIMESTAMP" },

    // ── converting ANSI to a vendor produces that vendor's idiomatic form ───
    { "ansi->mssql idiomatic",
      Dialect::ANSI, Dialect::MSSQL,
      "SELECT COALESCE(a,b), CHAR_LENGTH(s)",
      "SELECT ISNULL(a,b), LEN(s)" },

    // ── same dialect is a no-op ────────────────────────────────────────────
    { "sf->sf noop",
      Dialect::Snowflake, Dialect::Snowflake,
      "SELECT NVL(y, 0), LENGTH(s)",
      "SELECT NVL(y, 0), LENGTH(s)" },

    // ── comments / strings / backtick identifiers are never rewritten ───────
    { "verbatim runs protected",
      Dialect::Snowflake, Dialect::MSSQL,
      "SELECT NVL(a,b), 'NVL(x)' AS t, `LENGTH` FROM t -- NVL(y) LENGTH(z)",
      "SELECT ISNULL(a,b), 'NVL(x)' AS t, `LENGTH` FROM t -- NVL(y) LENGTH(z)" },

    // ── word boundary: LEN( must not match inside a longer identifier ───────
    { "word boundary",
      Dialect::MSSQL, Dialect::ANSI,
      "SELECT MYLEN(x), LEN(y)",
      "SELECT MYLEN(x), CHAR_LENGTH(y)" },
};

int main() {
    int fails = 0;
    for (const auto& c : CASES) {
        std::string got = convert_dialect(c.in, c.from, c.to);
        if (got != c.want) {
            ++fails;
            std::cout << "FAIL  " << c.name << "\n"
                      << "  in:   " << c.in   << "\n"
                      << "  want: " << c.want << "\n"
                      << "  got:  " << got    << "\n";
        } else {
            std::cout << "ok    " << c.name << "\n";
        }
    }
    std::cout << (fails ? "\nDIALECT TESTS FAILED\n" : "\nall dialect tests passed\n");
    return fails ? 1 : 0;
}
