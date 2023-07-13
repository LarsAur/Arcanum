#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <player.hpp>
#include <uci.hpp>

using namespace ChessEngine2;

int main(int argc, char *argv[])
{
    ChessEngine2::initGenerateKnightAttacks();
    ChessEngine2::initGenerateKingMoves();
    ChessEngine2::initGenerateRookMoves();
    ChessEngine2::initGenerateBishopMoves();


    if(argc == 1 || (argc > 1 && strncmp("--nouci", argv[1], 8)))
    {
        UCI::loop();
        exit(EXIT_SUCCESS);
    }

    bool exitAfterTesting = false;
    for(int i = 1; i < argc; i++)
    {
        if(!strncmp("--perft", argv[i], 8))
        {
            Test::perft();
            exitAfterTesting = true;
        }

        if(!strncmp("--test", argv[i], 8))
        {
            Test::captureMoves();
            Test::zobrist();
            Test::perft();
            exitAfterTesting = true;
        }

        if(!strncmp("--perf", argv[i], 8))
        {
            Perf::engineTest();
            Perf::search();
            exitAfterTesting = true;
        }
    }

    if(exitAfterTesting)
    {
        exit(EXIT_SUCCESS);
    }

    ChessEngine2::Board board = ChessEngine2::Board(ChessEngine2::startFEN);
    board.addBoardToHistory();
    CE2_LOG(std::endl << board.getBoardString())

    // Use two different searchers so they use separate transposition tables
    Searcher searcher1 = Searcher(); 
    Searcher searcher2 = Searcher();
    Player player = Player();

    for(int i = 0; i < 400; i++)
    {
        board.getLegalMoves();
        board.generateCaptureInfo();
        if(board.getNumLegalMoves() == 0)
        {
            CE2_LOG("Game Ended")
            break;
        }

        auto boardHistory = Board::getBoardHistory();
        auto it = boardHistory->find(board.getHash());
        if(it != boardHistory->end())
        {
            if(it->second == 3) // The check id done after the board is added to history
            {
                CE2_LOG("Stalemate")
                break;
            }
        }

        Move move;
        if(board.getTurn() == WHITE)
        {
            // move = player.promptForMove(board);
            // move = searcher1.getBestMove(board, 6, 4);
            move = searcher1.getBestMoveInTime(board, 2000, 4);
        }
        else
        {
            move = player.promptForMove(board);
            // move = searcher2.getBestMoveInTime(board, 10000, 4);
        }
        
        board.performMove(move);
        board.addBoardToHistory();

        CE2_LOG(std::endl << board.getBoardString())

        // CE2_LOG("Eval of current board: " << board.evaluate());
        CE2_LOG("Turn: " << (board.getTurn() == WHITE ? "White" : "Black"));
    }

    return 0;
}