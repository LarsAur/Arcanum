#include <tuning/gamerunner.hpp>
#include <fen.hpp>
using namespace Arcanum;

GameRunner::GameRunner() :
    m_allowDrawAdjudication(false),
    m_allowResignAdjudication(false),
    m_moveLimit(0)
{
    m_generator.seed(0);
    m_initialBoard = Board(FEN::startpos);
    m_searchers[0].setVerbose(false);
    m_searchers[1].setVerbose(false);
}

void GameRunner::setDatagenMode(bool enable)
{
    m_searchers[0].setDatagenMode(enable);
    m_searchers[1].setDatagenMode(enable);
}

void GameRunner::setTTSize(uint32_t mbSize)
{
    m_searchers[0].resizeTT(mbSize);
    m_searchers[1].resizeTT(mbSize);
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

const Board& GameRunner::getInitialPosition() const
{
    return m_initialBoard;
}

const std::vector<Move>& GameRunner::getMoves() const
{
    return m_moves;
}

const std::vector<eval_t>& GameRunner::getEvals() const
{
    return m_evals;
}

GameResult GameRunner::getResult() const
{
    return m_result;
}

Searcher& GameRunner::getSearcher(Color color)
{
    return m_searchers[color];
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
    if(m_resignAdjudicationRepeats > m_evals.size())
    {
        return false;
    }

    // Check if there are any moves in the repeat-window with
    // an absolute value smaller than the resign score
    bool resign = true;
    for(uint32_t i = 0; i < m_resignAdjudicationRepeats; i++)
    {
        uint32_t index = m_evals.size() - i - 1;
        uint32_t absEval = std::abs(m_evals.at(index));
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
        if(m_evals.at(m_evals.size() - 1) > 0 && m_board.getTurn() == Color::WHITE)
        {
            m_result = GameResult::BLACK_WIN;
        }
        else
        {
            m_result = GameResult::WHITE_WIN;
        }
    }

    return resign;
}

bool GameRunner::m_isDrawAdjudicated()
{
    // Check for move limit
    // Note that this overrides m_allowDrawAdjudication
    if((m_moveLimit != 0) && (m_moves.size() >= m_moveLimit))
    {
        m_result = GameResult::DRAW;
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
    if(m_drawAdjudicationRepeats > m_evals.size())
    {
        return false;
    }

    // Check if there are any moves in the repeat-window with
    // an absolute value larger than the draw adjudication score
    bool adjudicate = true;
    for(uint32_t i = 0; i < m_drawAdjudicationRepeats; i++)
    {
        uint32_t index = m_evals.size() - i - 1;
        uint32_t absEval = std::abs(m_evals.at(index));
        if(absEval > m_drawAdjudicationScore)
        {
            adjudicate = false;
            break;
        }
    }

    if(adjudicate)
    {
        m_result = GameResult::DRAW;
    }

    return adjudicate;
}

bool GameRunner::m_isGameCompleted()
{
    // Check if the position is repeated
    auto history = m_searchers[0].getHistory();
    if(history.at(m_board.getHash()) > 2)
    {
        m_result = GameResult::DRAW;
        return true;
    }

    // Check if there is not enough material to make checkmate
    if(m_board.isMaterialDraw())
    {
        m_result = GameResult::DRAW;
        return true;
    }

    // Check for stalemate or checkmate
    if(!m_board.hasLegalMove())
    {
        if(m_board.isChecked())
        {
            m_result = (m_board.getTurn() == WHITE) ? GameResult::BLACK_WIN : GameResult::WHITE_WIN;
        }
        else
        {
            m_result = GameResult::DRAW;
        }

        return true;
    }

    // Check for 50 move rule
    if(m_board.getHalfMoves() >= 100)
    {
        m_result = GameResult::DRAW;
        return true;
    }

    return false;
}

void GameRunner::m_resetGame()
{
    m_moves.clear();
    m_evals.clear();
    m_result = GameResult::DRAW;
    m_searchers[0].clearHistory();
    m_searchers[1].clearHistory();
    m_searchers[0].clear();
    m_searchers[1].clear();
}

void GameRunner::setInitialPosition(const Board& board)
{
    m_initialBoard = board;
}

void GameRunner::setRandomSeed(uint32_t seed)
{
    m_generator.seed(seed);
}

void GameRunner::randomizeInitialPosition(uint32_t plies, const Board& board, eval_t maxEval)
{
    SearchResult searchResult;
    SearchParameters searchParams = SearchParameters();
    searchParams.useDepth = true;
    searchParams.depth = 10;
    searchParams.useNodes = true;
    searchParams.nodes = 1000000;

    while(true)
    {
        m_initialBoard = board;

        for(uint32_t i = 0; i < plies; i++)
        {
            Move* moves = m_initialBoard.getLegalMoves();
            uint8_t numMoves = m_initialBoard.getNumLegalMoves();

            if(numMoves == 0)
            {
                break;
            }

            // Select random move and perform it
            m_initialBoard.generateCaptureInfo();
            std::uniform_int_distribution<uint8_t> dist(0, numMoves - 1);
            Move move = moves[dist(m_generator)];
            m_initialBoard.performMove(move);
        }

        // Check that the position has legal moves
        if(!m_initialBoard.hasLegalMove())
        {
            continue;
        }

        // Check that the position has an acceptable evaluation by performing a short search
        if(maxEval != MATE_SCORE)
        {
            m_searchers[0].clear();
            m_searchers[0].search(m_initialBoard, searchParams, &searchResult);
            m_searchers[0].clear();
            if(std::abs(searchResult.eval) > maxEval)
            {
                continue;
            }
        }

        // The position is valid
        return;
    }
}

void GameRunner::play(bool newGame)
{
    m_board = m_initialBoard;

    // Add the start position to the game history
    if(newGame)
    {
        m_resetGame();
    }

    m_searchers[0].addBoardToHistory(m_board);
    m_searchers[1].addBoardToHistory(m_board);

    while(!m_isGameCompleted() && !m_isResignAdjudicated() && !m_isDrawAdjudicated())
    {
        // Find the best move using the corresponding searcher and search parameters
        SearchResult searchResult;
        Move move = m_searchers[m_board.getTurn()].search(m_board, m_searchParameters, &searchResult);

        // Push the move and eval
        m_moves.push_back(move);
        m_evals.push_back(searchResult.eval);

        // Perform the move
        m_board.performMove(move);

        // Add the new position to the game history
        m_searchers[0].addBoardToHistory(m_board);
        m_searchers[1].addBoardToHistory(m_board);
    }
}