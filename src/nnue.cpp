#include <nnue.hpp>
#include <tuning/nnueformat.hpp>

using namespace Arcanum;

// Calculate the feature indices of the board with the white perspective
// To the feature indices of the black perspective, xor the indices with 1
uint16_t NNUE::getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective)
{
    if(pieceColor == BLACK)
    {
        pieceSquare = FLIP_RANK(pieceSquare);
    }

    return (((uint16_t(pieceType) << 6) | uint16_t(pieceSquare)) << 1) | (pieceColor ^ perspective);
}

uint32_t NNUE::getOutputBucket(const Board& board)
{
    constexpr uint32_t Divisor = (32 + NumOutputBuckets - 1) / NumOutputBuckets;
    return (board.getNumPieces() - 2) / Divisor;
}

// Calculate the delta features of the board when performing a move
// The board should be in the state before the move is performed
void NNUE::findDeltaFeatures(const Board& board, const Move& move, DeltaFeatures& delta)
{
    delta.numAdded = 0;
    delta.numRemoved = 0;

    Color turn = board.getTurn();
    Color opponent = Color(turn^1);

    // Remove the moved piece from the old position
    auto pieceType = move.movedPiece();
    delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(move.from, turn, pieceType, Color::WHITE);
    delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(move.from, turn, pieceType, Color::BLACK);

    // Add the moved piece to the new position
    if(move.isPromotion())
    {
        Piece promotionType = move.promotedPiece();
        delta.added[Color::WHITE][delta.numAdded]   = getFeatureIndex(move.to, turn, promotionType, Color::WHITE);
        delta.added[Color::BLACK][delta.numAdded++] = getFeatureIndex(move.to, turn, promotionType, Color::BLACK);
    }
    else
    {
        Piece pieceType = move.movedPiece();
        delta.added[Color::WHITE][delta.numAdded]   = getFeatureIndex(move.to, turn, pieceType, Color::WHITE);
        delta.added[Color::BLACK][delta.numAdded++] = getFeatureIndex(move.to, turn, pieceType, Color::BLACK);
    }

    // Remove the captured piece or move the rook in the case of castling
    if(move.isEnpassant())
    {
        square_t targetSquare = board.getEnpassantTarget();
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(targetSquare, opponent, Piece::PAWN, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(targetSquare, opponent, Piece::PAWN, Color::BLACK);
    }
    else if(move.isCapture())
    {
        Piece capturedPiece = move.capturedPiece();
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(move.to, opponent, capturedPiece, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(move.to, opponent, capturedPiece, Color::BLACK);
    }
    else if(move.isCastle())
    {
        const CastleIndex castleIndex = move.castleIndex();
        const square_t rookFrom = Move::CastleRookFrom[castleIndex];
        const square_t rookTo = Move::CastleRookTo[castleIndex];

        // Remove the rook from the old position
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(rookFrom, turn, Piece::ROOK, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(rookFrom, turn, Piece::ROOK, Color::BLACK);

        // Add the rook to the new position
        delta.added[Color::WHITE][delta.numAdded]   = getFeatureIndex(rookTo, turn, Piece::ROOK, Color::WHITE);
        delta.added[Color::BLACK][delta.numAdded++] = getFeatureIndex(rookTo, turn, Piece::ROOK, Color::BLACK);
    }
}

void NNUE::findFullFeatureSet(const Board& board, FullFeatureSet& featureSet)
{
    featureSet.numFeatures = 0;
    for(uint32_t color = 0; color < 2; color++)
    {
        for(uint32_t type = 0; type < 6; type++)
        {
            bitboard_t pieces = board.getTypedPieces(Piece(type), Color(color));
            while(pieces)
            {
                square_t pieceSquare = popLS1B(&pieces);
                uint16_t wfindex = getFeatureIndex(pieceSquare, Color(color), Piece(type), Color::WHITE);
                uint16_t bfindex = getFeatureIndex(pieceSquare, Color(color), Piece(type), Color::BLACK);
                featureSet.features[Color::WHITE][featureSet.numFeatures]   = wfindex;
                featureSet.features[Color::BLACK][featureSet.numFeatures++] = bfindex;
            }
        }
    }
}

NNUE::NNUE()
{
    m_net = new NNUE::Net();
}

NNUE::~NNUE()
{
    delete m_net;
}

void NNUE::initializeAccumulator(Accumulator* acc, const Board& board)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    FullFeatureSet featureSet;
    findFullFeatureSet(board, featureSet);

    __m256i* wacc = reinterpret_cast<__m256i*>(acc->acc[Color::WHITE]);
    __m256i* bacc = reinterpret_cast<__m256i*>(acc->acc[Color::BLACK]);
    const __m256i* biases = reinterpret_cast<const __m256i*>(m_net->ftBiases);


    for(uint32_t i = 0; i < NumChunks; i++)
    {
        const __m256i bias = _mm256_load_si256(biases + i);
        _mm256_store_si256(wacc + i, bias);
        _mm256_store_si256(bacc + i, bias);
    }

    for(uint32_t i = 0; i < featureSet.numFeatures; i++)
    {
        uint32_t wfindex = featureSet.features[Color::WHITE][i];
        uint32_t bfindex = featureSet.features[Color::BLACK][i];
        const __m256i* wbase = reinterpret_cast<const __m256i*>(&m_net->ftWeights[wfindex * L1Size]);
        const __m256i* bbase = reinterpret_cast<const __m256i*>(&m_net->ftWeights[bfindex * L1Size]);

        for(uint32_t j = 0; j < NumChunks; j++)
        {
            const __m256i wsum = _mm256_add_epi16(_mm256_load_si256(wacc + j), _mm256_load_si256(wbase + j));
            const __m256i bsum = _mm256_add_epi16(_mm256_load_si256(bacc + j), _mm256_load_si256(bbase + j));
            _mm256_store_si256(wacc + j, wsum);
            _mm256_store_si256(bacc + j, bsum);
        }
    }
}

// The board should be in the state before the move is performed
void NNUE::incrementAccumulator(Accumulator* acc, Accumulator* nextAcc, const Board& board, const Move& move)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    DeltaFeatures delta;
    findDeltaFeatures(board, move, delta);

    __m256i* wacc = (__m256i*) acc->acc[Color::WHITE];
    __m256i* bacc = (__m256i*) acc->acc[Color::BLACK];
    __m256i* wnextAcc = (__m256i*) nextAcc->acc[Color::WHITE];
    __m256i* bnextAcc = (__m256i*) nextAcc->acc[Color::BLACK];

    // Copy from the old accumulator to the new accumulator
    for(uint32_t i = 0; i < NumChunks; i++)
    {
        *(wnextAcc + i) = _mm256_load_si256(wacc + i);
        *(bnextAcc + i) = _mm256_load_si256(bacc + i);
    }

    for(uint32_t i = 0; i < delta.numAdded; i++)
    {
        uint32_t wfindex = delta.added[Color::WHITE][i];
        uint32_t bfindex = delta.added[Color::BLACK][i];
        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(wnextAcc + j) = _mm256_add_epi16(*(wnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[wfindex*L1Size])) + j));
            *(bnextAcc + j) = _mm256_add_epi16(*(bnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[bfindex*L1Size])) + j));
        }
    }

    for(uint32_t i = 0; i < delta.numRemoved; i++)
    {
        uint32_t wfindex = delta.removed[Color::WHITE][i];
        uint32_t bfindex = delta.removed[Color::BLACK][i];
        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(wnextAcc + j) = _mm256_sub_epi16(*(wnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[wfindex*L1Size])) + j));
            *(bnextAcc + j) = _mm256_sub_epi16(*(bnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[bfindex*L1Size])) + j));
        }
    }
}

