#include <eval.hpp>
#include <algorithm>
#include <memory.hpp>

using namespace Arcanum;

static constexpr uint8_t phaseValues[6] = {0, 1, 1, 2, 4, 0};
static constexpr uint8_t totalPhase = phaseValues[W_PAWN]*16 + phaseValues[W_KNIGHT]*4 + phaseValues[W_BISHOP]*4 + phaseValues[W_ROOK]*4 + phaseValues[W_QUEEN]*2;

static inline eval_t phaseLerp(eval_t early, eval_t late, uint8_t phase)
{
    return ((phase * early) + ((totalPhase - phase) * late)) / totalPhase;
}
// Distance to edge based on rank/file
static constexpr uint8_t edgeDistance[8] = {0, 1, 2, 3, 3, 2, 1, 0};

// Index mirrored around the center file
#define PST_INDEX(_idx) ((((_idx) & 0b111000) >> 1) | edgeDistance[(_idx) & 0b111])

// Index mirrored around the center file, and rank is flipped
#define MIRROR_PST_INDEX(_idx) (((7 - ((_idx) >> 3)) << 2) | edgeDistance[(_idx) & 0b111])

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
    m_shelterEvalTable  = static_cast<EvalEntry*>(Memory::pageAlignedMalloc(shelterTableSize  * sizeof(EvalEntry)));

    // Use default value 0xff...ff for initial value, as it is more common that 0 is the value of for example the pawnHash
    memset(m_pawnEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::pawnTableSize);
    memset(m_shelterEvalTable, 0xFF, sizeof(EvalEntry) * Evaluator::shelterTableSize);
}

Evaluator::~Evaluator()
{
    Memory::alignedFree(m_pawnEvalTable);
    Memory::alignedFree(m_shelterEvalTable);

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
    uint32_t widx = 0;
    #define LOAD_SINGLE(_var) _var = weights[widx++];
    #define LOAD_ARRAY(_arr, _num) memcpy(&_arr, weights + widx, sizeof(eval_t) * _num); widx += _num;
    #define LOAD_ARRAY_2D(_arr, _num1, _num2) memcpy(&_arr, weights + widx, sizeof(eval_t) * (_num1) * (_num2)); widx += (_num1) * (_num2);

    LOAD_ARRAY(m_pieceValues, 6)
    LOAD_ARRAY_2D(m_pieceSquareTablesEarly, 6, 32)
    LOAD_ARRAY_2D(m_pieceSquareTablesLate, 6, 32)

    #undef LOAD_ARRAY
    #undef LOAD_SINGLE
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
    if(!m_enabledNNUE || !m_nnue.isLoaded())
    {
        if(m_immAccumulatorStack.empty())
            m_immAccumulatorStack.emplace_back(ImmEvalEntry());

        m_accumulatorStackPointer = 0;
        m_initImmediateEval(m_immAccumulatorStack[0], board);
    }
    else
    {
        if(m_accumulatorStack.empty())
            m_accumulatorStack.push_back(new NN::Accumulator); // 'new' should account for alignas(64) for Accumulator

        m_accumulatorStackPointer = 0;
        m_nnue.initializeAccumulator(*m_accumulatorStack[0], board);
    }
}

