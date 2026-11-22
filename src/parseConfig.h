#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

inline constexpr std::string_view VERSION = "0.1_alpha";
constexpr std::size_t MiB(std::size_t size) { return size * 1024 * 1024; }

struct ScanCommandFlagsRes {
    bool exit{false};
    std::optional<std::filesystem::path> configFile;
};

// Explicit testcase paths
struct TestcasePaths {
    std::filesystem::path input;
    std::filesystem::path output;
};

// Either testcase directory or explicit testcase paths
using TestcaseSource = std::variant<std::filesystem::path, TestcasePaths>;

// Config
struct Config {
    std::vector<std::filesystem::path> sources;
    std::optional<std::string> compiler{std::nullopt};
    std::chrono::milliseconds timeLimit{1000};
    std::size_t memoryLimit{MiB(128)};
    std::size_t outputLimit{50000};

    std::vector<std::string> compileFlags{"-Wall", "-O2", "-s", "-march=native",
                                          "-lm"};

    TestcaseSource testcases{std::filesystem::path("test/")};

    std::filesystem::path configFile{"config.json"};

    bool verbose{false};
};

ScanCommandFlagsRes scanCommandFlags(std::span<char const *const> args);
void parseCommandFlags(Config &config, std::span<char const *const> args);
void parseConfigFile(Config &config, const std::filesystem::path &jsonfile);
