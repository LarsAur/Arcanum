#include <eval.hpp>
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
#define PHASE_LERP(_begin, _end, _phase) (((_phase * _begin) + ((totalPhase - _phase) * _end)) / totalPhase)

// Distance to edge based on rank/file
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

static constexpr eval_t pawnRankBonusBegin[]         = {0, 0, 4, 4, 8, 12, 16, 0};
static constexpr eval_t pawnRankBonusEnd[]           = {0, 0, 4, 16, 32, 64, 128, 0};
static constexpr eval_t isolatedPawnRankBonusBegin[] = {0, 16, 25, 25, 32, 64, 128, 0};
static constexpr eval_t isolatedPawnRankBonusEnd[]   = {0, 25, 50, 50, 75, 125, 200, 0};

Evaluator::Evaluator()
{
    m_enabledNNUE = false;
    m_accumulatorStackPointer = 0;

    // Use default value 0xff...ff for initial value, as it is more common that 0 is the value of for example the pawnHash
    memset(m_pawnEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::pawnTableSize);
    memset(m_materialEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::materialTableSize);
    memset(m_shelterEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::shelterTableSize);
    memset(m_phaseTable, 0xFF, sizeof(PhaseEntry) * Evaluator::phaseTableSize);
}

Evaluator::~Evaluator()
{
    for(auto accPtr : m_accumulatorStack)
    {
        delete accPtr;
    }
}

void Evaluator::setEnableNNUE(bool enabled)
{
    m_enabledNNUE = enabled;
    if(!enabled) return;

    if(!m_nnue.isLoaded())
    {
        m_nnue.loadRelative("nn-04cf2b4ed1da.nnue");
    }
}

void Evaluator::initializeAccumulatorStack(const Board& board)
{
    if(!m_enabledNNUE) return;

    if(m_accumulatorStack.empty())
        m_accumulatorStack.push_back(new NN::Accumulator); // 'new' should account for alignas(64) for Accumulator

    m_accumulatorStackPointer = 0;
    m_nnue.initializeAccumulator(*m_accumulatorStack[0], board);
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(!m_enabledNNUE) return;

    if(m_accumulatorStack.size() == m_accumulatorStackPointer + 1)
    {
        m_accumulatorStack.push_back(new NN::Accumulator);
    }

    m_nnue.incrementAccumulator(
        *m_accumulatorStack[m_accumulatorStackPointer],
        *m_accumulatorStack[m_accumulatorStackPointer+1],
        Color(1^board.getTurn()),
        board,
        move
    );

    m_accumulatorStackPointer++;

    #ifdef VERIFY_NNUE_INCR
    eval_t e1 = m_nnue.evaluate(*m_accumulatorStack[m_accumulatorStackPointer], board.m_turn);
    eval_t e2 = m_nnue.evaluateBoard(board);
    if(e1 != e2)
    {
        LOG(unsigned(move.from) << " " << unsigned(move.to) << " Type: " << (move.moveInfo & MOVE_INFO_MOVE_MASK) << " Capture: " << (move.moveInfo & MOVE_INFO_CAPTURE_MASK) << " Castle: " << (move.moveInfo & MOVE_INFO_CASTLE_MASK) << " Enpassant " << (move.moveInfo & MOVE_INFO_ENPASSANT))

        exit(-1);
    }
    #endif
}

void Evaluator::popMoveFromAccumulator()
{
    m_accumulatorStackPointer--;
}

EvalTrace Evaluator::getDrawValue(Board& board, uint8_t plyFromRoot)
{
    EvalTrace trace = EvalTrace(0);
    // TODO: encurage not drawing at an early stage
    return trace;
}

bool Evaluator::isCheckMateScore(EvalTrace eval)
{
    return std::abs(eval.total) > (INT16_MAX - UINT8_MAX);
}

