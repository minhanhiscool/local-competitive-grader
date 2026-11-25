#include "compile.h"
#include "judge.h"
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

fs::path createTempDir(std::string_view name) {
    auto dir = fs::temp_directory_path() / name;
    fs::create_directories(dir);
    return dir;
}

fs::path compileHelper(const fs::path &dir, std::string_view filename,
                       std::string_view code) {
    auto srcPath = dir / filename;
    {
        std::ofstream out(srcPath);
        out << code;
    }
    Config compileCfg;
    auto compRes = compile(compileCfg, srcPath);
    REQUIRE(compRes.has_value());
    return compRes->binary;
}

} // namespace

TEST_CASE("judge - Accepted (AC) and whitespace trimming", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_ac");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "15 27\n";
    }

    Config config;
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    SECTION("Exact match AC") {
        {
            std::ofstream out(outPath);
            out << "42\n";
        }

        auto binary = compileHelper(
            tempDir, "sol_ac.cpp",
            "#include <iostream>\n"
            "int main() {\n"
            "    int a, b;\n"
            "    if (std::cin >> a >> b) std::cout << (a + b) << \"\\n\";\n"
            "    return 0;\n"
            "}\n");

        auto res = judge(config, binary);
        REQUIRE(res.overallVerdict == Verdict::AC);
        REQUIRE(res.results.size() == 1);
        REQUIRE(res.results[0].verdict == Verdict::AC);
        REQUIRE(res.results[0].exitStatus == 0);

        std::error_code ec;
        fs::remove(binary, ec);
    }

    SECTION("Whitespace and trailing newline trimming AC") {
        {
            std::ofstream out(outPath);
            out << "hello world\n";
        }

        // Program prints trailing spaces and multiple extra newlines
        auto binary =
            compileHelper(tempDir, "sol_trim.cpp",
                          "#include <iostream>\n"
                          "int main() {\n"
                          "    std::cout << \"hello world   \\n\\n\\n\";\n"
                          "    return 0;\n"
                          "}\n");

        auto res = judge(config, binary);
        REQUIRE(res.overallVerdict == Verdict::AC);
        REQUIRE(res.results.size() == 1);
        REQUIRE(res.results[0].verdict == Verdict::AC);

        std::error_code ec;
        fs::remove(binary, ec);
    }

    fs::remove_all(tempDir);
}

TEST_CASE("judge - Wrong Answer (WA)", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_wa");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "input data\n";
        std::ofstream out(outPath);
        out << "expected output\n";
    }

    Config config;
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    auto binary = compileHelper(tempDir, "sol_wa.cpp",
                                "#include <iostream>\n"
                                "int main() {\n"
                                "    std::cout << \"different output\\n\";\n"
                                "    return 0;\n"
                                "}\n");

    auto res = judge(config, binary);
    REQUIRE(res.overallVerdict == Verdict::WA);
    REQUIRE(res.results.size() == 1);
    REQUIRE(res.results[0].verdict == Verdict::WA);

    std::error_code ec;
    fs::remove(binary, ec);
    fs::remove_all(tempDir);
}

TEST_CASE("judge - Time Limit Exceeded (TLE)", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_tle");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "\n";
        std::ofstream out(outPath);
        out << "done\n";
    }

    Config config;
    config.timeLimit = std::chrono::milliseconds(150); // Short time limit
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    SECTION("Sleeping past time limit returns TLE promptly") {
        auto binary = compileHelper(
            tempDir, "sol_tle_sleep.cpp",
            "#include <chrono>\n"
            "#include <thread>\n"
            "int main() {\n"
            "    std::this_thread::sleep_for(std::chrono::seconds(5));\n"
            "    return 0;\n"
            "}\n");

        auto start = std::chrono::steady_clock::now();
        auto res = judge(config, binary);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        REQUIRE(res.overallVerdict == Verdict::TLE);
        REQUIRE(res.results.size() == 1);
        REQUIRE(res.results[0].verdict == Verdict::TLE);
        REQUIRE(elapsed < std::chrono::milliseconds(2000));

        std::error_code ec;
        fs::remove(binary, ec);
    }

    SECTION("Infinite busy loop returns TLE without hanging") {
        auto binary = compileHelper(tempDir, "sol_tle_loop.cpp",
                                    "int main() {\n"
                                    "    volatile int x = 0;\n"
                                    "    while (true) { x = x + 1; }\n"
                                    "    return 0;\n"
                                    "}\n");

        auto start = std::chrono::steady_clock::now();
        auto res = judge(config, binary);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        REQUIRE(res.overallVerdict == Verdict::TLE);
        REQUIRE(res.results.size() == 1);
        REQUIRE(res.results[0].verdict == Verdict::TLE);
        REQUIRE(elapsed < std::chrono::milliseconds(2000));

        std::error_code ec;
        fs::remove(binary, ec);
    }

    fs::remove_all(tempDir);
}

