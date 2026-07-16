#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

constexpr std::size_t MiB(std::size_t size) { return size * 1024 * 1024; }

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

    std::vector<std::string> compileFlags{"-Wall", "-O2", "-s", "-march=native",
                                          "-lm"};

    TestcaseSource testcases{std::filesystem::path("test/")};

    std::filesystem::path configFile{"config.json"};

    bool verbose{false};
};

std::optional<std::filesystem::path>
scanCommandFlags(std::span<char const *const> args);
void parseCommandFlags(Config &config, std::span<char const *const> args);
void parseConfigFile(Config &config, const std::filesystem::path &jsonfile);
