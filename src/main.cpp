#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>
#include <player.hpp>

using namespace ChessEngine2;

int main(int argc, char *argv[])
{
    ChessEngine2::initGenerateKnightAttacks();
    ChessEngine2::initGenerateKingMoves();
    ChessEngine2::initGenerateRookMoves();
    ChessEngine2::initGenerateBishopMoves();

    bool exitAfterTesting = false;
    for(int i = 1; i < argc; i++)
    {
        if(!strncmp("--perft", argv[i], 8))
        {
            runAllPerft();
            exitAfterTesting = true;
        }

        if(!strncmp("--test", argv[i], 8))
        {
            runAllCaptureMoves();
            runAllZobristTests();
            exitAfterTesting = true;
        }

        if(!strncmp("--perf", argv[i], 8))
        {
            runSearchPerformanceTest();
            exitAfterTesting = true;
        }
    }

    if(exitAfterTesting)
    {
        exit(0);
    }

    ChessEngine2::Board board = ChessEngine2::Board(ChessEngine2::startFEN);
    board.addBoardToHistory();
    std::cout << board.getBoardString();


    // Use two different searchers so they use separate transposition tables
    Searcher searcher1 = Searcher(); 
    Searcher searcher2 = Searcher();
    Player player = Player();

    for(int i = 0; i < 400; i++)
    {

        board.getLegalMoves();
        if(board.getNumLegalMoves() == 0)
        {
            CHESS_ENGINE2_LOG("Game Ended")
            break;
        }

        std::unordered_map<hash_t, uint8_t>* boardHistory = Board::getBoardHistory();
        auto it = boardHistory->find(board.getHash());
        if(it != boardHistory->end())
        {
            if(it->second == 3) // The check id done after the board is added to history
            {
                CHESS_ENGINE2_LOG("Stalemate")
                break;
            }
        }

        Move move;
        if(board.getTurn() == WHITE)
        {
            move = player.promptForMove(board);
            // move = searcher1.getBestMove(board, 6, 4);
        }
        else
        {
            move = searcher2.getBestMove(board, 6, 4);
        }
        
        board.performMove(move);
        board.addBoardToHistory();

        std::cout << board.getBoardString();
        std::cout << board.evaluate() << std::endl;
        std::cout << "Turn: " << board.getTurn() << std::endl;
    }

    return 0;
}

// -- Plan
/**
 * Create function to generate set of legal moves when in check
 * Create a function for generating only the capturing or get out of check moves
*/

/**
 * Log for test speeds
 * Initial move generator: ~120M nps
 * Use tzcnt and blsr: ~140M nps
 * Improve rook generator: 154M nps
 * Do all pawn forward moves in parallel: 162M nps
 * Change king move checking by generating oponentAttacks: 162M nps
 * Allocate legal move list on the stack: 260M nps
 * Parallel pawn attacks and double moves: 270M nps
 * Use enpassant bitboards 274M nps
 * Use one promotion move to verify all 296M nps
 * Use if-else on attemptAddPseudoLegalMove to move the piece from the correct bitboard 305M nps
*/