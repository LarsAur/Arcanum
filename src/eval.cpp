#include <board.hpp>

using namespace ChessEngine2;

bool EvalTrace::operator==(const EvalTrace& other) const { return total == other.total; }
bool EvalTrace::operator> (const EvalTrace& other) const { return total > other.total;  }
bool EvalTrace::operator< (const EvalTrace& other) const { return total < other.total;  }
bool EvalTrace::operator>=(const EvalTrace& other) const { return total >= other.total; }
bool EvalTrace::operator<=(const EvalTrace& other) const { return total <= other.total; }

Eval::Eval(uint8_t pawnEvalIndicies, uint8_t materialEvalIndicies)
{
    // Create eval lookup tables
    m_pawnEvalTableSize = 1LL << (pawnEvalIndicies - 1);
    m_materialEvalTableSize = 1LL << (materialEvalIndicies - 1);
    m_pawnEvalTableMask = m_pawnEvalTableSize - 1;
    m_materialEvalTableMask = m_materialEvalTableSize - 1;
    m_pawnEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_pawnEvalTableSize]);
    m_materialEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_materialEvalTableSize]);

    // Initialize the tables
    for(uint64_t i = 0; i < m_pawnEvalTableSize; i++)
    {
        m_pawnEvalTable[i].hash = 0LL;
        m_pawnEvalTable[i].value = 0;
    }

    for(uint64_t i = 0; i < m_materialEvalTableSize; i++)
    {
        m_materialEvalTable[i].hash = 0LL;
        m_materialEvalTable[i].value = 0;
    }
}

// Evaluates positive value for WHITE
EvalTrace Eval::evaluate(Board& board)
{
    EvalTrace trace = EvalTrace();

    // Check for stalemate and checkmate
    // TODO: Create function hasLegalMoves
    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0)
    {
        if(board.isChecked(board.m_turn))
        {
            // subtract number of full moves from the score to have incentive for fastest checkmate
            // If there are more checkmates and this is not done, it might be "confused" and move between
            trace.total = board.m_turn == WHITE ? (-INT16_MAX + board.m_fullMoves) : (INT16_MAX - board.m_fullMoves);
            #ifdef FULL_TRACE
            trace.checkmate = true;
            #endif // FULL_TRACE
            return trace;
        }
        
        #ifdef FULL_TRACE
        trace.stalemate = true;
        #endif // FULL_TRACE
        trace.total = 0;
        return trace;
    }

    #ifdef FULL_TRACE    
    trace.pawns = m_getPawnEval(board);
    trace.material = m_getMaterialEval(board);
    trace.mobility = m_getMobilityEval(board);
    trace.total = trace.pawns + trace.material + trace.mobility;
    #else
    trace.total = m_getPawnEval(board)
    + m_getMaterialEval(board)
    + m_getMobilityEval(board);
    #endif // FULL_TRACE
    return trace;
}

constexpr eval_t doublePawnScore = -12;
constexpr eval_t passedPawnScore = 25;
constexpr eval_t pawnRankScore = 4;
constexpr eval_t pawnSupportScore = 12;
constexpr eval_t pawnBackwardScore = -12;

