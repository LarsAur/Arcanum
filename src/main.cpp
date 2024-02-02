#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <uci.hpp>
#include <tuning/tuning.hpp>
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

        if("--tune"         == std::string(argv[i]))
        {
            Tuning::Tuner tuner = Tuning::Tuner();
            if(argc < i + 3)
            {
                ERROR("Missing arguments for tuning '--tune input output dataset'")
                exit(-1);
            }
            tuner.setInputFile(argv[i + 1]);
            tuner.setOutputFile(argv[i + 2]);
            tuner.setTrainingDataFilePath(argv[i + 3]);
            tuner.start();
            argc += 3;
        }
    }

    return 0;
}