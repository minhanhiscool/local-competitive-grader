#include "compile.h"
#include "parseConfig.h"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <print>
#include <span>

int main(int argc, char *argv[]) {
    Config config;
    try {
        std::optional<std::filesystem::path> configFile =
            scanCommandFlags(std::span(argv, argc));

        if (configFile) {
            config.configFile = *configFile;
        }

        parseConfigFile(config, config.configFile);
        parseCommandFlags(config, std::span(argv, argc));

    } catch (const std::exception &e) {
        std::println(std::cerr, "\033[1;31mError:\033[0m {}", e.what());

        return EXIT_FAILURE;
    }

    for (const auto &source : config.sources) {
        if (!compile(config, source)) {
            continue;
        }
        execute();
    }

    return EXIT_SUCCESS;
}
