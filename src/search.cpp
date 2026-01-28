#include <search.hpp>
#include <uci/uci.hpp>
#include <utils.hpp>
#include <syzygy.hpp>
#include <algorithm>
#include <cmath>

using namespace Arcanum;

#define DRAW_VALUE 0

Searcher::Searcher(bool verbose) :
m_tt(TranspositionTable()),
m_stats(SearchStats()),
m_verbose(verbose),
m_datagenMode(false)
{
    m_tt.resize(32);

    m_lmrReductions = new uint8_t[MaxSearchDepth * MaxMoveCount];
    ASSERT_OR_EXIT(m_lmrReductions != nullptr, "Failed to allocate memory for LMR reductions")

    m_initializeTables();
}

Searcher::~Searcher()
{
    delete[] m_lmrReductions;
}

void Searcher::m_initializeTables()
{
    // Initialize the LMR reduction lookup table
    for(uint8_t d = 0; d < MaxSearchDepth; d++)
    {
        for(uint8_t m = 0; m < MaxMoveCount; m++)
        {
            if((d == 0) || (m == 0))
            {
                m_lmrReductions[d * MaxMoveCount + m] = 0;
            }
            else
            {
                m_lmrReductions[d * MaxMoveCount + m] = static_cast<uint8_t>((std::log2(m) * std::log2(d) / 4));
            }
        }
    }

    // Initialize the LMP threshold lookup table
    for(uint8_t d = 0; d < MaxSearchDepth; d++)
    {
        m_lmpThresholds[0][d] = 1.5f + 0.5f * d * d;
        m_lmpThresholds[1][d] = 3.0f + 1.5f * d * d;
        m_staticPruneMargins[0][d] = std::clamp(-25 * d * d, -10000, 0);  // Quiet moves
        m_staticPruneMargins[1][d] = std::clamp(-100 * d, -10000, 0); // Captures
    }
}

inline uint8_t Searcher::m_getReduction(uint8_t depth, uint8_t moveNumber) const
{
    return m_lmrReductions[depth * MaxMoveCount + moveNumber];
}

void Searcher::setVerbose(bool enable)
{
    m_verbose = enable;
}

void Searcher::setDatagenMode(bool enable)
{
    m_datagenMode = enable;
}

void Searcher::resizeTT(uint32_t mbSize)
{
    m_tt.resize(mbSize);
}

void Searcher::clear()
{
    m_tt.clear();
    m_heuristics.clear();
}

eval_t Searcher::m_adjustEval(eval_t rawEval, Board& board)
{
    if(board.isChecked() || Evaluator::isMateScore(rawEval))
    {
        return rawEval;
    }

    eval_t adjustedEval = rawEval + m_heuristics.correctionHistory.get(board);
    return Evaluator::clampEval(adjustedEval);
}

template <bool isPv>
eval_t Searcher::m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int plyFromRoot)
{
    if(m_shouldStop())
    {
        return 0;
    }

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
    {
        return DRAW_VALUE;
    }

    eval_t bestScore = -Evaluator::MateScore;

    std::optional<TTEntry> entry = m_tt.get(board.getHash(), plyFromRoot);
    Move ttMove = NULL_MOVE;
    if(entry.has_value())
    {
        PackedMove packedMove = entry->getPackedMove();
        ttMove = board.generateMoveWithInfo(packedMove.from(), packedMove.to(), packedMove.promotionInfo());
    }

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

    m_heuristics.killerManager.clearPly(plyFromRoot + 1);

    eval_t rawEval;
    if(entry.has_value())
    {
        rawEval = entry->rawEval;
    }
    else
    {
        m_stats.evaluations++;
        rawEval = m_evaluator.evaluate(board, plyFromRoot);
    }

    eval_t staticEval = m_adjustEval(rawEval, board);

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
    m_searchStacks.hashes     [plyFromRoot] = board.getHash();
    m_searchStacks.staticEvals[plyFromRoot] = staticEval;
    m_searchStacks.moves      [plyFromRoot] = NULL_MOVE;

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_heuristics, &board, ttMove, m_searchStacks.moves);
    TTFlag ttFlag = TTFlag::UPPER_BOUND;
    Move bestMove = NULL_MOVE;
    while(const Move *move = moveSelector.getNextMove())
    {
        if(!isChecked && !move->isPromotion() && !board.see(*move))
        {
            m_stats.quietSeeCuts++;
            continue;
        }

        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt.prefetch(newBoard.getHash());
        m_evaluator.pushMoveToAccumulator(board, *move);
        m_searchStacks.moves[plyFromRoot] = *move;
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
                m_heuristics.killerManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    if(m_stopSearch)
    {
        return 0;
    }

    m_tt.add(bestScore, bestMove, isPv, 0, plyFromRoot, rawEval, ttFlag, m_numPiecesRoot, board.getNumPieces(), board.getHash());

    return bestScore;
}

