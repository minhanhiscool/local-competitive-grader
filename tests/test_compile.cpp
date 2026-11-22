#include "compile.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("compile - compilation results", "[compile]") {
    auto tempDir = fs::temp_directory_path() / "grade_test_compile";
    fs::create_directories(tempDir);

    Config config;

    SECTION("Successful compilation of valid C++ source") {
        auto validSource = tempDir / "valid.cpp";
        {
            std::ofstream out(validSource);
            out << "int main() { return 0; }\n";
        }

        auto result = compile(config, validSource);
        REQUIRE(result.has_value());
        REQUIRE(fs::exists(result->binary));

        // Clean up binary
        std::error_code ec;
        fs::remove(result->binary, ec);
    }

    SECTION("Failed compilation of invalid C++ source returns CompilerError") {
        auto invalidSource = tempDir / "invalid.cpp";
        {
            std::ofstream out(invalidSource);
            out << "invalid syntax error;\n";
        }

        auto result = compile(config, invalidSource);
        REQUIRE_FALSE(result.has_value());
        REQUIRE_FALSE(result.error().log.empty());
    }

    SECTION("Custom compiler flags are respected") {
        auto macroSource = tempDir / "macro_check.cpp";
        {
            std::ofstream out(macroSource);
            out << "#ifndef EXPECTED_MACRO\n"
                << "#error \"EXPECTED_MACRO not defined\"\n"
                << "#endif\n"
                << "int main() { return 0; }\n";
        }

        config.compileFlags.push_back("-DEXPECTED_MACRO");
        auto result = compile(config, macroSource);
        REQUIRE(result.has_value());
        REQUIRE(fs::exists(result->binary));

        std::error_code ec;
        fs::remove(result->binary, ec);
    }

    fs::remove_all(tempDir);
}
