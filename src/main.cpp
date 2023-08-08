#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <player.hpp>
#include <uci.hpp>

using namespace ChessEngine2;

void play(Color color, std::string fen, int ms)
{

    ChessEngine2::Board board = ChessEngine2::Board(fen);
    board.addBoardToHistory();
    

    // Use two different searchers so they use separate transposition tables
    Searcher searcher = Searcher();
    Player player = Player();

    while(true)
    {
        CE2_LOG(std::endl << board.getBoardString())
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

        CE2_LOG("Turn: " << (board.getTurn() == WHITE ? "White" : "Black"));
        Move move;
        if(board.getTurn() == color)
            move = player.promptForMove(board);
        else
            move = searcher.getBestMoveInTime(board, ms, 4);
        
        board.performMove(move);
        board.addBoardToHistory();
    }
}

int main(int argc, char *argv[])
{
    ChessEngine2::initGenerateKnightAttacks();
    ChessEngine2::initGenerateKingMoves();
    ChessEngine2::initGenerateRookMoves();
    ChessEngine2::initGenerateBishopMoves();

    if(argc == 1)
    {
        UCI::loop();
        exit(EXIT_SUCCESS);
    }

    for(int i = 1; i < argc; i++)
    {
        if(!strncmp("--play", argv[i], 8))
        {
            play(Color::BLACK, ChessEngine2::startFEN, 3000);
        }

        if(!strncmp("--perft-test", argv[i], 13))
            Test::perft();

        if(!strncmp("--capture-test", argv[i], 15))
            Test::captureMoves();
         
        if(!strncmp("--zobrist-test", argv[i], 15))
            Test::zobrist();

        if(!strncmp("--search-perf", argv[i], 14))
            Perf::search();

        if(!strncmp("--engine-perf", argv[i], 14))
            Perf::engineTest();
    }

    return 0;
}