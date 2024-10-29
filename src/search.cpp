#include <search.hpp>
#include <uci/uci.hpp>
#include <utils.hpp>
#include <syzygy.hpp>
#include <algorithm>

using namespace Arcanum;

#define DRAW_VALUE 0

Searcher::Searcher()
{
    m_tt = TranspositionTable();
    m_tt.resize(32);
    m_stats = SearchStats();
    m_generation = 0;

    // Setup a table of known endgame draws based on material
    // This is based on: https://www.chess.com/terms/draw-chess
    Board kings = Board("K1k5/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWBishop = Board("K1k1B3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBBishop = Board("K1k1b3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsWKnight = Board("K1k1N3/8/8/8/8/8/8/8 w - - 0 1");
    Board kingsBKnight = Board("K1k1n3/8/8/8/8/8/8/8 w - - 0 1");
    m_knownEndgameMaterialDraws.push_back(kings.getMaterialHash());
    m_knownEndgameMaterialDraws.push_back(kingsWBishop.getMaterialHash());
    m_knownEndgameMaterialDraws.push_back(kingsBBishop.getMaterialHash());
    m_knownEndgameMaterialDraws.push_back(kingsWKnight.getMaterialHash());
    m_knownEndgameMaterialDraws.push_back(kingsBKnight.getMaterialHash());

    // Initialize the LMR reduction lookup table
    for(uint8_t d = 0; d < MAX_SEARCH_DEPTH; d++)
        for(uint8_t m = 0; m < MAX_MOVE_COUNT; m++)
            m_lmrReductions[d][m] = static_cast<uint8_t>(1 + (std::log2(m) * std::log2(d) / 4));

    // Initialize the LMP threshold lookup table
    for(uint8_t d = 0; d < MAX_SEARCH_DEPTH; d++)
    {
        m_lmpThresholds[0][d] = 2 + 1 * d * d;
        m_lmpThresholds[1][d] = 4 + 2 * d * d;
    }

    m_verbose = true;
}

Searcher::~Searcher()
{

}

void Searcher::setVerbose(bool enable)
{
    m_verbose = enable;
}

void Searcher::resizeTT(uint32_t mbSize)
{
    m_tt.resize(mbSize);
}

void Searcher::clear()
{
    m_generation = 0;
    m_tt.clear();
    m_relativeHistory.clear();
    m_killerMoveManager.clear();
}

template <bool isPv>
eval_t Searcher::m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int plyFromRoot)
{
    if(m_shouldStop())
        return 0;

    m_numNodesSearched++;
    m_stats.qSearchNodes++;

    if constexpr (isPv)
    {
        m_pvTable.updatePvLength(plyFromRoot);
        m_seldepth = std::max(m_seldepth, uint8_t(plyFromRoot));
        m_stats.pvNodes++;
    }
    else
    {
        m_stats.nonPvNodes++;
    }

    if(m_isDraw(board, plyFromRoot))
        return DRAW_VALUE;

    // Table base probe
    if(board.getNumPieces() <= TB_LARGEST && board.getHalfMoves() == 0)
    {
        uint32_t tbResult = TBProbeWDL(board);

        if(tbResult != TB_RESULT_FAILED)
            m_stats.tbHits++;

        if(tbResult == TB_DRAW) return 0;
        if(tbResult == TB_WIN) return TB_MATE_SCORE - plyFromRoot;
        if(tbResult == TB_LOSS) return -TB_MATE_SCORE + plyFromRoot;
    }

    std::optional<ttEntry_t> entry = m_tt.get(board.getHash(), plyFromRoot);
    if(!isPv && entry.has_value())
    {
        switch (entry->flags)
        {
        case TTFlag::EXACT:
            m_stats.exactTTValuesUsed++;
            return entry->value;
        case TTFlag::LOWER_BOUND:
            if(entry->value >= beta)
            {
                m_stats.lowerTTValuesUsed++;
                return entry->value;
            }
            break;
        case TTFlag::UPPER_BOUND:
            if(entry->value <= alpha)
            {
                m_stats.upperTTValuesUsed++;
                return entry->value;
            }
        }
    }

    eval_t staticEval;
    if(entry.has_value())
    {
        staticEval = entry->staticEval;
    }
    else
    {
        m_stats.evaluations++;
        staticEval = m_evaluator.evaluate(board, plyFromRoot);
    }

    eval_t bestScore = -MATE_SCORE;
    bool isChecked = board.isChecked();

    if(!isChecked)
    {
        if(staticEval >= beta)
        {
            return staticEval;
        }

        if(staticEval > alpha)
        {
            alpha = staticEval;
        }

        bestScore = staticEval;
    }

    // Genereate only capture moves if not in check, else generate all moves
    Move *moves = board.getLegalCaptureMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        return staticEval;
    }

    // Push the board on the search stack
    m_searchStack[plyFromRoot] = {
        .hash = board.getHash(),
        .staticEval = staticEval
    };

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : NULL_MOVE);
    TTFlag ttFlag = TTFlag::UPPER_BOUND;
    Move bestMove = NULL_MOVE;
    for (int i = 0; i < numMoves; i++)  {
        const Move *move = moveSelector.getNextMove();

        if(!isChecked && !board.see(*move))
            continue;

        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_evaluator.pushMoveToAccumulator(newBoard, *move);
        eval_t score = -m_alphaBetaQuiet<isPv>(newBoard, -beta, -alpha, plyFromRoot + 1);
        m_evaluator.popMoveFromAccumulator();

        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
        }

        if(bestScore >= alpha)
        {
            if constexpr(isPv)
            {
                m_pvTable.updatePv(*move, plyFromRoot);
            }
            alpha = bestScore;
            ttFlag = TTFlag::EXACT;
        }

        if(alpha >= beta)
        {
            ttFlag = TTFlag::LOWER_BOUND;
            if(IS_QUIET(move->moveInfo))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    m_tt.add(bestScore, bestMove, 0, plyFromRoot, staticEval, ttFlag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());

    return bestScore;
}

