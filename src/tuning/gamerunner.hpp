#include <search.hpp>
#include <types.hpp>
namespace Arcanum
{
    class GameRunner
    {
        private:
        Searcher* m_searchers[2];
        SearchParameters m_searchParameters;
        std::vector<Move>*   m_moves;
        std::vector<eval_t>* m_evals; // This contains the evaluation of the position from the perspective of the current turn
        GameResult* m_result;
        Board m_board;

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
        public:
        GameRunner();
        void setSearchers(Searcher* s1, Searcher* s2);
        void setSearchParameters(SearchParameters parameters);
        void setDrawAdjudication(bool enable, uint32_t score = 0, uint32_t repeats = 0, uint32_t moves = 0);
        void setResignAdjudication(bool enable, uint32_t score = 0, uint32_t repeats = 0, uint32_t moves = 0);
        void setMoveLimit(uint32_t limit);
        void play(Board board, std::vector<Move>* moves, std::vector<eval_t>* evals, GameResult* result);
    };
}