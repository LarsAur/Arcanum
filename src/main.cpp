#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <player.hpp>
#include <uci.hpp>
#include <nnueHelper.hpp>
#include <windows.h>


using namespace Arcanum;

void play(Color color, std::string fen, int ms)
{
    Arcanum::Board board = Arcanum::Board(fen);
    board.getBoardHistory()->clear();
    board.addBoardToHistory();

    // Use two different searchers so they use separate transposition tables
    Searcher searcher = Searcher();
    Player player = Player();

    while(true)
    {
        LOG(std::endl << board.getBoardString())
        board.getLegalMoves();
        board.generateCaptureInfo();

        if(board.getNumLegalMoves() == 0)
        {
            LOG("Game Ended")
            break;
        }

        auto boardHistory = Board::getBoardHistory();
        auto it = boardHistory->find(board.getHash());
        if(it != boardHistory->end())
        {
            if(it->second == 3) // The check id done after the board is added to history
            {
                LOG("Stalemate")
                break;
            }
        }

        // Check for 50 move rule
        if(board.getHalfMoves() >= 100)
        {
            LOG("Draw: Rule50")
            break;
        }

        LOG("Turn: " << (board.getTurn() == WHITE ? "White" : "Black"));
        Move move;
        if(board.getTurn() == color)
            move = player.promptForMove(board);
        else
            move = searcher.getBestMoveInTime(board, ms, 4);
        
        board.performMove(move);
        board.addBoardToHistory();
    }
}

std::string _logFileName;
int main(int argc, char *argv[])
{
    CREATE_LOG_FILE(argv[0]);

    Arcanum::initGenerateKnightAttacks();
    Arcanum::initGenerateKingMoves();
    Arcanum::initGenerateRookMoves();
    Arcanum::initGenerateBishopMoves();

    char executablePath[1024];
    GetModuleFileNameA(NULL, executablePath, 1024);
    std::string fullExecPath = std::string(executablePath);
    size_t execNameStart = fullExecPath.find_last_of('\\');
    std::string path = std::string(executablePath).substr(0, execNameStart);
    execNameStart = path.find_last_of('\\');
    path = std::string(path).substr(0, execNameStart);
    path.append("\\nn-04cf2b4ed1da.nnue");
    NN::loadNNUE(path);

    if(argc == 1)
    {
        UCI::loop();
        exit(EXIT_SUCCESS);
    }

    for(int i = 1; i < argc; i++)
    {
        if(!strncmp("--play", argv[i], 7))
            // TODO: Add input about player, computer, fen and search 
            play(Color::BLACK, Arcanum::startFEN, 3000);

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