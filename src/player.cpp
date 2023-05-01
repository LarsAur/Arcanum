#include <player.hpp>
#include <chessutils.hpp>

using namespace ChessEngine2;

Player::Player()
{

}

Player::~Player()
{

}

static inline std::string getUCINotation(Move move)
{
    std::stringstream ss;

    ss << ChessEngine2::getArithmeticNotation(move.from)  << ChessEngine2::getArithmeticNotation(move.to);

    if(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)
    {
        ss << "q";
    }
    else if(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)
    {
        ss << "r";
    }
    else if(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP)
    {
        ss << "b";
    }
    else if(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT)
    {
        ss << "n";
    }

    return ss.str();
}

Move Player::promptForMove(Board& board)
{
    Move* moves = board.getLegalMoves();
    int numLegalMoves = board.getNumLegalMoves();

    std::stringstream ss;
    for(int i = 0; i < numLegalMoves; i++)
    {
        ss << i << "\t" << getUCINotation(moves[i]) << std::endl;
    }

    std::cout << ss.str() << std::endl;

    int selected;
    std::cin >> selected;

    while(selected < 0 && selected >= numLegalMoves)
    {
        std::cout << "Try again" << std::endl;
        std::cin >> selected;
    }

    return moves[selected];

}