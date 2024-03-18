#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <uci.hpp>
#include <tuning/fenGen.hpp>
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
        if("--train"        == std::string(argv[i]))
        {
            NN::NNUE nnue = NN::NNUE();
            nnue.load("../nnue/test768_180");
            nnue.train(256, 128, "dataset.txt");
            exit(-1);
        }

        if("--fengen" ==  std::string(argv[i]))
        {
            Tuning::FenGen dataCreator = Tuning::FenGen();
            if(argc < i + 2)
            {
                ERROR("Missing arguments for data creation '--fengen output pgndir'")
                exit(-1);
            }
            dataCreator.setOutputFile(argv[i + 1]);
            dataCreator.start(10, argv[i + 2]);
            argc += 2;
        }

    }

    return 0;
}