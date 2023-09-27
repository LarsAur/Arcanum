#include <board.hpp>
#include <algorithm>

using namespace Arcanum;

bool EvalTrace::operator==(const EvalTrace& other) const { return total == other.total; }
bool EvalTrace::operator> (const EvalTrace& other) const { return total > other.total;  }
bool EvalTrace::operator< (const EvalTrace& other) const { return total < other.total;  }
bool EvalTrace::operator>=(const EvalTrace& other) const { return total >= other.total; }
bool EvalTrace::operator<=(const EvalTrace& other) const { return total <= other.total; }

static constexpr uint8_t knightPhase = 1;
static constexpr uint8_t bishopPhase = 1;
static constexpr uint8_t rookPhase = 2;
static constexpr uint8_t queenPhase = 4;
static constexpr uint8_t totalPhase = knightPhase*4 + bishopPhase*4 + rookPhase*4 + queenPhase*2;

static constexpr uint8_t edgeDistance[8] = {0, 1, 2, 3, 3, 2, 1, 0};

// Constants used during evaluations
static constexpr bitboard_t bbAFile = 0x0101010101010101;
static constexpr bitboard_t bbBFile = 0x0202020202020202;
static constexpr bitboard_t bbCFile = 0x0404040404040404;
static constexpr bitboard_t bbDFile = 0x0808080808080808;
static constexpr bitboard_t bbEFile = 0x1010101010101010;
static constexpr bitboard_t bbFFile = 0x2020202020202020;
static constexpr bitboard_t bbGFile = 0x4040404040404040;
static constexpr bitboard_t bbHFile = 0x8080808080808080;

static constexpr bitboard_t wForwardLookup[] = { // indexed by rank
    0xFFFFFFFFFFFFFF00,
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
    0x00FFFFFFFFFFFFFF,
};

// Values are from: https://github.com/official-stockfish/Stockfish/blob/sf_15.1/src/evaluate.cpp
static constexpr eval_t mobilityBonusKnightBegin[] = {-62, -53, -12, -3, 3, 12, 21, 28, 37};
static constexpr eval_t mobilityBonusKnightEnd[]   = {-79, -57, -31, -17, 7, 13, 16, 21, 26};
static constexpr eval_t mobilityBonusBishopBegin[] = {-47, -20, 14, 29, 39, 53, 53, 60, 62, 69, 78, 83, 91, 96};
static constexpr eval_t mobilityBonusBishopEnd[]   = {-59, -25, -8, 12, 21, 40, 56, 58, 65, 72, 78, 87, 88, 98};
static constexpr eval_t mobilityBonusRookBegin[]   = {-60, -24, 0, 3, 4, 14, 20, 30, 41, 41, 41, 45, 57, 58, 67};
static constexpr eval_t mobilityBonusRookEnd[]     = {-82, -15, 17, 43, 72, 100, 102, 122, 133, 139, 153, 160,165, 170, 175};
static constexpr eval_t mobilityBonusQueenBegin[]  = {-29, -16, -8, -8, 18, 25, 23, 37, 41,  54, 65, 68, 69, 70, 70,  70 , 71, 72, 74, 76, 90, 104, 105, 106, 112, 114, 114, 119};
static constexpr eval_t mobilityBonusQueenEnd[]    = {-49,-29, -8, 17, 39,  54, 59, 73, 76, 95, 95 ,101, 124, 128, 132, 133, 136, 140, 147, 149, 153, 169, 171, 171, 178, 185, 187, 221};

static constexpr eval_t doublePawnScore = -12;
static constexpr eval_t passedPawnScore = 25;
static constexpr eval_t pawnRankScore = 5;
static constexpr eval_t pawnSupportScore = 6;
static constexpr eval_t pawnBackwardScore = -12;

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
    EvalTrace eval = EvalTrace(0);

    // Check for stalemate and checkmate
    if(!board.hasLegalMove())
    {
        if(board.isChecked(board.m_turn))
        {
            eval.total = board.m_turn == WHITE ? -INT16_MAX + plyFromRoot : INT16_MAX - plyFromRoot;
            return eval;
        }
        
        return eval;
    };

    m_initEval(board);

    // TODO: Special material evaluation

    uint8_t phase = m_getPhase(board, eval);
    eval.total += m_getPawnEval(board, phase, eval);
    eval.total += m_getMaterialEval(board, phase, eval);
    eval.total += m_getMobilityEval(board, phase, eval);
    eval.total += m_getKingEval(board, phase, eval);
    eval.total += m_getCenterEval(board, phase, eval);
    return eval;
}

