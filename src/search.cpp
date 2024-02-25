#include <search.hpp>
#include <moveSelector.hpp>
#include <uci.hpp>
#include <chrono>
#include <algorithm>
#include <utils.hpp>
#include <thread>
#include <syzygy.hpp>

using namespace Arcanum;

#define DRAW_VALUE EvalTrace(0)

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(32));

    #if SEARCH_RECORD_STATS
    m_stats = {
        .evaluatedPositions = 0LL,
        .exactTTValuesUsed  = 0LL,
        .lowerTTValuesUsed  = 0LL,
        .upperTTValuesUsed  = 0LL,
        .tbHits             = 0LL,
        .researchesRequired = 0LL,
        .nullWindowSearches = 0LL,
        .nullMoveCutoffs    = 0LL,
        .failedNullMoveCutoffs = 0LL,
        .futilityPrunedMoves = 0LL,
        .reverseFutilityCutoffs = 0LL,
    };
    #endif

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
}

Searcher::~Searcher()
{

}

void Searcher::setEnableNNUE(bool enabled)
{
    m_evaluator.setEnableNNUE(enabled);
}

void Searcher::setHCEModelFile(std::string path)
{
    m_evaluator.setHCEModelFile(path);
}

void Searcher::resizeTT(uint32_t mbSize)
{
    m_tt->resize(mbSize);
}

void Searcher::clearTT()
{
    m_generation = 0;
    m_tt->clear();
    m_relativeHistory.clear();
    m_killerMoveManager.clear();
}

EvalTrace Searcher::m_alphaBetaQuiet(Board& board, EvalTrace alpha, EvalTrace beta, int plyFromRoot)
{
    if(m_stopSearch)
        return EvalTrace(0);

    if(m_isDraw(board))
        return DRAW_VALUE;

    bool isChecked = board.isChecked(board.getTurn());

    if(!isChecked)
    {
        m_numNodesSearched++;
        EvalTrace standPat = board.getTurn() == WHITE ? m_evaluator.evaluate(board, plyFromRoot) : -m_evaluator.evaluate(board, plyFromRoot);
        if(standPat >= beta)
        {
            return beta;
        }
        if(alpha < standPat)
        {
            alpha = standPat;
        }
    }

    // Genereate only capture moves if not in check, else generate all moves
    Move *moves = board.getLegalCaptureMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        m_numNodesSearched++;
        // Note: The noMoves parameter for evaluate cannot be used here, as numMoves only check for captures.
        //       Thus there may be other legal quiet moves
        return board.getTurn() == WHITE ? m_evaluator.evaluate(board, plyFromRoot) : -m_evaluator.evaluate(board, plyFromRoot);
    }

    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board);
    EvalTrace bestScore = EvalTrace(-INF);
    for (int i = 0; i < numMoves; i++)  {
        const Move *move = moveSelector.getNextMove();

        if(!board.see(*move) && !isChecked)
            continue;

        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_evaluator.pushMoveToAccumulator(newBoard, *move);
        EvalTrace score = -m_alphaBetaQuiet(newBoard, -beta, -alpha, plyFromRoot + 1);
        m_evaluator.popMoveFromAccumulator();
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            if(!(CAPTURED_PIECE(move->moveInfo) | PROMOTED_PIECE(move->moveInfo)))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    // Pop the board off the search stack
    m_search_stack.pop_back();
    return alpha;
}

