// Emscripten bindings for the SQL Formatter web playground on datamodder.com.
// Compiled ONLY by emcc (see build.cmd's WASM step), never by cl.exe - links
// the exact same formatter.cpp/dialects.cpp/settings_io.cpp used by the DLL
// and the CLI, so the website can never drift from the plugin's behavior.
#include <emscripten/emscripten.h>
#include "formatter.h"
#include "dialects.h"
#include "settings_io.h"
#include <cstring>
#include <cstdlib>
#include <string>

FormatSettings g_settings;

namespace {

// Every wasm_* function below that returns char* hands back a malloc'd
// buffer. The JS side MUST call formatsql_free() on it - ccall(..., 'string',
// ...) marshals the value back but does NOT give you the pointer to free, so
// the JS glue code has to use ccall(..., 'number', ...) + UTF8ToString +
// formatsql_free instead.
char* to_owned_cstr(const std::string& s) {
    char* out = (char*)malloc(s.size() + 1);
    if (out) memcpy(out, s.c_str(), s.size() + 1);
    return out;
}

Dialect dialect_from_slug(const char* s) {
    if (!s) return Dialect::ANSI;
    if (strcmp(s, "snowflake")  == 0) return Dialect::Snowflake;
    if (strcmp(s, "postgresql") == 0) return Dialect::PostgreSQL;
    if (strcmp(s, "mssql")      == 0) return Dialect::MSSQL;
    if (strcmp(s, "mysql")      == 0) return Dialect::MySQL;
    if (strcmp(s, "sqlite")     == 0) return Dialect::SQLite;
    if (strcmp(s, "databricks") == 0) return Dialect::Databricks;
    return Dialect::ANSI;
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
char* formatsql_format(const char* sql) {
    return to_owned_cstr(format_sql(sql ? sql : ""));
}

EMSCRIPTEN_KEEPALIVE
char* formatsql_minify(const char* sql) {
    return to_owned_cstr(minify_sql(sql ? sql : ""));
}

EMSCRIPTEN_KEEPALIVE
char* formatsql_convert_dialect(const char* sql, const char* from, const char* to) {
    std::string converted = convert_dialect(sql ? sql : "", dialect_from_slug(from), dialect_from_slug(to));
    return to_owned_cstr(converted);
}

EMSCRIPTEN_KEEPALIVE
void formatsql_load_settings_ini(const char* ini_text) {
    g_settings = settings_from_ini(ini_text ? ini_text : "");
}

EMSCRIPTEN_KEEPALIVE
char* formatsql_export_settings_ini(void) {
    return to_owned_cstr(settings_to_ini(g_settings));
}

EMSCRIPTEN_KEEPALIVE
void formatsql_reset_settings(void) {
    g_settings = FormatSettings{};
}

EMSCRIPTEN_KEEPALIVE
void formatsql_free(char* p) {
    free(p);
}

} // extern "C"
