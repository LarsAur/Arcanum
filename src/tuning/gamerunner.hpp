#include <search.hpp>
#include <fen.hpp>
#include <types.hpp>
#include <random>
#include <stdint.h>

namespace Arcanum
{
    class GameRunner
    {
        private:
        Searcher m_searchers[2];
        SearchParameters m_searchParameters;
        std::vector<Move> m_moves;
        std::vector<eval_t> m_evals; // This contains the evaluation of the position from the perspective of the current turn
        GameResult m_result;
        Board m_board;
        Board m_initialBoard;

        std::mt19937 m_generator;

        bool m_allowDrawAdjudication;         // Enable / Disable draw adjudication
        uint32_t m_drawAdjudicationScore;     // If consecutive absolute scores are less than or equal to this, adjudication is performed
        uint32_t m_drawAdjudicationRepeats;   // Number of half moves with an agreed score lower than draw adjudication score
        uint32_t m_drawAdjudicationMoves;     // Number of full moves in the game before adjudication is allowed
        bool m_allowResignAdjudication;       // Enable / Disable resign adjudication
        uint32_t m_resignAdjudicationScore;   // If consecutive absolute scores are greater than or equal to this, adjudication is performed
        uint32_t m_resignAdjudicationRepeats; // Number of half moves required with an agreed score greater than resign adjudication score
        uint32_t m_resignAdjudicationMoves;   // Number of full moves in the game before adjudication is allowed
        uint32_t m_moveLimit;                 // Maximum number of moves in the game. Disabled if 0.

        bool m_isDrawAdjudicated();
        bool m_isResignAdjudicated();
        bool m_isGameCompleted();
        void m_resetGame();
        public:
        GameRunner();
        void setDatagenMode(bool enable);
        void setTTSize(uint32_t mbSize);
        void setSearchParameters(SearchParameters parameters);
        void setDrawAdjudication(bool enable, uint32_t score = 0, uint32_t repeats = 0, uint32_t moves = 0);
        void setResignAdjudication(bool enable, uint32_t score = 0, uint32_t repeats = 0, uint32_t moves = 0);
        void setMoveLimit(uint32_t limit);
        void setInitialPosition(const Board& board);
        void setRandomSeed(uint32_t seed);
        void randomizeInitialPosition(uint32_t numMoves, const Board& board = Board(FEN::startpos), eval_t maxEval = Evaluator::MateScore);
        void play(bool newGame = true);

        const Board& getInitialPosition() const;
        const std::vector<Move>& getMoves() const;
        const std::vector<eval_t>& getEvals() const;
        GameResult getResult() const;
        std::string getPgn() const;
        Searcher& getSearcher(Color color);
    };
}