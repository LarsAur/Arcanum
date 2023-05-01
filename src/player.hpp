#pragma once

#include <board.hpp>

namespace ChessEngine2
{
    class Player
    {
        private:

        public:
            Player();
            ~Player();

            Move promptForMove(Board& board);
    };
}