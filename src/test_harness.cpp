// Standalone console harness for regression-testing formatter.cpp in isolation
// from the Notepad++ plugin DLL. Not part of the shipped plugin.
#include "formatter.h"
#include "settings.h"
#include <fstream>
#include <iostream>
#include <sstream>

FormatSettings g_settings;

static std::string read_file(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: test_harness <file.sql> [profile]\n";
        return 1;
    }
    std::string content = read_file(argv[1]);
    std::string profile = argc > 2 ? argv[2] : "default";

    if (profile == "alt") {
        g_settings.kw_case          = KeywordCase::Upper;
        g_settings.fn_case          = KeywordCase::Upper;
        g_settings.comma_pos        = CommaPos::After;
        g_settings.case_stmt        = CaseStmt::ExpandJoined;
        g_settings.align_aliases    = true;
        g_settings.align_case_when  = true;
        g_settings.in_list_wrap     = 60;
        g_settings.strip_inner_outer = false;
        g_settings.semi_add         = true;
        g_settings.blank_between    = true;
    }

    std::cout << format_sql(content);
    return 0;
}
