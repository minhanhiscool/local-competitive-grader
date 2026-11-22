#include "compile.h"
#include "judge.h"
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
        ScanCommandFlagsRes result = scanCommandFlags(std::span(argv, argc));
        if (result.exit) {
            return EXIT_SUCCESS;
        }

        if (result.configFile) {
            config.configFile = *result.configFile;
        }

        parseConfigFile(config, config.configFile);
        parseCommandFlags(config, std::span(argv, argc));

    } catch (const std::exception &e) {
        std::println(std::cerr, "\033[1;31mError:\033[0m {}", e.what());

        return EXIT_FAILURE;
    }

    for (const auto &source : config.sources) {
        auto result = compile(config, source);
        if (!result) {
            continue;
        }
        judge(config, result->binary);
    }

    return EXIT_SUCCESS;
}
