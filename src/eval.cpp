#include <board.hpp>

using namespace Arcanum;

bool EvalTrace::operator==(const EvalTrace& other) const { return total == other.total; }
bool EvalTrace::operator> (const EvalTrace& other) const { return total > other.total;  }
bool EvalTrace::operator< (const EvalTrace& other) const { return total < other.total;  }
bool EvalTrace::operator>=(const EvalTrace& other) const { return total >= other.total; }
bool EvalTrace::operator<=(const EvalTrace& other) const { return total <= other.total; }

static const uint8_t pawnPhase = 0;
static const uint8_t knightPhase = 1;
static const uint8_t bishopPhase = 1;
static const uint8_t rookPhase = 2;
static const uint8_t queenPhase = 4;
static constexpr uint8_t totalPhase = pawnPhase*16 + knightPhase*4 + bishopPhase*4 + rookPhase*4 + queenPhase*2;

Eval::Eval(uint8_t pawnEvalIndicies, uint8_t materialEvalIndicies)
{
    // Create eval lookup tables
    m_pawnEvalTableSize = 1LL << (pawnEvalIndicies - 1);
    m_materialEvalTableSize = 1LL << (materialEvalIndicies - 1);
    m_pawnEvalTableMask = m_pawnEvalTableSize - 1;
    m_materialEvalTableMask = m_materialEvalTableSize - 1;
    m_pawnEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_pawnEvalTableSize]);
    m_materialEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_materialEvalTableSize]);
    m_phaseTable = std::unique_ptr<phaseEntry_t[]>(new phaseEntry_t[m_materialEvalTableSize]);
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

    for(uint64_t i = 0; i < m_materialEvalTableSize; i++)
    {
        m_phaseTable[i].hash = 0LL;
        m_phaseTable[i].value = 0;
    }
}

EvalTrace Eval::getDrawValue(Board& board, uint8_t plyFromRoot)
{
    EvalTrace trace = EvalTrace(0);
    #ifdef FULL_TRACE
    trace.stalemate = true;
    #endif
    // TODO: encurage not drawing at an early stage
    return trace;
}

bool Eval::isCheckMateScore(EvalTrace eval)
{
    return std::abs(eval.total) > (INT16_MAX - UINT8_MAX);
}