void NNUE::incrementAccumulatorPerspective(const Accumulator* acc, Accumulator* nextAcc, const DeltaFeatures& deltaFeatures, Color perspective)
{
    uint8_t funcIndex = deltaFeatures.numRemoved << 2 | deltaFeatures.numAdded;

    switch(funcIndex)
    {
        case 0b0101:
            m_accAddSub(acc, nextAcc, deltaFeatures, perspective);
            break;
        case 0b1001:
            m_accAddSubSub(acc, nextAcc, deltaFeatures, perspective);
            break;
        case 0b1010:
            m_accAddAddSubSub(acc, nextAcc, deltaFeatures, perspective);
            break;
        default:
            ERROR("No matching function for incrementing accumulator perspective: Remove: " << deltaFeatures.numRemoved << " Add: " << deltaFeatures.numAdded)
    }
}

void NNUE::m_accAddSub(const Accumulator* acc, Accumulator* nextAcc, const DeltaFeatures& deltaFeatures, Color perspective)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    const __m256i* acc256     = reinterpret_cast<const __m256i*>(acc->acc[perspective]);
    __m256i* nextAcc256       = reinterpret_cast<__m256i*>(nextAcc->acc[perspective]);

    const __m256i* ftAddBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.added[perspective][0] * L1Size]);
    const __m256i* ftSubBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.removed[perspective][0] * L1Size]);

    for(uint32_t i = 0; i < NumChunks; i++)
    {
        __m256i out = _mm256_add_epi16(_mm256_load_si256(acc256 + i), _mm256_load_si256(ftAddBase0 + i));
        out = _mm256_sub_epi16(out, _mm256_load_si256(ftSubBase0 + i));
        _mm256_store_si256(nextAcc256 + i, out);
    }
}

