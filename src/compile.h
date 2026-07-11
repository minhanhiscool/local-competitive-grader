#pragma once
#include "parseConfig.h"
#include <filesystem>

bool compile(const Config &config, const std::filesystem::path &source);