template <bool isPv>
eval_t Searcher::m_alphaBeta(Board& board, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool cutnode, bool isNullMoveSearch, uint8_t totalExtensions, Move skipMove)
{
    if(m_shouldStop())
        return 0;

    if(depth <= 0)
        return m_alphaBetaQuiet<isPv>(board, alpha, beta, plyFromRoot);

    m_numNodesSearched++;
    if constexpr (isPv)
    {
        m_pvTable.updatePvLength(plyFromRoot);
        m_seldepth = std::max(m_seldepth, uint8_t(plyFromRoot));
        m_stats.pvNodes++;
    }
    else
    {
        m_stats.nonPvNodes++;
    }

    if(m_isDraw(board, plyFromRoot))
        return DRAW_VALUE;

    eval_t originalAlpha = alpha;
    std::optional<ttEntry_t> entry = m_tt.get(board.getHash(), plyFromRoot);
    if(!isPv && entry.has_value() && (entry->depth >= depth) && skipMove== NULL_MOVE)
    {
        switch (entry->flags)
        {
        case TTFlag::EXACT:
            m_stats.exactTTValuesUsed++;
            return entry->value;
        case TTFlag::LOWER_BOUND:
            if(entry->value >= beta)
            {
                m_stats.lowerTTValuesUsed++;
                return entry->value;
            }
            break;
        case TTFlag::UPPER_BOUND:
            if(entry->value <= alpha)
            {
                m_stats.upperTTValuesUsed++;
                return entry->value;
            }
        }
    }

    // Table base probe
    if(board.getNumPieces() <= TB_LARGEST && board.getHalfMoves() == 0)
    {
        uint32_t tbResult = TBProbeWDL(board);

        if(tbResult != TB_RESULT_FAILED)
            m_stats.tbHits++;

        if(tbResult == TB_DRAW) return 0;
        if(tbResult == TB_WIN) return TB_MATE_SCORE - plyFromRoot;
        if(tbResult == TB_LOSS) return -TB_MATE_SCORE + plyFromRoot;
    }

    eval_t bestScore = -MATE_SCORE;
    Move bestMove = NULL_MOVE;
    Move* moves = nullptr;
    uint8_t numMoves = 0;

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();

    eval_t staticEval;
    if(entry.has_value())
    {
        staticEval = entry->staticEval;
    }
    else
    {
        m_stats.evaluations++;
        staticEval = m_evaluator.evaluate(board, plyFromRoot);
    }

    if(numMoves == 0)
    {
        return skipMove == NULL_MOVE ? staticEval : alpha;
    }

    board.generateCaptureInfo();
    bool isChecked = board.isChecked();
    bool isImproving = (plyFromRoot > 1) && (staticEval > m_searchStack[plyFromRoot - 2].staticEval);
    bool isWorsening = (plyFromRoot > 1) && (staticEval < m_searchStack[plyFromRoot - 2].staticEval);

    // Push the board on the search stack
    m_searchStack[plyFromRoot] = {
        .hash = board.getHash(),
        .staticEval = staticEval
    };

    // Internal Iterative Reductions
    if(isPv && depth >= 5 && !entry.has_value() && !isChecked)
    {
        depth--;
    }

    if(!isPv && !isChecked && skipMove == NULL_MOVE)
    {
        // Reverse futility pruning
        if(!Evaluator::isCloseToMate(board, beta) && depth < 9)
        {
            if(staticEval - 300 * depth  >= beta)
            {
                m_stats.reverseFutilityCutoffs++;
                return staticEval;
            }
        }

        // Razoring
        if(!Evaluator::isCloseToMate(board, alpha))
        {
            if(staticEval + 200 * depth < alpha)
            {
                eval_t razorEval = m_alphaBetaQuiet<false>(board, alpha, beta, plyFromRoot);
                if(razorEval <= alpha)
                {
                    m_stats.razorCutoffs++;
                    return razorEval;
                }
                m_stats.failedRazorCutoffs++;
            }
        }

        // Null move search
        if(depth > 2 && !isNullMoveSearch && staticEval >= beta && board.hasOfficers(board.getTurn()))
        {
            Board newBoard = Board(board);
            int R = 2 + isImproving + depth / 4;
            newBoard.performNullMove();
            m_tt.prefetch(newBoard.getHash());
            eval_t nullMoveScore = -m_alphaBeta<false>(newBoard, -beta, -beta + 1, depth - R, plyFromRoot + 1, !cutnode, true, totalExtensions);

            if(nullMoveScore >= beta)
            {
                m_stats.nullMoveCutoffs++;
                return nullMoveScore;
            }
            m_stats.failedNullMoveCutoffs++;
        }

        // ProbCut
        eval_t probBeta = beta + 300;
        if(depth >= 6 && !Evaluator::isMateScore(beta) && !(entry.has_value() && entry->depth >= depth - 3 && entry->value < probBeta))
        {
            MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : NULL_MOVE);

            for(uint8_t i = 0; i < numMoves; i++)
            {
                const Move* move = moveSelector.getNextMove();

                if(IS_QUIET(move->moveInfo))
                {
                    continue;
                }

                Board newBoard = Board(board);
                newBoard.performMove(*move);

                m_evaluator.pushMoveToAccumulator(newBoard, *move);

                m_stats.probCutQSearches++;
                eval_t score = -m_alphaBetaQuiet<false>(newBoard, -probBeta, -probBeta + 1, plyFromRoot + 1);

                if(score >= probBeta)
                {
                    m_stats.probCutSearches++;
                    score = -m_alphaBeta<false>(newBoard, -probBeta, -probBeta + 1, depth - 4, plyFromRoot + 1, cutnode, false, totalExtensions);
                }

                m_evaluator.popMoveFromAccumulator();

                if(score >= probBeta)
                {
                    m_stats.probCuts++;
                    return score;
                }
            }

            m_stats.failedProbCuts++;
        }
    }

    static constexpr eval_t futilityMargins[] = {300, 500, 900};
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : NULL_MOVE);
    uint8_t quietMovesPerformed = 0;
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        if(*move == skipMove)
            continue;

        // Late move pruning (LMP)
        // Skip quiet moves after having tried a certain number of quiet moves
        if( !isPv
        && !Evaluator::isCloseToMate(board, bestScore)
        && !isChecked && quietMovesPerformed > m_lmpThresholds[isImproving][depth]
        &&  IS_QUIET(move->moveInfo))
        {
            m_stats.lmpPrunedMoves++;
            continue;
        }

        // Generate new board and make the move
        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt.prefetch(newBoard.getHash());
        eval_t score;
        bool checkOrChecking = isChecked || newBoard.isChecked();

        // Count quiet moves for LMP
        quietMovesPerformed += IS_QUIET(move->moveInfo);

        // Futility pruning
        if(!isPv && depth < 4 && !checkOrChecking && IS_QUIET(move->moveInfo))
        {
            if(staticEval + futilityMargins[depth - 1] < alpha && std::abs(alpha) < 900 && std::abs(beta) < 900)
            {
                m_stats.futilityPrunedMoves++;
                continue;
            }
        }

        // Extend search for checking moves or check avoiding moves
        // This is to avoid horizon effect occuring by starting with a forced line
        uint8_t extension = (
            isChecked ||
            ((move->moveInfo & MoveInfoBit::PAWN_MOVE) && (RANK(move->to) == 6 || RANK(move->to) == 1)) || // Pawn moved to the 7th rank
            (numMoves == 1)
        ) ? 1 : 0;

        // Singular extension
        if(
            i == 0
            && skipMove == NULL_MOVE
            && extension == 0
            && depth >= 7
            && entry.has_value()
            && entry->flags != TTFlag::UPPER_BOUND
            && entry->depth >= depth - 2
            && !Evaluator::isMateScore(entry->value))
        {
            eval_t seBeta = entry->value - 3 * depth;
            uint8_t seDepth = (depth - 1) / 2;
            eval_t seScore = m_alphaBeta<false>(board, seBeta - 1, seBeta, seDepth, plyFromRoot, cutnode, isNullMoveSearch, totalExtensions, *move);

            if(seScore < seBeta)
            {
                extension++;
                m_stats.singularExtensions++;
            }
            else
            {
                m_stats.failedSingularExtensions++;
            }
        }

        // Limit the number of extensions
        if(totalExtensions > 32)
            extension = 0;

        m_evaluator.pushMoveToAccumulator(newBoard, *move);

        if(i == 0)
        {
            score = -m_alphaBeta<isPv>(newBoard, -beta, -alpha, depth + extension - 1, plyFromRoot + 1, !(isPv | cutnode), false, totalExtensions + extension);
        }
        else
        {
            // Late move reduction (LMR)
            int8_t R = 1;
            if(depth >= 3 && !CAPTURED_PIECE(move->moveInfo) && !checkOrChecking && !Evaluator::isMateScore(bestScore))
            {
                R =  m_lmrReductions[depth][i];
                R += isWorsening;
                R -= m_killerMoveManager.contains(*move, plyFromRoot);
                R = std::max(int8_t(1), R);
            }

            score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, depth + extension - R, plyFromRoot + 1, !cutnode, false, totalExtensions + extension);
            m_stats.researchesRequired += score > alpha && (isPv || R > 1);
            m_stats.nullWindowSearches += 1;

            // Potential research of LMR returns a score > alpha
            if(score > alpha && R > 1)
            {
                score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, depth + extension - 1 , plyFromRoot + 1, !cutnode, false, totalExtensions + extension);
                m_stats.researchesRequired += score > alpha && isPv;
                m_stats.nullWindowSearches += 1;
            }

            if(score > alpha && isPv)
            {
                score = -m_alphaBeta<isPv>(newBoard, -beta, -alpha, depth + extension - 1, plyFromRoot + 1, false, false, totalExtensions + extension);
            }
        }

        m_evaluator.popMoveFromAccumulator();

        if(score > bestScore)
        {
            if constexpr(isPv)
            {
                m_pvTable.updatePv(*move, plyFromRoot);
            }

            bestScore = score;
            bestMove = *move;
        }

        alpha = std::max(alpha, bestScore);

        // Beta-Cutoff
        if(alpha >= beta)
        {
            // Update move history and killers for quiet moves
            if(IS_QUIET(move->moveInfo))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
                if(depth > 3)
                {
                    m_relativeHistory.addHistory(*move, depth, board.getTurn());
                }
            }
            break;
        }

        // Quiet move did not cause a beta-cutoff, increase the relative butterfly history
        if(IS_QUIET(move->moveInfo))
        {
            if(depth > 3)
            {
                m_relativeHistory.addButterfly(*move, depth, board.getTurn());
            }
        }
    }

    // Stop the thread from writing to the TT when search is stopped
    if(m_stopSearch)
    {
        return 0;
    }

    if(skipMove == NULL_MOVE)
    {
        TTFlag flag = TTFlag::EXACT;
        if(bestScore <= originalAlpha) flag = TTFlag::UPPER_BOUND;
        else if(bestScore >= beta)     flag = TTFlag::LOWER_BOUND;

        m_tt.add(bestScore, bestMove, depth, plyFromRoot, staticEval, flag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());
    }

    return bestScore;
}

