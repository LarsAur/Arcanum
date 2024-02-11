#include <eval.hpp>
#include <algorithm>
#include <memory.hpp>

using namespace Arcanum;

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

std::string Evaluator::s_hceWeightsFile = "hceWeights.dat";

Evaluator::Evaluator()
{
    m_enabledNNUE = false;
    m_accumulatorStackPointer = 0;

    setHCEModelFile(s_hceWeightsFile);

    m_pawnEvalTable     = static_cast<EvalEntry*>(Memory::pageAlignedMalloc(pawnTableSize     * sizeof(EvalEntry)));
    m_materialEvalTable = static_cast<EvalEntry*>(Memory::pageAlignedMalloc(materialTableSize * sizeof(EvalEntry)));
    m_shelterEvalTable  = static_cast<EvalEntry*>(Memory::pageAlignedMalloc(shelterTableSize  * sizeof(EvalEntry)));
    m_phaseTable        = static_cast<PhaseEntry*>(Memory::pageAlignedMalloc(phaseTableSize   * sizeof(PhaseEntry)));

    // Use default value 0xff...ff for initial value, as it is more common that 0 is the value of for example the pawnHash
    memset(m_pawnEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::pawnTableSize);
    memset(m_materialEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::materialTableSize);
    memset(m_shelterEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::shelterTableSize);
    memset(m_phaseTable, 0xFF, sizeof(PhaseEntry) * Evaluator::phaseTableSize);
}

Evaluator::~Evaluator()
{
    Memory::alignedFree(m_pawnEvalTable);
    Memory::alignedFree(m_materialEvalTable);
    Memory::alignedFree(m_shelterEvalTable);
    Memory::alignedFree(m_phaseTable);

    for(auto accPtr : m_accumulatorStack)
    {
        delete accPtr;
    }
}

void Evaluator::setHCEModelFile(std::string filename)
{
    Evaluator::s_hceWeightsFile = filename;

    std::string path = getWorkPath();
    path.append(filename);

    std::ifstream is(path, std::ios::in | std::ios::binary);

    if(!is.is_open())
    {
        ERROR("Unable to open file " << path)
        exit(-1);
    }

    std::streampos bytesize = is.tellg();
    is.seekg(0, std::ios::end);
    bytesize = is.tellg() - bytesize;
    size_t numWeights = bytesize >> 1;
    is.seekg(0);

    if(numWeights != 397)
    {
        ERROR("Illegal number of weights " << numWeights << " needs to be " << 397)
        exit(-1);
    }

    eval_t weights[397];
    is.read((char*) weights, bytesize);
    is.close();

    loadWeights(weights);
}

void Evaluator::loadWeights(eval_t* weights)
{
    // m_pawnValue   = 100;
    // m_rookValue   = weights[0];
    // m_knightValue = weights[1];
    // m_bishopValue = weights[2];
    // m_queenValue  = weights[3];
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
    if(!m_enabledNNUE || !m_nnue.isLoaded()) return;

    if(m_accumulatorStack.empty())
        m_accumulatorStack.push_back(new NN::Accumulator); // 'new' should account for alignas(64) for Accumulator

    m_accumulatorStackPointer = 0;
    m_nnue.initializeAccumulator(*m_accumulatorStack[0], board);
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(!m_enabledNNUE || !m_nnue.isLoaded()) return;

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

bool Evaluator::isCheckMateScore(eval_t eval)
{
    return std::abs(eval) > (INT16_MAX - UINT8_MAX);
}

// Evaluates positive value for WHITE
eval_t Evaluator::evaluate(Board& board, uint8_t plyFromRoot, bool noMoves)
{
    // If it is known from search that the position has no moves
    // Checking for legal moves can be skipped
    if(!noMoves) noMoves = !board.hasLegalMove();

    // Check for stalemate and checkmate
    if(noMoves)
    {
        if(board.isChecked(board.m_turn))
        {
            return board.m_turn == WHITE ? -INT16_MAX + plyFromRoot : INT16_MAX - plyFromRoot;
        }

        return 0;
    };

    if(m_enabledNNUE && m_nnue.isLoaded())
    {
        eval_t score = m_nnue.evaluate(*m_accumulatorStack[m_accumulatorStackPointer], board.m_turn);
        return board.m_turn == WHITE ? score : -score;
    }

    eval_t eval = 0;
    m_initEval(board);
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
