#include "parseConfig.h"
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::optional<std::string_view>
optionValue(std::string_view arg, std::optional<std::string_view> shortOpt,
            std::string_view longOpt, std::size_t &idx,
            std::span<char const *const> args) {

    auto match =
        [&](std::string_view prefix) -> std::optional<std::string_view> {
        if (arg == prefix) {
            if (++idx >= args.size()) {
                throw std::invalid_argument("Missing value for " +
                                            std::string(prefix));
            }

            return args[idx];
        }

        if (arg.starts_with(prefix) && arg.size() > prefix.size() &&
            arg[prefix.size()] == '=') {

            return arg.substr(prefix.size() + 1);
        }

        return std::nullopt;
    };

    if (shortOpt) {
        if (auto value = match(*shortOpt)) {
            return value;
        }
    }

    return match(longOpt);
}

int parseInteger(std::string_view value, std::string_view optionName) {
    int raw = 0;

    const char *const first = value.data();
    const char *const last = first + value.size();
    auto [p, ec] = std::from_chars(first, last, raw);

    if (ec != std::errc()) {
        throw std::invalid_argument("Invalid " + std::string(optionName) +
                                    ": " + std::string(value));
    }

    if (p != last) {
        std::string_view prefix(first, p - first);
        std::println(std::cerr, "\033[1;36mHint:\033[0m Did you mean \"{}\"?",
                     prefix);
        throw std::invalid_argument("Invalid " + std::string(optionName) +
                                    ": " + std::string(value));
    }

    return raw;
}

void printHelp() {
    std::println(
        std::cout,
        "\033[1;4mUsage:\033[0m grade [OPTIONS] [FILE...]\n"
        "\n"
        "\033[1;4mOptions:\033[0m\n"
        "-t, --time-limit <int>         Time limit in milliseconds\n"
        "-m, --memory-limit <int>       Memory limit in MiB\n"
        "-O, --output-limit <int>       Output limit in bytes\n"
        "-i, --testcase-input <path>    Testcase input file "
        "(incompatible with -d)\n"
        "-o, --testcase-output <path>   Testcase output file "
        "(incompatible with -d)\n"
        "-d, --testcase-dir <path>      Testcase directory "
        "(incompatible with -i/-o)\n"
        "-c, --compile-flags <flags>    Compiler flags\n"
        "    --config-file <path>       Path to config file\n"
        "    --compiler <compiler>      Compiler to use\n"
        "-v, --verbose                  Print more verbose logs\n"
        "-h, --help                     Print this help message\n"
        "-V, --version                  Print version information\n\n"
        "All command-line options override corresponding configuration "
        "settings.");
}

void prepareTestcasePaths(
    const std::optional<std::filesystem::path> &testcaseDir,
    const std::optional<std::filesystem::path> &testcaseInput,
    const std::optional<std::filesystem::path> &testcaseOutput,
    Config &config) {

    if (testcaseDir && (testcaseInput || testcaseOutput)) {
        throw(std::invalid_argument("Testcase directory "
                                    "incompatible with testcase input/output"));
    }

    if (testcaseInput.has_value() != testcaseOutput.has_value()) {
        throw std::invalid_argument("Testcase input/output "
                                    "must be specified together");
    }

    if (testcaseDir) {
        config.testcases = *testcaseDir;
    } else if (testcaseInput && testcaseOutput) {
        config.testcases =
            TestcasePaths{.input = *testcaseInput, .output = *testcaseOutput};
    }
}
} // namespace

ScanCommandFlagsRes scanCommandFlags(std::span<char const *const> args) {
    std::optional<std::filesystem::path> path;
    if (args.size() == 1) {
        printHelp();
        return {.exit = true, .configFile = std::nullopt};
    }
    for (std::size_t i = 1; i < args.size(); i++) {
        std::string_view arg(args[i]);
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return {.exit = true, .configFile = std::nullopt};
        }

        if (arg == "-V" || arg == "--version") {
            std::println(std::cout, "grade v{}", VERSION);
            return {.exit = true, .configFile = std::nullopt};
        }

        if (auto value =
                optionValue(arg, std::nullopt, "--config-file", i, args)) {

            if (path.has_value()) {
                throw std::invalid_argument("Config file path declared twice");
            }
            path = *value;
        }
    }
    if (path && !std::filesystem::is_regular_file(*path)) {
        throw std::invalid_argument("Invalid config file path: " +
                                    path->string());
    }
    return {.exit = false, .configFile = path};
}