void Eval::m_initEval(const Board& board)
{
    // Count the number of each piece
    m_pawnAttacks[Color::WHITE] = getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][Color::WHITE]);
    m_pawnAttacks[Color::BLACK] = getBlackPawnAttacks(board.m_bbTypedPieces[W_PAWN][Color::BLACK]);

    Color c = Color::WHITE;
    while(1)
    {
        bitboard_t rooks   = board.m_bbTypedPieces[W_ROOK][c];
        bitboard_t knights = board.m_bbTypedPieces[W_KNIGHT][c];
        bitboard_t bishops = board.m_bbTypedPieces[W_BISHOP][c];
        bitboard_t queens  = board.m_bbTypedPieces[W_QUEEN][c];
        bitboard_t king    = board.m_bbTypedPieces[W_KING][c];

        bitboard_t nColoredPieces = ~board.m_bbColoredPieces[c];
        
        m_numPawns[c] = CNTSBITS(board.m_bbTypedPieces[W_PAWN][c]);
        
        int i = 0;
        while(knights) m_knightMoves[c][i++] = getKnightAttacks(popLS1B(&knights)) & nColoredPieces;
        m_numKnights[c] = i;
        
        i = 0;
        while(rooks) m_rookMoves[c][i++] = getRookMoves(board.m_bbAllPieces, popLS1B(&rooks)) & nColoredPieces;
        m_numRooks[c] = i;
        
        i = 0;
        while(bishops) m_bishopMoves[c][i++] = getBishopMoves(board.m_bbAllPieces, popLS1B(&bishops)) & nColoredPieces;
        m_numBishops[c] = i;

        i = 0;
        while(queens) m_queenMoves[c][i++] = getQueenMoves(board.m_bbAllPieces, popLS1B(&queens)) & nColoredPieces;
        m_numQueens[c] = i;

        m_kingMoves[c] = getKingMoves(LS1B(king)) & nColoredPieces;
        
        if(c == Color::BLACK)
            break;

        c = Color::BLACK;
    }
}

// https://www.chessprogramming.org/Pawn_Structure
inline eval_t Eval::m_getPawnEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_pawnHash & m_pawnEvalTableMask;
    evalEntry_t* evalEntry = &m_pawnEvalTable[evalIdx];
    if(evalEntry->hash == board.m_pawnHash)
    {
        #ifdef FULL_TRACE
        eval.pawns = evalEntry->value;
        #endif

        return evalEntry->value;
    }

    // Pre-calculate pawn movements
    bitboard_t wPawns = board.m_bbTypedPieces[W_PAWN][Color::WHITE]; 
    bitboard_t bPawns = board.m_bbTypedPieces[W_PAWN][Color::BLACK];
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
        pawnScore += CNTSBITS((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][WHITE])) * pawnSupportScore;

        // Is not a doubled pawn
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
        pawnScore -= (((pawnNeighbourFiles | pawnFile) & forward & board.m_bbTypedPieces[W_PAWN][WHITE]) == 0) * passedPawnScore;

        // Pawn has supporting pawns (in the neighbour files)
        pawnScore -= CNTSBITS((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][BLACK])) * pawnSupportScore;
        
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

    #ifdef FULL_TRACE
    eval.pawns = pawnScore;
    #endif

    return pawnScore;
}

inline eval_t Eval::m_getMaterialEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Check the material eval table
    uint64_t evalIdx = board.m_materialHash & m_materialEvalTableMask;
    evalEntry_t* evalEntry = &m_materialEvalTable[evalIdx];
    if(evalEntry->hash == board.m_materialHash)
    {
        #ifdef FULL_TRACE
            eval.material = evalEntry->value;
        #endif

        return evalEntry->value;
    }

    // Evaluate the piece count
    eval_t pieceScore;
    pieceScore  = 100 * (m_numPawns[Color::WHITE]   - m_numPawns[Color::BLACK]);
    pieceScore += 300 * (m_numKnights[Color::WHITE] - m_numKnights[Color::BLACK]);
    pieceScore += 300 * (m_numBishops[Color::WHITE] - m_numBishops[Color::BLACK]);
    pieceScore += 500 * (m_numRooks[Color::WHITE]   - m_numRooks[Color::BLACK]);
    pieceScore += 900 * (m_numQueens[Color::WHITE]  - m_numQueens[Color::BLACK]);

    // Write the pawn evaluation to the table
    evalEntry->hash = board.m_materialHash;
    evalEntry->value = pieceScore;

    #ifdef FULL_TRACE
        eval.material = evalEntry->value;
    #endif

    return pieceScore;
}

