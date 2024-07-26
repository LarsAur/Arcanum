#include <search.hpp>
#include <uci.hpp>
#include <utils.hpp>
#include <syzygy.hpp>
#include <algorithm>

using namespace Arcanum;

#define DRAW_VALUE 0

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(32));
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
    for(uint8_t d = 0; d < SEARCH_MAX_PV_LENGTH; d++)
        for(uint8_t m = 0; m < 218; m++)
            m_lmrReductions[d][m] = static_cast<uint8_t>(1 + (std::log2(m) * std::log2(d) / 4));

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
    m_tt->resize(mbSize);
}

void Searcher::clear()
{
    m_generation = 0;
    m_tt->clear();
    m_relativeHistory.clear();
    m_killerMoveManager.clear();
}

eval_t Searcher::m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int plyFromRoot)
{
    if(m_shouldStop())
        return 0;

    m_numNodesSearched++;
    m_seldepth = std::max(m_seldepth, uint8_t(plyFromRoot));

    if(m_isDraw(board))
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

    std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), plyFromRoot);
    if(entry.has_value())
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
    m_searchStack.push_back({.hash = board.getHash(), .staticEval = staticEval});

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
        eval_t score = -m_alphaBetaQuiet(newBoard, -beta, -alpha, plyFromRoot + 1);
        m_evaluator.popMoveFromAccumulator();

        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
        }

        if(bestScore >= alpha)
        {
            alpha = bestScore;
            ttFlag = TTFlag::EXACT;
        }

        if(alpha >= beta)
        {
            ttFlag = TTFlag::LOWER_BOUND;
            if(!(CAPTURED_PIECE(move->moveInfo) | PROMOTED_PIECE(move->moveInfo)))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    m_tt->add(bestScore, bestMove, 0, plyFromRoot, staticEval, ttFlag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());

    // Pop the board off the search stack
    m_searchStack.pop_back();
    return bestScore;
}