template <bool isPv>
eval_t Searcher::m_alphaBeta(Board& board, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool cutnode, uint8_t totalExtensions, Move skipMove)
{
    if(m_shouldStop())
    {
        return 0;
    }

    if(depth <= 0)
    {
        return m_alphaBetaQuiet<isPv>(board, alpha, beta, plyFromRoot);
    }

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
    {
        return DRAW_VALUE;
    }

    // Mate distance pruning
    alpha = std::max(alpha, eval_t(plyFromRoot - Evaluator::MateScore));
    beta = std::min(beta, eval_t(Evaluator::MateScore - plyFromRoot - 1));
    if (alpha >= beta)
    {
        return alpha;
    }

    eval_t originalAlpha = alpha;
    eval_t bestScore = -Evaluator::MateScore;
    eval_t maxScore = Evaluator::MateScore;

    std::optional<TTEntry> entry = m_tt.get(board.getHash(), plyFromRoot);
    Move ttMove = NULL_MOVE;
    if(entry.has_value())
    {
        PackedMove packedMove = entry->getPackedMove();
        ttMove = board.generateMoveWithInfo(packedMove.from(), packedMove.to(), packedMove.promotionInfo());
    }

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
    if(skipMove.isNull())
    {
        Syzygy::WDLResult tbResult = Syzygy::TBProbeWDL(board);

        if(tbResult != Syzygy::WDLResult::FAILED)
        {
            m_tbHits++;
            eval_t tbScore;
            TTFlag tbFlag;

            switch (tbResult)
            {
            case Syzygy::WDLResult::WIN:
                tbFlag = TTFlag::LOWER_BOUND;
                tbScore = Evaluator::TbMateScore - plyFromRoot;
                break;
            case Syzygy::WDLResult::LOSS:
                tbFlag = TTFlag::UPPER_BOUND;
                tbScore = -Evaluator::TbMateScore + plyFromRoot;
                break;
            default: // DRAW
                tbFlag = TTFlag::EXACT;
                tbScore = DRAW_VALUE;
            }

            if((tbFlag == TTFlag::EXACT) || (tbFlag == TTFlag::LOWER_BOUND ? tbScore >= beta : tbScore <= alpha))
            {
                // TODO: Might be some bad effects of using tbScore as static eval. Add some value for unknown score.
                m_tt.add(tbScore, NULL_MOVE, isPv, depth, plyFromRoot, tbScore, tbFlag, m_numPiecesRoot, board.getNumPieces(), board.getHash());
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

    m_heuristics.killerManager.clearPly(plyFromRoot + 1);

    Move bestMove = NULL_MOVE;
    Move* moves = nullptr;
    uint8_t numMoves = 0;

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();

    eval_t rawEval;
    if(entry.has_value())
    {
        rawEval = entry->rawEval;
    }
    else
    {
        m_stats.evaluations++;
        rawEval = m_evaluator.evaluate(board, plyFromRoot);
    }

    eval_t staticEval = m_adjustEval(rawEval, board);

    if(numMoves == 0)
    {
        return skipMove.isNull() ? staticEval : alpha;
    }

    board.generateCaptureInfo();
    bool isChecked = board.isChecked();
    bool isImproving = (plyFromRoot > 1) && (staticEval > m_searchStacks.staticEvals[plyFromRoot - 2]);
    bool isWorsening = (plyFromRoot > 1) && (staticEval < m_searchStacks.staticEvals[plyFromRoot - 2]);
    bool opponentHasEasyCapture = board.hasEasyCapture(Color(board.getTurn()^1));
    Move prevMove = m_searchStacks.moves[plyFromRoot-1];
    bool isNullMoveSearch = prevMove.isNull();

    // Push the board on the search stack
    m_searchStacks.hashes     [plyFromRoot] = board.getHash();
    m_searchStacks.staticEvals[plyFromRoot] = staticEval;
    m_searchStacks.moves      [plyFromRoot] = NULL_MOVE;

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
            m_searchStacks.moves[plyFromRoot] = NULL_MOVE;
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
            MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_heuristics, &board, ttMove, m_searchStacks.moves);
            moveSelector.skipQuiets(); // Note: Killers and counters are still included
            while(const Move* move = moveSelector.getNextMove())
            {
                Board newBoard = Board(board);
                newBoard.performMove(*move);
                m_tt.prefetch(newBoard.getHash());
                m_evaluator.pushMoveToAccumulator(board, *move);
                m_searchStacks.moves[plyFromRoot] = *move;

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

    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_heuristics, &board, ttMove, m_searchStacks.moves);
    uint8_t quietMovesPerformed = 0;
    uint8_t captureMovesPerformed = 0;
    Move performedMoves[MaxMoveCount];
    uint32_t moveNumber = 0;

    while (const Move* move = moveSelector.getNextMove())
    {
        if(*move == skipMove)
        {
            continue;
        }

        if(!m_datagenMode
        && !isPv
        && board.hasOfficers(board.getTurn())
        && !Evaluator::isLosingScore(bestScore))
        {
            if(!move->isPromotion()
            && !move->isCastle())
            {
                eval_t margin = m_staticPruneMargins[move->isCapture()][depth];
                if(!board.see(*move, margin))
                {
                    m_stats.seePrunedMoves++;
                    continue;
                }
            }

            // Late move pruning (LMP)
            // Skip quiet moves after having tried a certain number of moves
            if(!isChecked
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
            && (m_heuristics.quietHistory.get(*move, board.getTurn()) < (-3000 * depth))
            && !m_heuristics.killerManager.contains(*move, plyFromRoot)
            && !m_heuristics.counterManager.contains(*move, prevMove, board.getTurn())
            && (moveNumber != 0))
            {
                moveSelector.skipQuiets();
                // Track the number of quiet moves skipped
                // +1 as this move is skipped as well
                m_stats.historyPrunedMoves += moveSelector.getNumQuietsLeft() + 1;
                continue;
            }

            // Futility pruning
            if(move->isQuiet()
            && moveNumber >= 1
            && depth <= 10
            && !isChecked
            && (staticEval + 150 * (depth + 1) <= alpha))
            {
                m_stats.futilityPrunedMoves += moveSelector.getNumQuietsLeft();
                moveSelector.skipQuiets();
                continue;
            }
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
        {
            extension = 0;
        }

        m_evaluator.pushMoveToAccumulator(board, *move);
        m_searchStacks.moves[plyFromRoot] = *move;

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
                R =  m_getReduction(depth, moveNumber);
                R += isWorsening;
                R -= m_heuristics.killerManager.contains(*move, plyFromRoot);
                R -= m_heuristics.counterManager.contains(*move, prevMove, board.getTurn());
                R += cutnode;
                R -= isPv;
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
                m_heuristics.killerManager.add(*move, plyFromRoot);
                m_heuristics.counterManager.setCounter(*move, prevMove, board.getTurn());
                m_heuristics.quietHistory.update(*move, &performedMoves[MaxMoveCount - quietMovesPerformed], quietMovesPerformed, depth, board.getTurn());
                m_heuristics.continuationHistory.update(m_searchStacks.moves, plyFromRoot, *move, &performedMoves[MaxMoveCount - quietMovesPerformed], quietMovesPerformed, board.getTurn(), depth);
            }

            // Update capture history if the move was a capture
            if(move->isCapture())
            {
                m_heuristics.captureHistory.update(*move, &performedMoves[0], captureMovesPerformed, depth, board.getTurn());
            }
            break;
        }

        moveNumber++;

        if(move->isQuiet())
        {
            // Count and track quiet moves for LMP and History
            // Quiets are added from the back
            quietMovesPerformed++;
            performedMoves[MaxMoveCount - quietMovesPerformed] = *move;
        }

        // Note: Capture and Quiet are not inverse as promotions can be non-capture but is not quiet
        if(move->isCapture())
        {
            // Count and track capture moves for CaptureHistory
            // Captures are added from the front
            performedMoves[captureMovesPerformed++] = *move;
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

        m_tt.add(bestScore, bestMove, isPv, depth, plyFromRoot, rawEval, flag, m_numPiecesRoot, board.getNumPieces(), board.getHash());

        if (!board.isChecked() && !bestMove.isCapture() && ((flag == TTFlag::EXACT) || (flag == (bestScore >= staticEval ? TTFlag::LOWER_BOUND : TTFlag::UPPER_BOUND)))) {
            m_heuristics.correctionHistory.update(board, bestScore, staticEval, depth);
        }
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
        if(m_searchStacks.hashes[plyFromRoot - i] == board.getHash())
            return true;
    }

    // Check for repeated positions from previous searches
    auto globalSearchIt = m_gameHistory.find(board.getHash());
    if((globalSearchIt != m_gameHistory.end()) && (globalSearchIt->second >= 2))
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

Move Searcher::search(Board board, SearchParameters parameters, SearchResult* searchResult)
{
    eval_t searchScore = 0;
    eval_t rawEval = 0;
    Move searchBestMove = NULL_MOVE;

    m_tbHits = 0;
    m_stopSearch = false;
    m_numNodesSearched = 0;
    m_parameters = parameters;

    m_pvTable = PvTable();
    m_tt.incrementGeneration();
    m_numPiecesRoot = board.getNumPieces();
    m_timer.start();

    // Copy the search moves from the parameters
    Move* moves = m_parameters.searchMoves;
    uint8_t numMoves = m_parameters.numSearchMoves;

    // Only generate moves or probe tablebase if no search moves are provided
    Syzygy::WDLResult tbResult = Syzygy::WDLResult::FAILED;
    if(numMoves == 0)
    {
        tbResult = Syzygy::TBProbeDTZ(board, moves, numMoves);
        if(tbResult == Syzygy::WDLResult::FAILED)
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
        PackedMove packedMove = ttEntry->getPackedMove();
        Move ttMove = board.generateMoveWithInfo(packedMove.from(), packedMove.to(), packedMove.promotionInfo());

        rawEval = ttEntry->rawEval;

        // We have to check that the move from TT is legal,
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
        rawEval = m_evaluator.evaluate(board, 0);
    }

    eval_t staticEval = m_adjustEval(rawEval, board);

    // Initialize the search stack by pushing the initial board
    m_searchStacks.hashes[0]      = board.getHash();
    m_searchStacks.staticEvals[0] = staticEval;
    m_searchStacks.moves[0]       = NULL_MOVE;

    uint32_t depth = 0;
    while(!m_parameters.useDepth || m_parameters.depth > depth)
    {
        depth++;
        eval_t alpha = -Evaluator::MateScore;
        eval_t beta = Evaluator::MateScore;
        Move bestMove = NULL_MOVE;

        eval_t aspirationWindowAlpha = 25;
        eval_t aspirationWindowBeta  = 25;
        bool rerun = true;

        while(rerun && !m_stopSearch)
        {
            rerun = false;

            bestMove = NULL_MOVE;

            // Reset seldepth for each depth interation
            m_seldepth = 0;

            // The local variable for the best move from the previous iteration is used by the move selector.
            // This is in case the move from the transposition is not 'correct' due to a miss.
            // Misses can happen if the position cannot replace another position
            // This is required to allow using results of incomplete searches
            MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_heuristics, &board, searchBestMove, m_searchStacks.moves);

            // Aspiration window
            // Stop using aspiration if the search score or window size is too high
            bool useAspAlpha = depth > 5 && searchScore < 900 && aspirationWindowAlpha < 600;
            bool useAspBeta  = depth > 5 && searchScore < 900 && aspirationWindowBeta < 600;
            alpha = useAspAlpha ? (searchScore - aspirationWindowAlpha) : -Evaluator::MateScore;
            beta  = useAspBeta  ? (searchScore + aspirationWindowBeta ) : Evaluator::MateScore;

            m_heuristics.killerManager.clearPly(1);

            for (int i = 0; i < numMoves; i++)
            {
                const Move *move = moveSelector.getNextMove();
                Board newBoard = Board(board);
                newBoard.performMove(*move);
                m_tt.prefetch(newBoard.getHash());
                m_evaluator.pushMoveToAccumulator(board, *move);
                m_searchStacks.moves[0] = *move;

                eval_t score;
                if(i == 0)
                {
                    score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, 0);

                    // Aspiration window
                    // Check if the score is lower than alpha for the first move
                    if(useAspAlpha && score <= alpha)
                    {
                        rerun = true;
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
                    {
                        score = -m_alphaBeta<true>(newBoard, -beta, -alpha, depth - 1, 1, false, 0);
                    }
                }

                m_evaluator.popMoveFromAccumulator();

                if(m_shouldStop())
                {
                    break;
                }

                // Check if the score was outside the aspiration window
                // It is important to break before the best move is assigned,
                // to avoid returning a move which is outside the window when search is stopped
                if(useAspBeta && (score >= beta))
                {
                    rerun = true;
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
        if(m_stopSearch)
        {
            if(depth == 1)
            {
                WARNING("Search was stopped before completing the first depth iteration: " << board.fen())
            }

            // Reduce the depth to match the depth of the last complete iteration
            depth = std::max(1u, depth - 1);
            break;
        }

        // If search is not canceled, save the best move found in this iteration
        m_tt.add(alpha, bestMove, true, depth, 0, rawEval, TTFlag::EXACT, m_numPiecesRoot, m_numPiecesRoot, board.getHash());

        // Send UCI info
        m_sendUciInfo(board, searchScore, depth, tbResult);

        if(depth >= MaxSearchDepth - 1)
        {
            break;
        }
    }

    // If the search is so short that no score is calculated for any moves.
    // The first available move is returned to avoid returning a null move
    if(searchBestMove.isNull())
    {
        WARNING("Search was stopped before any moves could be evaluated. Returning the first move with score 0")
        searchBestMove = moves[0];
        searchScore = 0;
    }

    m_sendUciInfo(board, searchScore, depth, tbResult);

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
        if(m_parameters.useTime && m_timer.getMs() >= m_parameters.msTime)
        {
            m_stopSearch = true;
            return true;
        }
    }

    if(m_parameters.useNodes && m_numNodesSearched >= m_parameters.nodes)
    {
        m_stopSearch = true;
        return true;
    }

    return false;
}

void Searcher::m_sendUciInfo(const Board& board, eval_t score, uint32_t depth, Syzygy::WDLResult tbResult)
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
        info.mateDistance = Evaluator::getMateDistance(score);
    }
    else
    {
        // Check for table base result
        switch (tbResult)
        {
        case Syzygy::WDLResult::DRAW: info.score = 0; break;
        case Syzygy::WDLResult::LOSS: info.score = -Evaluator::TbMateScore + Evaluator::TbMaxMateDistance; break;
        case Syzygy::WDLResult::WIN:  info.score =  Evaluator::TbMateScore - Evaluator::TbMaxMateDistance; break;
        case Syzygy::WDLResult::FAILED: break;
        }
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
    ss << "\nQuiet SEE Cuts:            " << m_stats.quietSeeCuts;
    ss << "\nSEE Pruned Moves:          " << m_stats.seePrunedMoves;
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

    DEBUG(ss.str())
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