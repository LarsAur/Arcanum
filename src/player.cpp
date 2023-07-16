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

    std::stringstream ss;
    for(int i = 1; i <= numLegalMoves; i++)
    {
        ss << i << "\t" << moves[i-1] << std::endl;
    }

    CE2_LOG(std::endl << ss.str())

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

        int index = atoi(input.c_str());
        if(index > 0 && index <= numLegalMoves)
        {
            return moves[index - 1];
        }

        CE2_LOG("Invalid input, must be index or UCI notation. Try again");
    }
}