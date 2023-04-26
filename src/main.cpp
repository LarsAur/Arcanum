#include <chessutils.hpp>
#include <board.hpp>
#include <iostream>
#include <bitset>
#include <test.hpp>

using namespace ChessEngine2;

#include  <random>
#include  <iterator>
template<typename Iter, typename RandomGenerator>
Iter select_randomly(Iter start, Iter end, RandomGenerator& g) {
    std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
    std::advance(start, dis(g));
    return start;
}

template<typename Iter>
Iter select_randomly(Iter start, Iter end) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return select_randomly(start, end, gen);
}

int main()
{
    ChessEngine2::initGenerateKnightAttacks();
    ChessEngine2::initGenerateKingMoves();
    ChessEngine2::initGenerateRookMoves();
    ChessEngine2::initGenerateBishopMoves();

    runAllPerft();

    // std::string fen = ChessEngine2::startFEN;
    // ChessEngine2::Board board = ChessEngine2::Board(fen);

    // uint64_t count = 0;
    // findNumMovesAtDepth(9, &board, &count);
    // std::cout << count << std::endl;

    // std::vector<ChessEngine2::Move>* legalMoves = board.getLegalMoves();
    // for(auto it = legalMoves->begin(); it != legalMoves->end(); it++)
    // {
    //     if(it->moveInfo & (MOVE_INFO_ENPASSANT))
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " EnPassant" << std::endl;
    //     }
    //     else if(it->moveInfo & (MOVE_INFO_CASTLE_BLACK_KING | MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_CASTLE_WHITE_QUEEN))
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " Castle" << std::endl;
    //     }
    //     else if(it->moveInfo & MOVE_INFO_PROMOTE_QUEEN)
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " Promote: Q" << std::endl;
    //     }
    //     else if(it->moveInfo & MOVE_INFO_PROMOTE_ROOK)
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " Promote: R" << std::endl;
    //     }
    //     else if(it->moveInfo & MOVE_INFO_PROMOTE_BISHOP)
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " Promote: B" << std::endl;
    //     }
    //     else if(it->moveInfo & MOVE_INFO_PROMOTE_KNIGHT)
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " Promote: N" << std::endl;
    //     }
    //     else if(it->moveInfo & MOVE_INFO_KING)
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << " King" << std::endl;
    //     }
    //     else
    //     {
    //         std::cout << ChessEngine2::getArithmeticNotation(it->from) << " -> " << ChessEngine2::getArithmeticNotation(it->to) << std::endl;
    //     }
    // }

    return 0;
}


// -- Plan
/**
 * Create function to generate set of legal moves when in check
 * 
 * Create function to generate attack bitboard from king
 * When we do a move we only have to check for sliding checks, except when moving the king
 * When a piece is moved, we only have to check for sliding checks if the moved piece was positioned on a queens move from the king
*/

/**
 * Log for test speeds
 * Initial move generator: ~120M nps
 * Use tzcnt and blsr: ~140M nps
 * Improve rook generator: 154M nps
 * Do all pawn forward moves in parallel: 162M nps
 * Change king move checking by generating oponentAttacks: 162M nps
*/