// Evaluates positive value for WHITE
EvalTrace Eval::evaluate(Board& board, uint8_t plyFromRoot)
{
    EvalTrace trace = EvalTrace();

    // Check for stalemate and checkmate
    if(!board.hasLegalMove())
    {
        if(board.isChecked(board.m_turn))
        {
            trace.total = board.m_turn == WHITE ? -INT16_MAX + plyFromRoot : INT16_MAX - plyFromRoot;
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

    uint8_t phase = m_getPhase(board);
    #ifdef FULL_TRACE    
    trace.pawns = m_getPawnEval(board, phase);
    trace.material = m_getMaterialEval(board, phase);
    trace.mobility = m_getMobilityEval(board, phase);
    trace.king     = m_getKingEval(board, phase);
    trace.total = trace.pawns + trace.material + trace.mobility + trace.king;
    #else
    trace.total = m_getPawnEval(board, phase)
    + m_getMaterialEval(board, phase)
    + m_getMobilityEval(board, phase)
    + m_getKingEval(board, phase);
    #endif // FULL_TRACE
    return trace;
}

constexpr eval_t doublePawnScore = -12;
constexpr eval_t passedPawnScore = 25;
constexpr eval_t pawnRankScore = 4;
constexpr eval_t pawnSupportScore = 12;
constexpr eval_t pawnBackwardScore = -12;

// https://www.chessprogramming.org/Pawn_Structure
inline eval_t Eval::m_getPawnEval(Board& board, uint8_t phase)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_pawnHash & m_pawnEvalTableMask;
    evalEntry_t* evalEntry = &m_pawnEvalTable[evalIdx];
    if(evalEntry->hash == board.m_pawnHash)
    {
        return evalEntry->value;
    }

    // Constants used during evaluations
    static constexpr bitboard_t bbAFile = 0x0101010101010101;
    // static constexpr bitboard_t bbHFile = 0xF0F0F0F0F0F0F0F0;
    static constexpr bitboard_t wForwardLookup[] = { // indexed by rank
        0x0000000000000000,
        0xFFFFFFFFFFFF0000,
        0xFFFFFFFFFF000000,
        0xFFFFFFFF00000000,
        0xFFFFFF0000000000,
        0xFFFF000000000000,
        0xFF00000000000000,
        0x0000000000000000,
    };
    static constexpr bitboard_t bForwardLookup[] = { // indexed by rank
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

inline eval_t Eval::m_getMaterialEval(Board& board, uint8_t phase)
{
    // Check the material eval table
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

// Values are from: https://github.com/official-stockfish/Stockfish/blob/sf_15.1/src/evaluate.cpp
constexpr eval_t mobilityBonusPawn = 1;
constexpr eval_t mobilityBonusKing = 1; // TODO: Add phase
constexpr eval_t mobilityBonusKnightBegin[] = {-62, -53, -12, -3, 3, 12, 21, 28, 37};
constexpr eval_t mobilityBonusKnightEnd[]   = {-79, -57, -31, -17, 7, 13, 16, 21, 26};
constexpr eval_t mobilityBonusBishopBegin[] = {-47, -20, 14, 29, 39, 53, 53, 60, 62, 69, 78, 83, 91, 96};
constexpr eval_t mobilityBonusBishopEnd[]   = {-59, -25, -8, 12, 21, 40, 56, 58, 65, 72, 78, 87, 88, 98};
constexpr eval_t mobilityBonusRookBegin[]   = {-60, -24, 0, 3, 4, 14, 20, 30, 41, 41, 41, 45, 57, 58, 67};
constexpr eval_t mobilityBonusRookEnd[]     = {-82, -15, 17, 43, 72, 100, 102, 122, 133, 139, 153, 160,165, 170, 175};
constexpr eval_t mobilityBonusQueenBegin[]  = {-29, -16, -8, -8, 18, 25, 23, 37, 41,  54, 65, 68, 69, 70, 70,  70 , 71, 72, 74, 76, 90, 104, 105, 106, 112, 114, 114, 119};
constexpr eval_t mobilityBonusQueenEnd[]    = {-49,-29, -8, 17, 39,  54, 59, 73, 76, 95, 95 ,101, 124, 128, 132, 133, 136, 140, 147, 149, 153, 169, 171, 171, 178, 185, 187, 221};

inline eval_t Eval::m_getMobilityEval(Board& board, uint8_t phase)
{
    eval_t mobilityScoreBegin = 0;
    eval_t mobilityScoreEnd = 0;
    // White mobility
    {
        bitboard_t wRooks = board.m_bbTypedPieces[W_ROOK][WHITE];
        while(wRooks)
        {
            int rookIdx = popLS1B(&wRooks);
            int cnt = CNTSBITS(getRookMoves(board.m_bbAllPieces, rookIdx) & ~board.m_bbColoredPieces[WHITE]);
            mobilityScoreBegin += mobilityBonusRookBegin[cnt];
            mobilityScoreEnd += mobilityBonusRookEnd[cnt];
        }

        bitboard_t wKnights = board.m_bbTypedPieces[W_KNIGHT][WHITE];
        while(wKnights)
        {
            int knightIdx = popLS1B(&wKnights);
            int cnt = CNTSBITS(getKnightAttacks(knightIdx) & ~board.m_bbColoredPieces[WHITE]);
            mobilityScoreBegin += mobilityBonusKnightBegin[cnt];
            mobilityScoreEnd += mobilityBonusKnightEnd[cnt];
        }

        bitboard_t wBishops = board.m_bbTypedPieces[W_BISHOP][WHITE];
        while(wBishops)
        {
            int bishopIdx = popLS1B(&wBishops);
            int cnt = CNTSBITS(getBishopMoves(board.m_bbAllPieces, bishopIdx) & ~board.m_bbColoredPieces[WHITE]);
            mobilityScoreBegin += mobilityBonusBishopBegin[cnt];
            mobilityScoreEnd += mobilityBonusBishopEnd[cnt];
        }

        bitboard_t wQueens = board.m_bbTypedPieces[W_QUEEN][WHITE];
        while(wQueens)
        {
            int queenIdx = popLS1B(&wQueens);
            int cnt = CNTSBITS((getBishopMoves(board.m_bbAllPieces, queenIdx) | getRookMoves(board.m_bbAllPieces, queenIdx)) & ~board.m_bbColoredPieces[WHITE]);
            mobilityScoreBegin += mobilityBonusQueenBegin[cnt];
            mobilityScoreEnd += mobilityBonusQueenEnd[cnt];
        }
    }

    // Black mobility
    {
        bitboard_t bRooks = board.m_bbTypedPieces[W_ROOK][BLACK];
        while(bRooks)
        {
            int rookIdx = popLS1B(&bRooks);
            int cnt = CNTSBITS(getRookMoves(board.m_bbAllPieces, rookIdx) & ~board.m_bbColoredPieces[BLACK]);
            mobilityScoreBegin -= mobilityBonusRookBegin[cnt];
            mobilityScoreEnd -= mobilityBonusRookEnd[cnt];
        }

        bitboard_t bKnights = board.m_bbTypedPieces[W_KNIGHT][BLACK];
        while(bKnights)
        {
            int knightIdx = popLS1B(&bKnights);
            int cnt = CNTSBITS(getKnightAttacks(knightIdx) & ~board.m_bbColoredPieces[BLACK]);
            mobilityScoreBegin -= mobilityBonusKnightBegin[cnt];
            mobilityScoreEnd -= mobilityBonusKnightEnd[cnt];
        }

        bitboard_t bBishops = board.m_bbTypedPieces[W_BISHOP][BLACK];
        while(bBishops)
        {
            int bishopIdx = popLS1B(&bBishops);
            int cnt = CNTSBITS(getBishopMoves(board.m_bbAllPieces, bishopIdx) & ~board.m_bbColoredPieces[BLACK]);
            mobilityScoreBegin -= mobilityBonusBishopBegin[cnt];
            mobilityScoreEnd -= mobilityBonusBishopEnd[cnt];
        }

        bitboard_t bQueens = board.m_bbTypedPieces[W_QUEEN][BLACK];
        while(bQueens)
        {
            int queenIdx = popLS1B(&bQueens);
            int cnt = CNTSBITS((getBishopMoves(board.m_bbAllPieces, queenIdx) | getRookMoves(board.m_bbAllPieces, queenIdx)) & ~board.m_bbColoredPieces[BLACK]);
            mobilityScoreBegin -= mobilityBonusQueenBegin[cnt];
            mobilityScoreEnd -= mobilityBonusQueenEnd[cnt];
        }

    }

    eval_t mobilityScore = (phase * mobilityScoreBegin + (totalPhase - phase) * mobilityScoreEnd) / totalPhase;

    // Pawn mobility
    mobilityScore += CNTSBITS(getWhitePawnMoves(board.m_bbTypedPieces[W_PAWN][WHITE]) & ~board.m_bbAllPieces) * mobilityBonusPawn;
    mobilityScore += CNTSBITS(getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][WHITE]) & board.m_bbColoredPieces[BLACK]) * mobilityBonusPawn;
    mobilityScore -= CNTSBITS(getWhitePawnMoves(board.m_bbTypedPieces[W_PAWN][BLACK]) & ~board.m_bbAllPieces) * mobilityBonusPawn;
    mobilityScore -= CNTSBITS(getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][BLACK]) & board.m_bbColoredPieces[WHITE]) * mobilityBonusPawn;
    // King mobility
    mobilityScore += CNTSBITS(getKingMoves(LS1B(board.m_bbTypedPieces[W_KING][WHITE])) & ~board.m_bbColoredPieces[WHITE]) * mobilityBonusKing;
    mobilityScore -= CNTSBITS(getKingMoves(LS1B(board.m_bbTypedPieces[W_KING][BLACK])) & ~board.m_bbColoredPieces[BLACK]) * mobilityBonusKing;

    return mobilityScore;
}

