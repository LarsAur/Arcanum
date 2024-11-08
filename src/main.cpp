#include <uci/uci.hpp>
#include <test.hpp>
#include <test/engineTest.hpp>
#include <eval.hpp>
#include <bitboardlookups.hpp>

using namespace Arcanum;

int main(int argc, char *argv[])
{
    CREATE_LOG_FILE(argv[0]);

    BitboardLookups::generateBitboardLookups();

    Evaluator::nnue.load(Interface::UCI::optionNNUEPath.value);

    if(argc == 1)
    {
        Interface::UCI::loop();
        exit(EXIT_SUCCESS);
    }

    for(int i = 1; i < argc; i++)
    {
        if("--perft-test"   == std::string(argv[i])) Test::perft();
        if("--capture-test" == std::string(argv[i])) Test::captureMoves();
        if("--zobrist-test" == std::string(argv[i])) Test::zobrist();
        if("--draw-test"    == std::string(argv[i])) Test::draw();
        if("--see-test"     == std::string(argv[i])) Test::see();
        if("--search-perf"  == std::string(argv[i])) Perf::search();
        if("--engine-perf"  == std::string(argv[i])) Benchmark::EngineTest::runEngineTest();
    }

    return 0;
}