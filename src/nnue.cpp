#include <nnue.hpp>

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
    DeltaFeatures delta;
    findDeltaFeatures(board, move, delta);

    // Copy from the old accumulator to the new accumulator
    for(uint32_t i = 0; i < L1Size; i++)
    {
        nextAcc->acc[Color::WHITE][i] = acc->acc[Color::WHITE][i];
        nextAcc->acc[Color::BLACK][i] = acc->acc[Color::BLACK][i];
    }

    for(uint32_t i = 0; i < delta.numAdded; i++)
    {
        uint32_t wfindex = delta.added[Color::WHITE][i];
        uint32_t bfindex = delta.added[Color::BLACK][i];
        for(uint32_t j = 0; j < L1Size; j++)
        {
            nextAcc->acc[Color::WHITE][j] += m_net.ftWeights[wfindex*L1Size + j];
            nextAcc->acc[Color::BLACK][j] += m_net.ftWeights[bfindex*L1Size + j];
        }
    }

    for(uint32_t i = 0; i < delta.numRemoved; i++)
    {
        uint32_t wfindex = delta.removed[Color::WHITE][i];
        uint32_t bfindex = delta.removed[Color::BLACK][i];
        for(uint32_t j = 0; j < L1Size; j++)
        {
            nextAcc->acc[Color::WHITE][j] -= m_net.ftWeights[wfindex*L1Size + j];
            nextAcc->acc[Color::BLACK][j] -= m_net.ftWeights[bfindex*L1Size + j];
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

    for(uint32_t i = 0; i < L2Size; i++)
    {
        l1Out[i] = m_net.l1Biases[i];
    }

    for(uint32_t i = 0; i < L2Size; i++)
    {
        for(uint32_t j = 0; j < L1Size; j++)
        {
            l1Out[i] += m_net.l1Weights[j*L2Size + i] * clampedAcc[j];
        }
    }

    for (uint32_t i = 0; i < L2Size; i++)
    {
        l1Out[i] = std::clamp(l1Out[i], 0, int32_t(FTQ * LQ));
    }

    float sum = m_net.l2Biases[0];
    for(uint32_t i = 0; i < L2Size; i++)
    {
        sum += l1Out[i] * m_net.l2Weights[i];
    }

    return sum / (FTQ * LQ);
}

void NNUE::load(const std::string filename)
{
    LOG("Loading QNNUE: " << filename)

    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open())
    {
        ERROR("Unable to open file: " << filename);
        return;
    }

    // Check the NNUE magic string
    std::string magic;
    magic.resize(strlen(NNUE::QNNUE_MAGIC));
    ifs.read(magic.data(), strlen(NNUE::QNNUE_MAGIC));
    if(NNUE::QNNUE_MAGIC != magic)
    {
        ERROR("Invalid magic number in file: " << filename);
        ifs.close();
        return;
    }

    // Read the net
    ifs.read((char*) m_net.ftWeights, sizeof(NNUE::Net::ftWeights));
    ifs.read((char*) m_net.ftBiases,  sizeof(NNUE::Net::ftBiases));
    ifs.read((char*) m_net.l1Weights, sizeof(NNUE::Net::l1Weights));
    ifs.read((char*) m_net.l1Biases,  sizeof(NNUE::Net::l1Biases));
    ifs.read((char*) m_net.l2Weights, sizeof(NNUE::Net::l2Weights));
    ifs.read((char*) m_net.l2Biases,  sizeof(NNUE::Net::l2Biases));

    LOG("Finished loading QNNUE: " << filename)
}

eval_t NNUE::predictBoard(const Board& board)
{
    Accumulator acc;
    initializeAccumulator(&acc, board);
    return predict(&acc, board.getTurn());
}