// Evaluates positive value for WHITE
EvalTrace Evaluator::evaluate(Board& board, uint8_t plyFromRoot)
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

    if(m_enabledNNUE)
    {
        eval_t score = m_nnue.evaluate(*m_accumulatorStack[m_accumulatorStackPointer], board.m_turn);
        eval.total = board.m_turn == WHITE ? score : -score;
        return eval;
    }

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

void Evaluator::m_initEval(const Board& board)
{
    // Count the number of each piece
    m_pawnAttacks[Color::WHITE] = getWhitePawnAttacks(board.m_bbTypedPieces[W_PAWN][Color::WHITE]);
    m_pawnAttacks[Color::BLACK] = getBlackPawnAttacks(board.m_bbTypedPieces[W_PAWN][Color::BLACK]);

    {
        bitboard_t rooks   = board.m_bbTypedPieces[W_ROOK][Color::WHITE];
        bitboard_t knights = board.m_bbTypedPieces[W_KNIGHT][Color::WHITE];
        bitboard_t bishops = board.m_bbTypedPieces[W_BISHOP][Color::WHITE];
        bitboard_t queens  = board.m_bbTypedPieces[W_QUEEN][Color::WHITE];
        bitboard_t king    = board.m_bbTypedPieces[W_KING][Color::WHITE];

        bitboard_t nColoredPieces = ~board.m_bbColoredPieces[Color::WHITE];

        m_numPawns[Color::WHITE] = CNTSBITS(board.m_bbTypedPieces[W_PAWN][Color::WHITE]);

        int i = 0;
        while(knights) m_knightMoves[Color::WHITE][i++] = getKnightAttacks(popLS1B(&knights)) & nColoredPieces;
        m_numKnights[Color::WHITE] = i;

        i = 0;
        while(rooks) m_rookMoves[Color::WHITE][i++] = getRookMoves(board.m_bbAllPieces, popLS1B(&rooks)) & nColoredPieces;
        m_numRooks[Color::WHITE] = i;

        i = 0;
        while(bishops) m_bishopMoves[Color::WHITE][i++] = getBishopMoves(board.m_bbAllPieces, popLS1B(&bishops)) & nColoredPieces;
        m_numBishops[Color::WHITE] = i;

        i = 0;
        while(queens) m_queenMoves[Color::WHITE][i++] = getQueenMoves(board.m_bbAllPieces, popLS1B(&queens)) & nColoredPieces;
        m_numQueens[Color::WHITE] = i;

        m_kingMoves[Color::WHITE] = getKingMoves(LS1B(king)) & nColoredPieces;
    }

    {
        bitboard_t rooks   = board.m_bbTypedPieces[W_ROOK][Color::BLACK];
        bitboard_t knights = board.m_bbTypedPieces[W_KNIGHT][Color::BLACK];
        bitboard_t bishops = board.m_bbTypedPieces[W_BISHOP][Color::BLACK];
        bitboard_t queens  = board.m_bbTypedPieces[W_QUEEN][Color::BLACK];
        bitboard_t king    = board.m_bbTypedPieces[W_KING][Color::BLACK];

        bitboard_t nColoredPieces = ~board.m_bbColoredPieces[Color::BLACK];

        m_numPawns[Color::BLACK] = CNTSBITS(board.m_bbTypedPieces[W_PAWN][Color::BLACK]);

        int i = 0;
        while(knights) m_knightMoves[Color::BLACK][i++] = getKnightAttacks(popLS1B(&knights)) & nColoredPieces;
        m_numKnights[Color::BLACK] = i;

        i = 0;
        while(rooks) m_rookMoves[Color::BLACK][i++] = getRookMoves(board.m_bbAllPieces, popLS1B(&rooks)) & nColoredPieces;
        m_numRooks[Color::BLACK] = i;

        i = 0;
        while(bishops) m_bishopMoves[Color::BLACK][i++] = getBishopMoves(board.m_bbAllPieces, popLS1B(&bishops)) & nColoredPieces;
        m_numBishops[Color::BLACK] = i;

        i = 0;
        while(queens) m_queenMoves[Color::BLACK][i++] = getQueenMoves(board.m_bbAllPieces, popLS1B(&queens)) & nColoredPieces;
        m_numQueens[Color::BLACK] = i;

        m_kingMoves[Color::BLACK] = getKingMoves(LS1B(king)) & nColoredPieces;
    }
}

