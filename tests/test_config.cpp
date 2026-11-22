#include "parseConfig.h"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("parseCommandFlags - source file parsing", "[config][cli]") {
    Config config;

    SECTION("Single source file") {
        std::array<const char *, 2> args{"grade", "solution.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.sources.size() == 1);
        REQUIRE(config.sources[0] == "solution.cpp");
    }

    SECTION("Multiple source files") {
        std::array<const char *, 4> args{"grade", "sol1.cpp", "sol2.cpp",
                                         "sol3.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.sources.size() == 3);
        REQUIRE(config.sources[0] == "sol1.cpp");
        REQUIRE(config.sources[1] == "sol2.cpp");
        REQUIRE(config.sources[2] == "sol3.cpp");
    }

    SECTION("No source files throws invalid_argument") {
        std::array<const char *, 1> args{"grade"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Positional separator -- allows flags as filenames") {
        std::array<const char *, 4> args{"grade", "-v", "--", "-file.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.verbose == true);
        REQUIRE(config.sources.size() == 1);
        REQUIRE(config.sources[0] == "-file.cpp");
    }
}

TEST_CASE("parseCommandFlags - time limit flag", "[config][cli]") {
    Config config;

    SECTION("Valid time limit with separate argument") {
        std::array<const char *, 4> args{"grade", "-t", "1500", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.timeLimit == std::chrono::milliseconds(1500));
    }

    SECTION("Valid time limit with long flag and equals sign") {
        std::array<const char *, 3> args{"grade", "--time-limit=2500",
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.timeLimit == std::chrono::milliseconds(2500));
    }

    SECTION("Negative time limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-t", "-100", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Non-numeric time limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-t", "abc", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Trailing characters in time limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-t", "100ms", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }
}

TEST_CASE("parseCommandFlags - memory limit flag", "[config][cli]") {
    Config config;

    SECTION("Valid memory limit") {
        std::array<const char *, 4> args{"grade", "-m", "256", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.memoryLimit == MiB(256));
    }

    SECTION("Valid memory limit with long flag and equals") {
        std::array<const char *, 3> args{"grade", "--memory-limit=512",
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.memoryLimit == MiB(512));
    }

    SECTION("Negative memory limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-m", "-1", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }
}

TEST_CASE("parseCommandFlags - output limit flag", "[config][cli]") {
    Config config;

    SECTION("Valid output limit with separate argument") {
        std::array<const char *, 4> args{"grade", "-O", "100000", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.outputLimit == 100000);
    }

    SECTION("Valid output limit with long flag and equals sign") {
        std::array<const char *, 3> args{"grade", "--output-limit=65536",
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.outputLimit == 65536);
    }

    SECTION("Negative output limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-O", "-1", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Non-numeric output limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-O", "abc", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Trailing characters in output limit throws invalid_argument") {
        std::array<const char *, 4> args{"grade", "-O", "50000bytes",
                                         "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }
}

TEST_CASE("parseCommandFlags - compiler and verbose flags", "[config][cli]") {
    Config config;

    SECTION("Compiler flag") {
        std::array<const char *, 4> args{"grade", "--compiler", "clang++-18",
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.compiler.has_value());
        REQUIRE(*config.compiler == "clang++-18");
    }

    SECTION("Compile flags") {
        std::array<const char *, 4> args{"grade", "-c", "-O3 -Wall -DDEBUG",
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.compileFlags ==
                std::vector<std::string>{"-O3", "-Wall", "-DDEBUG"});
    }

    SECTION("Verbose flag short and long") {
        std::array<const char *, 3> args1{"grade", "-v", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args1));
        REQUIRE(config.verbose == true);

        Config config2;
        std::array<const char *, 3> args2{"grade", "--verbose", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config2, args2));
        REQUIRE(config2.verbose == true);
    }

    SECTION("Unknown option throws invalid_argument") {
        std::array<const char *, 3> args{"grade", "--some-unknown-opt",
                                         "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }
}

TEST_CASE("parseCommandFlags - testcase options", "[config][cli]") {
    Config config;
    auto tempDir = fs::temp_directory_path() / "grade_test_cli_cases";
    fs::create_directories(tempDir);
    auto inFile = tempDir / "sample.in";
    auto outFile = tempDir / "sample.out";
    {
        std::ofstream in(inFile);
        std::ofstream out(outFile);
    }

    SECTION("Valid testcase directory") {
        std::string dirStr = tempDir.string();
        std::array<const char *, 4> args{"grade", "-d", dirStr.c_str(),
                                         "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(std::holds_alternative<fs::path>(config.testcases));
        REQUIRE(std::get<fs::path>(config.testcases) == tempDir);
    }

    SECTION("Valid testcase input and output files") {
        std::string inStr = inFile.string();
        std::string outStr = outFile.string();
        std::array<const char *, 6> args{"grade", "-i",           inStr.c_str(),
                                         "-o",    outStr.c_str(), "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(std::holds_alternative<TestcasePaths>(config.testcases));
        auto paths = std::get<TestcasePaths>(config.testcases);
        REQUIRE(paths.input == inFile);
        REQUIRE(paths.output == outFile);
    }

    SECTION("Testcase input without output throws") {
        std::string inStr = inFile.string();
        std::array<const char *, 4> args{"grade", "-i", inStr.c_str(),
                                         "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Combining testcase dir with input/output throws") {
        std::string dirStr = tempDir.string();
        std::string inStr = inFile.string();
        std::string outStr = outFile.string();
        std::array<const char *, 8> args{
            "grade",       "-d", dirStr.c_str(), "-i",
            inStr.c_str(), "-o", outStr.c_str(), "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Non-existent testcase dir throws") {
        std::array<const char *, 4> args{"grade", "-d", "/nonexistent/dir/path",
                                         "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    fs::remove_all(tempDir);
}

TEST_CASE("parseConfigFile - JSON config parsing", "[config][json]") {
    auto tempDir = fs::temp_directory_path() / "grade_test_json_config";
    fs::create_directories(tempDir);
    auto configFile = tempDir / "config.json";

    SECTION("Parse full valid configuration") {
        {
            std::ofstream out(configFile);
            out << R"({
                "compiler": "clang++",
                "time_limit": 2000,
                "memory_limit": 512,
                "output_limit": 100000,
                "compile_flags": ["-O3", "-std=c++20"],
                "verbose": true
            })";
        }

        Config config;
        REQUIRE_NOTHROW(parseConfigFile(config, configFile));
        REQUIRE(config.compiler == "clang++");
        REQUIRE(config.timeLimit == std::chrono::milliseconds(2000));
        REQUIRE(config.memoryLimit == MiB(512));
        REQUIRE(config.outputLimit == 100000);
        REQUIRE(config.compileFlags ==
                std::vector<std::string>{"-O3", "-std=c++20"});
        REQUIRE(config.verbose == true);
    }

    SECTION("Negative time_limit in JSON throws") {
        {
            std::ofstream out(configFile);
            out << R"({ "time_limit": -10 })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Negative memory_limit in JSON throws") {
        {
            std::ofstream out(configFile);
            out << R"({ "memory_limit": -5 })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Negative output_limit in JSON throws") {
        {
            std::ofstream out(configFile);
            out << R"({ "output_limit": -1 })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Non-existent config file is safely ignored") {
        Config config;
        auto initialTime = config.timeLimit;
        REQUIRE_NOTHROW(
            parseConfigFile(config, tempDir / "does_not_exist.json"));
        REQUIRE(config.timeLimit == initialTime);
    }

    SECTION("Invalid JSON syntax throws exception") {
        {
            std::ofstream out(configFile);
            out << R"({ "invalid_json": )";
        }
        Config config;
        REQUIRE_THROWS(parseConfigFile(config, configFile));
    }

    fs::remove_all(tempDir);
}

TEST_CASE("scanCommandFlags - scan config-file flag", "[config][cli]") {
    auto tempDir = fs::temp_directory_path() / "grade_test_scan_flags";
    fs::create_directories(tempDir);
    auto configFile = tempDir / "custom_config.json";
    {
        std::ofstream out(configFile);
        out << "{}";
    }

    SECTION("Finds existing config file") {
        std::string pathStr = configFile.string();
        std::array<const char *, 4> args{"grade", "--config-file",
                                         pathStr.c_str(), "sol.cpp"};
        auto result = scanCommandFlags(args);
        REQUIRE_FALSE(result.exit);
        REQUIRE(result.configFile.has_value());
        REQUIRE(*result.configFile == configFile);
    }

    SECTION("Throws if config file does not exist") {
        std::array<const char *, 4> args{"grade", "--config-file",
                                         "/nonexistent/config.json", "sol.cpp"};
        REQUIRE_THROWS_AS(scanCommandFlags(args), std::invalid_argument);
    }

    SECTION("Throws if config file declared twice") {
        std::string pathStr = configFile.string();
        std::array<const char *, 6> args{"grade",         "--config-file",
                                         pathStr.c_str(), "--config-file",
                                         pathStr.c_str(), "sol.cpp"};
        REQUIRE_THROWS_AS(scanCommandFlags(args), std::invalid_argument);
    }

    SECTION("Returns nullopt when --config-file not specified") {
        std::array<const char *, 3> args{"grade", "-v", "sol.cpp"};
        auto result = scanCommandFlags(args);
        REQUIRE_FALSE(result.exit);
        REQUIRE_FALSE(result.configFile.has_value());
    }

    fs::remove_all(tempDir);
}

TEST_CASE("scanCommandFlags - help, version and blank invocations",
          "[config][cli]") {
    SECTION("Blank invocation outputs help and exits") {
        std::array<const char *, 1> args{"grade"};
        auto result = scanCommandFlags(args);
        REQUIRE(result.exit == true);
        REQUIRE_FALSE(result.configFile.has_value());
    }

    SECTION("Short help flag -h") {
        std::array<const char *, 2> args{"grade", "-h"};
        auto result = scanCommandFlags(args);
        REQUIRE(result.exit == true);
        REQUIRE_FALSE(result.configFile.has_value());
    }

    SECTION("Long help flag --help") {
        std::array<const char *, 3> args{"grade", "--help", "sol.cpp"};
        auto result = scanCommandFlags(args);
        REQUIRE(result.exit == true);
        REQUIRE_FALSE(result.configFile.has_value());
    }

    SECTION("Short version flag -V") {
        std::array<const char *, 2> args{"grade", "-V"};
        auto result = scanCommandFlags(args);
        REQUIRE(result.exit == true);
        REQUIRE_FALSE(result.configFile.has_value());
    }

    SECTION("Long version flag --version") {
        std::array<const char *, 3> args{"grade", "--version", "sol.cpp"};
        auto result = scanCommandFlags(args);
        REQUIRE(result.exit == true);
        REQUIRE_FALSE(result.configFile.has_value());
    }
}
