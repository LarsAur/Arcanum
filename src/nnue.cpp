#include <nnue.hpp>
#include <tuning/nnuetrainer.hpp>
#include <tuning/matrix.hpp>

using namespace Arcanum;

const char* NNUE::QNNUE_MAGIC = "Arcanum QNNUE";

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
uint32_t NNUE::getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective)
{
    if(pieceColor == BLACK)
    {
        pieceSquare = ((7 - RANK(pieceSquare)) << 3) | FILE(pieceSquare);
    }

    return (((uint32_t(pieceType) << 6) | uint32_t(pieceSquare)) << 1) | (pieceColor ^ perspective);
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
                uint32_t wfindex = getFeatureIndex(pieceSquare, Color(color), Piece(type), Color::WHITE);
                uint32_t bfindex = getFeatureIndex(pieceSquare, Color(color), Piece(type), Color::BLACK);
                featureSet.features[Color::WHITE][featureSet.numFeatures]   = wfindex;
                featureSet.features[Color::BLACK][featureSet.numFeatures++] = bfindex;
            }
        }
    }
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
        *(wacc + i) = _mm256_load_si256(((__m256i*) (m_net.ftBiases)) + i);
        *(bacc + i) = _mm256_load_si256(((__m256i*) (m_net.ftBiases)) + i);
    }

    for(uint32_t i = 0; i < featureSet.numFeatures; i++)
    {
        uint32_t wfindex = featureSet.features[Color::WHITE][i];
        uint32_t bfindex = featureSet.features[Color::BLACK][i];

        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(wacc + j) = _mm256_add_epi16(*(wacc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[wfindex*L1Size])) + j));
            *(bacc + j) = _mm256_add_epi16(*(bacc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[bfindex*L1Size])) + j));
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
            *(wnextAcc + j) = _mm256_add_epi16(*(wnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[wfindex*L1Size])) + j));
            *(bnextAcc + j) = _mm256_add_epi16(*(bnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[bfindex*L1Size])) + j));
        }
    }

    for(uint32_t i = 0; i < delta.numRemoved; i++)
    {
        uint32_t wfindex = delta.removed[Color::WHITE][i];
        uint32_t bfindex = delta.removed[Color::BLACK][i];
        for(uint32_t j = 0; j < NumChunks; j++)
        {
            *(wnextAcc + j) = _mm256_sub_epi16(*(wnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[wfindex*L1Size])) + j));
            *(bnextAcc + j) = _mm256_sub_epi16(*(bnextAcc + j), _mm256_load_si256(((__m256i*) (&m_net.ftWeights[bfindex*L1Size])) + j));
        }
    }
}

eval_t NNUE::predict(const Accumulator* acc, Color perspective)
{
    alignas(64) int8_t clampedAcc[L1Size];
    alignas(64) int32_t l1Out[L2Size];

    // Clipped RELU of the accumulator
    for(uint32_t i = 0; i < L1Size; i++)
    {
        clampedAcc[i] = std::clamp(acc->acc[perspective][i], int16_t(0), int16_t(FTQ));
    }

    m_l1AffineRelu(clampedAcc, m_net.l1Weights, m_net.l1Biases, l1Out);

    float sum = m_net.l2Biases[0];
    for(uint32_t i = 0; i < L2Size; i++)
    {
        sum += l1Out[i] * m_net.l2Weights[i];
    }

    return sum / (FTQ * LQ);
}

eval_t NNUE::predictBoard(const Board& board)
{
    Accumulator acc;
    initializeAccumulator(&acc, board);
    return predict(&acc, board.getTurn());
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

    // Quantize the featuretransformer and the L1 linear layer
    quantizeMatrix(m_net.ftWeights, fLoader.getNet()->ftWeights, FTQ);
    quantizeMatrix(m_net.ftBiases,  fLoader.getNet()->ftBiases,  FTQ);
    quantizeTransposeMatrix(m_net.l1Weights, fLoader.getNet()->l1Weights, LQ);
    quantizeMatrix(m_net.l1Biases, fLoader.getNet()->l1Biases, LQ * FTQ);

    // // Load float layers
    memcpy(m_net.l2Weights, fLoader.getNet()->l2Weights.data(), sizeof(m_net.l2Weights));
    m_net.l2Biases[0] =  *fLoader.getNet()->l2Biases.data() * FTQ * LQ;

    LOG("Finished loading and quantizing: " << filename)
}