inline uint8_t Eval::m_getPhase(Board& board)
{
    // Check the phase table
    uint64_t phaseIdx = board.m_materialHash & m_materialEvalTableMask;
    phaseEntry_t* phaseEntry = &m_phaseTable[phaseIdx];
    if(phaseEntry->hash == board.m_materialHash)
    {
        return phaseEntry->value;
    }

    uint8_t phase = (
        pawnPhase * CNTSBITS(board.m_bbTypedPieces[W_PAWN][Color::WHITE] | board.m_bbTypedPieces[W_PAWN][Color::BLACK])
        + knightPhase * CNTSBITS(board.m_bbTypedPieces[W_KNIGHT][Color::WHITE] | board.m_bbTypedPieces[W_KNIGHT][Color::BLACK])
        + bishopPhase * CNTSBITS(board.m_bbTypedPieces[W_BISHOP][Color::WHITE] | board.m_bbTypedPieces[W_BISHOP][Color::BLACK])
        + rookPhase * CNTSBITS(board.m_bbTypedPieces[W_ROOK][Color::WHITE] | board.m_bbTypedPieces[W_ROOK][Color::BLACK])
        + queenPhase * CNTSBITS(board.m_bbTypedPieces[W_QUEEN][Color::WHITE] | board.m_bbTypedPieces[W_QUEEN][Color::BLACK])
    );

    // Limit phase between 0 and totalphase
    // this is done to avoid cases with for example many queens boosting the phase
    phase = std::min(phase, totalPhase);
    
    // Write the phase to the table
    phaseEntry->hash = board.m_materialHash;
    phaseEntry->value = phase;
    return phase;
}

