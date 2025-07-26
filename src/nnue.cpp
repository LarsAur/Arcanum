#include <nnue.hpp>
#include <tuning/nnuetrainer.hpp>
#include <tuning/matrix.hpp>

using namespace Arcanum;

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
uint16_t NNUE::getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective)
{
    if(pieceColor == BLACK)
    {
        pieceSquare = ((7 - RANK(pieceSquare)) << 3) | FILE(pieceSquare);
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
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(targetSquare, opponent, Piece::W_PAWN, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(targetSquare, opponent, Piece::W_PAWN, Color::BLACK);
    }
    else if(move.isCapture())
    {
        Piece capturedPiece = move.capturedPiece();
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(move.to, opponent, capturedPiece, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(move.to, opponent, capturedPiece, Color::BLACK);
    }
    else if(move.isCastle())
    {
        square_t rookFrom;
        square_t rookTo;
        switch (CASTLE_SIDE(move.moveInfo))
        {
        case MoveInfoBit::CASTLE_WHITE_KING:
            rookFrom = Square::H1;
            rookTo = Square::F1;
            break;
        case MoveInfoBit::CASTLE_BLACK_KING:
            rookFrom = Square::H8;
            rookTo = Square::F8;
            break;
        case MoveInfoBit::CASTLE_WHITE_QUEEN:
            rookFrom = Square::A1;
            rookTo = Square::D1;
            break;
        case MoveInfoBit::CASTLE_BLACK_QUEEN:
            rookFrom = Square::A8;
            rookTo = Square::D8;
            break;
        default:
            rookFrom = Square::NONE;
            rookTo   = Square::NONE;
            ERROR("Unknown castle bit in move")
        }

        // Remove the rook from the old position
        delta.removed[Color::WHITE][delta.numRemoved]   = getFeatureIndex(rookFrom, turn, Piece::W_ROOK, Color::WHITE);
        delta.removed[Color::BLACK][delta.numRemoved++] = getFeatureIndex(rookFrom, turn, Piece::W_ROOK, Color::BLACK);

        // Add the rook to the new position
        delta.added[Color::WHITE][delta.numAdded]   = getFeatureIndex(rookTo, turn, Piece::W_ROOK, Color::WHITE);
        delta.added[Color::BLACK][delta.numAdded++] = getFeatureIndex(rookTo, turn, Piece::W_ROOK, Color::BLACK);
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

    __m256i* wacc = (__m256i*) acc->acc[Color::WHITE];
    __m256i* bacc = (__m256i*) acc->acc[Color::BLACK];

    for(uint32_t i = 0; i < NumChunks; i++)
    {
        *(wacc + i) = _mm256_load_si256(((__m256i*) (m_net->ftBiases)) + i);
        *(bacc + i) = _mm256_load_si256(((__m256i*) (m_net->ftBiases)) + i);
    }

    for(uint32_t i = 0; i < featureSet.numFeatures; i++)
    {
        uint32_t wfindex = featureSet.features[Color::WHITE][i];
        uint32_t bfindex = featureSet.features[Color::BLACK][i];

        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(wacc + j) = _mm256_add_epi16(*(wacc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[wfindex*L1Size])) + j));
            *(bacc + j) = _mm256_add_epi16(*(bacc + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[bfindex*L1Size])) + j));
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

void NNUE::incrementAccumulatorPerspective(Accumulator* acc, Accumulator* nextAcc, const DeltaFeatures& deltaFeatures, Color perspective)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    __m256i* acc256     = (__m256i*) acc->acc[perspective];
    __m256i* nextAcc256 = (__m256i*) nextAcc->acc[perspective];

    // TODO: The copy can be merged into the first add or subtract

    // Copy from the old accumulator to the new accumulator
    for(uint32_t i = 0; i < NumChunks; i++)
    {
        *(nextAcc256 + i) = _mm256_load_si256(acc256 + i);
    }

    for(uint32_t i = 0; i < deltaFeatures.numAdded; i++)
    {
        uint32_t findex = deltaFeatures.added[perspective][i];
        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(nextAcc256 + j) = _mm256_add_epi16(*(nextAcc256 + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[findex*L1Size])) + j));
        }
    }

    for(uint32_t i = 0; i < deltaFeatures.numRemoved; i++)
    {
        uint32_t findex = deltaFeatures.removed[perspective][i];
        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(nextAcc256 + j) = _mm256_sub_epi16(*(nextAcc256 + j), _mm256_load_si256(((__m256i*) (&m_net->ftWeights[findex*L1Size])) + j));
        }
    }
}

eval_t NNUE::predict(const Accumulator* acc, const Board& board)
{
    alignas(64) int8_t clampedAcc[L1Size];
    alignas(64) int32_t l1Out[L2Size];

    uint32_t bucket = getOutputBucket(board);

    m_clampAcc(acc->acc[board.getTurn()], clampedAcc);

    m_l1AffineRelu(clampedAcc, m_net->l1Weights[bucket], m_net->l1Biases[bucket], l1Out);

    float sum = m_net->l2Biases[bucket][0];
    for(uint32_t i = 0; i < L2Size; i++)
    {
        sum += l1Out[i] * m_net->l2Weights[bucket][i];
    }

    return sum / (FTQ * LQ);
}

eval_t NNUE::predictBoard(const Board& board)
{
    Accumulator acc;
    initializeAccumulator(&acc, board);
    return predict(&acc, board);
}