void Evaluator::pushMoveToAccumulator(const Board& board, const Move& move)
{
    if(!m_enabledNNUE || !m_nnue.isLoaded())
    {
        if(m_immAccumulatorStack.size() == m_accumulatorStackPointer + 1)
        {
            m_immAccumulatorStack.emplace_back(ImmEvalEntry());
        }

        m_incrementImmediateEval(
            m_immAccumulatorStack[m_accumulatorStackPointer],
            m_immAccumulatorStack[m_accumulatorStackPointer+1],
            board,
            move
        );

        m_accumulatorStackPointer++;

        #ifdef VERIFY_INCR
        ImmEvalEntry _immEval;
        m_initImmediateEval(_immEval, board);
        if((m_immAccumulatorStack[m_accumulatorStackPointer].early != _immEval.early) ||
        (m_immAccumulatorStack[m_accumulatorStackPointer].late != _immEval.late) ||
        (m_immAccumulatorStack[m_accumulatorStackPointer].phase != _immEval.phase))
        {
            ERROR(m_immAccumulatorStack[m_accumulatorStackPointer].early << "  " << _immEval.early << " | " <<
            m_immAccumulatorStack[m_accumulatorStackPointer].late << "  " << _immEval.late << " | " <<
            unsigned(m_immAccumulatorStack[m_accumulatorStackPointer].phase) << "  " << unsigned(_immEval.phase))

            LOG(unsigned(move.from) << " " << unsigned(move.to) << " Type: " << (move.moveInfo & MOVE_INFO_MOVE_MASK) << " Capture: " << (move.moveInfo & MOVE_INFO_CAPTURE_MASK) << " Castle: " << (move.moveInfo & MOVE_INFO_CASTLE_MASK) << " Enpassant " << (move.moveInfo & MOVE_INFO_ENPASSANT))
            DEBUG(board.getBoardString())
            exit(-1);
        }
        #endif
    }
    else
    {
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
}

void Evaluator::popMoveFromAccumulator()
{
    m_accumulatorStackPointer--;
}

void Evaluator::m_initImmediateEval(ImmEvalEntry& immEval, const Board& board)
{
    immEval = ImmEvalEntry();
    for(uint8_t piece = 0; piece < 6; piece++)
    {
        uint8_t wPieceCount = CNTSBITS(board.m_bbTypedPieces[piece][WHITE]);
        uint8_t bPieceCount = CNTSBITS(board.m_bbTypedPieces[piece][BLACK]);

        eval_t material = m_pieceValues[piece] * (wPieceCount - bPieceCount);
        immEval.early += material;
        immEval.late  += material;
        immEval.phase += phaseValues[piece] * (wPieceCount + bPieceCount);

        bitboard_t wpieces = board.m_bbTypedPieces[piece][WHITE];
        bitboard_t bpieces = board.m_bbTypedPieces[piece][BLACK];
        while (wpieces)
        {
            square_t idx = popLS1B(&wpieces);
            immEval.early += m_pieceSquareTablesEarly[piece][PST_INDEX(idx)];
            immEval.late  += m_pieceSquareTablesLate[piece][PST_INDEX(idx)];
        }

        while (bpieces)
        {
            square_t idx = popLS1B(&bpieces);
            immEval.early -= m_pieceSquareTablesEarly[piece][MIRROR_PST_INDEX(idx)];
            immEval.late  -= m_pieceSquareTablesLate[piece][MIRROR_PST_INDEX(idx)];
        }
    }
}

void Evaluator::m_incrementImmediateEval(ImmEvalEntry& prevImmEval, ImmEvalEntry& newImmEval, const Board& board, const Move& move)
{
    newImmEval.early = prevImmEval.early;
    newImmEval.late  = prevImmEval.late;
    newImmEval.phase = prevImmEval.phase;

    Piece movedPiece = Piece(LS1B(move.moveInfo & MOVE_INFO_MOVE_MASK));

    // Pusing of move is done with the new board after the move is done.
    // Thus the turn is black if white performed the move
    if(board.getTurn() == BLACK) // Whites move
    {
        newImmEval.early -= m_pieceSquareTablesEarly[movedPiece][PST_INDEX(move.from)];
        newImmEval.late  -= m_pieceSquareTablesLate[movedPiece][PST_INDEX(move.from)];

        if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
        {
            Piece promotePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11);
            newImmEval.early += m_pieceSquareTablesEarly[promotePiece][PST_INDEX(move.to)];
            newImmEval.late  += m_pieceSquareTablesLate[promotePiece][PST_INDEX(move.to)];
            newImmEval.early += m_pieceValues[promotePiece] - m_pieceValues[W_PAWN];
            newImmEval.late  += m_pieceValues[promotePiece] - m_pieceValues[W_PAWN];
            newImmEval.phase += phaseValues[promotePiece] - phaseValues[W_PAWN];
        }
        else
        {
            newImmEval.early += m_pieceSquareTablesEarly[movedPiece][PST_INDEX(move.to)];
            newImmEval.late  += m_pieceSquareTablesLate[movedPiece][PST_INDEX(move.to)];
        }

        // Move the rook when castling
        if(CASTLE_SIDE(move.moveInfo))
        {
            if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_QUEEN)
            {
                newImmEval.early += m_pieceSquareTablesEarly[W_ROOK][PST_INDEX(3)] - m_pieceSquareTablesEarly[W_ROOK][PST_INDEX(0)];
                newImmEval.late  += m_pieceSquareTablesLate[W_ROOK][PST_INDEX(3)]  - m_pieceSquareTablesLate[W_ROOK][PST_INDEX(0)];
            }
            else if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_KING)
            {
                newImmEval.early += m_pieceSquareTablesEarly[W_ROOK][PST_INDEX(5)] - m_pieceSquareTablesEarly[W_ROOK][PST_INDEX(7)];
                newImmEval.late  += m_pieceSquareTablesLate[W_ROOK][PST_INDEX(5)]  - m_pieceSquareTablesLate[W_ROOK][PST_INDEX(7)];
            }
        }

        if(CAPTURED_PIECE(move.moveInfo))
        {
            if(move.moveInfo & MoveInfoBit::ENPASSANT)
            {
                square_t targetPawnPosition = move.to - 8;
                newImmEval.early += m_pieceValues[W_PAWN] + m_pieceSquareTablesEarly[W_PAWN][MIRROR_PST_INDEX(targetPawnPosition)];
                newImmEval.late  += m_pieceValues[W_PAWN] + m_pieceSquareTablesLate[W_PAWN][MIRROR_PST_INDEX(targetPawnPosition)];
                newImmEval.phase -= phaseValues[W_PAWN];
            }
            else
            {
                Piece capturedPiece = Piece(LS1B(move.moveInfo & MOVE_INFO_CAPTURE_MASK) - 16);
                newImmEval.early += m_pieceValues[capturedPiece] + m_pieceSquareTablesEarly[capturedPiece][MIRROR_PST_INDEX(move.to)];
                newImmEval.late  += m_pieceValues[capturedPiece] + m_pieceSquareTablesLate[capturedPiece][MIRROR_PST_INDEX(move.to)];
                newImmEval.phase -= phaseValues[capturedPiece];
            }
        }
    }
    else // Black's move
    {
        newImmEval.early += m_pieceSquareTablesEarly[movedPiece][MIRROR_PST_INDEX(move.from)];
        newImmEval.late  += m_pieceSquareTablesLate[movedPiece][MIRROR_PST_INDEX(move.from)];

        if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
        {
            Piece promotePiece = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11);
            newImmEval.early -= m_pieceSquareTablesEarly[promotePiece][MIRROR_PST_INDEX(move.to)];
            newImmEval.late  -= m_pieceSquareTablesLate[promotePiece][MIRROR_PST_INDEX(move.to)];
            newImmEval.early -= m_pieceValues[promotePiece] - m_pieceValues[W_PAWN];
            newImmEval.late  -= m_pieceValues[promotePiece] - m_pieceValues[W_PAWN];
            newImmEval.phase += phaseValues[promotePiece] - phaseValues[W_PAWN];
        }
        else
        {
            newImmEval.early -= m_pieceSquareTablesEarly[movedPiece][MIRROR_PST_INDEX(move.to)];
            newImmEval.late  -= m_pieceSquareTablesLate[movedPiece][MIRROR_PST_INDEX(move.to)];
        }

        // Move the rook when castling
        if(CASTLE_SIDE(move.moveInfo))
        {
            if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_QUEEN)
            {
                newImmEval.early -= m_pieceSquareTablesEarly[W_ROOK][MIRROR_PST_INDEX(59)] - m_pieceSquareTablesEarly[W_ROOK][MIRROR_PST_INDEX(56)];
                newImmEval.late  -= m_pieceSquareTablesLate[W_ROOK][MIRROR_PST_INDEX(59)]  - m_pieceSquareTablesLate[W_ROOK][MIRROR_PST_INDEX(56)];
            }
            else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_KING)
            {
                newImmEval.early -= m_pieceSquareTablesEarly[W_ROOK][MIRROR_PST_INDEX(61)] - m_pieceSquareTablesEarly[W_ROOK][MIRROR_PST_INDEX(63)];
                newImmEval.late  -= m_pieceSquareTablesLate[W_ROOK][MIRROR_PST_INDEX(61)]  - m_pieceSquareTablesLate[W_ROOK][MIRROR_PST_INDEX(63)];
            }
        }

        if(CAPTURED_PIECE(move.moveInfo))
        {
            if(move.moveInfo & MoveInfoBit::ENPASSANT)
            {
                square_t targetPawnPosition = move.to + 8;
                newImmEval.early -= m_pieceValues[W_PAWN] + m_pieceSquareTablesEarly[W_PAWN][PST_INDEX(targetPawnPosition)];
                newImmEval.late  -= m_pieceValues[W_PAWN] + m_pieceSquareTablesLate[W_PAWN][PST_INDEX(targetPawnPosition)];
                newImmEval.phase -= phaseValues[W_PAWN];
            }
            else
            {
                Piece capturedPiece = Piece(LS1B(move.moveInfo & MOVE_INFO_CAPTURE_MASK) - 16);
                newImmEval.early -= m_pieceValues[capturedPiece] + m_pieceSquareTablesEarly[capturedPiece][PST_INDEX(move.to)];
                newImmEval.late  -= m_pieceValues[capturedPiece] + m_pieceSquareTablesLate[capturedPiece][PST_INDEX(move.to)];
                newImmEval.phase -= phaseValues[capturedPiece];
            }
        }
    }
}

eval_t Evaluator::immEvaluation(Board& board, uint8_t plyFromRoot)
{
    return 0;
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

    return phaseLerp(
        m_immAccumulatorStack[m_accumulatorStackPointer].early,
        m_immAccumulatorStack[m_accumulatorStackPointer].late,
        m_immAccumulatorStack[m_accumulatorStackPointer].phase
    );
}