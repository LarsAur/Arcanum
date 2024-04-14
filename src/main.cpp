#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <uci.hpp>
#include <eval.hpp>

using namespace Arcanum;

std::string _logFileName;
int main(int argc, char *argv[])
{
    CREATE_LOG_FILE(argv[0]);

    Arcanum::initGenerateKnightAttacks();
    Arcanum::initGenerateKingMoves();
    Arcanum::initGenerateRookMoves();
    Arcanum::initGenerateBishopMoves();
    Arcanum::initGenerateBetweens();

    Evaluator::nnue.load(Evaluator::nnuePathDefault);

    if(argc == 1)
    {
        UCI::loop();
        exit(EXIT_SUCCESS);
    }

    for(int i = 1; i < argc; i++)
    {
        if("--perft-test"   == std::string(argv[i])) Test::perft();
        if("--capture-test" == std::string(argv[i])) Test::captureMoves();
        if("--zobrist-test" == std::string(argv[i])) Test::zobrist();
        if("--draw-test"    == std::string(argv[i])) Test::draw();
        if("--symeval-test" == std::string(argv[i])) Test::symmetricEvaluation();
        if("--see-test"     == std::string(argv[i])) Test::see();
        if("--search-perf"  == std::string(argv[i])) Perf::search();
        if("--engine-perf"  == std::string(argv[i])) Perf::engineTest();
        if("--train"        == std::string(argv[i]))
        {
            NN::NNUE nnue = NN::NNUE();
            nnue.train(256, 16384, "generatedPositions.txt");
            exit(-1);
        }
    }

    return 0;
}