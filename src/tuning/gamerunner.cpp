#include <tuning/gamerunner.hpp>
#include <fen.hpp>
using namespace Arcanum;

GameRunner::GameRunner() :
    m_moves(nullptr),
    m_evals(nullptr),
    m_result(nullptr),
    m_allowDrawAdjudication(false),
    m_allowResignAdjudication(false),
    m_moveLimit(0)
{
    m_searchers[0] = nullptr;
    m_searchers[1] = nullptr;
}

void GameRunner::setSearchers(Searcher* s1, Searcher* s2)
{
    m_searchers[0] = s1;
    m_searchers[1] = s2;
}

void GameRunner::setSearchParameters(SearchParameters parameters)
{
    m_searchParameters = parameters;
}

void GameRunner::setDrawAdjudication(bool enable, uint32_t score, uint32_t repeats, uint32_t moves)
{
    m_allowDrawAdjudication   = enable;
    m_drawAdjudicationScore   = score;
    m_drawAdjudicationRepeats = repeats;
    m_drawAdjudicationMoves   = moves;
}

void GameRunner::setResignAdjudication(bool enable, uint32_t score, uint32_t repeats, uint32_t moves)
{
    m_allowResignAdjudication   = enable;
    m_resignAdjudicationScore   = score;
    m_resignAdjudicationRepeats = repeats;
    m_resignAdjudicationMoves   = moves;
}

void GameRunner::setMoveLimit(uint32_t limit)
{
    m_moveLimit = limit;
}

bool GameRunner::m_isResignAdjudicated()
{
    // Check if resign adjudication is enabled
    if(!m_allowResignAdjudication)
    {
        return false;
    }

    // Check if the number of full moves is passed
    if(m_resignAdjudicationMoves > m_board.getFullMoves())
    {
        return false;
    }

    // Check that there is enough evaluations to check for repeats
    if(m_resignAdjudicationRepeats > m_evals->size())
    {
        return false;
    }

    // Check if there are any moves in the repeat-window with
    // an absolute value smaller than the resign score
    bool resign = true;
    for(uint32_t i = 0; i < m_resignAdjudicationRepeats; i++)
    {
        uint32_t index = m_evals->size() - i - 1;
        uint32_t absEval = std::abs(m_evals->at(index));
        if(absEval < m_resignAdjudicationScore)
        {
            resign = false;
            break;
        }
    }

    if(resign)
    {
        // Set the winner based on the eval from the last search
        // If the previous eval was positive, the color making that move is the winner
        // Note that the move is performed before checking this
        if(m_evals->at(m_evals->size() - 1) > 0 && m_board.getTurn() == Color::WHITE)
        {
            *m_result = GameResult::BLACK_WIN;
        }
        else
        {
            *m_result = GameResult::WHITE_WIN;
        }
    }

    return resign;
}

bool GameRunner::m_isDrawAdjudicated()
{
    // Check for move limit
    // Note that this overrides m_allowDrawAdjudication
    if((m_moveLimit != 0) && (m_moves->size() >= m_moveLimit))
    {
        *m_result = GameResult::DRAW;
        return true;
    }

    // Check if resign adjudication is enabled
    if(!m_allowDrawAdjudication)
    {
        return false;
    }

    // Check if the number of full moves is passed
    if(m_drawAdjudicationMoves > m_board.getFullMoves())
    {
        return false;
    }

    // Check that there is enough evaluations to check for repeats
    if(m_drawAdjudicationRepeats > m_evals->size())
    {
        return false;
    }

    // Check if there are any moves in the repeat-window with
    // an absolute value larger than the draw adjudication score
    bool adjudicate = true;
    for(uint32_t i = 0; i < m_drawAdjudicationRepeats; i++)
    {
        uint32_t index = m_evals->size() - i - 1;
        uint32_t absEval = std::abs(m_evals->at(index));
        if(absEval > m_drawAdjudicationScore)
        {
            adjudicate = false;
            break;
        }
    }

    if(adjudicate)
    {
        *m_result = GameResult::DRAW;
    }

    return adjudicate;
}

bool GameRunner::m_isGameCompleted()
{
    // Check if the position is repeated
    auto history = m_searchers[0]->getHistory();
    if(history.at(m_board.getHash()) > 2)
    {
        *m_result = GameResult::DRAW;
        return true;
    }

    // Check if there is not enough material to make checkmate
    if(m_board.isMaterialDraw())
    {
        *m_result = GameResult::DRAW;
        return true;
    }

    // Check for stalemate or checkmate
    if(!m_board.hasLegalMove())
    {
        if(m_board.isChecked())
        {
            *m_result = (m_board.getTurn() == WHITE) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
        }
        else
        {
            *m_result = GameResult::DRAW;
        }

        return true;
    }

    // Check for 50 move rule
    if(m_board.getHalfMoves() >= 100)
    {
        *m_result = GameResult::DRAW;
        return true;
    }

    return false;
}

// When this is called, it expects the move and eval vectors to be cleared as well as the searchers
void GameRunner::play(Board board, std::vector<Move>* moves, std::vector<eval_t>* evals, GameResult* result)
{
    m_moves = moves;
    m_evals = evals;
    m_result = result;
    m_board = board;

    // Add the start position to the game history
    m_searchers[0]->addBoardToHistory(m_board);
    m_searchers[1]->addBoardToHistory(m_board);

    while(!m_isGameCompleted() && !m_isResignAdjudicated() && !m_isDrawAdjudicated())
    {
        // Find the best move using the corresponding searcher and search parameters
        SearchResult searchResult;
        Move move = m_searchers[m_board.getTurn()]->search(m_board, m_searchParameters, &searchResult);

        // Push the move and eval
        m_moves->push_back(move);
        m_evals->push_back(searchResult.eval);

        // Perform the move
        m_board.performMove(move);

        // Add the new position to the game history
        m_searchers[0]->addBoardToHistory(m_board);
        m_searchers[1]->addBoardToHistory(m_board);
    }
}