// https://www.chessprogramming.org/Pawn_Structure
inline eval_t Evaluator::m_getPawnEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Check the pawn eval table
    size_t evalIdx = board.m_pawnHash & Evaluator::pawnTableMask;
    EvalEntry* evalEntry = &m_pawnEvalTable[evalIdx];
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
    eval_t pawnScoreBegin = 0;
    eval_t pawnScoreEnd = 0;
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
        if(((pawnNeighbourFiles | pawnFile) & forward & board.m_bbTypedPieces[W_PAWN][BLACK]) == 0)
        {
            pawnScoreBegin += isolatedPawnRankBonusBegin[rank];
            pawnScoreEnd += isolatedPawnRankBonusEnd[rank];
        }
        else
        {
            pawnScoreBegin += pawnRankBonusBegin[rank];
            pawnScoreEnd += pawnRankBonusEnd[rank];
        }

        // Pawn has supporting pawns (in the neighbour files)
        pawnScore += CNTSBITS((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][WHITE])) * pawnSupportScore;

        // Is not a doubled pawn
        pawnScore += ((pawnFile & forward & board.m_bbTypedPieces[W_PAWN][WHITE]) != 0) * doublePawnScore;
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
        if(((pawnNeighbourFiles | pawnFile) & forward & board.m_bbTypedPieces[W_PAWN][WHITE]) == 0)
        {
            pawnScoreBegin -= isolatedPawnRankBonusBegin[7 - rank];
            pawnScoreEnd -= isolatedPawnRankBonusEnd[7 - rank];
        }
        else
        {
            pawnScoreBegin -= pawnRankBonusBegin[7 - rank];
            pawnScoreEnd -= pawnRankBonusEnd[7 - rank];
        }

        // Pawn has supporting pawns (in the neighbour files)
        pawnScore -= CNTSBITS((pawnNeighbourFiles & backward & board.m_bbTypedPieces[W_PAWN][BLACK])) * pawnSupportScore;

        // Is not a doubled pawn
        pawnScore -= ((pawnFile & forward & board.m_bbTypedPieces[W_PAWN][BLACK]) != 0) * doublePawnScore;
    }

    pawnScore += ((pawnScoreBegin * phase) + (pawnScoreEnd * (totalPhase - phase))) / totalPhase;

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

inline eval_t Evaluator::m_getMaterialEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Check the material eval table
    size_t evalIdx = board.m_materialHash & Evaluator::materialTableMask;
    EvalEntry* evalEntry = &m_materialEvalTable[evalIdx];
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

inline eval_t Evaluator::m_getMobilityEval(const Board& board, uint8_t phase, EvalTrace& eval)
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