void parseConfigFile(Config &config, const std::filesystem::path &jsonfile) {
    using json = nlohmann::json;

    std::optional<std::filesystem::path> testcaseDir;
    std::optional<std::filesystem::path> testcaseInput;
    std::optional<std::filesystem::path> testcaseOutput;

    if (!std::filesystem::is_regular_file(jsonfile)) {
        return;
    }

    std::ifstream file(jsonfile);

    if (!file) {
        throw std::runtime_error("Failed to open config file: " +
                                 jsonfile.string());
    }
    json jsconfig = json::parse(file);

    if (jsconfig.contains("compiler")) {
        config.compiler = jsconfig.at("compiler").get<std::string>();
    }

    if (jsconfig.contains("time_limit")) {
        int timeLimit = jsconfig.at("time_limit").get<int>();

        if (timeLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Time limit cannot be negative");
        }

        config.timeLimit = std::chrono::milliseconds(timeLimit);
    }

    if (jsconfig.contains("memory_limit")) {
        int memoryLimit = jsconfig.at("memory_limit").get<int>();

        if (memoryLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Memory limit cannot be negative");
        }
        config.memoryLimit = MiB(memoryLimit);
    }

    if (jsconfig.contains("output_limit")) {
        int outputLimit = jsconfig.at("output_limit").get<int>();

        if (outputLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Output limit cannot be negative");
        }
        config.outputLimit = outputLimit;
    }

    if (jsconfig.contains("compile_flags")) {
        config.compileFlags =
            jsconfig.at("compile_flags").get<std::vector<std::string>>();
    }

    if (jsconfig.contains("testcase_dir")) {
        testcaseDir = jsconfig.at("testcase_dir").get<std::string>();
    }
    if (jsconfig.contains("testcase_input")) {
        testcaseInput = jsconfig.at("testcase_input").get<std::string>();
    }
    if (jsconfig.contains("testcase_output")) {
        testcaseOutput = jsconfig.at("testcase_output").get<std::string>();
    }

    if (jsconfig.contains("verbose")) {
        config.verbose = jsconfig.at("verbose").get<bool>();
    }

    try {
        prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput,
                             config);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("Invalid config file: " +
                                    std::string(e.what()));
    }
}

void parseCommandFlags(Config &config, std::span<char const *const> args) {
    bool positional = false;
    std::optional<std::filesystem::path> testcaseDir;
    std::optional<std::filesystem::path> testcaseInput;
    std::optional<std::filesystem::path> testcaseOutput;
    std::optional<std::vector<std::string>> compilerFlags;

    for (std::size_t i = 1; i < args.size(); i++) {
        std::string_view arg(args[i]);

        if (positional) {
            config.sources.emplace_back(arg);
            continue;
        }
        if (arg == "--") {
            positional = true;
            continue;
        }

        // VALUE OPTIONS
        if (auto value =
                optionValue(arg, std::nullopt, "--compiler", i, args)) {
            config.compiler = *value;
            continue;
        }

        if (auto value = optionValue(arg, "-t", "--time-limit", i, args)) {
            int raw = parseInteger(*value, "time limit");

            if (raw < 0) {
                throw std::invalid_argument("Time limit cannot be negative");
            }
            config.timeLimit = std::chrono::milliseconds(raw);

            continue;
        }

        if (auto value = optionValue(arg, "-m", "--memory-limit", i, args)) {
            int raw = parseInteger(*value, "memory limit");

            if (raw < 0) {
                throw std::invalid_argument("Memory limit cannot be negative");
            }
            config.memoryLimit = MiB(raw);

            continue;
        }

        if (auto value = optionValue(arg, "-O", "--output-limit", i, args)) {
            int raw = parseInteger(*value, "output limit");

            if (raw < 0) {
                throw std::invalid_argument("Output limit cannot be negative");
            }
            config.outputLimit = raw;
            continue;
        }

        if (auto value = optionValue(arg, "-i", "--testcase-input", i, args)) {

            if (!std::filesystem::exists(*value)) {
                throw std::invalid_argument("Testcase input does not exist");
            }

            if (std::filesystem::is_directory(*value)) {
                throw std::invalid_argument("Testcase input is a directory");
            }

            testcaseInput = *value;

            continue;
        }

        if (auto value = optionValue(arg, "-o", "--testcase-output", i, args)) {
            if (!std::filesystem::exists(*value)) {
                throw std::invalid_argument("Testcase output does not exist");
            }

            if (std::filesystem::is_directory(*value)) {
                throw std::invalid_argument("Testcase output is a directory");
            }

            testcaseOutput = *value;

            continue;
        }

        if (auto value = optionValue(arg, "-d", "--testcase-dir", i, args)) {
            if (!std::filesystem::exists(*value)) {
                throw std::invalid_argument(
                    "Testcase directory does not exist");
            }

            if (!std::filesystem::is_directory(*value)) {
                throw std::invalid_argument(
                    "Testcase directory is not a directory");
            }

            testcaseDir = *value;

            continue;
        }

        if (auto value = optionValue(arg, "-c", "--compile-flags", i, args)) {
            auto flags = value.value() | std::ranges::views::split(' ');

            for (auto &&flag : flags) {
                if (flag.empty()) {
                    continue;
                }

                if (!compilerFlags) {
                    compilerFlags.emplace();
                }

                compilerFlags->emplace_back(std::string_view(flag));
            }

            continue;
        }

        // TOGGLE OPTIONS

        if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
            continue;
        }

        // UNKNOWN ARGUMENT
        if (arg.starts_with("-")) {
            throw std::invalid_argument("Unknown option: " + std::string(arg));
        }

        config.sources.emplace_back(arg);
    }

    if (compilerFlags) {
        config.compileFlags = std::move(*compilerFlags);
    }

    prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput, config);

    if (config.sources.empty()) {
        throw std::invalid_argument("No source files specified");
    }
}