eval_t Searcher::m_alphaBeta(Board& board, pvLine_t* pvLine, eval_t alpha, eval_t beta, int depth, int plyFromRoot, bool isNullMoveSearch, uint8_t totalExtensions)
{
    // NOTE: It is important that the size of the pv line is set to zero
    //       before returning due to searchStop, this is because the size
    //       is used in a memcpy and might corrupt the memory if it is undefined
    pvLine->count = 0;

    if(m_shouldStop())
        return 0;

    if(depth <= 0)
        return m_alphaBetaQuiet(board, alpha, beta, plyFromRoot);

    m_numNodesSearched++;
    m_seldepth = std::max(m_seldepth, uint8_t(plyFromRoot));

    if(m_isDraw(board))
        return DRAW_VALUE;

    eval_t originalAlpha = alpha;
    std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), plyFromRoot);
    if(entry.has_value() && (entry->depth >= depth))
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
    pvLine_t _pvLine;


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
        return staticEval;
    }

    board.generateCaptureInfo();
    bool isChecked = board.isChecked();
    bool isImproving = (plyFromRoot > 1) && (staticEval > m_searchStack[plyFromRoot - 2].staticEval);
    bool isWorsening = (plyFromRoot > 1) && (staticEval < m_searchStack[plyFromRoot - 2].staticEval);

    static constexpr eval_t futilityMargins[] = {300, 500, 900};
    if(depth > 0 && depth < 4 && !isChecked)
    {
        // Reverse futility pruning
        if(staticEval - futilityMargins[depth - 1] >= beta)
        {
            m_stats.reverseFutilityCutoffs++;
            return staticEval;
        }
    }

    if(!isChecked && std::abs(beta) < 900)
    {
        if(staticEval + 200 * depth < alpha)
        {
            eval_t razorEval = m_alphaBetaQuiet(board, alpha, beta, plyFromRoot);
            if(razorEval <= alpha)
            {
                m_stats.razorCutoffs++;
                return razorEval;
            }
            m_stats.failedRazorCutoffs++;
        }
    }

    // Perform potential null move search
    bool nullMoveAllowed = board.numOfficers(board.getTurn()) > 1 && board.getColoredPieces(board.getTurn()) > 5 && !isNullMoveSearch && !isChecked && depth > 2;
    if(nullMoveAllowed)
    {
        if(staticEval >= beta)
        {
            Board newBoard = Board(board);
            // int R = 3 + depth / 3;
            int R = 3 + isImproving;
            newBoard.performNullMove();
            eval_t nullMoveScore = -m_alphaBeta(newBoard, &_pvLine, -beta, -beta + 1, depth - R, plyFromRoot + 1, true, totalExtensions);

            if(nullMoveScore >= beta)
            {
                m_stats.nullMoveCutoffs++;
                return nullMoveScore;
            }
            m_stats.failedNullMoveCutoffs++;
        }
    }

    // Push the board on the search stack
    m_searchStack.push_back({.hash = board.getHash(), .staticEval = staticEval});

    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : NULL_MOVE);
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        // Generate new board and make the move
        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt->prefetch(newBoard.getHash());
        eval_t score;
        bool requireFullSearch = true;
        bool checkOrChecking = isChecked || newBoard.isChecked();

        // Futility pruning
        if(depth > 0 && depth < 4 && !checkOrChecking && !(PROMOTED_PIECE(move->moveInfo) | CAPTURED_PIECE(move->moveInfo)))
        {
            if(staticEval + futilityMargins[depth - 1] < alpha && std::abs(alpha) < 900 && std::abs(beta) < 900)
            {
                m_stats.futilityPrunedMoves++;
                continue;
            }
        }

        m_evaluator.pushMoveToAccumulator(newBoard, *move);

        // Check for late move reduction
        // Conditions for not doing LMR
        // * Move is a capture move
        // * The previous board was a check
        // * The move is a checking move
        if(i >= 2 && depth >= 3 && !CAPTURED_PIECE(move->moveInfo) && !checkOrChecking && !Evaluator::isTbCheckMateScore(bestScore) && !Evaluator::isCheckMateScore(bestScore))
        {
            // Perform a reduced search with null-window
            int8_t R = m_lmrReductions[depth][i] + isWorsening - m_killerMoveManager.contains(*move, plyFromRoot);
            R = std::max(int8_t(1), R);
            score = -m_alphaBeta(newBoard, &_pvLine, -alpha - 1, -alpha, depth - R, plyFromRoot + 1, false, totalExtensions);

            // Perform full search if the move is better than expected
            requireFullSearch = score > alpha;

            m_stats.researchesRequired += requireFullSearch;
            m_stats.nullWindowSearches += 1;
        }

        if(requireFullSearch)
        {
            // Extend search for checking moves or check avoiding moves
            // This is to avoid horizon effect occuring by starting with a forced line
            uint8_t extension = (
                isChecked ||
                ((move->moveInfo & MoveInfoBit::PAWN_MOVE) && (RANK(move->to) == 6 || RANK(move->to) == 1)) || // Pawn moved to the 7th rank
                (numMoves == 1)
            ) ? 1 : 0;
            // Limit the number of extensions
            if(totalExtensions > 32)
                extension = 0;

            score = -m_alphaBeta(newBoard, &_pvLine, -beta, -alpha, depth + extension - 1, plyFromRoot + 1, false, totalExtensions + extension);
        }

        m_evaluator.popMoveFromAccumulator();

        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
            pvLine->moves[0] = *move;
            memcpy(pvLine->moves + 1, _pvLine.moves, _pvLine.count * sizeof(Move));
            pvLine->count = _pvLine.count + 1;
        }

        alpha = std::max(alpha, bestScore);

        if(alpha >= beta) // Beta-cutoff
        {
            if(!(CAPTURED_PIECE(move->moveInfo) | PROMOTED_PIECE(move->moveInfo)))
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
        if(!(CAPTURED_PIECE(move->moveInfo) | PROMOTED_PIECE(move->moveInfo)))
        {
            if(depth > 3)
            {
                m_relativeHistory.addButterfly(*move, depth, board.getTurn());
            }
        }
    }

    // Pop the board off the search stack
    m_searchStack.pop_back();

    // Stop the thread from writing to the TT when search is stopped
    if(m_stopSearch)
    {
        return 0;
    }

    TTFlag flag = TTFlag::EXACT;
    if(bestScore <= originalAlpha) flag = TTFlag::UPPER_BOUND;
    else if(bestScore >= beta)     flag = TTFlag::LOWER_BOUND;

    m_tt->add(bestScore, bestMove, depth, plyFromRoot, staticEval, flag, m_generation, m_numPiecesRoot, board.getNumPieces(), board.getHash());

    return bestScore;
}

