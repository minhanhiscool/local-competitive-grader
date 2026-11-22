#pragma once
#include "parseConfig.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class Verdict : std::uint8_t {
    None = 0,
    AC = 1,
    WA = 2,
    TLE = 3,
    OLE = 4,
    MLE = 5,
    RTE = 6
};

struct Results {
    std::optional<std::string> output{std::nullopt};
    std::chrono::milliseconds timeSpent{0};
    std::size_t peakMemory{0};
    int exitStatus{0};
    Verdict verdict{Verdict::None};
};

struct JudgeResult {
    Verdict overallVerdict{Verdict::None};
    std::vector<Results> results;
};

JudgeResult judge(const Config &config, const std::filesystem::path &binary);
