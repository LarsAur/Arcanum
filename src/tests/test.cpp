#include <tests/test.hpp>
#include <utils.hpp>
#include <map>

using namespace Arcanum;

static bool listResults(const std::vector<std::tuple<std::string, bool>>& results)
{
    bool passed = true;
    TESTINFO("Test results:")
    for(auto const& [name, result] : results)
    {
        if(result)
        {
            SUCCESS(name << ": Passed")
        }
        else
        {
            FAIL(name << ": Failed")
            passed = false;
        }
    }

    return passed;
}

static bool runAllTests(std::unordered_map<std::string, bool(*)()>& testMap)
{
    std::vector<std::tuple<std::string, bool>> results;

    for(auto const& [arg, func] : testMap)
    {
        const std::string testName = arg.substr(2); // Remove the leading '--' for display
        TESTINFO("Running test: " << testName)
        results.push_back({testName, func()});
    }

    return listResults(results);
}

static bool runSelectedTests(std::unordered_map<std::string, bool(*)()>& testMap, int argc, char* argv[])
{
    std::vector<std::tuple<std::string, bool>> results;

    for(int i = 1; i < argc; i++)
    {
        const std::string arg = std::string(argv[i]);
        auto it = testMap.find(arg);
        if(it != testMap.end())
        {
            const std::string testName = arg.substr(2); // Remove the leading '--' for display
            TESTINFO("Running test: " << testName)
            results.push_back({testName, it->second()});
        }
        else
        {
            TESTINFO("Unknown test argument: " << arg)
        }
    }

    return listResults(results);
}

bool Test::parseArgumentsAndRunTests(int argc, char* argv[])
{
    std::unordered_map<std::string, bool(*)()> testMap = {
        {"--selfplay-test", Test::runSelfplayTest},
        {"--see-test",      Test::runSeeTest},
        {"--engine-test",   Test::runEngineTest},
        {"--binpack-test",  Test::runBinpackTest},
        {"--perft-test",    Test::runPerftTest},
        {"--zobrist-test",  Test::runZobristTest},
        {"--capture-test",  Test::runCaptureTest},
        {"--draw-test",     Test::runDrawTest},
    };

    // Check if --all-tests is given, if so run all tests and ignore other arguments.
    for(int i = 1; i < argc; i++)
    {
        const std::string arg = std::string(argv[i]);
        if(arg == "--all-tests")
        {
            return runAllTests(testMap);
        }
    }

    // Otherwise run only the selected tests
    return runSelectedTests(testMap, argc, argv);
}