inline bool Searcher::m_isDraw(const Board& board, uint8_t plyFromRoot) const
{
    // Check for repeated positions in the current search
    // * Only check for boards backwards until captures occur (halfMoves)
    // * Only check every other board, as the turn has to be correct
    const uint16_t limit = std::min(uint16_t(plyFromRoot), board.getHalfMoves());
    for(uint16_t i = 2; i <= limit; i += 2)
    {
        if(m_searchStack[plyFromRoot - i].hash == board.getHash())
            return true;
    }

    // Check for repeated positions from previous searches
    auto globalSearchIt = m_gameHistory.find(board.getHash());
    if(globalSearchIt != m_gameHistory.end())
    {
        return true;
    }

    if(board.getNumPieces() <= 3)
    {
        for(auto it = m_knownEndgameMaterialDraws.begin(); it != m_knownEndgameMaterialDraws.end(); it++)
        {
            if(*it == board.getMaterialHash())
            {
                return true;
            }
        }
    }

    // Check for 50 move rule
    if(board.getHalfMoves() >= 100)
    {
        return true;
    }

    return false;
}

Move Searcher::getBestMove(Board& board, int depth, SearchResult* searchResult)
{
    SearchParameters parameters = SearchParameters();
    parameters.depth = depth;
    parameters.useDepth = true;
    return search(Board(board), parameters, searchResult);
}

