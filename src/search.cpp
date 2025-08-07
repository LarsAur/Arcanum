#include <search.hpp>
#include <uci/uci.hpp>
#include <utils.hpp>
#include <syzygy.hpp>
#include <algorithm>
#include <cmath>

using namespace Arcanum;

#define DRAW_VALUE 0

Searcher::Searcher(bool verbose)
{
    m_tt = TranspositionTable();
    m_tt.resize(32);
    m_stats = SearchStats();
    m_generation = 0;

    // Initialize the LMR reduction lookup table
    for(uint8_t d = 0; d < MAX_SEARCH_DEPTH; d++)
        for(uint8_t m = 0; m < MAX_MOVE_COUNT; m++)
            m_lmrReductions[d][m] = static_cast<uint8_t>((std::log2(m) * std::log2(d) / 4));

    // Initialize the LMP threshold lookup table
    for(uint8_t d = 0; d < MAX_SEARCH_DEPTH; d++)
    {
        m_lmpThresholds[0][d] = 1.5f + 0.5f * d * d;
        m_lmpThresholds[1][d] = 3.0f + 1.5f * d * d;
    }

    m_verbose = verbose;
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
    m_history.clear();
    m_captureHistory.clear();
    m_killerMoveManager.clear();
    m_counterMoveManager.clear();
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

    eval_t bestScore = -MATE_SCORE;
    eval_t maxScore = MATE_SCORE;

    // Table base probe
    if(board.getNumPieces() <= TB_LARGEST && board.getHalfMoves() == 0)
    {
        uint32_t tbResult = TBProbeWDL(board);

        if(tbResult != TB_RESULT_FAILED)
        {
            m_tbHits++;
            eval_t tbScore;
            TTFlag tbFlag;

            switch (tbResult)
            {
            case TB_WIN:
                tbFlag = TTFlag::LOWER_BOUND;
                tbScore = TB_MATE_SCORE - plyFromRoot;
                break;
            case TB_LOSS:
                tbFlag = TTFlag::UPPER_BOUND;
                tbScore = -TB_MATE_SCORE + plyFromRoot;
                break;
            default: // TB_DRAW
                tbFlag = TTFlag::EXACT;
                tbScore = DRAW_VALUE;
            }

            if((tbFlag == TTFlag::EXACT) || (tbFlag == TTFlag::LOWER_BOUND ? tbScore >= beta : tbScore <= alpha))
            {
                // TODO: Might be some bad effects of using tbScore as static eval. Add some value for unknown score.
                m_tt.add(tbScore, NULL_MOVE, isPv, 0, plyFromRoot, tbScore, tbFlag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());
                return tbScore;
            }

            if(tbFlag == TTFlag::LOWER_BOUND)
            {
                bestScore = tbScore;
                alpha = std::max(alpha, tbScore);
            }
            else
            {
                maxScore = tbScore;
            }
        }
    }

    std::optional<TTEntry> entry = m_tt.get(board.getHash(), plyFromRoot);
    const PackedMove ttMove = entry.has_value() ? entry->getPackedMove() : PACKED_NULL_MOVE;
    if(!isPv && entry.has_value())
    {
        switch (entry->getTTFlag())
        {
        case TTFlag::EXACT:
            m_stats.exactTTValuesUsed++;
            return entry->eval;
        case TTFlag::LOWER_BOUND:
            if(entry->eval >= beta)
            {
                m_stats.lowerTTValuesUsed++;
                return entry->eval;
            }
            break;
        case TTFlag::UPPER_BOUND:
            if(entry->eval < alpha)
            {
                m_stats.upperTTValuesUsed++;
                return entry->eval;
            }
        }
    }

    m_killerMoveManager.clearPly(plyFromRoot + 1);

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
    m_searchStack[plyFromRoot].hash       = board.getHash();
    m_searchStack[plyFromRoot].staticEval = staticEval;
    m_searchStack[plyFromRoot].move       = NULL_MOVE;

    board.generateCaptureInfo();
    Move prevMove = m_searchStack[plyFromRoot-1].move;
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_history, &m_captureHistory, &m_counterMoveManager, &board, ttMove, prevMove);
    TTFlag ttFlag = TTFlag::UPPER_BOUND;
    Move bestMove = NULL_MOVE;
    while(const Move *move = moveSelector.getNextMove())
    {
        if(!isChecked && !board.see(*move))
            continue;

        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt.prefetch(newBoard.getHash());
        m_evaluator.pushMoveToAccumulator(board, *move);
        m_searchStack[plyFromRoot].move = *move;
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
                ttFlag = TTFlag::EXACT;
            }
            alpha = bestScore;
        }

        if(alpha >= beta)
        {
            ttFlag = TTFlag::LOWER_BOUND;
            if(move->isQuiet())
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    bestScore = std::min(bestScore, maxScore);

    m_tt.add(bestScore, bestMove, isPv, 0, plyFromRoot, staticEval, ttFlag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());

    return bestScore;
}

