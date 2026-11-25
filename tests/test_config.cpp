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

    SECTION("Positional separator -- stops flag scanning") {
        std::array<const char *, 4> args{"grade", "--", "-h", "--config-file"};
        auto result = scanCommandFlags(args);
        REQUIRE_FALSE(result.exit);
        REQUIRE_FALSE(result.configFile.has_value());
    }
}

TEST_CASE("parseCommandFlags - partial, combined flags, and empty compile flags",
          "[config][cli]") {
    Config config;

    SECTION("Short and long partial flags") {
        std::array<const char *, 3> args1{"grade", "-p", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args1));
        REQUIRE(config.partial == true);

        Config config2;
        std::array<const char *, 3> args2{"grade", "--partial", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config2, args2));
        REQUIRE(config2.partial == true);
    }

    SECTION("Combined short flags -vp") {
        std::array<const char *, 3> args{"grade", "-vp", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.verbose == true);
        REQUIRE(config.partial == true);
    }

    SECTION("Unknown short flag throws invalid_argument") {
        std::array<const char *, 3> args{"grade", "-vx", "sol.cpp"};
        REQUIRE_THROWS_AS(parseCommandFlags(config, args),
                          std::invalid_argument);
    }

    SECTION("Empty -c clears default compile flags") {
        std::array<const char *, 4> args{"grade", "-c", "", "sol.cpp"};
        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.compileFlags.empty());
    }
}

TEST_CASE("parseConfigFile - subtasks and strict JSON validation",
          "[config][json][subtasks]") {
    auto tempDir = fs::temp_directory_path() / "grade_test_json_subtasks";
    fs::remove_all(tempDir);
    fs::create_directories(tempDir / "tests" / "sub1");
    fs::create_directories(tempDir / "tests" / "sub2");
    fs::create_directories(tempDir / "tests" / "sub3");
    auto inFile = tempDir / "sample.in";
    auto outFile = tempDir / "sample.out";
    {
        std::ofstream in(inFile);
        std::ofstream out(outFile);
    }
    auto configFile = tempDir / "config.json";

    SECTION("Valid subtasks with string and array dirs relative to testcase_dir") {
        {
            std::ofstream out(configFile);
            out << R"({
                "testcase_dir": "tests",
                "partial": true,
                "subtasks": [
                    { "dirs": "sub1", "percentage": 30 },
                    { "dirs": ["sub2", "sub3"], "percentage": 70 }
                ]
            })";
        }

        Config config;
        REQUIRE_NOTHROW(parseConfigFile(config, configFile));
        REQUIRE(config.partial == true);
        REQUIRE(std::holds_alternative<fs::path>(config.testcases));
        REQUIRE(std::get<fs::path>(config.testcases) == tempDir / "tests");
        REQUIRE(config.subtasks.size() == 2);
        REQUIRE(config.subtasks[0].percentage == 30);
        REQUIRE(config.subtasks[0].dirs ==
                std::vector<fs::path>{tempDir / "tests" / "sub1"});
        REQUIRE(config.subtasks[1].percentage == 70);
        REQUIRE(config.subtasks[1].dirs ==
                std::vector<fs::path>{tempDir / "tests" / "sub2",
                                      tempDir / "tests" / "sub3"});
    }

    SECTION("Empty subtasks array is allowed without testcase_dir") {
        {
            std::ofstream out(configFile);
            out << R"({ "subtasks": [] })";
        }
        Config config;
        REQUIRE_NOTHROW(parseConfigFile(config, configFile));
        REQUIRE(config.subtasks.empty());
    }

    SECTION("Subtasks without testcase_dir throws") {
        {
            std::ofstream out(configFile);
            out << R"({
                "subtasks": [ { "dirs": "sub1", "percentage": 100 } ]
            })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Subtasks with testcase_input and testcase_output throws") {
        {
            std::ofstream out(configFile);
            out << R"({
                "testcase_input": "sample.in",
                "testcase_output": "sample.out",
                "subtasks": [ { "dirs": "sub1", "percentage": 100 } ]
            })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Subtasks percentage not summing to 100 throws") {
        {
            std::ofstream out(configFile);
            out << R"({
                "testcase_dir": "tests",
                "subtasks": [
                    { "dirs": "sub1", "percentage": 40 },
                    { "dirs": "sub2", "percentage": 50 }
                ]
            })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Subtask with nonexistent directory inside testcase_dir throws") {
        {
            std::ofstream out(configFile);
            out << R"({
                "testcase_dir": "tests",
                "subtasks": [
                    { "dirs": "missing", "percentage": 100 }
                ]
            })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);
    }

    SECTION("Non-integer JSON numbers reject bools and floats") {
        {
            std::ofstream out(configFile);
            out << R"({ "time_limit": true })";
        }
        Config config;
        REQUIRE_THROWS_AS(parseConfigFile(config, configFile),
                          std::invalid_argument);

        {
            std::ofstream out(configFile);
            out << R"({ "memory_limit": 12.5 })";
        }
        Config config2;
        REQUIRE_THROWS_AS(parseConfigFile(config2, configFile),
                          std::invalid_argument);

        {
            std::ofstream out(configFile);
            out << R"({
                "testcase_dir": "tests",
                "subtasks": [ { "dirs": "sub1", "percentage": 99.5 } ]
            })";
        }
        Config config3;
        REQUIRE_THROWS_AS(parseConfigFile(config3, configFile),
                          std::invalid_argument);
    }

    fs::remove_all(tempDir);
}