void NNUE::m_accAddSubSub(const Accumulator* acc, Accumulator* nextAcc, const DeltaFeatures& deltaFeatures, Color perspective)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    const __m256i* acc256     = reinterpret_cast<const __m256i*>(acc->acc[perspective]);
    __m256i* nextAcc256       = reinterpret_cast<__m256i*>(nextAcc->acc[perspective]);

    const __m256i* ftAddBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.added[perspective][0] * L1Size]);
    const __m256i* ftSubBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.removed[perspective][0] * L1Size]);
    const __m256i* ftSubBase1 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.removed[perspective][1] * L1Size]);

    for(uint32_t i = 0; i < NumChunks; i++)
    {
        __m256i out = _mm256_add_epi16(_mm256_load_si256(acc256 + i), _mm256_load_si256(ftAddBase0 + i));
        out = _mm256_sub_epi16(out, _mm256_load_si256(ftSubBase0 + i));
        out = _mm256_sub_epi16(out, _mm256_load_si256(ftSubBase1 + i));
        _mm256_store_si256(nextAcc256 + i, out);
    }
}

void NNUE::m_accAddAddSubSub(const Accumulator* acc, Accumulator* nextAcc, const DeltaFeatures& deltaFeatures, Color perspective)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    const __m256i* acc256     = reinterpret_cast<const __m256i*>(acc->acc[perspective]);
    __m256i* nextAcc256       = reinterpret_cast<__m256i*>(nextAcc->acc[perspective]);

    const __m256i* ftAddBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.added[perspective][0] * L1Size]);
    const __m256i* ftAddBase1 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.added[perspective][1] * L1Size]);
    const __m256i* ftSubBase0 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.removed[perspective][0] * L1Size]);
    const __m256i* ftSubBase1 = reinterpret_cast<const __m256i*>(&m_net->ftWeights[deltaFeatures.removed[perspective][1] * L1Size]);

    for(uint32_t i = 0; i < NumChunks; i++)
    {
        __m256i out = _mm256_add_epi16(_mm256_load_si256(acc256 + i), _mm256_load_si256(ftAddBase0 + i));
        out = _mm256_add_epi16(out, _mm256_load_si256(ftAddBase1 + i));
        out = _mm256_sub_epi16(out, _mm256_load_si256(ftSubBase0 + i));
        out = _mm256_sub_epi16(out, _mm256_load_si256(ftSubBase1 + i));
        _mm256_store_si256(nextAcc256 + i, out);
    }
}

eval_t NNUE::predict(const Accumulator* acc, const Board& board)
{
    uint32_t bucket = getOutputBucket(board);

    int32_t l1Out = m_l1AffineTransform(acc->acc[board.getTurn()], m_net->l1Weights[bucket], m_net->l1Biases[bucket]);

    return static_cast<eval_t>((l1Out * NetworkScale) / (FTQ * LQ));
}

eval_t NNUE::predictBoard(const Board& board)
{
    Accumulator acc;
    initializeAccumulator(&acc, board);
    return predict(&acc, board);
}

// The clipped ReLU is fused into the transform to avoid a round-trip through a temporary buffer
inline int32_t NNUE::m_l1AffineTransform(const int16_t* in, const int16_t* weights, const int32_t* biases)
{
    constexpr uint32_t ChunkSize = sizeof(__m256i) / sizeof(in[0]);
    constexpr uint32_t NumInChunks  = L1Size / ChunkSize;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i clip = _mm256_set1_epi16(FTQ);

    const __m256i* in256 = reinterpret_cast<const __m256i*>(in);
    const __m256i* w256  = reinterpret_cast<const __m256i*>(weights);

    __m256i acc0 = _mm256_setzero_si256();
    __m256i acc1 = _mm256_setzero_si256();

    for(uint32_t j = 0; j < NumInChunks; j += 2)
    {
        __m256i factors0 = _mm256_load_si256(in256 + j);
        __m256i factors1 = _mm256_load_si256(in256 + j + 1);

        factors0 = _mm256_min_epi16(_mm256_max_epi16(factors0, zero), clip);
        factors1 = _mm256_min_epi16(_mm256_max_epi16(factors1, zero), clip);

        acc0 = _mm256_add_epi32(acc0, _mm256_madd_epi16(factors0, _mm256_load_si256(w256 + j)));
        acc1 = _mm256_add_epi32(acc1, _mm256_madd_epi16(factors1, _mm256_load_si256(w256 + j + 1)));
    }

    __m256i acc = _mm256_add_epi32(acc0, acc1);

    // Horizontal sum over the 8 int32 lanes.
    __m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(acc), _mm256_extracti128_si256(acc, 1));
    sum128 = _mm_hadd_epi32(sum128, sum128);
    sum128 = _mm_hadd_epi32(sum128, sum128);
    return _mm_cvtsi128_si32(sum128) + biases[0];
}

void NNUE::load(const std::string filename)
{
    NNUEParser parser;
    if(!parser.load(filename))
    {
        return;
    }

    // Quantize the featuretransformer
    parser.read(m_net->ftWeights, L1Size, FTSize, FTQ);
    parser.read(m_net->ftBiases,  L1Size,      1, FTQ);

    // Quantize the output layers with buckets
    for(uint32_t i = 0; i < NumOutputBuckets; i++)
    {
        parser.readTranspose(m_net->l1Weights[i], 1, L1Size, LQ);
        parser.read(m_net->l1Biases[i], 1, 1, LQ * FTQ);
    }

    DEBUG("Finished loading and quantizing: " << filename)
}