template <bool isPv>
eval_t Searcher::m_alphaBeta(Board& board, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool cutnode, uint8_t totalExtensions, Move skipMove)
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

    // Mate distance pruning
    alpha = std::max(alpha, eval_t(plyFromRoot - MATE_SCORE));
    beta = std::min(beta, eval_t(MATE_SCORE - plyFromRoot - 1));
    if (alpha >= beta)
      return alpha;

    eval_t originalAlpha = alpha;
    eval_t bestScore = -MATE_SCORE;
    eval_t maxScore = MATE_SCORE;

    std::optional<TTEntry> entry = m_tt.get(board.getHash(), plyFromRoot);
    const PackedMove ttMove = entry.has_value() ? entry->getPackedMove() : PACKED_NULL_MOVE;
    if(!isPv && entry.has_value() && (entry->depth >= depth) && skipMove.isNull())
    {
        switch (entry->getTTFlag())
        {
        case TTFlag::EXACT:
            m_stats.exactTTValuesUsed++;
            return entry->eval;
        case TTFlag::LOWER_BOUND:
            if(entry->eval >= beta)
            {
                m_stats.lowerTTValuesUsed++;
                return entry->eval;
            }
            break;
        case TTFlag::UPPER_BOUND:
            if(entry->eval < alpha)
            {
                m_stats.upperTTValuesUsed++;
                return entry->eval;
            }
        }
    }

    // Table base probe
    if(board.getNumPieces() <= TB_LARGEST && board.getHalfMoves() == 0 && skipMove.isNull())
    {
        uint32_t tbResult = TBProbeWDL(board);

        if(tbResult != TB_RESULT_FAILED)
        {
            m_tbHits++;
            eval_t tbScore;
            TTFlag tbFlag;

            switch (tbResult)
            {
            case TB_WIN:
                tbFlag = TTFlag::LOWER_BOUND;
                tbScore = TB_MATE_SCORE - plyFromRoot;
                break;
            case TB_LOSS:
                tbFlag = TTFlag::UPPER_BOUND;
                tbScore = -TB_MATE_SCORE + plyFromRoot;
                break;
            default: // TB_DRAW
                tbFlag = TTFlag::EXACT;
                tbScore = DRAW_VALUE;
            }

            if((tbFlag == TTFlag::EXACT) || (tbFlag == TTFlag::LOWER_BOUND ? tbScore >= beta : tbScore <= alpha))
            {
                // TODO: Might be some bad effects of using tbScore as static eval. Add some value for unknown score.
                m_tt.add(tbScore, NULL_MOVE, isPv, depth, plyFromRoot, tbScore, tbFlag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());
                return tbScore;
            }

            if(tbFlag == TTFlag::LOWER_BOUND)
            {
                bestScore = tbScore;
                alpha = std::max(alpha, tbScore);
            }
            else
            {
                maxScore = tbScore;
            }
        }
    }

    m_killerMoveManager.clearPly(plyFromRoot + 1);

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
        return skipMove.isNull() ? staticEval : alpha;
    }

    board.generateCaptureInfo();
    bool isChecked = board.isChecked();
    bool isImproving = (plyFromRoot > 1) && (staticEval > m_searchStack[plyFromRoot - 2].staticEval);
    bool isWorsening = (plyFromRoot > 1) && (staticEval < m_searchStack[plyFromRoot - 2].staticEval);
    bool opponentHasEasyCapture = board.hasEasyCapture(Color(board.getTurn()^1));
    Move prevMove = m_searchStack[plyFromRoot-1].move;
    bool isNullMoveSearch = prevMove.isNull();

    // Push the board on the search stack
    m_searchStack[plyFromRoot].hash       = board.getHash();
    m_searchStack[plyFromRoot].staticEval = staticEval;
    m_searchStack[plyFromRoot].move       = NULL_MOVE;

    // Internal Iterative Reductions
    if(isPv && depth >= 5 && !entry.has_value() && !isChecked && skipMove.isNull())
    {
        depth--;
    }

    if(!isPv && !isChecked && skipMove.isNull())
    {
        // Reverse futility pruning
        if(!Evaluator::isCloseToMate(board, beta) && depth < 9)
        {
            if(staticEval - 150 * (depth - !opponentHasEasyCapture)  >= beta)
            {
                m_stats.reverseFutilityCutoffs++;
                return (staticEval + beta) / 2;
            }
        }

        // Razoring
        if(!Evaluator::isCloseToMate(board, alpha))
        {
            if(staticEval + 200 * depth < alpha)
            {
                Board newBoard = Board(board);
                eval_t razorEval = m_alphaBetaQuiet<false>(newBoard, alpha, beta, plyFromRoot);
                if(razorEval <= alpha)
                {
                    m_stats.razorCutoffs++;
                    return razorEval;
                }
                m_stats.failedRazorCutoffs++;
            }
        }

        // Null move search
        if(depth > 2 && !isNullMoveSearch && staticEval >= beta && !Evaluator::isMateScore(beta) && board.hasOfficers(board.getTurn()))
        {
            Board newBoard = Board(board);
            int R = 2 + isImproving + depth / 4;
            newBoard.performNullMove();
            m_tt.prefetch(newBoard.getHash());
            m_searchStack[plyFromRoot].move = NULL_MOVE;
            eval_t nullMoveScore = -m_alphaBeta<false>(newBoard, -beta, -beta + 1, depth - R, plyFromRoot + 1, !cutnode, totalExtensions);

            if(nullMoveScore >= beta)
            {
                m_stats.nullMoveCutoffs++;
                return Evaluator::isMateScore(nullMoveScore) ? beta : nullMoveScore;
            }
            m_stats.failedNullMoveCutoffs++;
        }

        // ProbCut
        eval_t probBeta = beta + 300;
        if(depth >= 6 && !Evaluator::isMateScore(beta) && !(entry.has_value() && entry->depth >= depth - 3 && entry->eval < probBeta))
        {
            MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_history, &m_captureHistory, &m_counterMoveManager, &board, ttMove, prevMove);
            moveSelector.skipQuiets(); // Note: Killers and counters are still included
            while(const Move* move = moveSelector.getNextMove())
            {
                Board newBoard = Board(board);
                newBoard.performMove(*move);
                m_tt.prefetch(newBoard.getHash());
                m_evaluator.pushMoveToAccumulator(board, *move);
                m_searchStack[plyFromRoot].move = *move;

                m_stats.probCutQSearches++;
                eval_t score = -m_alphaBetaQuiet<false>(newBoard, -probBeta, -probBeta + 1, plyFromRoot + 1);

                if(score >= probBeta)
                {
                    m_stats.probCutSearches++;
                    score = -m_alphaBeta<false>(newBoard, -probBeta, -probBeta + 1, depth - 4, plyFromRoot + 1, cutnode, totalExtensions);
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

    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_history, &m_captureHistory, &m_counterMoveManager, &board, ttMove, prevMove);
    uint8_t quietMovesPerformed = 0;
    uint8_t captureMovesPerformed = 0;
    std::array<Move, MAX_MOVE_COUNT> quiets;
    std::array<Move, MAX_MOVE_COUNT> captures;
    uint32_t moveNumber = 0;

    // Futility pruning
    if(!isPv
    && depth <= 10
    && !isChecked
    && (staticEval + 150 * (depth + 1) <= alpha)
    && !Evaluator::isCloseToMate(board, alpha)
    && board.hasOfficers(board.getTurn()))
    {
        m_stats.futilityPrunedMoves += moveSelector.getNumQuietsLeft();
        moveSelector.skipQuiets();
    }

    while (const Move* move = moveSelector.getNextMove())
    {
        if(*move == skipMove)
            continue;

        // Late move pruning (LMP)
        // Skip quiet moves after having tried a certain number of moves
        if( !isPv
        && !Evaluator::isCloseToMate(board, bestScore)
        && !isChecked
        && !moveSelector.isSkippingQuiets()
        && moveNumber >= m_lmpThresholds[isImproving][depth])
        {
            moveSelector.skipQuiets();

            // Track the number of quiet moves skipped
            m_stats.lmpPrunedMoves += moveSelector.getNumQuietsLeft();
        }

        // Prune quiet positions with low history scores
        // Note that we keep counter moves and killer moves
        // in case they have a low history score
        if(depth < 4
        && move->isQuiet()
        && (m_history.get(*move, board.getTurn()) < (-3000 * depth))
        && !m_killerMoveManager.contains(*move, plyFromRoot)
        && !m_counterMoveManager.contains(*move, prevMove, board.getTurn())
        && (moveNumber != 0))
        {
            moveSelector.skipQuiets();
            // Track the number of quiet moves skipped
            // +1 as this move is skipped as well
            m_stats.historyPrunedMoves += moveSelector.getNumQuietsLeft() + 1;
            continue;
        }

        // Generate new board and make the move
        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt.prefetch(newBoard.getHash());
        eval_t score;
        bool checkOrChecking = isChecked || newBoard.isChecked();


        // Extend search when only a single move is available
        uint8_t extension = numMoves == 1;

        // Singular extension
        if(
            skipMove.isNull()
            && extension == 0
            && numMoves > 1
            && depth >= 7
            && entry.has_value()
            && ttMove == *move
            && entry->getTTFlag() != TTFlag::UPPER_BOUND
            && entry->depth >= depth - 2
            && !Evaluator::isMateScore(entry->eval))
        {
            m_stats.singularExtensionAttempts++;

            eval_t seBeta = entry->eval - 3 * (depth / 2);
            uint8_t seDepth = (depth - 1) / 2;
            eval_t seScore = m_alphaBeta<false>(board, seBeta - 1, seBeta, seDepth, plyFromRoot, cutnode, totalExtensions, *move);

            if(seScore < seBeta)
            {
                extension++;
                m_stats.singularExtensions++;
            }
            else if(!isPv && seBeta >= beta && !Evaluator::isMateScore(seScore))
            {
                m_stats.singularExtensionCuts++;
                return seBeta;
            }
        }

        // Limit the number of extensions
        if(totalExtensions > 32)
            extension = 0;

        m_evaluator.pushMoveToAccumulator(board, *move);
        m_searchStack[plyFromRoot].move = *move;

        int32_t newDepth = depth + extension - 1;
        totalExtensions += extension;

        if(moveNumber == 0)
        {
            score = -m_alphaBeta<isPv>(newBoard, -beta, -alpha, newDepth, plyFromRoot + 1, !(isPv | cutnode), totalExtensions);
        }
        else
        {
            // Late move reduction (LMR)
            int8_t R = 0;
            if(depth >= 3 && !move->isCapture() && !checkOrChecking && !Evaluator::isMateScore(bestScore))
            {
                R =  m_lmrReductions[depth][moveNumber];
                R += isWorsening;
                R -= m_killerMoveManager.contains(*move, plyFromRoot);
                R += cutnode;
                R = std::max(int8_t(0), R);
            }

            int32_t reducedDepth = newDepth - R;

            score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, reducedDepth, plyFromRoot + 1, !cutnode, totalExtensions);
            m_stats.researchesRequired += score > alpha && (isPv || newDepth > reducedDepth);
            m_stats.nullWindowSearches += 1;

            // Potential research of LMR returns a score > alpha
            if(score > alpha && newDepth > reducedDepth)
            {
                score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, newDepth, plyFromRoot + 1, !cutnode, totalExtensions);
                m_stats.researchesRequired += score > alpha && isPv;
                m_stats.nullWindowSearches += 1;
            }

            if(score > alpha && isPv)
            {
                score = -m_alphaBeta<isPv>(newBoard, -beta, -alpha, newDepth, plyFromRoot + 1, false, totalExtensions);
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
            if(move->isQuiet())
            {
                m_killerMoveManager.add(*move, plyFromRoot);
                m_counterMoveManager.setCounter(*move, prevMove, board.getTurn());
                m_history.updateHistory(*move, quiets, quietMovesPerformed, depth, board.getTurn());
            }

            // Update capture history if the move was a capture
            if(move->isCapture())
            {
                m_captureHistory.updateHistory(*move, captures, captureMovesPerformed, depth, board.getTurn());
            }
            break;
        }

        moveNumber++;

        if(move->isQuiet())
        {
            // Count and track quiet moves for LMP and History
            quiets[quietMovesPerformed++] = *move;
        }

        // Note: Capture and Quiet are not inverse as promotions can be non-capture but is not quiet
        if(move->isCapture())
        {
            // Count and track capture moves for CaptureHistory
            captures[captureMovesPerformed++] = *move;
        }
    }

    // Stop the thread from writing to the TT when search is stopped
    if(m_stopSearch)
    {
        return 0;
    }

    bestScore = std::min(bestScore, maxScore);

    if(skipMove.isNull())
    {
        TTFlag flag = isPv ? TTFlag::EXACT : TTFlag::UPPER_BOUND;
        if(bestScore <= originalAlpha) flag = TTFlag::UPPER_BOUND;
        else if(bestScore >= beta)     flag = TTFlag::LOWER_BOUND;

        m_tt.add(bestScore, bestMove, isPv, depth, plyFromRoot, staticEval, flag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());
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

    // Check for 50 move rule
    if(board.getHalfMoves() >= 100)
    {
        return true;
    }

    // Check material draw
    return board.isMaterialDraw();
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
    m_tbHits = 0;
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

    m_evaluator.initAccumulatorStack(board);

    std::optional<TTEntry> ttEntry = m_tt.get(board.getHash(), 0);

    if(ttEntry.has_value())
    {
        PackedMove ttMove = ttEntry->getPackedMove();
        staticEval = ttEntry->staticEval;

        // We have to check that the move from TT is legal,
        // and find the matching non-packed move.
        // This is to avoid returning an illegal move in this position
        // in case the search ends in the first iteration
        for(uint8_t i = 0; i < numMoves; i++)
        {
            if(ttMove == moves[i])
            {
                searchBestMove = moves[i];
                break;
            }
        }
    }
    else
    {
        staticEval = m_evaluator.evaluate(board, 0);
    }

    // Initialize the search stack by pushing the initial board
    m_searchStack[0].hash       = board.getHash();
    m_searchStack[0].staticEval = staticEval;
    m_searchStack[0].move       = NULL_MOVE;

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
        MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &m_history, &m_captureHistory, &m_counterMoveManager, &board, PackedMove(searchBestMove), NULL_MOVE);

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

        m_killerMoveManager.clearPly(1);

        for (int i = 0; i < numMoves; i++)  {
            const Move *move = moveSelector.getNextMove();
            Board newBoard = Board(board);
            newBoard.performMove(*move);
            m_tt.prefetch(newBoard.getHash());
            m_evaluator.pushMoveToAccumulator(board, *move);
            m_searchStack[0].move = *move;

            eval_t score;
            if(i == 0)
            {
                score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, 0);

                // Aspiration window
                // Check if the score is lower than alpha for the first move
                if(useAspAlpha && score <= alpha)
                {
                    restartSearch = true;
                    aspirationWindowAlpha += aspirationWindowAlpha;
                    m_evaluator.popMoveFromAccumulator();
                    m_stats.aspirationAlphaFails++;
                    break;
                }
            }
            else
            {
                score = -m_alphaBeta<false>(newBoard, -alpha - 1, -alpha, depth - 1, 1, true, 0);

                if(score > alpha)
                    score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, 0);
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
                m_stats.aspirationBetaFails++;
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
        if(!bestMove.isNull())
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
            if(searchBestMove.isNull())
            {
                WARNING("Not enough time to find the value of any moves. Returning the first move with score 0")
                searchBestMove = moves[0];
                searchScore = 0;
            }

            depth--;
            break;
        }

        // If search is not canceled, save the best move found in this iteration
        m_tt.add(alpha, bestMove, true, depth, 0, staticEval, TTFlag::EXACT, m_generation, m_numPiecesRoot, m_numPiecesRoot, board.getHash());

        // Send UCI info
        m_sendUciInfo(board, alpha, depth, forceTBScore, wdlTB);

        if(depth >= MAX_SEARCH_DEPTH - 1)
            break;
    }

    m_sendUciInfo(board, searchScore, depth, forceTBScore, wdlTB);

    if(m_verbose)
    {
        Interface::UCI::sendBestMove(searchBestMove);
    }

    m_stats.nodes += m_numNodesSearched;
    m_stats.tbHits += m_tbHits;

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