TEST_CASE("config + CLI integration - precedence and subtask clearing",
          "[config][integration]") {
    auto tempDir = fs::temp_directory_path() / "grade_test_integration";
    fs::remove_all(tempDir);
    auto testsDir = tempDir / "tests";
    auto otherDir = tempDir / "other_tests";
    fs::create_directories(testsDir / "sub1");
    fs::create_directories(testsDir / "sub2");
    fs::create_directories(otherDir);

    auto inFile = tempDir / "custom.in";
    auto outFile = tempDir / "custom.out";
    {
        std::ofstream in(inFile);
        std::ofstream out(outFile);
    }

    auto configFile = tempDir / "config.json";
    {
        std::ofstream out(configFile);
        out << R"({
            "compiler": "g++",
            "time_limit": 1500,
            "memory_limit": 256,
            "output_limit": 4096,
            "compile_flags": ["-O2", "-Wall"],
            "testcase_dir": "tests",
            "verbose": false,
            "partial": false,
            "subtasks": [
                { "dirs": ["sub1"], "percentage": 40 },
                { "dirs": ["sub2"], "percentage": 60 }
            ]
        })";
    }

    std::string cfgStr = configFile.string();

    SECTION("CLI scalar overrides keep config subtasks when testcases not overridden") {
        std::array<const char *, 9> args{
            "grade", "--config-file", cfgStr.c_str(), "-t", "3000",
            "--compiler", "clang++", "-vp", "sol.cpp"};

        Config config;
        auto scan = scanCommandFlags(args);
        REQUIRE_FALSE(scan.exit);
        REQUIRE(scan.configFile == configFile);
        config.configFile = *scan.configFile;

        REQUIRE_NOTHROW(parseConfigFile(config, config.configFile));
        REQUIRE_NOTHROW(parseCommandFlags(config, args));

        REQUIRE(config.compiler == "clang++");
        REQUIRE(config.timeLimit == std::chrono::milliseconds(3000));
        REQUIRE(config.memoryLimit == MiB(256));
        REQUIRE(config.outputLimit == 4096);
        REQUIRE(config.compileFlags == std::vector<std::string>{"-O2", "-Wall"});
        REQUIRE(config.verbose == true);
        REQUIRE(config.partial == true);
        REQUIRE(std::holds_alternative<fs::path>(config.testcases));
        REQUIRE(std::get<fs::path>(config.testcases) == testsDir);
        REQUIRE(config.subtasks.size() == 2);
        REQUIRE(config.sources == std::vector<fs::path>{"sol.cpp"});
    }

    SECTION("CLI -i/-o throws out subtasks from config file") {
        std::string inStr = inFile.string();
        std::string outStr = outFile.string();
        std::array<const char *, 8> args{
            "grade", "--config-file", cfgStr.c_str(), "-i",
            inStr.c_str(), "-o", outStr.c_str(), "sol.cpp"};

        Config config;
        auto scan = scanCommandFlags(args);
        REQUIRE_FALSE(scan.exit);
        config.configFile = *scan.configFile;

        REQUIRE_NOTHROW(parseConfigFile(config, config.configFile));
        REQUIRE(config.subtasks.size() == 2);

        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.subtasks.empty());
        REQUIRE(std::holds_alternative<TestcasePaths>(config.testcases));
        const auto &paths = std::get<TestcasePaths>(config.testcases);
        REQUIRE(paths.input == inFile);
        REQUIRE(paths.output == outFile);
    }

    SECTION("CLI -d throws out subtasks from config file") {
        std::string otherStr = otherDir.string();
        std::array<const char *, 6> args{"grade", "--config-file",
                                         cfgStr.c_str(), "-d",
                                         otherStr.c_str(), "sol.cpp"};

        Config config;
        auto scan = scanCommandFlags(args);
        REQUIRE_FALSE(scan.exit);
        config.configFile = *scan.configFile;

        REQUIRE_NOTHROW(parseConfigFile(config, config.configFile));
        REQUIRE(config.subtasks.size() == 2);

        REQUIRE_NOTHROW(parseCommandFlags(config, args));
        REQUIRE(config.subtasks.empty());
        REQUIRE(std::holds_alternative<fs::path>(config.testcases));
        REQUIRE(std::get<fs::path>(config.testcases) == otherDir);
    }

    fs::remove_all(tempDir);
}
