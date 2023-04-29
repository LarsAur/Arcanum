#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <test.hpp>
#include <search.hpp>

using namespace ChessEngine2;

int main(int argc, char *argv[])
{
    ChessEngine2::initGenerateKnightAttacks();
    ChessEngine2::initGenerateKingMoves();
    ChessEngine2::initGenerateRookMoves();
    ChessEngine2::initGenerateBishopMoves();

    for(int i = 1; i < argc; i++)
    {
        if(!strncmp("--test", argv[i], 8))
        {
            runAllPerft();
            runAllCaptureMoves();
        }
    }

    ChessEngine2::Board board = ChessEngine2::Board(ChessEngine2::startFEN);
    std::cout << board.getBoardString();
    std::cout << board.evaluate() << std::endl;

    Searcher searcher = Searcher();

    srand(3425645);
    for(int i = 0; i < 200; i++)
    {
        std::cout << board.getBoardString();
        std::cout << board.evaluate() << std::endl;
        std::cout << "Turn: " << board.getTurn() << std::endl;

        Move move;
        if(board.getTurn() == WHITE)
        {
            move = searcher.getBestMove(board, 6);
        }
        else
        {
            Move* moves = board.getLegalMoves();
            int num = board.getNumLegalMoves();
            move = moves[rand() % num];
        }
        board.performMove(move);

        if(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)
        {
            std::cout << ChessEngine2::getArithmeticNotation(move.from) << " -> " << ChessEngine2::getArithmeticNotation(move.to) << " Promote: Q" << std::endl;
        }
        else if(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)
        {
            std::cout << ChessEngine2::getArithmeticNotation(move.from) << " -> " << ChessEngine2::getArithmeticNotation(move.to) << " Promote: R" << std::endl;
        }
        else if(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP)
        {
            std::cout << ChessEngine2::getArithmeticNotation(move.from) << " -> " << ChessEngine2::getArithmeticNotation(move.to) << " Promote: B" << std::endl;
        }
        else if(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT)
        {
            std::cout << ChessEngine2::getArithmeticNotation(move.from) << " -> " << ChessEngine2::getArithmeticNotation(move.to) << " Promote: N" << std::endl;
        }
        else
        {
            std::cout << ChessEngine2::getArithmeticNotation(move.from) << " -> " << ChessEngine2::getArithmeticNotation(move.to) << std::endl;
        }
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