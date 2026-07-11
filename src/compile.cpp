#include "compile.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <stdexcept>
#include <string>

namespace {

bool isAvailable(const std::string &compiler) {
#ifdef _WIN32
  std::string cmd = compiler + " --version >nul 2>&1";
#else
  std::string cmd = compiler + " --version >/dev/null 2>&1";
#endif

  return std::system(cmd.c_str()) == 0;
}

std::string detectCompiler() {
  const char *cxx = std::getenv("CXX");
  if (cxx) {
    std::println(std::cerr,
                 "\033[1;36mNote:\033[0m Using compiler from CXX variable: {}",
                 cxx);
    return std::string(cxx);
  }

  if (isAvailable("g++")) {
    std::println(std::cerr, "\033[1;36mNote:\033[0m g++ is found. Using g++.");
    return "g++";
  } else if (isAvailable("clang++")) {
    std::println(std::cerr,
                 "\033[1;36mNote:\033[0m clang++ is found. Using clang++.");
    return "clang++";
  } else
    throw std::runtime_error("No C++ compiler found.");
}

} // namespace

bool compile(Config &config, const std::filesystem::path &source) {
  static const std::string compiler =
      config.compiler ? *config.compiler : detectCompiler();

  std::string compileCommands = compiler;
  for (const auto &flag : config.compileFlags)
    compileCommands += " " + flag;

  std::string outputName = source.stem().string();
#ifdef _WIN32
  outputName += ".exe";
#endif
  std::string logFile = source.stem().string() + ".compile.err";

  std::string command = compileCommands + " \"" + source.string() + "\" -o \"" +
                        outputName + "\" > \"" + logFile + "\" 2>&1";

  if (config.verbose) {
    std::println(std::cerr, "\033[1;30mDebug:\033[0m Executing command: {}",
                 command);
  }

  std::println(std::cout, "Compiling {}...", source.string());

  auto start = std::chrono::steady_clock::now();
  int status = std::system(command.c_str());
  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  std::error_code ec;
  bool hasOutput = std::filesystem::exists(logFile, ec) &&
                   std::filesystem::file_size(logFile, ec) > 0;

  if (status != 0) {
    std::println(std::cerr, "\033[1;31mCompilation Error (CE):\033[0m {}",
                 source.string());
    if (hasOutput) {
      std::ifstream log(logFile);
      std::string line;
      while (std::getline(log, line)) {
        std::println(std::cerr, "  {}", line);
      }
    }
    return false;
  } else {
    if (hasOutput) {
      std::println(std::cerr, "\033[1;33mCompiler Warnings:\033[0m {}",
                   source.string());
      std::ifstream log(logFile);
      std::string line;
      while (std::getline(log, line)) {
        std::println(std::cerr, "  {}", line);
      }
    } else {
      std::filesystem::remove(logFile, ec);
    }
    std::println(std::cout, "Compilation done in {:.3f}s.", elapsed);
    return true;
  }
}
