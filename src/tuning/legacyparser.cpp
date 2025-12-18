#include <tuning/legacy.hpp>
#include <fen.hpp>

using namespace Arcanum;

LegacyParser::LegacyParser()
{
    m_board = Board(FEN::startpos);
}

bool LegacyParser::open(std::string path)
{
    m_ifs.open(path, std::ios::binary);

    if(!m_ifs.is_open())
    {
        ERROR("Unable to open " << path)
        return false;
    }

    return true;
}

void LegacyParser::close()
{
    m_ifs.close();
}

bool LegacyParser::eof()
{
    m_ifs.peek();
    return m_ifs.eof();
}

bool LegacyParser::isEndOfGame()
{
    // TODO: It is possible to reconstruct the game from the FEN strings
    return true; // Each position is stored independently in this format
}

Board* LegacyParser::getNextBoard()
{
    std::string resultStr;
    std::string cpStr;
    std::string fenStr;

    std::getline(m_ifs, resultStr);
    std::getline(m_ifs, cpStr);
    std::getline(m_ifs, fenStr);

    m_result = GameResult(atoi(resultStr.c_str()));
    m_score = atoi(cpStr.c_str());
    m_board = Board(fenStr);

    // Flip the score to be from the perspective of the current player
    // The score is stored from the white perspective
    if(m_board.getTurn() == BLACK)
    {
        m_score = -m_score;
    }

    return &m_board;
}

Move LegacyParser::getMove()
{
    // This format does not store the move
    // thus a null move is returned.
    // TODO: It is possible to reconstruct the move from the FEN strings
    return NULL_MOVE;
}

eval_t LegacyParser::getScore()
{
    return m_score;
}

GameResult LegacyParser::getResult()
{
    return m_result;
}