static constexpr eval_t s_kingAreaAttackScore[100] = {
    0,  0,   1,   2,   3,   5,   7,   9,  12,  15,
  18,  22,  26,  30,  35,  39,  44,  50,  56,  62,
  68,  75,  82,  85,  89,  97, 105, 113, 122, 131,
 140, 150, 169, 180, 191, 202, 213, 225, 237, 248,
 260, 272, 283, 295, 307, 319, 330, 342, 354, 366,
 377, 389, 401, 412, 424, 436, 448, 459, 471, 483,
 494, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500,
 500, 500, 500, 500, 500, 500, 500, 500, 500, 500
};

static constexpr eval_t s_whiteKingPositionBegin[64] = {
    20,  25,  30,   0,  12,  25,  30,  20, 
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
};

static constexpr eval_t s_blackKingPositionBegin[64] = {
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
   -12, -12, -12, -12, -12, -12, -12, -12,
    20,  25,  30,   0,  12,  25,  30,  20, 
};

static constexpr eval_t s_kingPositionEnd[64] = {
   -50, -25, -25, -25, -25, -25, -25, -50,
   -25, -25, -12, -12, -12, -12, -25, -25,
   -25, -12,  12,  12,  12,  12, -12, -25,
   -25, -12,  12,  12,  12,  12, -12, -25,
   -25, -12,  12,  12,  12,  12, -12, -25,
   -25, -12,  12,  12,  12,  12, -12, -25,
   -25, -12, -12, -12, -12, -12, -25, -25,
   -50, -25, -25, -25, -25, -25, -25, -50,
};