inline uint8_t Evaluator::m_getPhase(const Board& board, EvalTrace& eval)
{
    // Check the phase table
    size_t phaseIdx = board.m_materialHash & Evaluator::phaseTableMask;
    PhaseEntry* phaseEntry = &m_phaseTable[phaseIdx];
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

static constexpr eval_t pawnShelterScores[4][8] =
{
    {  -3,  40,  45,  25,  20,  10,  15, 0 },
    { -25,  30,  15, -27, -15, -10,  -5, 0 },
    { -10,  35,  10,  10,  10,   5, -25, 0 },
    { -20,  -5, -10, -20, -25, -30, -35, 0 },
};

template <typename Arcanum::Color turn>
inline eval_t Evaluator::m_getShelterEval(const Board& board, uint8_t square)
{
    hash_t shelterHash = board.getPawnHash() ^ (square << 1) ^ turn;
    size_t idx = shelterHash & Evaluator::shelterTableMask;
    EvalEntry* shelterEntry = &m_shelterEvalTable[idx];
    if(shelterEntry->hash == shelterHash)
    {
        return shelterEntry->value;
    }

    constexpr Color opponent = Color(turn^1);

    uint8_t rank = square >> 3;
    eval_t shelterScore = 0;

    bitboard_t opponentPawnAttacks = m_pawnAttacks[opponent];
    bitboard_t opponentPawns = board.m_bbTypedPieces[W_PAWN][opponent];
    bitboard_t forward  = turn == WHITE ? wForwardLookup[rank] : bForwardLookup[rank];
    bitboard_t forwardPawns = board.m_bbTypedPieces[W_PAWN][turn] & forward & ~opponentPawnAttacks;

    uint8_t centerFile = std::clamp(square & 0b111, 1, 6);
    for(int file = centerFile - 1; file <= centerFile + 1; file++)
    {
        bitboard_t bbFile = bbAFile << file;
        bitboard_t filePawns = forwardPawns & bbFile;
        bitboard_t opponentFilePawns = opponentPawns & bbFile;
        uint8_t ed = edgeDistance[file];

        // Get the rank of the pawn closest to the king
        uint8_t pawnRank;
        uint8_t opponentPawnRank;
        if constexpr(turn == Color::WHITE)
        {
            pawnRank = filePawns ? LS1B(filePawns) >> 3 : 0;
            opponentPawnRank = opponentFilePawns ? LS1B(opponentFilePawns) >> 3: 0;
            shelterScore += pawnShelterScores[ed][pawnRank];
        }
        else
        {
            pawnRank = filePawns ? MS1B(filePawns) >> 3 : 7;
            opponentPawnRank = opponentFilePawns ? MS1B(opponentFilePawns) >> 3 : 0;
            shelterScore += pawnShelterScores[ed][7 - pawnRank];
        }
    }

    shelterEntry->hash = shelterHash;
    shelterEntry->value = shelterScore;

    return shelterScore;
}

inline eval_t Evaluator::m_getKingEval(const Board& board, uint8_t phase, EvalTrace& eval)
{
    // Calculate the pawn shelter bonus
    // By multiplying by 17/16, the bonus is somewhat larger for the actual king position
    eval_t wKingShelter = (m_getShelterEval<Color::WHITE>(board, LS1B(board.m_bbTypedPieces[W_KING][Color::WHITE])) * 17) >> 4;
    if(board.m_castleRights & WHITE_KING_SIDE)
        wKingShelter = std::max(wKingShelter, m_getShelterEval<Color::WHITE>(board, 6));
    if(board.m_castleRights & WHITE_QUEEN_SIDE)
        wKingShelter = std::max(wKingShelter, m_getShelterEval<Color::WHITE>(board, 2));

    eval_t bKingShelter = (m_getShelterEval<Color::BLACK>(board, LS1B(board.m_bbTypedPieces[W_KING][Color::BLACK])) * 17) >> 4;
    if(board.m_castleRights & BLACK_KING_SIDE)
        bKingShelter = std::max(bKingShelter, m_getShelterEval<Color::BLACK>(board, 62));
    if(board.m_castleRights & BLACK_QUEEN_SIDE)
        bKingShelter = std::max(bKingShelter, m_getShelterEval<Color::BLACK>(board, 58));

    eval_t kingShelterScore = (wKingShelter - bKingShelter);

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
    eval_t kingPositionScore = PHASE_LERP(
        (s_whiteKingPositionBegin[wKingIdx] - s_blackKingPositionBegin[bKingIdx] + kingShelterScore),
        (s_kingPositionEnd[wKingIdx] - s_kingPositionEnd[bKingIdx]),
        phase);

    #ifdef FULL_TRACE
    eval.king = kingAttackingScore + kingPositionScore;
    #endif

    return kingAttackingScore + kingPositionScore;
}

eval_t Evaluator::m_getCenterEval(const Board& board, uint8_t phase, EvalTrace& eval)
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