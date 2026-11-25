#include "parseConfig.h"
#include <charconv>
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
        "-p, --partial                  Enable partial marking\n"
        "-h, --help                     Print this help message\n"
        "-V, --version                  Print version information\n\n"
        "Subtask options are specified in config file \n"
        "All command-line options override corresponding configuration "
        "settings.");
}

void prepareTestcasePaths(
    const std::optional<std::filesystem::path> &testcaseDir,
    const std::optional<std::filesystem::path> &testcaseInput,
    const std::optional<std::filesystem::path> &testcaseOutput,
    const std::filesystem::path &cwd, Config &config) {

    if (testcaseDir && (testcaseInput || testcaseOutput)) {
        throw(std::invalid_argument("Testcase directory "
                                    "incompatible with testcase input/output"));
    }

    if (testcaseInput.has_value() != testcaseOutput.has_value()) {
        throw std::invalid_argument("Testcase input/output "
                                    "must be specified together");
    }

    if (testcaseDir) {
        if (!std::filesystem::is_directory(cwd / *testcaseDir)) {
            throw std::invalid_argument("Testcase directory does not exist");
        }
        config.testcases = cwd / *testcaseDir;
    } else if (testcaseInput && testcaseOutput) {
        if (!std::filesystem::is_regular_file(cwd / *testcaseInput)) {
            throw std::invalid_argument("Testcase input does not exist");
        }
        if (!std::filesystem::is_regular_file(cwd / *testcaseOutput)) {
            throw std::invalid_argument("Testcase output does not exist");
        }
        config.testcases = TestcasePaths{.input = cwd / *testcaseInput,
                                         .output = cwd / *testcaseOutput};
    }
}

int requireJsonInt(const nlohmann::json &obj, std::string_view key) {
    const auto &val = obj.at(key);
    if (!val.is_number_integer()) {
        throw std::invalid_argument("Expected integer for '" +
                                    std::string(key) + "'");
    }
    return val.get<int>();
}

void validateSubtasks(const std::filesystem::path &testcaseBase,
                      std::vector<Subtask> &subtasks) {
    unsigned int sum(0);
    for (auto &subtask : subtasks) {
        for (auto &dir : subtask.dirs) {
            if (!std::filesystem::is_directory(testcaseBase / dir)) {
                throw std::invalid_argument(
                    "Subtask directory does not exist: " +
                    (testcaseBase / dir).string());
            }

            dir = testcaseBase / dir;
        }
        sum += subtask.percentage;
    }

    if (sum != 100) {
        throw std::invalid_argument(
            "Subtasks' percentage do not add up to 100%");
    }
}

std::vector<Subtask>
parseSubtasks(const nlohmann::json &jsconfig,
              const std::optional<std::filesystem::path> &testcaseDir,
              const std::optional<std::filesystem::path> &testcaseInput,
              const std::optional<std::filesystem::path> &testcaseOutput,
              const std::filesystem::path &json_cwd) {

    if (!jsconfig.contains("subtasks")) {
        return {};
    }

    if (!jsconfig["subtasks"].is_array()) {
        throw std::invalid_argument("Subtasks must be an array");
    }

    if (jsconfig["subtasks"].empty()) {
        return {};
    }

    if (!testcaseDir) {
        throw std::invalid_argument("Subtasks depend on testcase directory");
    }

    if (testcaseInput || testcaseOutput) {
        throw std::invalid_argument(
            "Subtasks are incompatible with testcase input/output");
    }

    std::vector<Subtask> subtasks;

    for (const auto &subtask : jsconfig["subtasks"]) {
        if (!subtask.is_object()) {
            throw std::invalid_argument("Each subtask must be an object");
        }

        Subtask processedSub;

        if (!subtask.contains("dirs")) {
            throw std::invalid_argument("Subtask contains no directories");
        }

        const auto &dirsVal = subtask.at("dirs");
        if (dirsVal.is_array()) {
            for (const auto &entry : dirsVal) {
                if (!entry.is_string() ||
                    entry.get_ref<const std::string &>().empty()) {
                    throw std::invalid_argument(
                        "Subtask directory must be a non-empty string");
                }
                processedSub.dirs.emplace_back(
                    entry.get_ref<const std::string &>());
            }
        } else if (dirsVal.is_string() &&
                   !dirsVal.get_ref<const std::string &>().empty()) {
            processedSub.dirs = {
                std::filesystem::path(dirsVal.get_ref<const std::string &>())};
        } else {
            throw std::invalid_argument(
                "Subtask 'dirs' must be a string or array of strings");
        }

        if (processedSub.dirs.empty()) {
            throw std::invalid_argument("Subtask directories cannot be empty");
        }

        if (!subtask.contains("percentage")) {
            throw std::invalid_argument("Subtask has no percentage specified");
        }

        int percentage = requireJsonInt(subtask, "percentage");

        if (percentage < 0 || percentage > 100) {
            throw std::invalid_argument(
                "Subtask percentage must be between 0 and 100");
        }

        processedSub.percentage = static_cast<std::uint8_t>(percentage);

        subtasks.push_back(std::move(processedSub));
    }

    validateSubtasks(json_cwd / *testcaseDir, subtasks);

    return subtasks;
}

} // namespace

