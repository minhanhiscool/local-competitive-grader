#pragma once
#include "parseConfig.h"
#include <filesystem>

bool compile(Config &config, const std::filesystem::path &source);
