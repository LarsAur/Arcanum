#include <chrono>
#include <search.hpp>
#include <moveSelector.hpp>
#include <algorithm>
#include <utils.hpp>
#include <thread>

using namespace ChessEngine2;

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(20));
    m_eval = std::unique_ptr<Eval>(new Eval(16, 16));

    #if SEARCH_RECORD_STATS
    m_stats = {
        .evaluatedPositions = 0LL,
        .exactTTValuesUsed  = 0LL,
        .lowerTTValuesUsed  = 0LL,
        .upperTTValuesUsed  = 0LL,
    };
    #endif
}

Searcher::~Searcher()
{
    
}

eval_t Searcher::m_alphaBetaQuiet(Board& board, eval_t alpha, eval_t beta, int depth)
{
    if(m_stopSearch)
    {
        return 0;
    }

    auto boardHistory = Board::getBoardHistory();
    auto it = boardHistory->find(board.getHash());
    if(it != boardHistory->end())
    {
        if(it->second == 2)
        {
            return 0LL;
        }
    }

    if(!board.isChecked(board.getTurn()))
    {
        #if SEARCH_RECORD_STATS
        m_stats.evaluatedPositions++;
        #endif
        eval_t standPat = board.getTurn() == WHITE ? m_eval->evaluate(board) : -m_eval->evaluate(board);
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
        #if SEARCH_RECORD_STATS
        m_stats.evaluatedPositions++;
        #endif
        return board.getTurn() == WHITE ? m_eval->evaluate(board) : -m_eval->evaluate(board);
    }

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, &board);
    eval_t bestScore = -INF;
    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        const Move *move = moveSelector.getNextMove();
        new_board.performMove(*move);
        eval_t score = -m_alphaBetaQuiet(new_board, -beta, -alpha, depth - 1);
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            break;
        } 
    }

    return bestScore;
}

eval_t Searcher::m_alphaBeta(Board& board, pvLine_t* pvLine, eval_t alpha, eval_t beta, int depth, int quietDepth)
{
    if(m_stopSearch)
    {
        return 0;
    }

    pvLine->count = 0;

    // Check for repeated positions from previous searches
    auto boardHistory = Board::getBoardHistory();
    auto globalSearchIt = boardHistory->find(board.getHash());
    if(globalSearchIt != boardHistory->end())
    {
        if(globalSearchIt->second == 2)
        {
            return 0;
        }
    }

    int64_t originalAlpha = alpha;
    bool ttHit;
    ttEntry_t* entry = m_tt->getEntry(board.getHash(), &ttHit);
    if(ttHit && (entry->depth >= depth))
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
        return m_alphaBetaQuiet(board, alpha, beta, quietDepth);
    }

    // First search the move found to be best previously
    eval_t bestScore = -INF;
    Move bestMove = Move(0, 0);
    Move* moves = nullptr;
    uint8_t numMoves = 0;

    moves = board.getLegalMoves();
    numMoves = board.getNumLegalMoves();
    if(numMoves == 0)
    {
        #if SEARCH_RECORD_STATS
        m_stats.evaluatedPositions++;
        #endif
        return board.getTurn() == WHITE ? m_eval->evaluate(board) : -m_eval->evaluate(board);
    }
    board.generateCaptureInfo();

    pvLine_t _pvLine;
    MoveSelector moveSelector = MoveSelector(moves, numMoves, &board, ttHit ? entry->bestMove : Move(0,0));
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        // Generate new board and make the move
        Board new_board = Board(board);

        new_board.performMove(*move);
        eval_t score = -m_alphaBeta(new_board, &_pvLine, -beta, -alpha, depth - 1, quietDepth);
        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
            pvLine->moves[0] = *move;
            memcpy(pvLine->moves + 1, _pvLine.moves, _pvLine.count * sizeof(Move));
            pvLine->count = _pvLine.count + 1;
        }
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            break;
        } 
    }

    // Stop the thread from writing to the TT when search is stopped
    if(m_stopSearch)
    {
        return 0;
    }

    ttEntry_t newEntry;
    newEntry.value = bestScore;
    newEntry.bestMove = bestMove;
    if(bestScore <= originalAlpha)
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_UPPERBOUND;
    }
    else if(bestScore >= beta)
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_LOWERBOUND;
    }
    else
    {
        newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT;
    }

    newEntry.depth = depth;
    m_tt->addEntry(newEntry, board.getHash());

    return bestScore;
}

