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

Move Player::promptForMove(Board& board)
{
    Move* moves = board.getLegalMoves();
    int numLegalMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();

    while(true)
    {
        std::string input;
        std::cin >> input;

        for(int i = 0; i < numLegalMoves; i++)
        {
            if(strcmp(input.c_str(), moves[i].toString().c_str()) == 0)
            {
                return moves[i];
            }
        }

        LOG("Invalid input, must be UCI notation. Try again");
    }
}