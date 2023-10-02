#include <chrono>
#include <search.hpp>
#include <moveSelector.hpp>
#include <algorithm>
#include <utils.hpp>
#include <thread>

using namespace Arcanum;

#define DRAW_VALUE EvalTrace(0)

Searcher::Searcher()
{
    m_tt = std::unique_ptr<TranspositionTable>(new TranspositionTable(32));
    m_eval = std::unique_ptr<Eval>(new Eval(16, 16));
    
    #if SEARCH_RECORD_STATS
    m_stats = {
        .evaluatedPositions = 0LL,
        .exactTTValuesUsed  = 0LL,
        .lowerTTValuesUsed  = 0LL,
        .upperTTValuesUsed  = 0LL,
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

EvalTrace Searcher::m_alphaBetaQuiet(Board& board, EvalTrace alpha, EvalTrace beta, int depth, int plyFromRoot)
{
    if(m_stopSearch)
        return EvalTrace(0);

    if(m_isDraw(board))
        return DRAW_VALUE;

    if(!board.isChecked(board.getTurn()))
    {
        #if SEARCH_RECORD_STATS
        m_stats.evaluatedPositions++;
        #endif
        
        EvalTrace standPat = board.getTurn() == WHITE ? m_eval->evaluate(board, plyFromRoot) : -m_eval->evaluate(board, plyFromRoot);
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
        return board.getTurn() == WHITE ? m_eval->evaluate(board, plyFromRoot) : -m_eval->evaluate(board, plyFromRoot);
    }

    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    board.generateCaptureInfo();
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &board);
    EvalTrace bestScore = EvalTrace(-INF);
    for (int i = 0; i < numMoves; i++)  {
        Board new_board = Board(board);
        const Move *move = moveSelector.getNextMove();
        new_board.performMove(*move);
        EvalTrace score = -m_alphaBetaQuiet(new_board, -beta, -alpha, depth - 1, plyFromRoot + 1);
        bestScore = std::max(bestScore, score);
        alpha = std::max(alpha, bestScore);
        if(alpha >= beta)
        {
            if(!(move->moveInfo & MOVE_INFO_CAPTURE_MASK))
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
        #if SEARCH_RECORD_STATS
        m_stats.evaluatedPositions++;
        #endif
        return board.getTurn() == WHITE ? m_eval->evaluate(board, plyFromRoot) : -m_eval->evaluate(board, plyFromRoot);
    }
    board.generateCaptureInfo();
    bool isChecked = board.isChecked(board.getTurn());
    // Push the board on the search stack
    m_search_stack.push_back(board.getHash());

    pvLine_t _pvLine;
    MoveSelector moveSelector = MoveSelector(moves, numMoves, plyFromRoot, &m_killerMoveManager, &board, entry.has_value() ? entry->bestMove : Move(0,0));
    for (int i = 0; i < numMoves; i++)  {
        const Move* move = moveSelector.getNextMove();

        // Generate new board and make the move
        Board new_board = Board(board);
        new_board.performMove(*move);
        
        EvalTrace score;
        bool requireFullSearch = true;
        bool checkOrChecking = isChecked || new_board.isChecked(board.getTurn());

        // Check for late move reduction
        // Conditions for not doing LMR
        // * Move is a capture move
        // * The previous board was a check
        // * The move is a checking move
        if(i >= 3 && depth >= 3 && !(move->moveInfo & MOVE_INFO_CAPTURE_MASK) && !checkOrChecking)
        {
            EvalTrace lmrBeta = -alpha;
            lmrBeta.total -= 1;
            score = -m_alphaBeta(new_board, &_pvLine, lmrBeta, -alpha, depth - 2, plyFromRoot + 1, quietDepth);
            // Perform full search if the move is better than expected
            requireFullSearch = score > alpha;
        }
        
        if(requireFullSearch)
        {
            if(checkOrChecking)
            {
                // Extend search for checking moves or check avoiding moves
                // This is to avoid horizon effect occuring by starting with a forced line
                score = -m_alphaBeta(new_board, &_pvLine, -beta, -alpha, depth, plyFromRoot + 1, quietDepth);
            }
            else
            {
                score = -m_alphaBeta(new_board, &_pvLine, -beta, -alpha, depth - 1, plyFromRoot + 1, quietDepth);
            }
        }
        
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
            if(!(move->moveInfo & MOVE_INFO_CAPTURE_MASK))
            {
                m_killerMoveManager.add(*move, plyFromRoot);
            }
            break;
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

Move Searcher::getBestMove(Board& board, int depth, int quietDepth)
{
    // Safeguard for not searching deeper than SEARCH_MAX_PV_LENGTH
    if(depth > SEARCH_MAX_PV_LENGTH)
    {
        WARNING("Depth (" << depth << ") is higher than SEARCH_MAX_PV_LENGTH (" << SEARCH_MAX_PV_LENGTH << "). Setting depth to " << SEARCH_MAX_PV_LENGTH)
        depth = SEARCH_MAX_PV_LENGTH;
    }

    m_stopSearch = false;
    auto start = std::chrono::high_resolution_clock::now();

    Move bestMove;
    pvLine_t pvLine, _pvLine;
    EvalTrace bestScore = EvalTrace(-INF);
    for(int d = 1; d <= depth; d++)
    {
        Move* moves = board.getLegalMoves();
        uint8_t numMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();

        std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), 0);
        MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &board, entry.has_value() ? entry->bestMove : Move(0,0));
        
        EvalTrace alpha = EvalTrace(-INF);
        EvalTrace beta = EvalTrace(INF);
        bestScore = EvalTrace(-INF);

        for (int i = 0; i < numMoves; i++)  {
            Board new_board = Board(board);
            const Move *move = moveSelector.getNextMove();
            new_board.performMove(*move);
            EvalTrace score = -m_alphaBeta(new_board, &_pvLine, -beta, -alpha, depth - 1, 1, quietDepth);
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

        m_tt->add(bestScore, bestMove, depth, 0, (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT, board.getHash());
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    
    LOG("Best move: " << bestMove << "\n" << bestScore)
    // Print PV line
    std::stringstream ss;
    for(int i = 0; i < pvLine.count; i++)
    {
        ss << pvLine.moves[i] << " ";
    }
    LOG("PV Line: " << ss.str())
    LOG("Calculated to depth: " << depth << " in " << micros.count() / 1000 << "ms")

    #if TT_RECORD_STATS == 1
    ttStats_t stats = m_tt->getStats();
    LOG("Entries Added: " << stats.entriesAdded)
    LOG("Replacements: " << stats.replacements)
    LOG("Updates: " << stats.updates)
    LOG("Entries in table: " << stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates << " (" << 100 * (stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates) / float(m_tt->getEntryCount()) << "%)")
    LOG("Lookups: " << stats.lookups)
    LOG("Lookup misses: " << stats.lookupMisses)
    LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    LOG("Blocked replacements " << stats.blockedReplacements);
    #endif

    #if SEARCH_RECORD_STATS
    LOG("Search evaluations: " << m_stats.evaluatedPositions);
    LOG("Exact TT values used: " << m_stats.exactTTValuesUsed);
    LOG("Lower TT values used: " << m_stats.lowerTTValuesUsed);
    LOG("Upper TT values used: " << m_stats.upperTTValuesUsed);
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
    EvalTrace searchScore = EvalTrace(0);
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
            std::optional<ttEntry_t> entry = m_tt->get(board.getHash(), 0);
            MoveSelector moveSelector = MoveSelector(moves, numMoves, 0, &m_killerMoveManager, &board, entry.has_value() ? entry->bestMove : Move(0,0));
            
            EvalTrace alpha = EvalTrace(-INF);
            EvalTrace beta = EvalTrace(INF);
            EvalTrace bestScore = EvalTrace(-INF);
            Move bestMove = Move(0,0);

            for (int i = 0; i < numMoves; i++)  {
                Board new_board = Board(board);
                const Move *move = moveSelector.getNextMove();
                new_board.performMove(*move);
                EvalTrace score = -m_alphaBeta(new_board, &_pvLineTmp, -beta, -alpha, depth - 1, 1, quietDepth);

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
            m_tt->add(bestScore, bestMove, depth, 0, (m_generation & ~TT_FLAG_MASK) | TT_FLAG_EXACT, board.getHash());

            // If checkmate is found, search can be canceled
            // Checkmate score is given by INT16_MAX - board.getFullMoves()
            // absolute value of the checkmate score cannot be less than 
            // INT16_MAX - board.getFullMoves() - depth
            if(std::abs(bestScore.total) >= INT16_MAX - board.getFullMoves() - depth)
            {
                break;
            }

            // The search cannot go deeper than SEARCH_MAX_PV_LENGTH
            // or else it would overflow the pvline array
            // This is set to a high number, but this failsafe is added just in case
            if(depth >= SEARCH_MAX_PV_LENGTH - 1)
            {
                break;
            }
        }
        m_stopSearch = true;
    };

    // Start thread and wait for the time to pass
    // Note: using sleep is less precise, but is more efficient than using high_resolution clock for waiting
    auto start = std::chrono::high_resolution_clock::now();
    m_stopSearch = false;
    std::thread searchThread(search);
    std::this_thread::sleep_for(std::chrono::microseconds(ms * 1000));
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(diff);
    m_stopSearch = true;
    searchThread.join();
    
    if(searchBestMove == Move(0,0))
    {
        ERROR("No moves found by search")
        exit(EXIT_FAILURE);
    }

    LOG("Best move: " << searchBestMove << "\n" << searchScore)
    // Print PV line
    std::stringstream ss;
    for(int i = 0; i < pvLine.count; i++)
    {
        ss << pvLine.moves[i] << " ";
    }
    LOG("PV Line: " << ss.str())
    LOG("Calculated to depth: " << depth << " in " << micros.count() / 1000 << "ms")

    #if TT_RECORD_STATS
    ttStats_t stats = m_tt->getStats();
    LOG("Entries Added: " << stats.entriesAdded)
    LOG("Replacements: " << stats.replacements)
    LOG("Updates: " << stats.updates)
    LOG("Entries in table: " << stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates << " (" << 100 * (stats.entriesAdded - stats.replacements - stats.blockedReplacements - stats.updates) / float(m_tt->getEntryCount()) << "%)")
    LOG("Lookups: " << stats.lookups)
    LOG("Lookup misses: " << stats.lookupMisses)
    LOG("Lookup hits: " << stats.lookups - stats.lookupMisses)
    LOG("Blocked replacements " << stats.blockedReplacements);
    #endif

    #if SEARCH_RECORD_STATS
    LOG("Search evaluations: " << m_stats.evaluatedPositions);
    LOG("Exact TT values used: " << m_stats.exactTTValuesUsed);
    LOG("Lower TT values used: " << m_stats.lowerTTValuesUsed);
    LOG("Upper TT values used: " << m_stats.upperTTValuesUsed);
    #endif

    m_generation += 1; // Generation will update every 4th search

    return searchBestMove;
}

searchStats_t Searcher::getStats()
{
    return m_stats;
}

void Searcher::m_clearUCIInfo()
{
    m_uciInfo.depth = 0;
    m_uciInfo.seldepth = 0;
    m_uciInfo.time = 0;
    m_uciInfo.nodes = 0;
    m_uciInfo.score = 0;
    m_uciInfo.pv.count = 0;
}

uciInfo_t Searcher::getUCIInfo()
{
    return m_uciInfo;
}