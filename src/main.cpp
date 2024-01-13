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
        if(!strncmp("--perft-test", argv[i], 13))
            Test::perft();

        if(!strncmp("--capture-test", argv[i], 15))
            Test::captureMoves();

        if(!strncmp("--zobrist-test", argv[i], 15))
            Test::zobrist();

        if(!strncmp("--draw-test", argv[i], 12))
            Test::draw();

        if(!strncmp("--symeval-test", argv[i], 15))
            Test::symmetricEvaluation();

        if(!strncmp("--see-test", argv[i], 11))
            Test::see();

        if(!strncmp("--search-perf", argv[i], 14))
            Perf::search();

        if(!strncmp("--engine-perf", argv[i], 14))
            Perf::engineTest();

        if(!strncmp("--tune", argv[i], 14))
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