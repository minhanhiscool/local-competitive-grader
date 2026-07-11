#include "parseConfig.h"
#include "version.h"
#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

std::optional<std::string_view>
optionValue(std::string_view arg, std::optional<std::string_view> shortOpt,
            std::string_view longOpt, int &i, int argc, char *argv[]) {

  auto match = [&](std::string_view prefix) -> std::optional<std::string_view> {
    if (arg == prefix) {
      if (++i >= argc)
        throw std::invalid_argument("Missing value for " + std::string(prefix));

      return argv[i];
    }

    if (arg.starts_with(prefix) && arg.size() > prefix.size() &&
        arg[prefix.size()] == '=') {

      return arg.substr(prefix.size() + 1);
    }

    return std::nullopt;
  };

  if (shortOpt)
    if (auto v = match(*shortOpt))
      return v;

  return match(longOpt);
}

int parseInteger(std::string_view value, std::string_view optionName) {
  int raw = 0;
  auto [p, ec] =
      std::from_chars(value.data(), value.data() + value.size(), raw);

  if (ec != std::errc()) {
    throw std::invalid_argument("Invalid " + std::string(optionName) + ": " +
                                std::string(value));
  }

  if (p != value.data() + value.size()) {
    std::string_view prefix(value.data(), p - value.data());
    std::println(std::cerr, "\033[1;36mHint:\033[0m Did you mean \"{}\"?",
                 prefix);
    throw std::invalid_argument("Invalid " + std::string(optionName) + ": " +
                                std::string(value));
  }

  return raw;
}

void printHelp() {
  std::println(std::cout,
               "\033[1;4mUsage:\033[0m grade [OPTIONS] [FILE...]\n"
               "\n"
               "\033[1;4mOptions:\033[0m\n"
               "-t, --time-limit <int>         Time limit in milliseconds\n"
               "-m, --memory-limit <int>       Memory limit in MiB\n"
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

  if (testcaseDir && (testcaseInput || testcaseOutput))
    throw(std::invalid_argument("Testcase directory "
                                "incompatible with testcase input/output"));

  if (testcaseInput.has_value() != testcaseOutput.has_value())
    throw std::invalid_argument("Testcase input/output "
                                "must be specified together");

  if (testcaseDir) {
    config.testcases = *testcaseDir;
  } else if (testcaseInput && testcaseOutput) {
    config.testcases = TestcasePaths{*testcaseInput, *testcaseOutput};
  }
}
} // namespace

std::optional<std::filesystem::path> scanCommandFlags(int argc, char *argv[]) {
  for (int i = 1; i < argc; i++) {
    std::string_view arg(argv[i]);
    if (arg == "-h" || arg == "--help") {
      printHelp();
      std::exit(EXIT_SUCCESS);
    }

    if (arg == "-V" || arg == "--version") {
      std::println(std::cout, "grade v{}", VERSION);
      std::exit(EXIT_SUCCESS);
    }

    if (auto value =
            optionValue(arg, std::nullopt, "--config-file", i, argc, argv)) {

      std::filesystem::path path(*value);
      if (!std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument("Invalid config file path: " +
                                    path.string());
      }
      return path;
    }
  }

  return std::nullopt;
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
  json j = json::parse(file);

  if (j.contains("compiler"))
    config.compiler = j.at("compiler").get<std::string>();

  if (j.contains("time_limit")) {
    int timeLimit = j.at("time_limit").get<int>();

    if (timeLimit < 0)
      throw std::invalid_argument(
          "Invalid config file: Time limit cannot be negative");

    config.timeLimit = std::chrono::milliseconds(timeLimit);
  }

  if (j.contains("memory_limit")) {
    int memoryLimit = j.at("memory_limit").get<int>();

    if (memoryLimit < 0)
      throw std::invalid_argument(
          "Invalid config file: Memory limit cannot be negative");
    config.memoryLimit = MiB(memoryLimit);
  }

  if (j.contains("compile_flags"))
    config.compileFlags = j.at("compile_flags").get<std::vector<std::string>>();

  if (j.contains("testcase_dir"))
    testcaseDir = j.at("testcase_dir").get<std::string>();
  if (j.contains("testcase_input"))
    testcaseInput = j.at("testcase_input").get<std::string>();
  if (j.contains("testcase_output"))
    testcaseOutput = j.at("testcase_output").get<std::string>();

  if (j.contains("verbose"))
    config.verbose = j.at("verbose").get<bool>();

  try {
    prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput, config);
  } catch (const std::invalid_argument &e) {
    throw std::invalid_argument("Invalid config file: " +
                                std::string(e.what()));
  }

  return;
}

void parseCommandFlags(Config &config, int argc, char *argv[]) {
  bool positional = false;
  std::optional<std::filesystem::path> testcaseDir;
  std::optional<std::filesystem::path> testcaseInput;
  std::optional<std::filesystem::path> testcaseOutput;
  std::optional<std::vector<std::string>> compilerFlags;

  for (int i = 1; i < argc; i++) {
    std::string_view arg(argv[i]);

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
            optionValue(arg, std::nullopt, "--compiler", i, argc, argv)) {
      config.compiler = *value;
    }

    if (auto value = optionValue(arg, "-t", "--time-limit", i, argc, argv)) {
      int raw = parseInteger(*value, "time limit");

      if (raw < 0)
        throw std::invalid_argument("Time limit cannot be negative");
      config.timeLimit = std::chrono::milliseconds(raw);

      continue;
    }

    if (auto value = optionValue(arg, "-m", "--memory-limit", i, argc, argv)) {
      int raw = parseInteger(*value, "memory limit");

      if (raw < 0)
        throw std::invalid_argument("Memory limit cannot be negative");
      config.memoryLimit = MiB(raw);

      continue;
    }

    if (auto value =
            optionValue(arg, "-i", "--testcase-input", i, argc, argv)) {

      if (!std::filesystem::exists(*value))
        throw std::invalid_argument("Testcase input does not exist");

      if (std::filesystem::is_directory(*value))
        throw std::invalid_argument("Testcase input is a directory");

      testcaseInput = *value;

      continue;
    }

    if (auto value =
            optionValue(arg, "-o", "--testcase-output", i, argc, argv)) {
      if (!std::filesystem::exists(*value))
        throw std::invalid_argument("Testcase output does not exist");

      if (std::filesystem::is_directory(*value))
        throw std::invalid_argument("Testcase output is a directory");

      testcaseOutput = *value;

      continue;
    }

    if (auto value = optionValue(arg, "-d", "--testcase-dir", i, argc, argv)) {
      if (!std::filesystem::exists(*value))
        throw std::invalid_argument("Testcase directory does not exist");

      if (!std::filesystem::is_directory(*value))
        throw std::invalid_argument("Testcase directory is not a directory");

      testcaseDir = *value;

      continue;
    }

    if (auto value = optionValue(arg, "-c", "--compile-flags", i, argc, argv)) {
      auto flags = value.value() | std::ranges::views::split(' ');

      for (auto &&flag : flags) {
        if (flag.empty())
          continue;

        if (!compilerFlags)
          compilerFlags.emplace();

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

  prepareTestcasePaths(testcaseDir, testcaseInput, testcaseOutput, config);

  if (config.sources.empty()) {
    throw std::invalid_argument("No source files specified");
  }
}
