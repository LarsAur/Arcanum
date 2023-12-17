#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <uci.hpp>

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

        if(!strncmp("--search-perf", argv[i], 14))
            Perf::search();

        if(!strncmp("--engine-perf", argv[i], 14))
            Perf::engineTest();
    }

    return 0;
}