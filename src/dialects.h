#pragma once
#include <string>
#include "settings.h"   // Dialect

// ─── Dialect conversion (hub-and-spoke) ───────────────────────────────────────
// Every conversion goes through one canonical "hub" form:
//
//     from  ──normalise──▶  canonical  ──specialise──▶  to
//
// so each rewrite rule lives in exactly one per-dialect table instead of the
// old hand-written n² matrix of fn_x_to_y() functions. The canonical form is
// the ANSI-standard spelling where one exists (COALESCE, CHAR_LENGTH,
// SUBSTRING, CURRENT_TIMESTAMP, ARRAY_AGG), otherwise a neutral spelling for
// concepts ANSI has no standard for (IFF for the inline-if of IIF/IF).
//
// Comments and quoted literals ('...', "...", `...`) are never touched.
//
// Converting *to* ANSI is pure normalisation. Converting ANSI (or anything
// else) to a vendor dialect also produces that vendor's idiomatic spelling,
// e.g. ANSI COALESCE ▶ T-SQL ISNULL.
std::string convert_dialect(const std::string& sql, Dialect from, Dialect to);

// Human-readable dialect name for menus / messages ("Snowflake", "MS SQL", …).
const wchar_t* dialect_name(Dialect d);
