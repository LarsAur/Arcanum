#pragma once

#include <board.hpp>

namespace Arcanum
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