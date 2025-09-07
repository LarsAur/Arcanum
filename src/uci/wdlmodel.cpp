#include <uci/wdlmodel.hpp>
#include <algorithm>
#include <cmath>
#include <tuple>

using namespace Arcanum;

// This model is based on the model used in stockfish: https://github.com/official-stockfish/Stockfish/blob/master/src/uci.cpp.
// The values for "as" and "bs" are calculated using WDL_model: https://github.com/official-stockfish/WDL_model.
// Currently the sample size is a bit small (~600 games), so the model is might not be so accurate.

uint32_t WDLModel::m_getMaterialCount(const Board& board)
{
    uint32_t material = 0;

    for(uint8_t i = 0; i < 2; i++)
    {
        material += 1 * CNTSBITS(board.getTypedPieces(Piece::PAWN, Color(i)));
        material += 5 * CNTSBITS(board.getTypedPieces(Piece::ROOK, Color(i)));
        material += 3 * CNTSBITS(board.getTypedPieces(Piece::BISHOP, Color(i)));
        material += 3 * CNTSBITS(board.getTypedPieces(Piece::KNIGHT, Color(i)));
        material += 9 * CNTSBITS(board.getTypedPieces(Piece::QUEEN, Color(i)));
    }

    return material;
}

std::tuple<float, float> WDLModel::m_getWDLParameters(const Board& board)
{
    static constexpr float as[] = {33.53065744, -226.01258367, 202.59185975, 219.84711637};
    static constexpr float bs[] = {17.03207642, -149.44448927, 245.99889646, -9.68403525};
    
    uint32_t material = m_getMaterialCount(board);

    float m = std::clamp(material, uint32_t(17), uint32_t(78)) / 58.0f;

    float a = ((as[0] * m + as[1]) * m + as[2]) * m + as[3];
    float b = ((bs[0] * m + bs[1]) * m + bs[2]) * m + bs[3];

    return {a, b};
}

eval_t WDLModel::getNormalizedScore(const Board& board, eval_t eval)
{
    if(Evaluator::isMateScore(eval))
    {
        return eval;
    }

    const auto [a, _] = m_getWDLParameters(board);

    return 100 * eval / a;
}

WDL WDLModel::getExpectedWDL(const Board& board, eval_t eval)
{
    const auto [a, b] = m_getWDLParameters(board);

    WDL wdl;
    wdl.win = std::round(1000.0f / (1.0f + std::exp((a - eval) / b)));
    wdl.loss = std::round(1000.0f / (1.0f + std::exp((a + eval) / b)));
    wdl.draw = 1000 - wdl.win - wdl.loss;

    return wdl;
}