inline eval_t Eval::m_getMobilityEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    eval_t mobilityScoreBegin = 0;
    eval_t mobilityScoreEnd = 0;
    // White mobility
    {
        bitboard_t mobilityArea = ~(board.m_bbColoredPieces[Color::WHITE] | m_pawnAttacks[Color::BLACK]);

        for(int i = 0; i < m_numRooks[Color::WHITE]; i++)
        {
            int cnt = CNTSBITS(m_rookMoves[Color::WHITE][i] & mobilityArea);
            mobilityScoreBegin += mobilityBonusRookBegin[cnt];
            mobilityScoreEnd += mobilityBonusRookEnd[cnt];
        }

        for(int i = 0; i < m_numKnights[Color::WHITE]; i++)
        {
            int cnt = CNTSBITS(m_knightMoves[Color::WHITE][i] & mobilityArea);
            mobilityScoreBegin += mobilityBonusKnightBegin[cnt];
            mobilityScoreEnd += mobilityBonusKnightEnd[cnt];
        }

        for(int i = 0; i < m_numBishops[Color::WHITE]; i++)
        {
            int cnt = CNTSBITS(m_bishopMoves[Color::WHITE][i] & mobilityArea);
            mobilityScoreBegin += mobilityBonusBishopBegin[cnt];
            mobilityScoreEnd += mobilityBonusBishopEnd[cnt];
        }

        for(int i = 0; i < m_numQueens[Color::WHITE]; i++)
        {
            int cnt = CNTSBITS(m_queenMoves[Color::WHITE][i] & mobilityArea);
            mobilityScoreBegin += mobilityBonusQueenBegin[cnt];
            mobilityScoreEnd += mobilityBonusQueenEnd[cnt];
        }
    }

    // Black mobility
    {
        bitboard_t mobilityArea = ~(board.m_bbColoredPieces[Color::BLACK] | m_pawnAttacks[Color::WHITE]);

        for(int i = 0; i < m_numRooks[Color::BLACK]; i++)
        {
            int cnt = CNTSBITS(m_rookMoves[Color::BLACK][i] & mobilityArea);
            mobilityScoreBegin -= mobilityBonusRookBegin[cnt];
            mobilityScoreEnd -= mobilityBonusRookEnd[cnt];
        }

        for(int i = 0; i < m_numKnights[Color::BLACK]; i++)
        {  
            int cnt = CNTSBITS(m_knightMoves[Color::BLACK][i] & mobilityArea);
            mobilityScoreBegin -= mobilityBonusKnightBegin[cnt];
            mobilityScoreEnd -= mobilityBonusKnightEnd[cnt];
        }

        for(int i = 0; i < m_numBishops[Color::BLACK]; i++)
        {
            int cnt = CNTSBITS(m_bishopMoves[Color::BLACK][i] & mobilityArea);
            mobilityScoreBegin -= mobilityBonusBishopBegin[cnt];
            mobilityScoreEnd -= mobilityBonusBishopEnd[cnt];
        }

        for(int i = 0; i < m_numQueens[Color::BLACK]; i++)
        {
            int cnt = CNTSBITS(m_queenMoves[Color::BLACK][i] & mobilityArea);
            mobilityScoreBegin -= mobilityBonusQueenBegin[cnt];
            mobilityScoreEnd -= mobilityBonusQueenEnd[cnt];
        }
    }

    eval_t mobilityScore = (phase * mobilityScoreBegin + (totalPhase - phase) * mobilityScoreEnd) / totalPhase;

    #ifdef FULL_TRACE
        eval.mobility = mobilityScore;
    #endif

    return mobilityScore;
}

