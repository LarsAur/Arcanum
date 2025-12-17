#include <tests/test.hpp>
#include <utils.hpp>
#include <vector>
#include <unordered_map>

using namespace Arcanum;

static bool listResults(const std::vector<std::tuple<std::string, bool>>& results)
{
    bool passed = true;
    INFO("Test results:")
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
        INFO("Running test: " << testName)
        results.push_back({testName, func()});
    }

    return listResults(results);
}

static bool runSelectedTests(std::unordered_map<std::string, bool(*)()>& testMap, int argc, char* argv[])
{
    std::vector<std::tuple<std::string, bool>> results;

    for(int i = 2; i < argc; i++)
    {
        const std::string arg = std::string(argv[i]);
        auto it = testMap.find(arg);
        if(it != testMap.end())
        {
            const std::string testName = arg.substr(2); // Remove the leading '--' for display
            INFO("Running test: " << testName)
            results.push_back({testName, it->second()});
        }
        else
        {
            INFO("Unknown test argument: " << arg)
        }
    }

    return listResults(results);
}

bool Test::parseArgumentsAndRunTests(int argc, char* argv[])
{
    std::unordered_map<std::string, bool(*)()> testMap = {
        {"--selfplay", Test::runSelfplayTest},
        {"--see",      Test::runSeeTest},
        {"--engine",   Test::runEngineTest},
        {"--binpack",  Test::runBinpackTest},
        {"--perft",    Test::runPerftTest},
        {"--zobrist",  Test::runZobristTest},
        {"--capture",  Test::runCaptureTest},
        {"--draw",     Test::runDrawTest},
    };

    // Run all tests if no specific test is given
    if(argc == 2)
    {
        return runAllTests(testMap);
    }

    // Otherwise run only the selected tests
    return runSelectedTests(testMap, argc, argv);
}

