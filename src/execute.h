#pragma once
#include "parseConfig.h"
#include <filesystem>

void execute(const Config &config, const std::filesystem::path &binary);
