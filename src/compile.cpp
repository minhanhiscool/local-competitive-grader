#include "compile.h"
#include "reproc++/reproc.hpp"
#include "reproc++/run.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

bool isAvailable(std::string_view compiler) {
  reproc::process process;
  reproc::options options;

  options.redirect.out.type = options.redirect.err.type =
      reproc::redirect::discard;

  std::array args{
      std::string(compiler),
      std::string("--version"),
  };

  std::error_code ec = process.start(args, options);

  if (ec)
    return false;

  auto [status, wait_ec] = process.wait(reproc::infinite);
  return !wait_ec && status == 0;
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

bool compile(const Config &config, const std::filesystem::path &source) {
  static const std::string compiler =
      config.compiler ? *config.compiler : detectCompiler();

  std::string outputName = source.stem().string();
#ifdef _WIN32
  outputName += ".exe";
#endif

  std::vector<std::string> args = {compiler};
  args.insert(args.end(), config.compileFlags.begin(),
              config.compileFlags.end());
  args.push_back(source.string());
  args.push_back("-o");
  args.push_back(outputName);

  if (config.verbose) {
    std::string command_str = args.front();

    for (const auto &arg : args | std::views::drop(1)) {
      command_str += ' ';
      command_str += arg;
    }
    std::println(std::cerr, "\033[1;30mDebug:\033[0m Executing command: {}",
                 command_str);
  }

  std::println(std::cout, "Compiling {}...", source.string());

  auto start = std::chrono::steady_clock::now();

  reproc::options options;
  options.redirect.out.type = reproc::redirect::pipe;
  options.redirect.err.type = reproc::redirect::pipe;
  options.stop.first = {reproc::stop::wait, reproc::infinite};

  std::string compilerOutput;
  auto [status, wait_ec] =
      reproc::run(args, options, reproc::sink::string(compilerOutput),
                  reproc::sink::string(compilerOutput));

  auto end = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(end - start).count();

  if (wait_ec) {
    std::println(std::cerr,
                 "\033[1;31mError running compiler process:\033[0m {}",
                 wait_ec.message());
    return false;
  }

  bool hasOutput = !compilerOutput.empty();

  if (status != 0) {
    std::println(std::cerr, "\033[1;31mCompilation Error (CE):\033[0m {}",
                 source.string());
    if (hasOutput) {
      std::println(std::cerr, "{}", compilerOutput);
    }
    return false;
  } else {
    if (hasOutput) {
      std::println(std::cerr, "\033[1;33mCompiler Warnings:\033[0m {}",
                   source.string());
      std::println(std::cerr, "{}", compilerOutput);
    }
  }
  std::println(std::cout, "Compilation done in {:.3f}s.", elapsed);
  return true;
}