inline void NNUE::m_clampAcc(const int16_t* in, int8_t* out)
{
    constexpr uint32_t NumChunks = L1Size / 16;

    __m256i* in256 = (__m256i*) in;
    __m256i* out256 = (__m256i*) out;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i clip = _mm256_set1_epi16(FTQ);

    for(uint32_t i = 0; i < NumChunks / 2; i++)
    {
        __m256i acc1 = _mm256_load_si256(in256 + 2*i);
        __m256i acc2 = _mm256_load_si256(in256 + 2*i + 1);
        acc1 = _mm256_max_epi16(zero, _mm256_min_epi16(clip, acc1));
        acc2 = _mm256_max_epi16(zero, _mm256_min_epi16(clip, acc2));

        // Convert the two 16-bit vectors to 8-bit
        // Note that this shuffles the output [a1,a2,a3,a4], [b1,b2,b3,b4] -> [(a1,a2), (b1,b2), (a3, a4), (b3, b4)]
        __m256i acc8bit = _mm256_packs_epi16(acc1, acc2);

        // Unshuffle the shuffled output from above
        // 64-bit chunks are moved according to the select signal
        constexpr uint8_t select = 0b11011000; // 0, 2, 1, 3 (LSB first)
        acc8bit = _mm256_permute4x64_epi64(acc8bit, select);

        _mm256_store_si256(out256 + i, acc8bit);
    }
}

inline void NNUE::m_l1AffineRelu(const int8_t* in, int8_t* weights, int32_t* biases, int32_t* out)
{
    constexpr uint32_t NumInChunks  = L1Size / 32;
    constexpr uint32_t NumOutChunks = L2Size / 8;

    const __m256i zero = _mm256_setzero_si256();
    const __m256i clip = _mm256_set1_epi32(int32_t(FTQ * LQ));

    __m256i* out256 = (__m256i*) out;
    __m256i* in256  = (__m256i*) in;
    __m256i* w256   = (__m256i*) weights;

    for(uint32_t i = 0; i < L2Size; i++)
    {
        __m256i acc = _mm256_setzero_si256();

        for(uint32_t j = 0; j < NumInChunks; j++)
        {
            __m256i factors8 = _mm256_load_si256(in256 + j);
            __m256i weights8 = _mm256_load_si256(w256 + i*NumInChunks + j);

            __m256i sum16 = _mm256_maddubs_epi16(factors8, weights8);

            // Extract the upper and lower part of the 16-bit vectors and convert them to 32-bit
            __m256i sum32_1 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(sum16, 0));
            __m256i sum32_2 = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(sum16, 1));

            acc = _mm256_add_epi32(acc, _mm256_add_epi32(sum32_1, sum32_2));
        }

        // Horizontally add all the 32-bit values in the acc vector
        // Two sums will accumulate in acc[0] and acc[4] where acc is a 32-bit array
        acc = _mm256_hadd_epi32(acc, acc);
        acc = _mm256_hadd_epi32(acc, acc);
        int32_t* acc32 = (int32_t*) &acc;
        out[i] = acc32[0] + acc32[4];
    }

    // Add the bias and apply ReLu
    for(uint32_t i = 0; i < NumOutChunks; i++)
    {
        __m256i bias = _mm256_load_si256(((__m256i*)(biases) + i));
        *(out256 + i) = _mm256_add_epi32(*(out256 + i), bias);
        *(out256 + i) = _mm256_max_epi32(zero, _mm256_min_epi32(clip, *(out256 + i)));
    }
}

template <typename T, uint32_t rows, uint32_t cols>
static void quantizeMatrix(T* qMatrix, Matrix<rows, cols>& fMatrix, int32_t qFactor)
{
    float* data = fMatrix.data();
    for(uint32_t i = 0; i < rows * cols; i++)
    {
        qMatrix[i] = static_cast<T>(qFactor * data[i]);
    }
}

// Transposes / Converts the matrix to be row major instead of column major
// which is how the data is stored in the float Matrix
template <typename T, uint32_t rows, uint32_t cols>
static void quantizeTransposeMatrix(T* qMatrix, Matrix<rows, cols>& fMatrix, int32_t qFactor)
{
    float* data = fMatrix.data();

    for(uint32_t i = 0; i < rows; i++)
    {
        for(uint32_t j = 0; j < cols; j++)
        {
            qMatrix[i * cols + j] = static_cast<T>(qFactor * data[j*rows + i]);
        }
    }
}

void NNUE::load(const std::string filename)
{
    LOG("Loading NNUE: " << filename)

    // // Load the float net
    NNUETrainer fLoader;
    if(!fLoader.load(filename))
    {
        ERROR("Unable to load " << filename);
        return;
    }

    LOG("Quantizing NNUE")

    // Quantize the featuretransformer
    quantizeMatrix(m_net->ftWeights, fLoader.getNet()->ftWeights, FTQ);
    quantizeMatrix(m_net->ftBiases,  fLoader.getNet()->ftBiases,  FTQ);

    // Quantize the output layers with buckets
    for(uint32_t i = 0; i < NumOutputBuckets; i++)
    {
        quantizeTransposeMatrix(m_net->l1Weights[i], fLoader.getNet()->l1Weights[i], LQ);
        quantizeMatrix(m_net->l1Biases[i], fLoader.getNet()->l1Biases[i], LQ * FTQ);
        quantizeMatrix(m_net->l2Weights[i], fLoader.getNet()->l2Weights[i], 1);      // Note: Float layer
        quantizeMatrix(m_net->l2Biases[i], fLoader.getNet()->l2Biases[i], FTQ * LQ); // Note: Float layer
    }

    LOG("Finished loading and quantizing: " << filename)
}