Move Searcher::getBestMoveInTime(Board& board, uint32_t ms, SearchResult* searchResult)
{
    SearchParameters parameters = SearchParameters();
    parameters.msTime = ms;
    parameters.useTime = ms;
    return search(Board(board), parameters, searchResult);
}

Move Searcher::search(Board board, SearchParameters parameters, SearchResult* searchResult)
{
    m_stopSearch = false;
    m_numNodesSearched = 0;
    eval_t searchScore = 0;
    eval_t staticEval = 0;
    Move searchBestMove = NULL_MOVE;
    m_searchParameters = parameters;
    m_pvTable = PvTable();

    m_generation = (uint8_t) std::min(board.getFullMoves(), uint16_t(0x00ff));
    m_numPiecesRoot = board.getNumPieces();
    m_timer.start();

    // Check if only a select set of moves should be searched
    // This set can be set by the search parameters or the table base
    Move* moves = m_searchParameters.searchMoves;
    uint8_t numMoves = m_searchParameters.numSearchMoves;
    bool forceTBScore = false;
    uint8_t wdlTB = 0xff;
    if(m_searchParameters.numSearchMoves == 0)
    {
        if(TBProbeDTZ(board, moves, numMoves, wdlTB))
        {
            forceTBScore = true;
        }
        else
        {
            moves = board.getLegalMoves();
            numMoves = board.getNumLegalMoves();
            board.generateCaptureInfo();
        }
    }

    // If only one move is available, search it at depth 1 to not waist time
    // but still give it a score and pv-line
    if(numMoves == 1)
    {
        m_searchParameters.depth = 1;
        m_searchParameters.useDepth = true;
    }

    m_evaluator.initAccumulatorStack(board);

    std::optional<ttEntry_t> ttEntry = m_tt.get(board.getHash(), 0);

    if(ttEntry.has_value())
    {
        searchBestMove = ttEntry->bestMove;
        staticEval = ttEntry->staticEval;
    }
    else
    {
        searchBestMove = NULL_MOVE;
        staticEval = m_evaluator.evaluate(board, 0);
    }

    m_searchStack[0] = {
        .hash = board.getHash(),
        .staticEval = staticEval
    };

    uint32_t depth = 0;
    while(!m_searchParameters.useDepth || m_searchParameters.depth > depth)
    {
        depth++;

        eval_t aspirationWindowAlpha = 25;
        eval_t aspirationWindowBeta  = 25;

        searchStart:
        // Reset seldepth for each depth interation
        m_seldepth = 0;

        // The local variable for the best move from the previous iteration is used by the move selector.
        // This is in case the move from the transposition is not 'correct' due to a miss.
        // Misses can happen if the position cannot replace another position
        // This is required to allow using results of incomplete searches
        MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &m_relativeHistory, &board, searchBestMove);

        eval_t alpha = -MATE_SCORE;
        eval_t beta = MATE_SCORE;
        Move bestMove = NULL_MOVE;

        // Aspiration window
        bool restartSearch = false;
        bool useAspAlpha = depth > 5 && searchScore < 900 && aspirationWindowAlpha < 600;
        bool useAspBeta = depth > 5 && searchScore < 900 && aspirationWindowBeta < 600;

        // If the window becomes too large, continue using mate score as alpha/beta
        if(useAspAlpha) alpha = searchScore - aspirationWindowAlpha;
        if(useAspBeta)  beta  = searchScore + aspirationWindowBeta;

        for (int i = 0; i < numMoves; i++)  {
            const Move *move = moveSelector.getNextMove();
            Board newBoard = Board(board);
            newBoard.performMove(*move);
            m_evaluator.pushMoveToAccumulator(newBoard, *move);

            eval_t score;
            if(i == 0)
            {
                score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, false, 0);

                // Aspiration window
                // Check if the score is lower than alpha for the first move
                if(useAspAlpha && score <= alpha)
                {
                    restartSearch = true;
                    aspirationWindowAlpha += aspirationWindowAlpha;
                    m_evaluator.popMoveFromAccumulator();
                    break;
                }
            }
            else
            {
                score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, depth - 1, 1, true, false, 0);

                if(score > alpha)
                    score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, false, 0);
            }

            m_evaluator.popMoveFromAccumulator();

            if(m_shouldStop())
                break;

            // Check if the score was outside the aspiration window
            // It is important to break before the best move is assigned,
            // to avoid returning a move which is outside the window when search is stopped
            if(useAspBeta && (score >= beta))
            {
                restartSearch = true;
                aspirationWindowBeta += aspirationWindowBeta;
                break;
            }

            if(score > alpha)
            {
                m_pvTable.updatePv(*move, 0);
                alpha = score;
                bestMove = *move;
            }
        }

        // Restart search if the score fell outside of the aspiration window
        if(!m_stopSearch && restartSearch)
        {
            goto searchStart;
        }

        // The move found can be used even if search is canceled, if we search the previously best move first
        // If a better move is found, it is guaranteed to be better than the best move at the previous depth
        // If the search is so short that the first iteration does not finish, this will still assign a searchBestMove.
        // As long as bestMove is not a null move.
        if(bestMove != NULL_MOVE)
        {
            searchScore = alpha;
            searchBestMove = bestMove;
        }

        // Stop search from writing to TT
        // If search is stopped, corrigate depth to the depth from the previous iterations
        if(m_stopSearch)
        {
            if(depth == 1) WARNING("Not enough time to finish first iteration of search")

            // If the search is so short that no score is calculated for any moves.
            // The first available move is returned to avoid returning a null move
            if(searchBestMove == NULL_MOVE)
            {
                WARNING("Not enough time to find the value of any moves. Returning the first move with score 0")
                searchBestMove = moves[0];
                searchScore = 0;
            }

            depth--;
            break;
        }

        // If search is not canceled, save the best move found in this iteration
        m_tt.add(alpha, bestMove, depth, 0, staticEval, TTFlag::EXACT, m_generation, m_numPiecesRoot, m_numPiecesRoot, board.getHash());

        // Send UCI info
        m_sendUciInfo(alpha, NULL_MOVE, depth, forceTBScore, wdlTB);

        // End early if checkmate is found
        if(Evaluator::isRealMateScore(searchScore))
        {
            break;
        }

        if(depth >= MAX_SEARCH_DEPTH - 1)
            break;
    }

    m_sendUciInfo(searchScore, searchBestMove, depth, forceTBScore, wdlTB);

    m_stats.nodes += m_numNodesSearched;

    if(m_verbose)
    {
        m_tt.logStats();
        logStats();
    }

    // Report potential search results
    if(searchResult != nullptr)
    {
        searchResult->eval = searchScore;
    }

    return searchBestMove;
}