inline uint8_t Eval::m_getPhase(const Board& board, EvalTrace& eval)
{
    // Check the phase table
    uint64_t phaseIdx = board.m_materialHash & m_materialEvalTableMask;
    phaseEntry_t* phaseEntry = &m_phaseTable[phaseIdx];
    if(phaseEntry->hash == board.m_materialHash)
    {
        #ifdef FULL_TRACE
        eval.phase = phaseEntry->value;
        #endif
        return phaseEntry->value;
    }

    uint8_t phase = (
        + knightPhase * (m_numKnights[Color::WHITE] + m_numKnights[Color::BLACK])
        + bishopPhase * (m_numBishops[Color::WHITE] + m_numBishops[Color::BLACK])
        + rookPhase   * (m_numRooks[Color::WHITE]   + m_numRooks[Color::BLACK])
        + queenPhase  * (m_numQueens[Color::WHITE]  + m_numQueens[Color::BLACK])
    );

    // Limit phase between 0 and totalphase
    // this is done to avoid cases with for example many queens boosting the phase
    phase = std::min(phase, totalPhase);
    
    // Write the phase to the table
    phaseEntry->hash = board.m_materialHash;
    phaseEntry->value = phase;
    #ifdef FULL_TRACE
    eval.phase = phase;
    #endif
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
   -25, -25, -12, -12, -12, -12, -25, -25,
   -50, -25, -25, -25, -25, -25, -25, -50,
};

inline eval_t Eval::m_getKingEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Calculate the king attack zones (all squares the king can move)
    // The king square does not need to be included as a search would not stop on a check
    const uint8_t wKingIdx = LS1B(board.m_bbTypedPieces[W_KING][Color::WHITE]);
    const uint8_t bKingIdx = LS1B(board.m_bbTypedPieces[W_KING][Color::BLACK]);
    bitboard_t whiteKingZone = getKingMoves(wKingIdx);
    bitboard_t blackKingZone = getKingMoves(bKingIdx);

    uint8_t blackAttackingIndex = 0;
    uint8_t whiteAttackingIndex = 0;
    
    {
        blackAttackingIndex += 2 * CNTSBITS(getBlackPawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & whiteKingZone);
        blackAttackingIndex += 2 * CNTSBITS(getBlackPawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & whiteKingZone);

        for(int i = 0; i < m_numKnights[Color::BLACK]; i++) blackAttackingIndex += 2 * CNTSBITS(m_knightMoves[Color::BLACK][i] & whiteKingZone);
        for(int i = 0; i < m_numBishops[Color::BLACK]; i++) blackAttackingIndex += 2 * CNTSBITS(m_bishopMoves[Color::BLACK][i] & whiteKingZone);
        for(int i = 0; i < m_numRooks[Color::BLACK]; i++)   blackAttackingIndex += 3 * CNTSBITS(m_rookMoves[Color::BLACK][i]   & whiteKingZone);
        for(int i = 0; i < m_numQueens[Color::BLACK]; i++)  blackAttackingIndex += 5 * CNTSBITS(m_queenMoves[Color::BLACK][i]  & whiteKingZone);
    }

    {
        whiteAttackingIndex += 2 * CNTSBITS(getWhitePawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & blackKingZone);
        whiteAttackingIndex += 2 * CNTSBITS(getWhitePawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & blackKingZone);

        for(int i = 0; i < m_numKnights[Color::WHITE]; i++) whiteAttackingIndex += 2 * CNTSBITS(m_knightMoves[Color::WHITE][i] & blackKingZone);
        for(int i = 0; i < m_numBishops[Color::WHITE]; i++) whiteAttackingIndex += 2 * CNTSBITS(m_bishopMoves[Color::WHITE][i] & blackKingZone);
        for(int i = 0; i < m_numRooks[Color::WHITE]; i++)   whiteAttackingIndex += 3 * CNTSBITS(m_rookMoves[Color::WHITE][i]   & blackKingZone);
        for(int i = 0; i < m_numQueens[Color::WHITE]; i++)  whiteAttackingIndex += 5 * CNTSBITS(m_queenMoves[Color::WHITE][i]  & blackKingZone);
    }

    // NOTE: This is probably not required, but it might be needed if the kingZone is increased
    //       Anyways, it does not hurt to be safe and guard against out of bound reads :^)
    if(blackAttackingIndex >= 100 || whiteAttackingIndex >= 100)
    {
        WARNING("Safety index is too large " << whiteKingZone << " " << blackAttackingIndex)
        blackAttackingIndex = std::min(blackAttackingIndex, uint8_t(100));
        whiteAttackingIndex = std::min(whiteAttackingIndex, uint8_t(100));
    }

    eval_t kingAttackingScore = s_kingAreaAttackScore[whiteAttackingIndex] - s_kingAreaAttackScore[blackAttackingIndex];
    eval_t kingPositionScore = ((s_whiteKingPositionBegin[wKingIdx] - s_blackKingPositionBegin[bKingIdx]) * phase 
                             + (s_kingPositionEnd[wKingIdx] - s_kingPositionEnd[bKingIdx]) * (totalPhase - phase)) / totalPhase;
    
    #ifdef FULL_TRACE
    eval.king = kingAttackingScore + kingPositionScore;
    #endif

    return kingAttackingScore + kingPositionScore;
}