EvalTrace Searcher::m_alphaBeta(Board& board, pvLine_t* pvLine, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot, bool isNullMoveSearch, uint8_t totalExtensions)
{
    // NOTE: It is important that the size of the pv line is set to zero
    //       before returning due to searchStop, this is because the size
    //       is used in a memcpy and might corrupt the memory if it is undefined
    pvLine->count = 0;

    if(m_stopSearch)
        return EvalTrace(0);

    if(m_isDraw(board))
        return DRAW_VALUE;

    EvalTrace originalAlpha = alpha;
    std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), plyFromRoot);
    if(entry.has_value() && (entry->depth >= depth))
    {
        switch (entry->flags)
        {
        case TT_FLAG_EXACT:
            #if SEARCH_RECORD_STATS
            m_stats.exactTTValuesUsed++;
            #endif
            return entry->value;
        case TT_FLAG_LOWERBOUND:
            if(entry->value >= beta)
            {
                #if SEARCH_RECORD_STATS
                m_stats.lowerTTValuesUsed++;
                #endif
                return beta;
            }
            alpha = std::max(alpha, entry->value);
            break;
        case TT_FLAG_UPPERBOUND:
            if(entry->value <= alpha)
            {
                #if SEARCH_RECORD_STATS
                m_stats.upperTTValuesUsed++;
                #endif
                return alpha;
            }
            beta = std::min(beta, entry->value);
        }
    }

    // Table base probe
    if(board.getNumPiecesLeft() <= TB_LARGEST && board.getHalfMoves() == 0)
    {
        uint32_t tbResult = TBProbeWDL(board);

        #if SEARCH_RECORD_STATS
        if(tbResult != TB_RESULT_FAILED)
        {
            m_stats.tbHits++;
        }
        #endif

        if(tbResult == TB_DRAW) return 0;
        if(tbResult == TB_WIN) return TB_MATE_SCORE - plyFromRoot;
        if(tbResult == TB_LOSS) return -TB_MATE_SCORE + plyFromRoot;
    }

    if(depth == 0)
    {
        return m_alphaBetaQuiet(board, alpha, beta, plyFromRoot + 1);
    }

    EvalTrace bestScore = EvalTrace(-INF);
    Move bestMove = Move(0, 0);
    Move* moves = nullptr;
    uint8_t numMoves = 0;
    pvLine_t _pvLine;

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        m_numNodesSearched++;
        return board.getTurn() == WHITE ? m_evaluator.evaluate(board, plyFromRoot, true) : -m_evaluator.evaluate(board, plyFromRoot, true);
    }
    board.generateCaptureInfo();
    bool isChecked = board.isChecked(board.getTurn());
    bool nullMoveAllowed = board.hasOfficers(board.getTurn()) && !isNullMoveSearch && !isChecked && depth > 2;
    // TODO: Test R value for NMP, currently using R=3
    // Perform potential null move search
    if(nullMoveAllowed)
    {
        Board newBoard = Board(board);
        newBoard.performNullMove();
        EvalTrace score = -m_alphaBeta(newBoard, &_pvLine, -beta, -alpha, depth - 3, plyFromRoot + 1, true, totalExtensions);

        if(score >= beta)
        {
            #if SEARCH_RECORD_STATS
            m_stats.nullMoveCutoffs++;
            #endif
            return beta;
        }
        #if SEARCH_RECORD_STATS
            m_stats.failedNullMoveCutoffs++;
        #endif
    }

    eval_t staticEvaluation = 0;
    static constexpr eval_t futilityMargins[] = {300, 500, 900};
    if(depth > 0 && depth < 4 && !isChecked)
    {
        staticEvaluation = m_evaluator.evaluate(board, plyFromRoot).total;
        if(board.getTurn() == Color::BLACK) staticEvaluation *= -1;

        // Reverse futility pruning
        if(staticEvaluation - futilityMargins[depth - 1] >= beta.total)
        {
            #if SEARCH_RECORD_STATS
            m_stats.reverseFutilityCutoffs++;
            #endif
            return beta;
        }
    }

    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : Move(0,0));
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        // Generate new board and make the move
        Board newBoard = Board(board);
        newBoard.performMove(*move);
        m_tt->prefetch(newBoard.getHash());
        EvalTrace score;
        bool requireFullSearch = true;
        bool checkOrChecking = isChecked || newBoard.isChecked(board.getTurn());

        // Futility pruning
        if(depth > 0 && depth < 4 && !checkOrChecking && !(PROMOTED_PIECE(move->moveInfo) | CAPTURED_PIECE(move->moveInfo)))
        {
            if(staticEvaluation + futilityMargins[depth - 1] < alpha.total && alpha < 900)
            {
                #if SEARCH_RECORD_STATS
                m_stats.futilityPrunedMoves++;
                #endif
                continue;
            }
        }

        m_evaluator.pushMoveToAccumulator(newBoard, *move);

        // Check for late move reduction
        // Conditions for not doing LMR
        // * Move is a capture move
        // * The previous board was a check
        // * The move is a checking move
        if(i >= 3 && depth >= 3 && !CAPTURED_PIECE(move->moveInfo) && !checkOrChecking)
        {
            EvalTrace nullWindowBeta = -alpha;
            nullWindowBeta.total -= 1;
            score = -m_alphaBeta(newBoard, &_pvLine, nullWindowBeta, -alpha, depth - 2, plyFromRoot + 1, false, totalExtensions);
            // Perform full search if the move is better than expected
            requireFullSearch = score > alpha;
            #if SEARCH_RECORD_STATS
            m_stats.researchesRequired += requireFullSearch;
            m_stats.nullWindowSearches += 1;
            #endif
        }

        if(requireFullSearch)
        {
            // Extend search for checking moves or check avoiding moves
            // This is to avoid horizon effect occuring by starting with a forced line
            uint8_t extension = (
                checkOrChecking ||
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
    m_search_stack.pop_back();

    // Stop the thread from writing to the TT when search is stopped
    if(m_stopSearch)
    {
        return EvalTrace(0);
    }

    uint8_t flags;
    if(bestScore <= originalAlpha) flags = TT_FLAG_UPPERBOUND;
    else if(bestScore >= beta)     flags = TT_FLAG_LOWERBOUND;
    else                           flags = TT_FLAG_EXACT;

    m_tt->add(bestScore, bestMove, depth, plyFromRoot, flags, m_generation, m_nonRevMovesRoot, board.getNumNonReversableMovesPerformed(), board.getHash());

    return alpha;
}

inline bool Searcher::m_isDraw(const Board& board) const
{
    // Check for repeated positions in the current search
    for(auto it = m_search_stack.begin(); it != m_search_stack.end(); it++)
    {
        if(*it == board.getHash())
        {
            return true;
        }
    }

    // Check for repeated positions from previous searches
    auto boardHistory = Board::getBoardHistory();
    auto globalSearchIt = boardHistory->find(board.getHash());
    if(globalSearchIt != boardHistory->end())
    {
        return true;
    }

    if(board.getNumPiecesLeft() <= 3)
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

Move Searcher::getBestMove(Board& board, int depth)
{
    SearchParameters parameters = SearchParameters();
    parameters.depth = depth;
    return search(Board(board), parameters);
}

Move Searcher::getBestMoveInTime(Board& board, uint32_t ms)
{
    SearchParameters parameters = SearchParameters();
    parameters.msTime = ms;
    return search(Board(board), parameters);
}

Move Searcher::search(Board board, SearchParameters parameters)
{
    m_stopSearch = false;
    m_numNodesSearched = 0;
    EvalTrace searchScore = EvalTrace(0);
    Move searchBestMove = Move(0,0);
    pvline_t pvLine, pvLineTmp, _pvLineTmp;
    auto start = std::chrono::high_resolution_clock::now();

    m_generation = (uint8_t) std::min(board.getFullMoves(), uint16_t(0x00ff));
    m_nonRevMovesRoot = board.getNumNonReversableMovesPerformed();

    // Start a thread which will stop the search if the limit is reached
    std::thread trd = std::thread([&] {
        while (!m_stopSearch)
        {
            if(parameters.msTime > 0)
            {
                auto t = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> diff = t - start;
                auto millisecs = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

                if(millisecs.count() >= parameters.msTime)
                    stop();
            }

            // The number of nodes is checked in the thread, which makes it less precise.
            // Usually this results is searching about 30k more nodes.
            // This is however not a problem as the node limit is ambiguous to begin with: https://www.chessprogramming.org/Nodes_per_Second
            if(parameters.nodes > 0 && parameters.nodes <= m_numNodesSearched)
                stop();

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // Check if only a select set of moves should be searched
    // This set can be set by the search parameters or the table base
    Move* moves = parameters.searchMoves;
    uint8_t numMoves = parameters.numSearchMoves;
    bool forceTBScore = false;
    uint8_t tbwdl = 0xff;
    if(parameters.numSearchMoves == 0)
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

    uint32_t depth = 0;
    while(parameters.depth == 0 || parameters.depth > depth)
    {
        depth++;

        // The local variable for the best move from the previous iteration is used by the move selector.
        // This is in case the move from the transposition is not 'correct' due to a miss.
        // Misses can happen if the position cannot replace another position
        // This is required to allow using results of incomplete searches
        MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &m_relativeHistory, &board, searchBestMove);

        EvalTrace alpha = EvalTrace(-INF);
        EvalTrace beta = EvalTrace(INF);
        Move bestMove = Move(0,0);
        m_evaluator.initializeAccumulatorStack(board);

        for (int i = 0; i < numMoves; i++)  {
            const Move *move = moveSelector.getNextMove();
            Board newBoard = Board(board);
            newBoard.performMove(*move);
            m_evaluator.pushMoveToAccumulator(newBoard, *move);
            EvalTrace score = -m_alphaBeta(newBoard, &_pvLineTmp, -beta, -alpha, depth - 1, 1, false, 0);
            m_evaluator.popMoveFromAccumulator();

            if(m_stopSearch)
                break;

            if(score > alpha)
            {
                alpha = score;
                bestMove = *move;
                pvLineTmp.moves[0] = bestMove;
                memcpy(pvLineTmp.moves + 1, _pvLineTmp.moves, _pvLineTmp.count * sizeof(Move));
                pvLineTmp.count = _pvLineTmp.count + 1;
            }
        }

        // The move found can be used even if search is canceled, if we search the previously best move first
        // If a better move is found, is is guaranteed to be better than the best move at the previous depth
        if(!(bestMove == Move(0, 0)))
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
            depth--;
            break;
        }

        // If search is not canceled, save the best move found in this iteration
        m_tt->add(alpha, bestMove, depth, 0, TT_FLAG_EXACT, m_generation, m_nonRevMovesRoot, m_nonRevMovesRoot, board.getHash());

        // Send UCI info
        UCI::SearchInfo info = UCI::SearchInfo();
        info.depth = depth;
        info.msTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        info.nodes = m_numNodesSearched;
        info.score = alpha.total;
        info.hashfull = m_tt->permills();
        info.bestMove = bestMove;
        if(Evaluator::isCheckMateScore(alpha))
        {
            info.mate = true;
            uint16_t distance = (MATE_SCORE - std::abs(alpha.total)) / 2; // Divide by 2 to get moves and not plys.
            info.mateDistance = alpha.total > 0 ? distance : -distance;
        }
        else if(forceTBScore)
        {
            if(tbwdl >= TB_BLESSED_LOSS && tbwdl <= TB_CURSED_WIN) info.score = 0;
            if(tbwdl == TB_LOSS) info.score = -TB_MATE_SCORE + MAX_MATE_DISTANCE;
            if(tbwdl >= TB_WIN ) info.score =  TB_MATE_SCORE - MAX_MATE_DISTANCE;
        }
        for(uint32_t i = 0; i < pvLine.count; i++)
            info.pvLine.push_back(pvLine.moves[i]);
        UCI::sendUciInfo(info);

        // The search cannot go deeper than SEARCH_MAX_PV_LENGTH
        // or else it would overflow the pvline array
        // This is set to a high number, but this failsafe is added just in case
        if(depth >= SEARCH_MAX_PV_LENGTH - 1)
            break;
    }

    // If the search is not already stopped, stop the stopping thread before joining
    stop();
    if(trd.joinable())
        trd.join();

    // Send UCI info
    UCI::SearchInfo info = UCI::SearchInfo();
    info.depth = depth;
    info.msTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
    info.nodes = m_numNodesSearched;
    info.score = searchScore.total;
    info.hashfull = m_tt->permills();
    info.bestMove = searchBestMove;
    if(Evaluator::isCheckMateScore(searchScore))
    {
        info.mate = true;
        uint16_t distance = (MATE_SCORE - std::abs(searchScore.total)) / 2; // Divide by 2 to get moves and not plys.
        info.mateDistance = searchScore.total > 0 ? distance : -distance;
    }
    else if(forceTBScore)
    {
        if(tbwdl >= TB_BLESSED_LOSS && tbwdl <= TB_CURSED_WIN) info.score = 0;
        if(tbwdl == TB_LOSS) info.score = -TB_MATE_SCORE + MAX_MATE_DISTANCE;
        if(tbwdl >= TB_WIN ) info.score =  TB_MATE_SCORE - MAX_MATE_DISTANCE;
    }

    for(uint32_t i = 0; i < pvLine.count; i++)
        info.pvLine.push_back(pvLine.moves[i]);
    UCI::sendUciInfo(info);
    UCI::sendUciBestMove(searchBestMove);

    #if SEARCH_RECORD_STATS
    m_stats.evaluatedPositions += m_numNodesSearched;
    #endif
    m_tt->logStats();
    logStats();

    return searchBestMove;
}

void Searcher::stop()
{
    m_stopSearch = true;
}

SearchStats Searcher::getStats()
{
    return m_stats;
}

void Searcher::logStats()
{
    #if SEARCH_RECORD_STATS == 1
    std::stringstream ss;
    ss << "\n----------------------------------";
    ss << "\nSearcher Stats:";
    ss << "\n----------------------------------";
    ss << "\nEvaluated Positions:       " << m_stats.evaluatedPositions;
    ss << "\nExact TT Values used:      " << m_stats.exactTTValuesUsed;
    ss << "\nLower TT Values used:      " << m_stats.lowerTTValuesUsed;
    ss << "\nUpper TT Values used:      " << m_stats.upperTTValuesUsed;
    ss << "\nTB Hits:                   " << m_stats.tbHits;
    ss << "\nNull-window Searches:      " << m_stats.nullWindowSearches;
    ss << "\nNull-window Re-searches:   " << m_stats.researchesRequired;
    ss << "\nNull-Move Cutoffs:         " << m_stats.nullMoveCutoffs;
    ss << "\nFailed Null-Move Cutoffs:  " << m_stats.failedNullMoveCutoffs;
    ss << "\nFutilityPrunedMoves:       " << m_stats.futilityPrunedMoves;
    ss << "\nReverseFutilityCutoffs:    " << m_stats.reverseFutilityCutoffs;
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nRe-Searches:          " << (float) (100 * m_stats.researchesRequired) / m_stats.nullWindowSearches << "%";
    ss << "\nNull-Move Cutoffs:    " << (float) (100 * m_stats.nullMoveCutoffs) / (m_stats.nullMoveCutoffs + m_stats.failedNullMoveCutoffs) << "%";
    ss << "\n----------------------------------";

    LOG(ss.str())
    #endif
}