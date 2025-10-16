#include <tuning/legacy.hpp>

using namespace Arcanum;

LegacyEncoder::LegacyEncoder()
{

}

bool LegacyEncoder::open(std::string path)
{
    m_ofs.open(path, std::ios::app);

    if(!m_ofs.is_open())
    {
        ERROR("Unable to open " << path)
        return false;
    }

    return true;
}

void LegacyEncoder::close()
{
    m_ofs.close();
}

void LegacyEncoder::addPosition(
    const Board& board,
    const Move& move,
    eval_t score,
    GameResult result
)
{
    (void) move; // Unused in legacy format

    // Flip the score if it is blacks turn
    // The score is stored from whites perspective
    score = board.getTurn() == Color::WHITE ? score : -score;

    m_ofs << result << "\n";
    m_ofs << score << "\n";
    m_ofs << board.fen() << "\n";
}

void LegacyEncoder::addGame(
    const Board& startBoard,
    const std::vector<Move>& moves,
    const std::vector<eval_t>& scores,
    GameResult result
)
{
    Board board = Board(startBoard);
    size_t numPositions = scores.size();

    for(size_t i = 0; i < numPositions; i++)
    {
        // Flip the score if it is blacks turn
        // The score is stored from whites perspective
        eval_t score = board.getTurn() == Color::WHITE ? scores[i] : -scores[i];

        m_ofs << result << "\n";
        m_ofs << score << "\n";
        m_ofs << board.fen() << "\n";

        board.performMove(moves[i]);
    }
}