// https://www.chessprogramming.org/Pawn_Structure
inline eval_t Eval::m_getPawnEval(Board& board)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_pawnHash & m_pawnEvalTableMask;
    evalEntry_t* evalEntry = &m_pawnEvalTable[evalIdx];
    if(evalEntry->hash == board.m_pawnHash)
    {
        return evalEntry->value;
    }

    // Constants used during evaluations
    static const bitboard_t bbAFile = 0x0101010101010101;
    static const bitboard_t bbHFile = 0xF0F0F0F0F0F0F0F0; 
    static const bitboard_t wForwardLookup[] = { // indexed by rank
        0x0000000000000000,
        0xFFFFFFFFFFFF0000,
        0xFFFFFFFFFF000000,
        0xFFFFFFFF00000000,
        0xFFFFFF0000000000,
        0xFFFF000000000000,
        0xFF00000000000000,
        0x0000000000000000,
    };
    static const bitboard_t bForwardLookup[] = { // indexed by rank
        0x0000000000000000,
        0x00000000000000FF,
        0x000000000000FFFF,
        0x0000000000FFFFFF,
        0x00000000FFFFFFFF,
        0x000000FFFFFFFFFF,
        0x0000FFFFFFFFFFFF,
        0x0000000000000000,
    };

    // Pre-calculate pawn movements
    bitboard_t wPawns = board.m_bbTypedPieces[W_PAWN][WHITE]; 
    bitboard_t bPawns = board.m_bbTypedPieces[W_PAWN][BLACK];
    bitboard_t wPawnsAttacks = getWhitePawnAttacks(wPawns);
    bitboard_t bPawnsAttacks = getBlackPawnAttacks(bPawns);
    bitboard_t wPawnsMoves = getWhitePawnMoves(wPawns);
    bitboard_t bPawnsMoves = getBlackPawnMoves(bPawns);
    // bitboard_t wPawnsSides = ((wPawns & ~(bbAFile)) << 1) | ((wPawns & ~(bbHFile)) >> 1);
    // bitboard_t bPawnsSides = ((bPawns & ~(bbAFile)) << 1) | ((bPawns & ~(bbHFile)) >> 1);
    bitboard_t wPawnAttackSpans = 0L;
    bitboard_t bPawnAttackSpans = 0L;

    eval_t pawnScore = 0;

    // // Pawn has supporting pawns (in the neighbour files)
    // bitboard_t wPawnSupported = (wPawnsSides | wPawnsAttacks) & wPawns;
    // bitboard_t bPawnSupported = (bPawnsSides | bPawnsAttacks) & bPawns;
    // pawnScore += (CNTSBITS(wPawnSupported) - CNTSBITS(bPawnSupported)) * pawnSupportScore;

    while (wPawns)
    {
        int index = popLS1B(&wPawns);
        int rank = index >> 3;
        int file = index & 0b111;
        
        bitboard_t forward = wForwardLookup[rank];
        bitboard_t backward = bForwardLookup[rank];
        bitboard_t pawnFile = bbAFile << file;
        bitboard_t pawnNeighbourFiles = 0L;

        if(file != 0)
            pawnNeighbourFiles |= (bbAFile << (file - 1));
        if(file != 7)
            pawnNeighbourFiles |= (bbAFile << (file + 1));
        wPawnAttackSpans |= pawnNeighbourFiles & forward;

        // Is passed pawn
        pawnScore += (((pawnNeighbourFiles | pawnFile) & forward & board.m_bbTypedPieces[W_PAWN][BLACK]) == 0) * passedPawnScore;

        // Pawn has supporting pawns (in the neighbour files)
        pawnScore += ((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][WHITE]) != 0) * pawnSupportScore;

        // Is a doubled pawn
        pawnScore += ((pawnFile & forward & board.m_bbTypedPieces[W_PAWN][WHITE]) != 0) * doublePawnScore;
        
        // Pawns closer to pormotion gets value
        pawnScore += rank * pawnRankScore;
    }
    
    while (bPawns)
    {
        int index = popLS1B(&bPawns);
        int rank = index >> 3;
        int file = index & 0b111;
        
        bitboard_t forward = bForwardLookup[rank];
        bitboard_t backward = wForwardLookup[rank];
        bitboard_t pawnFile = bbAFile << file;
        bitboard_t pawnNeighbourFiles = 0L;

        if(file != 0)
            pawnNeighbourFiles |= (bbAFile << (file - 1));
        if(file != 7)
            pawnNeighbourFiles |= (bbAFile << (file + 1));
        bPawnAttackSpans |= pawnNeighbourFiles & forward;

        // Is passed pawn
        pawnScore += (((pawnNeighbourFiles | pawnFile) & forward & board.m_bbTypedPieces[W_PAWN][WHITE]) == 0) * passedPawnScore;

        // Pawn has supporting pawns (in the neighbour files)
        pawnScore -= ((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][BLACK]) != 0) * pawnSupportScore;
        
        // Is not a doubled pawn
        pawnScore -= ((pawnFile & forward & board.m_bbTypedPieces[W_PAWN][BLACK]) != 0) * doublePawnScore;

        // Pawns closer to pormotion gets value
        pawnScore -= (7 - rank) * pawnRankScore;
    }

    // Is backwards pawn: https://www.chessprogramming.org/Backward_Pawns_(Bitboards)
    // TODO: If used to calculate stragglers, move biatboard from move possiton to pawn possiton >> 8
    bitboard_t wBackwardsPawns = wPawnsMoves & bPawnsAttacks & ~wPawnAttackSpans;
    bitboard_t bBackwardsPawns = bPawnsMoves & wPawnsAttacks & ~bPawnAttackSpans;
    pawnScore += (CNTSBITS(wBackwardsPawns) - CNTSBITS(bBackwardsPawns)) * pawnBackwardScore;

    // Write the pawn evaluation to the table
    evalEntry->hash = board.m_pawnHash;
    evalEntry->value = pawnScore;

    return pawnScore;
}

inline eval_t Eval::m_getMaterialEval(Board& board)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_materialHash & m_materialEvalTableMask;
    evalEntry_t* evalEntry = &m_materialEvalTable[evalIdx];
    if(evalEntry->hash == board.m_materialHash)
    {
        return evalEntry->value;
    }

    // Evaluate the piece count
    eval_t pieceScore;
    pieceScore  = 100 * (CNTSBITS(board.m_bbTypedPieces[W_PAWN][WHITE])   - CNTSBITS(board.m_bbTypedPieces[W_PAWN][BLACK]));
    pieceScore += 300 * (CNTSBITS(board.m_bbTypedPieces[W_KNIGHT][WHITE]) - CNTSBITS(board.m_bbTypedPieces[W_KNIGHT][BLACK]));
    pieceScore += 300 * (CNTSBITS(board.m_bbTypedPieces[W_BISHOP][WHITE]) - CNTSBITS(board.m_bbTypedPieces[W_BISHOP][BLACK]));
    pieceScore += 500 * (CNTSBITS(board.m_bbTypedPieces[W_ROOK][WHITE])   - CNTSBITS(board.m_bbTypedPieces[W_ROOK][BLACK]));
    pieceScore += 900 * (CNTSBITS(board.m_bbTypedPieces[W_QUEEN][WHITE])  - CNTSBITS(board.m_bbTypedPieces[W_QUEEN][BLACK]));

    // Write the pawn evaluation to the table
    evalEntry->hash = board.m_materialHash;
    evalEntry->value = pieceScore;

    return pieceScore;
}

