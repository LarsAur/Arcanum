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

void LegacyEncoder::addGame(
    const Board& startBoard,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
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