TEST_CASE("judge - Runtime Error (RTE)", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_rte");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "\n";
        std::ofstream out(outPath);
        out << "\n";
    }

    Config config;
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    auto binary = compileHelper(tempDir, "sol_rte.cpp",
                                "#include <cstdlib>\n"
                                "int main() {\n"
                                "    return 42;\n"
                                "}\n");

    auto res = judge(config, binary);
    REQUIRE(res.overallVerdict == Verdict::RTE);
    REQUIRE(res.results.size() == 1);
    REQUIRE(res.results[0].verdict == Verdict::RTE);
    REQUIRE(res.results[0].exitStatus == 42);

    std::error_code ec;
    fs::remove(binary, ec);
    fs::remove_all(tempDir);
}

TEST_CASE("judge - Memory Limit Exceeded (MLE)", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_mle");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "\n";
        std::ofstream out(outPath);
        out << "\n";
    }

    Config config;
    // Set a very small memory limit (5 MiB)
    config.memoryLimit = MiB(5);
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    // Program allocates and touches ~50 MiB of memory
    auto binary = compileHelper(
        tempDir, "sol_mle.cpp",
        "#include <chrono>\n"
        "#include <thread>\n"
        "volatile char *buf;\n"
        "int main() {\n"
        "    const size_t sz = 50 * 1024 * 1024;\n"
        "    buf = new char[sz];\n"
        "    for (size_t i = 0; i < sz; i += 4096) {\n"
        "        buf[i] = 1;\n"
        "    }\n"
        "    std::this_thread::sleep_for(std::chrono::milliseconds(100));\n"
        "    delete[] buf;\n"
        "    return 0;\n"
        "}\n");

    auto res = judge(config, binary);
    REQUIRE(res.overallVerdict == Verdict::MLE);
    REQUIRE(res.results.size() == 1);
    REQUIRE(res.results[0].verdict == Verdict::MLE);

    std::error_code ec;
    fs::remove(binary, ec);
    fs::remove_all(tempDir);
}

TEST_CASE("judge - Output Limit Exceeded (OLE)", "[judge]") {
    auto tempDir = createTempDir("grade_test_judge_ole");
    auto inPath = tempDir / "test.in";
    auto outPath = tempDir / "test.out";

    {
        std::ofstream in(inPath);
        in << "\n";
        std::ofstream out(outPath);
        out << "short\n";
    }

    Config config;
    // Set a very small output limit (50 bytes)
    config.outputLimit = 50;
    config.testcases = TestcasePaths{.input = inPath, .output = outPath};

    // Program outputs much more than 50 bytes
    auto binary = compileHelper(tempDir, "sol_ole.cpp",
                                "#include <iostream>\n"
                                "int main() {\n"
                                "    for (int i = 0; i < 1000; ++i) {\n"
                                "        std::cout << \"This output definitely "
                                "exceeds the fifty bytes limit!\\n\";\n"
                                "    }\n"
                                "    return 0;\n"
                                "}\n");

    auto res = judge(config, binary);
    REQUIRE(res.overallVerdict == Verdict::OLE);
    REQUIRE(res.results.size() == 1);
    REQUIRE(res.results[0].verdict == Verdict::OLE);

    std::error_code ec;
    fs::remove(binary, ec);
    fs::remove_all(tempDir);
}
