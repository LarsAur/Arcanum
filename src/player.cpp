#include <player.hpp>
#include <chessutils.hpp>
#include <utils.hpp>

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
    for(int i = 1; i <= numLegalMoves; i++)
    {
        ss << i << "\t" << getUCINotation(moves[i-1]) << std::endl;
    }

    CE2_LOG(std::endl << ss.str())

    while(true)
    {
        std::string input;
        std::cin >> input;

        for(int i = 0; i < numLegalMoves; i++)
        {
            if(strcmp(input.c_str(), getUCINotation(moves[i]).c_str()) == 0)
            {
                return moves[i];
            }
        }

        int index = atoi(input.c_str());
        if(index > 0 && index <= numLegalMoves)
        {
            return moves[index - 1];
        }

        CE2_LOG("Invalid input, must be index or UCI notation. Try again");
    }
}