Move Searcher::getBestMove(Board& board, int depth, int quietDepth)
{
    m_stopSearch = false;
    auto start = std::chrono::high_resolution_clock::now();
    Move* moves = board.getLegalMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();
    Move bestMove;
    pvLine_t pvLine, _pvLine;

    bool ttHit;
    ttEntry_t* entry = m_tt->getEntry(board.getHash(), &ttHit);
    MoveSelector moveSelector = MoveSelector(moves, numMoves, &board, ttHit ? entry->bestMove: Move(0,0));
    
    eval_t alpha = -INF;
    eval_t beta = INF;
    eval_t bestScore = -INF;

    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        const Move *move = moveSelector.getNextMove();
        new_board.performMove(*move);
        eval_t score = -m_alphaBeta(new_board, &_pvLine, -beta, -alpha, depth - 1, quietDepth);
        if(score > bestScore)
        {
            bestScore = score;
            bestMove = *move;
            pvLine.moves[0] = bestMove;
            memcpy(pvLine.moves + 1, _pvLine.moves, _pvLine.count * sizeof(Move));
            pvLine.count = _pvLine.count + 1;

            if(score > alpha)
            {   
                alpha = score;
            }
        }
    }

    ttEntry_t newEntry;
    newEntry.value = bestScore;
    newEntry.bestMove = bestMove;
    newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT;
    newEntry.depth = depth;
    m_tt->addEntry(newEntry, board.getHash());

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    CE2_LOG("Best move: " << bestMove << " Score: " << bestScore)
    // Print PV line
    std::stringstream ss;
    for(int i = 0; i < pvLine.count; i++)
    {
        ss << pvLine.moves[i] << " ";
    }
    CE2_LOG("PV Line: " << ss.str())
    CE2_LOG("Calculated to depth: " << depth << " in " << micros.count() / 1000 << "ms")

    #if TT_RECORD_STATS == 1
    ttStats_t stats = m_tt->getStats();
    CE2_LOG("Entries Added: " << stats.entriesAdded)
    CE2_LOG("Replacements: " << stats.replacements)
    CE2_LOG("Updates: " << stats.updates)
    CE2_LOG("Entries in table: " << stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates << " (" << 100 * (stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates) / float(m_tt->getEntryCount()) << "%)")
    CE2_LOG("Lookups: " << stats.lookups)
    CE2_LOG("Lookup misses: " << stats.lookupMisses)
    CE2_LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    CE2_LOG("Blocked replacements " << stats.blockedReplacements);
    #endif

    #if SEARCH_RECORD_STATS
    CE2_LOG("Search evaluations: " << m_stats.evaluatedPositions);
    CE2_LOG("Exact TT values used: " << m_stats.exactTTValuesUsed);
    CE2_LOG("Lower TT values used: " << m_stats.lowerTTValuesUsed);
    CE2_LOG("Upper TT values used: " << m_stats.upperTTValuesUsed);
    #endif


    m_generation += 1; // Generation will update every 4th search

    return bestMove;
}

Move Searcher::getBestMoveInTime(Board& board, int ms, int quietDepth)
{
    #if SEARCH_RECORD_STATS
    m_stats.quietSearchDepth = 0;
    #endif

    int depth = 0;
    eval_t searchScore = 0;
    Move searchBestMove = Move(0,0);
    pvline_t pvLine, pvLineTmp, _pvLineTmp;

    auto search = [&]()
    {
        Move* moves = board.getLegalMoves();
        uint8_t numMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();

        while(true)
        {
            depth++;
            bool ttHit;
            ttEntry_t* entry = m_tt->getEntry(board.getHash(), &ttHit);
            MoveSelector moveSelector = MoveSelector(moves, numMoves, &board, ttHit ? entry->bestMove: Move(0,0));
            
            eval_t alpha = -INF;
            eval_t beta = INF;
            eval_t bestScore = -INF;
            Move bestMove = Move(0,0);

            for (int i = 0; i < numMoves; i++)  {
                Board new_board = Board(board);
                const Move *move = moveSelector.getNextMove();
                new_board.performMove(*move);

                eval_t score = -m_alphaBeta(new_board, &_pvLineTmp, -beta, -alpha, depth - 1, quietDepth);

                if(m_stopSearch)
                {
                    break;
                }

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
            ttEntry_t newEntry;
            newEntry.value = bestScore;
            newEntry.bestMove = bestMove;
            newEntry.flags = (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT;
            newEntry.depth = depth;
            m_tt->addEntry(newEntry, board.getHash());

            // If checkmate is found, search can be canceled
            // Checkmate score is given by INT16_MAX - board.getFullMoves()
            // absolute value of the checkmate score cannot be less than 
            // INT16_MAX - board.getFullMoves() - depth
            if(std::abs(bestScore) >= INT16_MAX - board.getFullMoves() - depth)
            {
                break;
            }
        }

        m_stopSearch = true;
    };

    // Start thread and wait for the time to pass
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start;
    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    m_stopSearch = false;
    std::thread searchThread(search);
    while((micros.count() < 1000 * ms || depth == 0) && !m_stopSearch)
    {
        end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    }
    m_stopSearch = true;
    searchThread.join();
    
    if(searchBestMove == Move(0,0))
    {
        CE2_ERROR("No moves found by search")
        exit(EXIT_FAILURE);
    }

    CE2_LOG("Best move: " << searchBestMove << " Score: " << searchScore)
    // Print PV line
    std::stringstream ss;
    for(int i = 0; i < pvLine.count; i++)
    {
        ss << pvLine.moves[i] << " ";
    }
    CE2_LOG("PV Line: " << ss.str())
    CE2_LOG("Calculated to depth: " << depth << " in " << micros.count() / 1000 << "ms")

    #if TT_RECORD_STATS
    ttStats_t stats = m_tt->getStats();
    CE2_LOG("Entries Added: " << stats.entriesAdded)
    CE2_LOG("Replacements: " << stats.replacements)
    CE2_LOG("Updates: " << stats.updates)
    CE2_LOG("Entries in table: " << stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates << " (" << 100 * (stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates) / float(m_tt->getEntryCount()) << "%)")
    CE2_LOG("Lookups: " << stats.lookups)
    CE2_LOG("Lookup misses: " << stats.lookupMisses)
    CE2_LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    CE2_LOG("Blocked replacements " << stats.blockedReplacements);
    #endif

    #if SEARCH_RECORD_STATS
    CE2_LOG("Search evaluations: " << m_stats.evaluatedPositions);
    CE2_LOG("Exact TT values used: " << m_stats.exactTTValuesUsed);
    CE2_LOG("Lower TT values used: " << m_stats.lowerTTValuesUsed);
    CE2_LOG("Upper TT values used: " << m_stats.upperTTValuesUsed);
    #endif

    m_generation += 1; // Generation will update every 4th search

    return searchBestMove;
}

searchStats_t Searcher::getStats()
{
    return m_stats;
}