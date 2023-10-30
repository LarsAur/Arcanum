#pragma once
#include <nnue-probe/nnue.hpp>
#include <board.hpp>
#include <string>

namespace NN
{
    void loadNNUE(std::string filename);
    int nnueEvaluateBoard(Arcanum::Board& board);
}