#pragma once
#include "parseConfig.h"
#include <expected>
#include <filesystem>
#include <string>

struct CompilerSuccess {
    std::filesystem::path binary;
    std::string log;
};

struct CompilerError {
    std::string log;
};

using compilerResult = std::expected<CompilerSuccess, CompilerError>;

compilerResult compile(const Config &config,
                       const std::filesystem::path &source);