void Searcher::m_sendUciInfo(const Board& board, eval_t score, uint32_t depth, bool forceTBScore, uint8_t wdlTB)
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
    info.tbHits = m_tbHits;
    info.board = board;
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
    ss << "\nHistory Pruned Moves:      " << m_stats.historyPrunedMoves;
    ss << "\nSingular Extensions:       " << m_stats.singularExtensions;
    ss << "\nSingular Extensions Tests: " << m_stats.singularExtensionAttempts;
    ss << "\nSingular Extensions Cuts:  " << m_stats.singularExtensionCuts;
    ss << "\nProbCuts:                  " << m_stats.probCuts;
    ss << "\nProbCut Quiet Searches:    " << m_stats.probCutQSearches;
    ss << "\nProbCut Searches:          " << m_stats.probCutSearches;
    ss << "\nFailed ProbCuts:           " << m_stats.failedProbCuts;
    ss << "\nAspiration Alpha Fails:    " << m_stats.aspirationAlphaFails;
    ss << "\nAspiration Beta Fails:     " << m_stats.aspirationBetaFails;
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nRe-Searches:          " << (float) (100 * m_stats.researchesRequired) / m_stats.nullWindowSearches << "%";
    ss << "\nNull-Move Cutoffs:    " << (float) (100 * m_stats.nullMoveCutoffs) / (m_stats.nullMoveCutoffs + m_stats.failedNullMoveCutoffs) << "%";
    ss << "\nRazor Cutoffs:        " << (float) (100 * m_stats.razorCutoffs) / (m_stats.razorCutoffs + m_stats.failedRazorCutoffs) << "%";
    ss << "\nSingular Extension    " << (float) (100 * m_stats.singularExtensions) / (m_stats.singularExtensionAttempts) << "%";
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