eval_t Eval::m_getCenterEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    constexpr uint8_t centerEvalThreshold = 6 * knightPhase;
    constexpr bitboard_t center = 0x0000001818000000LL;         // Four center squares  
    constexpr bitboard_t centerExtended = 0x00003C3C3C3C0000;   // Squares around center squares and the center

    // Only evaluate center until some number of non-pawn pieces are captured
    if(totalPhase - phase >= centerEvalThreshold)
    {
        #ifdef FULL_TRACE
        eval.center = 0;
        #endif
        return 0;
    }

    uint8_t blackCenterIndex = 0;
    uint8_t whiteCenterIndex = 0;

    {
        blackCenterIndex += CNTSBITS(getBlackPawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & center);
        blackCenterIndex += CNTSBITS(getBlackPawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & centerExtended);
        blackCenterIndex += CNTSBITS(getBlackPawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & center);
        blackCenterIndex += CNTSBITS(getBlackPawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::BLACK]) & centerExtended);

        for(int i = 0; i < m_numKnights[Color::BLACK]; i++)
        {
            blackCenterIndex += CNTSBITS(m_knightMoves[Color::BLACK][i] & center);
            blackCenterIndex += CNTSBITS(m_knightMoves[Color::BLACK][i] & centerExtended);
        }

        for(int i = 0; i < m_numRooks[Color::BLACK]; i++)
        {
            blackCenterIndex += CNTSBITS(m_rookMoves[Color::BLACK][i] & center);
            blackCenterIndex += CNTSBITS(m_rookMoves[Color::BLACK][i] & centerExtended);
        }

        for(int i = 0; i < m_numBishops[Color::BLACK]; i++)
        {
            blackCenterIndex += CNTSBITS(m_bishopMoves[Color::BLACK][i] & center);
            blackCenterIndex += CNTSBITS(m_bishopMoves[Color::BLACK][i] & centerExtended);
        }

        blackCenterIndex += CNTSBITS(board.m_bbColoredPieces[Color::BLACK] & center);
        blackCenterIndex += CNTSBITS(board.m_bbColoredPieces[Color::BLACK] & centerExtended);
    }

    {
        whiteCenterIndex += CNTSBITS(getWhitePawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & center);
        whiteCenterIndex += CNTSBITS(getWhitePawnAttacksLeft(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & centerExtended);
        whiteCenterIndex += CNTSBITS(getWhitePawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & center);
        whiteCenterIndex += CNTSBITS(getWhitePawnAttacksRight(board.m_bbTypedPieces[W_PAWN][Color::WHITE]) & centerExtended);

        for(int i = 0; i < m_numKnights[Color::WHITE]; i++)
        {
            whiteCenterIndex += CNTSBITS(m_knightMoves[Color::WHITE][i] & center);
            whiteCenterIndex += CNTSBITS(m_knightMoves[Color::WHITE][i] & centerExtended);
        }

        for(int i = 0; i < m_numRooks[Color::WHITE]; i++)
        {
            whiteCenterIndex += CNTSBITS(m_rookMoves[Color::WHITE][i] & center);
            whiteCenterIndex += CNTSBITS(m_rookMoves[Color::WHITE][i] & centerExtended);
        }

        for(int i = 0; i < m_numBishops[Color::WHITE]; i++)
        {
            whiteCenterIndex += CNTSBITS(m_bishopMoves[Color::WHITE][i] & center);
            whiteCenterIndex += CNTSBITS(m_bishopMoves[Color::WHITE][i] & centerExtended);
        }

        whiteCenterIndex += CNTSBITS(board.m_bbColoredPieces[Color::WHITE] & center);
        whiteCenterIndex += CNTSBITS(board.m_bbColoredPieces[Color::WHITE] & centerExtended);
    }

    eval_t centerEval = 3 * ((whiteCenterIndex - blackCenterIndex) * (centerEvalThreshold + phase - totalPhase));
    #ifdef FULL_TRACE
    eval.center = centerEval;
    #endif
    return centerEval;
}