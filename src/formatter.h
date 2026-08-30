#pragma once
#include <string>

std::string format_sql(const std::string& sql);
std::string minify_sql(const std::string& sql);

// ─── verbatim runs: comments and quoted literals ──────────────────────────────
// Shared primitive for "skip over block comments, line comments, and quoted
// literals ('...', "...", `...`) without looking inside them". Used by the
// formatter passes and by the dialect converter so a reserved word used as a
// quoted identifier is never mistaken for the keyword itself.
size_t verbatim_run_len(const std::string& sql, size_t i);
bool   copy_verbatim_run(const std::string& sql, size_t& i, std::string& out);
bool   skip_verbatim_run(const std::string& sql, size_t& i);