inline bool Searcher::m_isDraw(const Board& board) const
{
    // Check for repeated positions in the current search
    // * Only check for boards backwards until captures occur (halfMoves)
    // * Only check every other board, as the turn has to be correct
    const size_t stackSize = m_searchStack.size();
    const size_t limit = std::min(stackSize, size_t(board.getHalfMoves()));
    for(size_t i = 2; i <= limit; i += 2)
    {
        if(m_searchStack[stackSize - i].hash == board.getHash())
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
    pvline_t pvLine, pvLineTmp, _pvLineTmp;
    m_searchParameters = parameters;

    m_generation = (uint8_t) std::min(board.getFullMoves(), uint16_t(0x00ff));
    m_numPiecesRoot = board.getNumPieces();
    m_timer.start();

    // Check if only a select set of moves should be searched
    // This set can be set by the search parameters or the table base
    Move* moves = m_searchParameters.searchMoves;
    uint8_t numMoves = m_searchParameters.numSearchMoves;
    bool forceTBScore = false;
    uint8_t tbwdl = 0xff;
    if(m_searchParameters.numSearchMoves == 0)
    {
        if(TBProbeDTZ(board, moves, numMoves, tbwdl))
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

    std::optional<ttEntry_t> ttEntry = m_tt->get(board.getHash(), 0);

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
        bool useAspiration = depth > 5 && searchScore < 900;
        if(useAspiration)
        {
            // If the window becomes too large, continue using mate score as alpha/beta
            if(aspirationWindowAlpha < 600) alpha = searchScore - aspirationWindowAlpha;
            if(aspirationWindowBeta < 600)  beta  = searchScore + aspirationWindowBeta;
        }

        for (int i = 0; i < numMoves; i++)  {
            const Move *move = moveSelector.getNextMove();
            Board newBoard = Board(board);
            newBoard.performMove(*move);
            m_evaluator.pushMoveToAccumulator(newBoard, *move);
            eval_t score = -m_alphaBeta(newBoard, &_pvLineTmp, -beta, -alpha, depth - 1, 1, false, 0);
            m_evaluator.popMoveFromAccumulator();

            if(m_shouldStop())
                break;

            // Check if the score was outside the aspiration window
            // 1. Alpha exceeded beta
            // It is important to break before the best move is assigned,
            // to avoid returning a move which is outside the window when search is stopped
            if(useAspiration && (score > beta))
            {
                alpha = score; // Alpha is updated to correctly detect being outside the window
                break;
            }

            if(score > alpha)
            {
                alpha = score;
                bestMove = *move;
                pvLineTmp.moves[0] = bestMove;
                memcpy(pvLineTmp.moves + 1, _pvLineTmp.moves, _pvLineTmp.count * sizeof(Move));
                pvLineTmp.count = _pvLineTmp.count + 1;
            }
        }

        if(!m_stopSearch)
        {
            // Check if the score was outside the aspiration window
            // 1. Alpha exceeded beta
            if(useAspiration && (alpha > beta))
            {
                aspirationWindowBeta += aspirationWindowBeta;
                goto searchStart;
            }

            // Check if the score was outside the aspiration window
            // 2. Alpha did not improve
            if(useAspiration && (alpha == searchScore - aspirationWindowAlpha))
            {
                aspirationWindowAlpha += aspirationWindowAlpha;
                goto searchStart;
            }
        }

        // The move found can be used even if search is canceled, if we search the previously best move first
        // If a better move is found, it is guaranteed to be better than the best move at the previous depth
        // If the search is so short that the first iteration does not finish, this will still assign a searchBestMove.
        // As long as bestMove is not a null move.
        if(bestMove != NULL_MOVE)
        {
            searchScore = alpha;
            searchBestMove = bestMove;
            memcpy(pvLine.moves, pvLineTmp.moves, pvLineTmp.count * sizeof(Move));
            pvLine.count = pvLineTmp.count;
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
        m_tt->add(alpha, bestMove, depth, 0, staticEval, TTFlag::EXACT, m_generation, m_numPiecesRoot, m_numPiecesRoot, board.getHash());

        // Send UCI info
        UCI::SearchInfo info = UCI::SearchInfo();
        info.depth = depth;
        info.seldepth = m_seldepth;
        info.msTime = m_timer.getMs();
        info.nodes = m_numNodesSearched;
        info.score = alpha;
        info.hashfull = m_tt->permills();
        info.bestMove = bestMove;
        if(Evaluator::isCheckMateScore(alpha))
        {
            info.mate = true;
            // Divide by 2 to get moves and not plys.
            // Round away from zero, as the last ply in odd plys has to be counted as a move
            uint16_t distance = std::ceil((MATE_SCORE - std::abs(alpha)) / 2.0f);
            info.mateDistance = alpha > 0 ? distance : -distance;
        }
        else if(forceTBScore)
        {
            if(tbwdl >= TB_BLESSED_LOSS && tbwdl <= TB_CURSED_WIN) info.score = 0;
            if(tbwdl == TB_LOSS) info.score = -TB_MATE_SCORE + MAX_MATE_DISTANCE;
            if(tbwdl >= TB_WIN ) info.score =  TB_MATE_SCORE - MAX_MATE_DISTANCE;
        }

        if(m_verbose)
        {
            for(uint32_t i = 0; i < pvLine.count; i++)
                info.pvLine.push_back(pvLine.moves[i]);
            UCI::sendUciInfo(info);
        }

        // End early if checkmate is found
        if(Evaluator::isCheckMateScore(searchScore))
        {
            break;
        }

        // The search cannot go deeper than SEARCH_MAX_PV_LENGTH
        // or else it would overflow the pvline array
        // This is set to a high number, but this failsafe is added just in case
        if(depth >= SEARCH_MAX_PV_LENGTH - 1)
            break;
    }

    // Send UCI info
    UCI::SearchInfo info = UCI::SearchInfo();
    info.depth = depth;
    info.seldepth = m_seldepth;
    info.msTime = m_timer.getMs();
    info.nodes = m_numNodesSearched;
    info.score = searchScore;
    info.hashfull = m_tt->permills();
    info.bestMove = searchBestMove;
    if(Evaluator::isCheckMateScore(searchScore))
    {
        info.mate = true;
        // Divide by 2 to get moves and not plys.
        // Round away from zero, as the last ply in odd plys has to be counted as a move
        uint16_t distance = std::ceil((MATE_SCORE - std::abs(searchScore)) / 2.0f);
        info.mateDistance = searchScore > 0 ? distance : -distance;
    }
    else if(forceTBScore)
    {
        if(tbwdl >= TB_BLESSED_LOSS && tbwdl <= TB_CURSED_WIN) info.score = 0;
        if(tbwdl == TB_LOSS) info.score = -TB_MATE_SCORE + MAX_MATE_DISTANCE;
        if(tbwdl >= TB_WIN ) info.score =  TB_MATE_SCORE - MAX_MATE_DISTANCE;
    }

    if(m_verbose)
    {
        for(uint32_t i = 0; i < pvLine.count; i++)
            info.pvLine.push_back(pvLine.moves[i]);
        UCI::sendUciInfo(info);
        UCI::sendUciBestMove(searchBestMove);
    }

    m_stats.nodes += m_numNodesSearched;

    if(m_verbose)
    {
        m_tt->logStats();
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
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nRe-Searches:          " << (float) (100 * m_stats.researchesRequired) / m_stats.nullWindowSearches << "%";
    ss << "\nNull-Move Cutoffs:    " << (float) (100 * m_stats.nullMoveCutoffs) / (m_stats.nullMoveCutoffs + m_stats.failedNullMoveCutoffs) << "%";
    ss << "\nRazor Cutoffs:        " << (float) (100 * m_stats.razorCutoffs) / (m_stats.razorCutoffs + m_stats.failedRazorCutoffs) << "%";
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