void Searcher::stop()
{
    m_stopSearch = true;
}

bool Searcher::m_shouldStop()
{
    if(m_stopSearch) return true;

    // Limit the number of calls to getMs which is slow
    if((m_numNodesSearched & 0xff) == 0)
    {
        // Check for timeout
        if(m_searchParameters.useTime && m_timer.getMs() >= m_searchParameters.msTime )
        {
            m_stopSearch = true;
            return true;
        }
    }

    if(m_searchParameters.useNodes && m_numNodesSearched >= m_searchParameters.nodes)
    {
        m_stopSearch = true;
        return true;
    }

    return false;
}

void Searcher::m_sendUciInfo(eval_t score, Move move, uint32_t depth, bool forceTBScore, uint8_t wdlTB)
{
    if(!m_verbose)
    {
        return;
    }

    // Send UCI info
    Interface::SearchInfo info = Interface::SearchInfo();
    info.depth = depth;
    info.seldepth = m_seldepth;
    info.msTime = m_timer.getMs();
    info.nsTime = m_timer.getNs();
    info.nodes = m_numNodesSearched;
    info.score = score;
    info.hashfull = m_tt.permills();
    info.pvTable = &m_pvTable;
    if(Evaluator::isRealMateScore(score))
    {
        info.mate = true;
        // Divide by 2 to get moves and not plys.
        // Round away from zero, as the last ply in odd plys has to be counted as a move
        uint16_t distance = std::ceil((MATE_SCORE - std::abs(score)) / 2.0f);
        info.mateDistance = score > 0 ? distance : -distance;
    }
    else if(forceTBScore)
    {
        if(wdlTB >= TB_BLESSED_LOSS && wdlTB <= TB_CURSED_WIN) info.score = 0;
        if(wdlTB == TB_LOSS) info.score = -TB_MATE_SCORE + MAX_MATE_DISTANCE;
        if(wdlTB >= TB_WIN ) info.score =  TB_MATE_SCORE - MAX_MATE_DISTANCE;
    }

    Interface::UCI::sendInfo(info);
    if(move != NULL_MOVE)
    {
        Interface::UCI::sendBestMove(move);
    }
}

