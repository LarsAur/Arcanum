#include <search.hpp>
#include <moveSelector.hpp>
#include <uci.hpp>
#include <chrono>
#include <algorithm>
#include <utils.hpp>
#include <thread>

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
        .researchesRequired = 0LL,
        .nullWindowSearches = 0LL,
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

    m_evaluator.setEnableNNUE(true);
}

Searcher::~Searcher()
{

}

EvalTrace Searcher::m_alphaBetaQuiet(Board& board, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot)
{
    if(m_stopSearch)
        return EvalTrace(0);

    if(m_isDraw(board))
        return DRAW_VALUE;

    if(!board.isChecked(board.getTurn()))
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

    // Only genereate checking moves up to a certain depth
    Move *moves = depth > 0 ? board.getLegalCaptureAndCheckMoves() : board.getLegalCaptureMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        m_numNodesSearched++;
        return board.getTurn() == WHITE ? m_evaluator.evaluate(board, plyFromRoot) : -m_evaluator.evaluate(board, plyFromRoot);
    }

    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board);
    EvalTrace bestScore = EvalTrace(-INF);
    for (int i = 0; i < numMoves; i++)  {
        Board newBoard = Board(board);
        const Move *move = moveSelector.getNextMove();
        newBoard.performMove(*move);
        m_evaluator.pushMoveToAccumulator(newBoard, *move);
        EvalTrace score = -m_alphaBetaQuiet(newBoard, -beta, -alpha, depth - 1, plyFromRoot + 1);
        m_evaluator.popMoveFromAccumulator();
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            if(!(move->moveInfo & (MOVE_INFO_CAPTURE_MASK | MOVE_INFO_PROMOTE_MASK)))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
        }
    }

    // Pop the board off the search stack
    m_search_stack.pop_back();

    return bestScore;
}