inline eval_t Eval::m_getKingEval(Board& board, uint8_t phase)
{
    // Calculate the king attack zones (all squares the king can move)
    // The king square does not need to be included as a search would not stop on a check
    const uint8_t wKingIdx = LS1B(board.m_bbTypedPieces[W_KING][Color::WHITE]);
    const uint8_t bKingIdx = LS1B(board.m_bbTypedPieces[W_KING][Color::BLACK]);
    bitboard_t whiteKingZone = getKingMoves(wKingIdx);
    bitboard_t blackKingZone = getKingMoves(bKingIdx);

    uint8_t blackAttackingIndex = 0;
    uint8_t whiteAttackingIndex = 0;
    
    // TODO: For efficiency, this function can be integrated into the mobility function
    {
        blackAttackingIndex += 2 * CNTSBITS(getBlackPawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & whiteKingZone);
        blackAttackingIndex += 2 * CNTSBITS(getBlackPawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & whiteKingZone);

        bitboard_t knights  = board.m_bbTypedPieces[W_KNIGHT][Color::BLACK];
        bitboard_t rooks    = board.m_bbTypedPieces[W_ROOK][Color::BLACK];
        bitboard_t bishops  = board.m_bbTypedPieces[W_BISHOP][Color::BLACK];
        bitboard_t queens   = board.m_bbTypedPieces[W_QUEEN][Color::BLACK];

        while(knights)  blackAttackingIndex += 2 * CNTSBITS(getKnightAttacks(popLS1B(&knights)) & whiteKingZone);
        while(rooks)    blackAttackingIndex += 3 * CNTSBITS(getRookMoves(board.m_bbAllPieces, popLS1B(&rooks)) & whiteKingZone);
        while(bishops)  blackAttackingIndex += 2 * CNTSBITS(getBishopMoves(board.m_bbAllPieces, popLS1B(&bishops)) & whiteKingZone);
        while(queens)   blackAttackingIndex += 5 * CNTSBITS(getQueenMoves(board.m_bbAllPieces, popLS1B(&queens)) & whiteKingZone);
    }

    {
        whiteAttackingIndex += 2 * CNTSBITS(getWhitePawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & blackKingZone);
        whiteAttackingIndex += 2 * CNTSBITS(getWhitePawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & blackKingZone);

        bitboard_t knights  = board.m_bbTypedPieces[W_KNIGHT][Color::WHITE];
        bitboard_t rooks    = board.m_bbTypedPieces[W_ROOK][Color::WHITE];
        bitboard_t bishops  = board.m_bbTypedPieces[W_BISHOP][Color::WHITE];
        bitboard_t queens   = board.m_bbTypedPieces[W_QUEEN][Color::WHITE];
        
        while(knights)  whiteAttackingIndex += 2 * CNTSBITS(getKnightAttacks(popLS1B(&knights)) & blackKingZone);
        while(rooks)    whiteAttackingIndex += 3 * CNTSBITS(getRookMoves(board.m_bbAllPieces, popLS1B(&rooks)) & blackKingZone);
        while(bishops)  whiteAttackingIndex += 2 * CNTSBITS(getBishopMoves(board.m_bbAllPieces, popLS1B(&bishops)) & blackKingZone);
        while(queens)   whiteAttackingIndex += 5 * CNTSBITS(getQueenMoves(board.m_bbAllPieces, popLS1B(&queens)) & blackKingZone);
    }

    // NOTE: This is probably not required, but it might be needed if the kingZone is increased
    //       Anyways, it does not hurt to be safe and guard against out of bound reads :^)
    if(blackAttackingIndex >= 100 || whiteAttackingIndex >= 100)
    {
        WARNING("Safety index is too large " << whiteKingZone << " " << whiteAttackingIndex)
        blackAttackingIndex = std::min(blackAttackingIndex, uint8_t(100));
        whiteAttackingIndex = std::min(whiteAttackingIndex, uint8_t(100));
    }

    eval_t kingAttackingScore = s_kingAreaAttackScore[whiteAttackingIndex] - s_kingAreaAttackScore[blackAttackingIndex];
    eval_t kingPositionScore = ((s_whiteKingPositionBegin[wKingIdx] - s_blackKingPositionBegin[bKingIdx]) * phase 
                             + (s_kingPositionEnd[wKingIdx] - s_kingPositionEnd[bKingIdx]) * (totalPhase - phase)) / totalPhase;
    
    return kingAttackingScore + kingPositionScore;
}
