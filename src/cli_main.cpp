// Standalone command-line SQL formatter, built from the exact same formatter
// engine (formatter.cpp/dialects.cpp) that powers the Notepad++ plugin DLL.
// Windows-only (uses <windows.h> for wildcard expansion) but never compiled
// into the DLL or the WASM module - see build.cmd.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "formatter.h"
#include "dialects.h"
#include "settings_io.h"
#include "resource.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

FormatSettings g_settings;

namespace {

void print_usage() {
    std::cout <<
        "Datamodder SQL Formatter CLI " << FORMATSQL_VERSION << "\n\n"
        "Usage: formatsql <file|wildcard>... [options]\n\n"
        "Options:\n"
        "  --ini <path>     Load formatting settings from an exported .ini file\n"
        "  --minify         Minify instead of format\n"
        "  --stdout         (default) Print result to stdout; original file untouched\n"
        "  --in-place       Overwrite the original file (must be given explicitly)\n"
        "  --suffix <suf>   Write \"<name><suf><ext>\" next to the original\n"
        "  --out-dir <dir>  Write each file under the same name in <dir>\n"
        "  --version        Print the version and exit\n"
        "  --help           Show this help and exit\n";
}

std::string read_file(const std::string& path, bool& ok) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { ok = false; return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    ok = true;
    return ss.str();
}

bool write_file(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return (bool)f;
}

// Splits "dir\name.ext" into ("dir\name", ".ext"), so --suffix can insert
// before the extension.
void split_ext(const std::string& path, std::string& stem, std::string& ext) {
    size_t slash = path.find_last_of("\\/");
    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        stem = path.substr(0, dot);
        ext = path.substr(dot);
    } else {
        stem = path;
        ext.clear();
    }
}

std::string base_name(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string dir_name(const std::string& path) {
    size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

bool is_wildcard(const std::string& s) {
    return s.find('*') != std::string::npos || s.find('?') != std::string::npos;
}

// cmd.exe does not expand *.sql itself - do it ourselves so "formatsql *.sql"
// behaves like a Unix shell would.
std::vector<std::string> expand_wildcard(const std::string& pattern) {
    std::vector<std::string> out;
    std::string dir = dir_name(pattern);
    WIN32_FIND_DATAA fd = {};
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        out.push_back(dir.empty() ? fd.cFileName : dir + "\\" + fd.cFileName);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> inputs;
    std::string ini_path, suffix, out_dir;
    bool minify = false, want_stdout = false, want_in_place = false, want_suffix = false, want_out_dir = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { print_usage(); return 0; }
        if (a == "--version") { std::cout << FORMATSQL_VERSION << "\n"; return 0; }
        if (a == "--minify") { minify = true; continue; }
        if (a == "--stdout") { want_stdout = true; continue; }
        if (a == "--in-place") { want_in_place = true; continue; }
        if (a == "--ini") {
            if (++i >= argc) { std::cerr << "error: --ini requires a path\n"; return 1; }
            ini_path = argv[i];
            continue;
        }
        if (a == "--suffix") {
            if (++i >= argc) { std::cerr << "error: --suffix requires a value\n"; return 1; }
            suffix = argv[i];
            want_suffix = true;
            continue;
        }
        if (a == "--out-dir") {
            if (++i >= argc) { std::cerr << "error: --out-dir requires a path\n"; return 1; }
            out_dir = argv[i];
            want_out_dir = true;
            continue;
        }
        if (!a.empty() && a[0] == '-' && a != "-") {
            std::cerr << "error: unknown option '" << a << "'\n";
            return 1;
        }
        inputs.push_back(a);
    }

    int output_modes = (int)want_stdout + (int)want_in_place + (int)want_suffix + (int)want_out_dir;
    if (output_modes > 1) {
        std::cerr << "error: --stdout, --in-place, --suffix and --out-dir are mutually exclusive\n";
        return 1;
    }
    if (output_modes == 0) want_stdout = true; // safe default: never touch the original

    if (inputs.empty()) {
        print_usage();
        return 1;
    }

    if (!ini_path.empty()) {
        bool ok = false;
        std::string ini_text = read_file(ini_path, ok);
        if (!ok) {
            std::cerr << "error: cannot read ini file '" << ini_path << "'\n";
            return 2;
        }
        g_settings = settings_from_ini(ini_text);
    }

    // Expand wildcards; a plain filename that doesn't exist is still passed
    // through so the per-file read gives a clear "cannot read" error below.
    std::vector<std::string> files;
    for (const auto& in : inputs) {
        if (is_wildcard(in)) {
            auto matches = expand_wildcard(in);
            if (matches.empty()) {
                std::cerr << "error: wildcard '" << in << "' matched no files\n";
                return 3;
            }
            for (auto& m : matches) files.push_back(m);
        } else {
            files.push_back(in);
        }
    }

    if (want_out_dir) CreateDirectoryA(out_dir.c_str(), nullptr);

    bool had_error = false;
    for (const auto& path : files) {
        bool ok = false;
        std::string src = read_file(path, ok);
        if (!ok) {
            std::cerr << "error: cannot read '" << path << "'\n";
            had_error = true;
            continue;
        }

        std::string result = minify ? minify_sql(src) : format_sql(src);

        if (want_stdout) {
            if (files.size() > 1) std::cout << "== " << path << " ==\n";
            std::cout << result;
            if (!result.empty() && result.back() != '\n') std::cout << "\n";
        } else if (want_in_place) {
            if (!write_file(path, result)) {
                std::cerr << "error: cannot write '" << path << "'\n";
                had_error = true;
            }
        } else if (want_suffix) {
            std::string stem, ext;
            split_ext(path, stem, ext);
            std::string out_path = stem + suffix + ext;
            if (!write_file(out_path, result)) {
                std::cerr << "error: cannot write '" << out_path << "'\n";
                had_error = true;
            } else {
                std::cerr << "formatted " << path << " -> " << out_path << "\n";
            }
        } else if (want_out_dir) {
            std::string out_path = out_dir + "\\" + base_name(path);
            if (!write_file(out_path, result)) {
                std::cerr << "error: cannot write '" << out_path << "'\n";
                had_error = true;
            } else {
                std::cerr << "formatted " << path << " -> " << out_path << "\n";
            }
        }
    }

    return had_error ? 2 : 0;
}