EvalTrace Searcher::m_alphaBeta(Board& board, pvLine_t* pvLine, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot, int quietDepth)
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
        if((entry->flags & TT_FLAG_MASK) == TT_FLAG_EXACT)
        {
            #if SEARCH_RECORD_STATS
            m_stats.exactTTValuesUsed++;
            #endif
            return entry->value;
        }
        else if((entry->flags & TT_FLAG_MASK) == TT_FLAG_LOWERBOUND)
        {
            #if SEARCH_RECORD_STATS
            m_stats.lowerTTValuesUsed++;
            #endif
            alpha = std::max(alpha, entry->value);
        }
        else if((entry->flags & TT_FLAG_MASK) == TT_FLAG_UPPERBOUND)
        {
            #if SEARCH_RECORD_STATS
            m_stats.upperTTValuesUsed++;
            #endif
            beta = std::min(beta, entry->value);
        }

        if(alpha >= beta)
        {
            return entry->value;
        }
    }

    if(depth == 0)
    {
        return m_alphaBetaQuiet(board, alpha, beta, quietDepth, plyFromRoot + 1);
    }

    // First search the move found to be best previously
    EvalTrace bestScore = EvalTrace(-INF);
    Move bestMove = Move(0, 0);
    Move* moves = nullptr;
    uint8_t numMoves = 0;

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        m_numNodesSearched++;
        return board.getTurn() == WHITE ? m_evaluator.evaluate(board, plyFromRoot) : -m_evaluator.evaluate(board, plyFromRoot);
    }
    board.generateCaptureInfo();
    bool isChecked = board.isChecked(board.getTurn());
    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    pvLine_t _pvLine;
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : Move(0,0));
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        // Generate new board and make the move
        Board newBoard = Board(board);
        newBoard.performMove(*move);
        EvalTrace score;
        bool requireFullSearch = true;
        bool checkOrChecking = isChecked || newBoard.isChecked(board.getTurn());
        m_evaluator.pushMoveToAccumulator(newBoard, *move);

        // Check for late move reduction
        // Conditions for not doing LMR
        // * Move is a capture move
        // * The previous board was a check
        // * The move is a checking move
        if(i >= 3 && depth >= 3 && !(move->moveInfo & MOVE_INFO_CAPTURE_MASK) && !checkOrChecking)
        {
            EvalTrace nullWindowBeta = -alpha;
            nullWindowBeta.total -= 1;
            score = -m_alphaBeta(newBoard, &_pvLine, nullWindowBeta, -alpha, depth - 2, plyFromRoot + 1, quietDepth);
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
            uint8_t extension = checkOrChecking ? 1 : 0;
            score = -m_alphaBeta(newBoard, &_pvLine, -beta, -alpha, depth + extension - 1, plyFromRoot + 1, quietDepth);
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
            if(!(move->moveInfo & (MOVE_INFO_CAPTURE_MASK | MOVE_INFO_PROMOTE_MASK)))
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
        if(!(move->moveInfo & (MOVE_INFO_CAPTURE_MASK | MOVE_INFO_PROMOTE_MASK)))
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
    if(bestScore <= originalAlpha)
    {
        flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_UPPERBOUND;
    }
    else if(bestScore >= beta)
    {
        flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_LOWERBOUND;
    }
    else
    {
        flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT;
    }
    m_tt->add(bestScore, bestMove, depth, plyFromRoot, flags, board.getHash());

    return bestScore;
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
    Move* moves = parameters.searchMoves;
    uint8_t numMoves = parameters.numSearchMoves;
    if(parameters.numSearchMoves == 0)
    {
        moves = board.getLegalMoves();
        numMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();
    }

    uint32_t depth = 1;
    while(parameters.depth == 0 || parameters.depth >= depth)
    {
        std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), 0);
        MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &m_relativeHistory, &board, entry.has_value() ? entry->bestMove : Move(0,0));

        EvalTrace alpha = EvalTrace(-INF);
        EvalTrace beta = EvalTrace(INF);
        EvalTrace bestScore = EvalTrace(-INF);
        Move bestMove = Move(0,0);
        m_evaluator.initializeAccumulatorStack(board);

        for (int i = 0; i < numMoves; i++)  {
            const Move *move = moveSelector.getNextMove();
            Board newBoard = Board(board);
            newBoard.performMove(*move);
            m_evaluator.pushMoveToAccumulator(newBoard, *move);
            EvalTrace score = -m_alphaBeta(newBoard, &_pvLineTmp, -beta, -alpha, depth - 1, 1, 4);
            m_evaluator.popMoveFromAccumulator();

            if(m_stopSearch)
                break;

            if(score > bestScore)
            {
                bestScore = score;
                bestMove = *move;
                pvLineTmp.moves[0] = bestMove;
                memcpy(pvLineTmp.moves + 1, _pvLineTmp.moves, _pvLineTmp.count * sizeof(Move));
                pvLineTmp.count = _pvLineTmp.count + 1;
                if(score > alpha)
                {
                    alpha = score;
                }
            }
        }

        // The move found can be used even if search is canceled, if we search the previously best move first
        // If a better move is found, is is guaranteed to be better than the best move at the previous depth
        if(!(bestMove == Move(0, 0)))
        {
            searchScore = bestScore;
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
        m_tt->add(bestScore, bestMove, depth, 0, (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT, board.getHash());

        // Send UCI info
        UCI::SearchInfo info = UCI::SearchInfo();
        info.depth = depth;
        info.msTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        info.nodes = m_numNodesSearched;
        info.score = bestScore.total;
        info.hashfull = m_tt->permills();
        info.bestMove = bestMove;
        if(Evaluator::isCheckMateScore(bestScore))
        {
            info.mate = true;
            uint16_t distance = (INT16_MAX - std::abs(bestScore.total)) / 2; // Divide by 2 to get moves and not plys.
            info.mateDistance = bestScore.total > 0 ? distance : -distance;
        }
        for(uint32_t i = 0; i < pvLine.count; i++)
            info.pvLine.push_back(pvLine.moves[i]);
        UCI::sendUciInfo(info);

        // If checkmate is found, search can be stopped
        if(m_evaluator.isCheckMateScore(bestScore))
            break;

        depth++;

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
    info.hashfull = m_tt->permills();
    info.bestMove = searchBestMove;
    for(uint32_t i = 0; i < pvLine.count; i++)
        info.pvLine.push_back(pvLine.moves[i]);
    UCI::sendUciInfo(info);
    UCI::sendUciBestMove(searchBestMove);

    #if SEARCH_RECORD_STATS
    m_stats.evaluatedPositions += m_numNodesSearched;
    #endif
    m_tt->logStats();
    logStats();

    //TODO: Generation should use board full moves
    m_generation += 1; // Generation will update every 4th search


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
    ss << "\nNull-window Searches:      " << m_stats.nullWindowSearches;
    ss << "\nNull-window Re-searches:   " << m_stats.researchesRequired;
    ss << "\n";
    ss << "\nPercentages:";
    ss << "\n----------------------------------";
    ss << "\nRe-Searches:          " << (float) (100 * m_stats.researchesRequired) / m_stats.nullWindowSearches << "%";
    ss << "\n----------------------------------";

    LOG(ss.str())
    #endif
}