constexpr eval_t pawnMobilityScore = 10;
constexpr eval_t rookMobilityScore = 10;
constexpr eval_t knightMobilityScore = 20;
constexpr eval_t bishopMobilityScore = 20;
constexpr eval_t queenMobilityScore = 15;
constexpr eval_t kingMobilityScore = 5;
inline eval_t Eval::m_getMobilityEval(Board& board)
{
    eval_t mobilityScore = 0;

    // White mobility
    {
        mobilityScore += CNTSBITS(getWhitePawnMoves(board.m_bbTypedPieces[W_PAWN][WHITE]) & ~board.m_bbAllPieces) * pawnMobilityScore;
        mobilityScore += CNTSBITS(getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][WHITE]) & board.m_bbColoredPieces[BLACK]) * pawnMobilityScore;

        bitboard_t wRooks = board.m_bbTypedPieces[W_ROOK][WHITE];
        while(wRooks)
        {
            int rookIdx = popLS1B(&wRooks);
            mobilityScore += CNTSBITS(getRookMoves(board.m_bbAllPieces, rookIdx) & ~board.m_bbColoredPieces[WHITE]) * rookMobilityScore;
        }


        bitboard_t wKnights = board.m_bbTypedPieces[W_KNIGHT][WHITE];
        while(wKnights)
        {
            int knightIdx = popLS1B(&wKnights);
            mobilityScore += CNTSBITS(getKnightAttacks(knightIdx) & ~board.m_bbColoredPieces[WHITE]) * knightMobilityScore;
        }

        bitboard_t wBishops = board.m_bbTypedPieces[W_BISHOP][WHITE];
        while(wBishops)
        {
            int bishopIdx = popLS1B(&wBishops);
            mobilityScore += CNTSBITS(getBishopMoves(board.m_bbAllPieces, bishopIdx) & ~board.m_bbColoredPieces[WHITE]) * bishopMobilityScore;
        }

        bitboard_t wQueens = board.m_bbTypedPieces[W_QUEEN][WHITE];
        while(wQueens)
        {
            int queenIdx = popLS1B(&wQueens);
            mobilityScore += CNTSBITS((getBishopMoves(board.m_bbAllPieces, queenIdx) | getRookMoves(board.m_bbAllPieces, queenIdx)) & ~board.m_bbColoredPieces[WHITE]) * queenMobilityScore;
        }

        mobilityScore += CNTSBITS(getKingMoves(LS1B(board.m_bbTypedPieces[W_KING][WHITE])) & ~board.m_bbColoredPieces[WHITE]);
    }

    // Black mobility
    {
        mobilityScore -= CNTSBITS(getWhitePawnMoves(board.m_bbTypedPieces[W_PAWN][BLACK]) & ~board.m_bbAllPieces) * pawnMobilityScore;
        mobilityScore -= CNTSBITS(getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][BLACK]) & board.m_bbColoredPieces[WHITE]) * pawnMobilityScore;

        bitboard_t bRooks = board.m_bbTypedPieces[W_ROOK][BLACK];
        while(bRooks)
        {
            int rookIdx = popLS1B(&bRooks);
            mobilityScore -= CNTSBITS(getRookMoves(board.m_bbAllPieces, rookIdx) & ~board.m_bbColoredPieces[BLACK]) * rookMobilityScore;
        }

        bitboard_t bKnights = board.m_bbTypedPieces[W_KNIGHT][BLACK];
        while(bKnights)
        {
            int knightIdx = popLS1B(&bKnights);
            mobilityScore -= CNTSBITS(getKnightAttacks(knightIdx) & ~board.m_bbColoredPieces[BLACK]) * knightMobilityScore;
        }

        bitboard_t bBishops = board.m_bbTypedPieces[W_BISHOP][BLACK];
        while(bBishops)
        {
            int bishopIdx = popLS1B(&bBishops);
            mobilityScore -= CNTSBITS(getBishopMoves(board.m_bbAllPieces, bishopIdx) & ~board.m_bbColoredPieces[BLACK]) * bishopMobilityScore;
        }

        bitboard_t bQueens = board.m_bbTypedPieces[W_QUEEN][BLACK];
        while(bQueens)
        {
            int queenIdx = popLS1B(&bQueens);
            mobilityScore -= CNTSBITS((getBishopMoves(board.m_bbAllPieces, queenIdx) | getRookMoves(board.m_bbAllPieces, queenIdx)) & ~board.m_bbColoredPieces[BLACK]) * queenMobilityScore;
        }

        mobilityScore -= CNTSBITS(getKingMoves(LS1B(board.m_bbTypedPieces[W_KING][BLACK])) & ~board.m_bbColoredPieces[BLACK]) * kingMobilityScore;
    }

    return mobilityScore;
}