#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

// ─── enums ────────────────────────────────────────────────────────────────────
enum class KeywordCase  { Preserve, Lower, Upper };
enum class IdentCase    { Preserve, Lower, Upper };
enum class CommaPos     { Before, After };
enum class AliasOp      { Add, Remove, LeaveAsIs };
enum class AliasCase    { Lower, Upper };
enum class CaseStmt     { Expand, ExpandJoined, Inline, LeaveAsIs };
enum class SpacesOpt    { Add, Remove, LeaveAsIs };
enum class SemicolonPos { Preserve, SameLine, OwnLine, Remove };
enum class BlankLines   { Preserve, Merge, Remove };
enum class Dialect      { ANSI, Snowflake, PostgreSQL, MSSQL, MySQL, SQLite, Databricks };

// ─── settings struct (defaults = user-confirmed values) ───────────────────────
struct FormatSettings {
    KeywordCase  kw_case           = KeywordCase::Lower;
    KeywordCase  fn_case           = KeywordCase::Lower;
    IdentCase    id_case           = IdentCase::Lower;

    bool         split_cols        = true;
    CommaPos     comma_pos         = CommaPos::Before;
    AliasOp      alias_op          = AliasOp::Add;
    AliasCase    alias_case        = AliasCase::Lower;

    bool         align_keywords    = true;
    bool         align_join_on     = true;

    CaseStmt     case_stmt         = CaseStmt::Expand;
    bool         cte_nl            = true;
    bool         cte_indent        = true;
    bool         sub_indent        = true;
    bool         semi_add          = false;
    SemicolonPos semi_pos          = SemicolonPos::SameLine;

    SpacesOpt    spc_operators     = SpacesOpt::Add;
    SpacesOpt    spc_functions     = SpacesOpt::LeaveAsIs;
    bool         blank_between     = false;
    BlankLines   blank_lines       = BlankLines::Merge;
    int          max_length        = 0;

    bool         strip_inner_outer = true;

    Dialect      dialect           = Dialect::ANSI;

    std::string  fqdn_db;
    std::string  fqdn_schema;

    bool         format_on_save   = false;
    bool         align_aliases    = false;
    bool         align_case_when  = false;
    int          in_list_wrap     = 0;    // 0 = disabled; wrap IN (...) lists longer than N chars
};

extern FormatSettings g_settings;

void load_settings();
void save_settings();
void show_settings_dialog(HWND parent);
void show_convert_quotes_dialog(HWND parent);
