#include "judge.h"
#include "parseConfig.h"
#include "reproc++/reproc.hpp"
#include "reproc++/run.hpp"
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <variant>

namespace {

class LimitedOutputSink {
public:
    LimitedOutputSink(std::string &output, std::size_t limit)
        : output_(output), limit_(limit) {}

    std::error_code operator()(reproc::stream stream,
                               const std::uint8_t *buffer, std::size_t size) {
        if (stream != reproc::stream::out || size == 0) {
            return {};
        }

        const std::size_t remaining =
            output_.size() < limit_ ? limit_ - output_.size() : 0;

        const std::size_t toAppend = std::min(size, remaining);

        const auto oldSize = output_.size();
        output_.resize(oldSize + toAppend);

        if (toAppend > 0) {
            std::memcpy(output_.data() + oldSize, buffer, toAppend);
        }

        if (size > remaining) {
            exceeded_ = true;
            return std::make_error_code(std::errc::no_buffer_space);
        }

        return {};
    }

    [[nodiscard]] bool exceeded() const noexcept { return exceeded_; }

private:
    std::string &output_;
    std::size_t limit_;
    bool exceeded_{false};
};

std::size_t readPeakMemory(int pid) {
    std::ifstream file("/proc/" + std::to_string(pid) + "/status");
    std::string line;
    while (std::getline(file, line)) {
        if (line.starts_with("VmHWM:")) {
            auto pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                return std::stoull(line.substr(pos)) * 1024;
            }
        }
    }
    return 0;
}

Results execute(const Config &config, const std::filesystem::path &input,
                const std::filesystem::path &binary) {
    Results result;
    std::string output;

    reproc::options options;

    options.redirect.in.type = reproc::redirect::path_;
    options.redirect.in.path = input.c_str();

    options.redirect.out.type = reproc::redirect::pipe;
    options.redirect.err.type = reproc::redirect::discard;

    options.stop.first = {.action = reproc::stop::wait,
                          .timeout = config.timeLimit};
    options.stop.second = {.action = reproc::stop::kill,
                           .timeout = std::chrono::milliseconds(50)};
    std::filesystem::path absBinary = std::filesystem::absolute(binary);
    std::vector<std::string> args{absBinary.string()};

    reproc::process process;
    auto startTime = std::chrono::steady_clock::now();

    if (auto ec = process.start(args, options); ec) {
        throw std::runtime_error("Failed to start process: " + ec.message());
    }

    auto [pid, pid_ec] = process.pid();

    std::size_t maxMem{0};
    bool isMle = false;
    bool isOle = false;

    {
        std::jthread monitor([&](const std::stop_token &stopToken) {
            while (!stopToken.stop_requested()) {
                std::size_t mem = readPeakMemory(pid);
                maxMem = std::max(mem, maxMem);

                if (config.memoryLimit > 0 && mem > config.memoryLimit) {
                    isMle = true;
                    process.kill();
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });

        LimitedOutputSink sink(output, config.outputLimit);
        reproc::drain(process, sink, reproc::sink::null);
        isOle = sink.exceeded();
    }
    if (isOle) {
        if (auto ec = process.kill(); ec) {
            throw std::runtime_error("Failed to kill process: " + ec.message());
        }
    }
    auto [status, stop_ec] = process.stop(options.stop);
    auto endTime = std::chrono::steady_clock::now();

    result.timeSpent = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);
    result.peakMemory = maxMem;
    result.exitStatus = status;

    if (isMle || (config.memoryLimit > 0 && maxMem > config.memoryLimit)) {
        result.verdict = Verdict::MLE;
    } else if (isOle) {
        result.verdict = Verdict::OLE;
    } else if (result.timeSpent >= config.timeLimit) {
        result.verdict = Verdict::TLE;
    } else if (result.exitStatus != 0) {
        result.verdict = Verdict::RTE;
    }

    if (result.verdict == Verdict::None) {
        result.output = output;
    }
    return result;
}
std::string normalizeOutput(std::string_view s) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < s.size()) {
        auto end = s.find('\n', start);
        std::string_view line = (end == std::string_view::npos)
                                    ? s.substr(start)
                                    : s.substr(start, end - start);

        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' ||
                                 line.back() == '\r')) {
            line.remove_suffix(1);
        }
        lines.emplace_back(line);
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }

    std::string result;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        result += lines[i];
        if (i + 1 < lines.size()) {
            result += '\n';
        }
    }
    return result;
}

std::string readFileContent(const std::filesystem::path &p) {
    std::ifstream file(p);
    if (!file) {
        return "";
    }
    return {(std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()};
}

} // namespace

JudgeResult judge(const Config &config, const std::filesystem::path &binary) {
    JudgeResult judgeResult;

    if (std::holds_alternative<std::filesystem::path>(config.testcases)) {
        // TODO: subtask differentiation
        // partial scoring vs absolute scoring
        // config file (should add more stuff for parseConfig)
        // scoring system
    } else {
        const auto &paths = std::get<TestcasePaths>(config.testcases);
        Results res = execute(config, paths.input, binary);

        if (res.verdict == Verdict::None) {
            std::string expected = readFileContent(paths.output);
            std::string actual = res.output.value_or("");
            if (normalizeOutput(actual) == normalizeOutput(expected)) {
                res.verdict = Verdict::AC;
            } else {
                res.verdict = Verdict::WA;
            }
        }
        judgeResult.results.push_back(std::move(res));
    }

    if (judgeResult.results.empty()) {
        judgeResult.overallVerdict = Verdict::None;
    } else {
        judgeResult.overallVerdict = Verdict::AC;
        for (const auto &result : judgeResult.results) {
            judgeResult.overallVerdict =
                std::max(judgeResult.overallVerdict, result.verdict);
        }
    }

    return judgeResult;
}