ScanCommandFlagsRes scanCommandFlags(std::span<char const *const> args) {
    std::optional<std::filesystem::path> path;
    if (args.size() <= 1) {
        printHelp();
        return {.exit = true, .configFile = std::nullopt};
    }
    for (std::size_t i = 1; i < args.size(); i++) {
        std::string_view arg(args[i]);
        if (arg == "--") {
            break;
        }
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
        const auto &val = jsconfig.at("compiler");
        if (!val.is_string()) {
            throw std::invalid_argument(
                "Invalid config file: 'compiler' must be a string");
        }
        config.compiler = val.get<std::string>();
    }

    if (jsconfig.contains("time_limit")) {
        int timeLimit = requireJsonInt(jsconfig, "time_limit");

        if (timeLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Time limit cannot be negative");
        }

        config.timeLimit = std::chrono::milliseconds(timeLimit);
    }

    if (jsconfig.contains("memory_limit")) {
        int memoryLimit = requireJsonInt(jsconfig, "memory_limit");

        if (memoryLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Memory limit cannot be negative");
        }
        config.memoryLimit = MiB(memoryLimit);
    }

    if (jsconfig.contains("output_limit")) {
        int outputLimit = requireJsonInt(jsconfig, "output_limit");

        if (outputLimit < 0) {
            throw std::invalid_argument(
                "Invalid config file: Output limit cannot be negative");
        }
        config.outputLimit = outputLimit;
    }

    if (jsconfig.contains("compile_flags")) {
        const auto &flagsVal = jsconfig.at("compile_flags");
        if (!flagsVal.is_array()) {
            throw std::invalid_argument(
                "Invalid config file: 'compile_flags' must be an array");
        }
        std::vector<std::string> flags;
        for (const auto &flag : flagsVal) {
            if (!flag.is_string()) {
                throw std::invalid_argument(
                    "Invalid config file: compile flag must be a string");
            }
            flags.push_back(flag.get<std::string>());
        }
        config.compileFlags = std::move(flags);
    }

    if (jsconfig.contains("testcase_dir")) {
        const auto &val = jsconfig.at("testcase_dir");
        if (!val.is_string()) {
            throw std::invalid_argument(
                "Invalid config file: 'testcase_dir' must be a string");
        }
        testcaseDir = val.get<std::string>();
    }
    if (jsconfig.contains("testcase_input")) {
        const auto &val = jsconfig.at("testcase_input");
        if (!val.is_string()) {
            throw std::invalid_argument(
                "Invalid config file: 'testcase_input' must be a string");
        }
        testcaseInput = val.get<std::string>();
    }
    if (jsconfig.contains("testcase_output")) {
        const auto &val = jsconfig.at("testcase_output");
        if (!val.is_string()) {
            throw std::invalid_argument(
                "Invalid config file: 'testcase_output' must be a string");
        }
        testcaseOutput = val.get<std::string>();
    }

    if (jsconfig.contains("verbose")) {
        const auto &val = jsconfig.at("verbose");
        if (!val.is_boolean()) {
            throw std::invalid_argument(
                "Invalid config file: 'verbose' must be a boolean");
        }
        config.verbose = val.get<bool>();
    }
    if (jsconfig.contains("partial")) {
        const auto &val = jsconfig.at("partial");
        if (!val.is_boolean()) {
            throw std::invalid_argument(
                "Invalid config file: 'partial' must be a boolean");
        }
        config.partial = val.get<bool>();
    }

    std::filesystem::path json_cwd = jsonfile.has_parent_path()
                                         ? jsonfile.parent_path()
                                         : std::filesystem::current_path();

    try {
        prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput,
                             json_cwd, config);
    } catch (const std::invalid_argument &e) {
        throw std::invalid_argument("Invalid config file: " +
                                    std::string(e.what()));
    }

    config.subtasks = parseSubtasks(jsconfig, testcaseDir, testcaseInput,
                                    testcaseOutput, json_cwd);
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
        if (optionValue(arg, std::nullopt, "--config-file", i, args)) {
            continue;
        }

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
            compilerFlags.emplace();
            auto flags = value.value() | std::ranges::views::split(' ');

            for (auto &&flag : flags) {
                if (flag.empty()) {
                    continue;
                }

                compilerFlags->emplace_back(std::string_view(flag));
            }

            continue;
        }

        // TOGGLE OPTIONS

        if (arg == "--verbose") {
            config.verbose = true;
            continue;
        }

        if (arg == "--partial") {
            config.partial = true;
            continue;
        }

        if (arg.starts_with("-") && !arg.starts_with("--")) {
            for (std::size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                case 'v':
                    config.verbose = true;
                    break;
                case 'p':
                    config.partial = true;
                    break;
                default:
                    throw std::invalid_argument("Unknown flag: -" +
                                                std::string(1, arg[j]));
                }
            }

            continue;
        }

        // UNKNOWN ARGUMENT
        if (arg.starts_with("--")) {
            throw std::invalid_argument("Unknown option: " + std::string(arg));
        }

        config.sources.emplace_back(arg);
    }

    if (compilerFlags) {
        config.compileFlags = std::move(*compilerFlags);
    }

    std::filesystem::path cwd = std::filesystem::current_path();

    prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput, cwd,
                         config);

    if (testcaseDir || testcaseInput || testcaseOutput) {
        config.subtasks.clear();
    }

    if (config.sources.empty()) {
        throw std::invalid_argument("No source files specified");
    }
}