SearchStats Searcher::getStats()
{
    return m_stats;
}

void Searcher::logStats()
{
    std::stringstream ss;
    ss << "\n----------------------------------";
    ss << "\nSearcher Stats:";
    ss << "\n----------------------------------";
    ss << "\nNodes                      " << m_stats.nodes;
    ss << "\nEvaluated Positions:       " << m_stats.evaluations;
    ss << "\nPV-Nodes:                  " << m_stats.pvNodes;
    ss << "\nNon-PV-Nodes:              " << m_stats.nonPvNodes;
    ss << "\nQsearch-Nodes              " << m_stats.qSearchNodes;
    ss << "\nExact TT Values used:      " << m_stats.exactTTValuesUsed;
    ss << "\nLower TT Values used:      " << m_stats.lowerTTValuesUsed;
    ss << "\nUpper TT Values used:      " << m_stats.upperTTValuesUsed;
    ss << "\nTB Hits:                   " << m_stats.tbHits;
    ss << "\nNull-window Searches:      " << m_stats.nullWindowSearches;
    ss << "\nNull-window Re-searches:   " << m_stats.researchesRequired;
    ss << "\nNull-Move Cutoffs:         " << m_stats.nullMoveCutoffs;
    ss << "\nFailed Null-Move Cutoffs:  " << m_stats.failedNullMoveCutoffs;
    ss << "\nFutilityPrunedMoves:       " << m_stats.futilityPrunedMoves;
    ss << "\nRazor Cutoffs:             " << m_stats.razorCutoffs;
    ss << "\nFailed Razor Cutoffs:      " << m_stats.failedRazorCutoffs;
    ss << "\nReverseFutilityCutoffs:    " << m_stats.reverseFutilityCutoffs;
    ss << "\nLate Pruned Moves:         " << m_stats.lmpPrunedMoves;
    ss << "\nSingular Extensions:       " << m_stats.singularExtensions;
    ss << "\nFailed Singular Extensions:" << m_stats.failedSingularExtensions;
    ss << "\nProbCuts:                  " << m_stats.probCuts;
    ss << "\nProbCut Quiet Searches:    " << m_stats.probCutQSearches;
    ss << "\nProbCut Searches:          " << m_stats.probCutSearches;
    ss << "\nFailed ProbCuts:           " << m_stats.failedProbCuts;
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nRe-Searches:          " << (float) (100 * m_stats.researchesRequired) / m_stats.nullWindowSearches << "%";
    ss << "\nNull-Move Cutoffs:    " << (float) (100 * m_stats.nullMoveCutoffs) / (m_stats.nullMoveCutoffs + m_stats.failedNullMoveCutoffs) << "%";
    ss << "\nRazor Cutoffs:        " << (float) (100 * m_stats.razorCutoffs) / (m_stats.razorCutoffs + m_stats.failedRazorCutoffs) << "%";
    ss << "\nSingular Extension    " << (float) (100 * m_stats.singularExtensions) / (m_stats.failedSingularExtensions + m_stats.failedSingularExtensions) << "%";
    ss << "\nProbCuts              " << (float) (100 * m_stats.probCuts) / (m_stats.probCuts + m_stats.failedProbCuts) << "%";
    ss << "\nProbCut QSearches     " << (float) (100 * m_stats.probCuts) / m_stats.probCutQSearches << "%";
    ss << "\nProbCut Searches      " << (float) (100 * m_stats.probCuts) / m_stats.probCutSearches << "%";
    ss << "\n----------------------------------";

    LOG(ss.str())
}

std::unordered_map<hash_t, uint8_t, HashFunction>& Searcher::getHistory()
{
    return m_gameHistory;
}

void Searcher::addBoardToHistory(const Board& board)
{
    hash_t hash = board.getHash();
    auto it = m_gameHistory.find(hash);
    if(it == m_gameHistory.end())
        m_gameHistory.emplace(hash, 1);
    else
        it->second += 1;
}

void Searcher::clearHistory()
{
    m_gameHistory.clear();
}