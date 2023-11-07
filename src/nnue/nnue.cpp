#include <nnue/nnue.hpp>
#include <utils.hpp>
#include <fstream>
#include <memory.hpp>

#if defined(_WIN64)
#include <Libloaderapi.h>
#elif defined(__linux__)
    
#else
    LOG("Else")
#endif

using namespace NN;

#define ALIGN64(p) __builtin_assume_aligned((p), 64)

enum {
  PS_W_PAWN   =  1,
  PS_B_PAWN   =  1 * 64 + 1,
  PS_W_KNIGHT =  2 * 64 + 1,
  PS_B_KNIGHT =  3 * 64 + 1,
  PS_W_BISHOP =  4 * 64 + 1,
  PS_B_BISHOP =  5 * 64 + 1,
  PS_W_ROOK   =  6 * 64 + 1,
  PS_B_ROOK   =  7 * 64 + 1,
  PS_W_QUEEN  =  8 * 64 + 1,
  PS_B_QUEEN  =  9 * 64 + 1,
  PS_END      = 10 * 64 + 1
};

static uint32_t pieceToIndex[2][12] = {
  { PS_W_PAWN, PS_W_ROOK, PS_W_KNIGHT, PS_W_BISHOP, PS_W_QUEEN, 0/*KING*/, PS_B_PAWN, PS_B_ROOK, PS_B_KNIGHT, PS_B_BISHOP, PS_B_QUEEN, 0/*KING*/ },
  { PS_B_PAWN, PS_B_ROOK, PS_B_KNIGHT, PS_B_BISHOP, PS_B_QUEEN, 0/*KING*/, PS_W_PAWN, PS_W_ROOK, PS_W_KNIGHT, PS_W_BISHOP, PS_W_QUEEN, 0/*KING*/ },
};

static int lsb64(uint64_t v)
{
    return _tzcnt_u64(v);
}

static int popLsb64(uint64_t* v)
{
    int popIdx = _tzcnt_u64(*v);
    *v = _blsr_u64(*v);
    return popIdx;
}

template <typename T>
static inline T ifstreamGet(std::ifstream& fileStream)
{
    T value = 0;
    for(size_t i = 0; i < sizeof(T); i++)
    {
        value |= fileStream.get() << (8 * i);
    }
    return value;
}

template<class T> static inline void Log(const __m256i & value)
{
    const size_t n = sizeof(__m256i) / sizeof(T);
    T buffer[n];
    _mm256_storeu_si256((__m256i*)buffer, value);
    for (int i = 0; i < n; i++)
        std::cout << buffer[i] << " ";
}

// Weight index
static inline uint32_t windex(uint32_t row, uint32_t column, uint32_t dims)
{
    #if defined(USE_AVX2)
    if(dims > 32)
    {
        uint32_t b = column & 0x18;
        b = (b << 1) | (b >> 1);
        column = (column & ~0x18) | (b & 0x18);
    }
    #endif

    return column * 32 + row;
}

static inline uint8_t orientSquare(Arcanum::Color color, uint8_t square)
{
    return square ^ (color == Arcanum::Color::WHITE ? 0x00 : 0x3F);
}

// feature index
static inline uint32_t findex(Arcanum::Color perspective, uint8_t square, Arcanum::Piece piece, uint8_t kingSquare)
{
    return orientSquare(perspective, square) + pieceToIndex[perspective][piece] + PS_END * kingSquare;
}

NNUE::NNUE(){ m_loaded = false; }

NNUE::~NNUE()
{
    if(m_loaded)
    {
        Memory::alignedFree(m_featureTransformer.biases);
        Memory::alignedFree(m_featureTransformer.weights);
    }
}

bool NNUE::loadFullPath(std::string fullPath)
{
    LOG("Loading: " << fullPath)

    std::ifstream stream = std::ifstream(fullPath, std::ios::binary | std::ios::in);
    
    if(!stream.is_open())
    {
        ERROR("Unable to open file")
        return false;
    }

    m_loadHeader(stream);
    m_loadWeights(stream);
    m_loaded = true;
    stream.close();    

    return true;
}

bool NNUE::loadRelative(std::string filename)
{
    // Get the path of the executable file
    char execFullPath[2048];
    #if defined(_WIN64)
        GetModuleFileNameA(NULL, execFullPath, 2048);
    #elif defined(__linux__)
        ERROR("Missing implementation")
    #else
        ERROR("Missing implementation")
    #endif

    // Move one folder up
    std::string path = std::string(execFullPath);
    size_t idx = path.find_last_of('\\');
    path = std::string(path).substr(0, idx);
    idx = path.find_last_of('\\');
    path = std::string(path).substr(0, idx + 1); // Keep the last '\'
    path.append(filename);
    
    return loadFullPath(path);
}

int NNUE::evaluate(const Accumulator& accumulator, Arcanum::Color turn)
{
    constexpr uint32_t numChunks = 16 * ftOuts / 256; 
    alignas(8) Mask mask[numChunks];
    alignas(8) Mask hiddenMask[8 / sizeof(Mask)] = {0};
    NetData netData;

    // Transform the accumulator to Clipped netdata
    // The data is transformed from a vector of 16-bit values to a vector of 8-bit values
    const Arcanum::Color perspectives[2] = {turn, Arcanum::Color(turn^1)};
    for(uint32_t i = 0; i < 2; i++)
    {
        const Arcanum::Color p = perspectives[i];
        for(uint32_t j = 0; j < numChunks / 2; j++)
        {
            uint32_t index = j + i*(numChunks/2);
            __m256i s0 = ((__m256i*) accumulator.acc[p])[j * 2];
            __m256i s1 = ((__m256i*) accumulator.acc[p])[j * 2 + 1];
            __m256i packed = _mm256_packs_epi16(s0, s1);
            _mm256_store_si256(&((__m256i*) netData.input)[index], packed);
            // Create a bitmask (1 bit for each 8-bit element) for all elements larger than 0
            mask[index] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(packed,_mm256_setzero_si256()));
        }
    }
    
    m_affineTransform(netData.input, netData.hiddenOut1, 2*ftOuts, l1outs, m_hiddenLayer1.biases, m_hiddenLayer1.weights, mask, hiddenMask, true);

    m_affineTransform(netData.hiddenOut1, netData.hiddenOut2, l1outs, l2outs, m_hiddenLayer2.biases, m_hiddenLayer2.weights, hiddenMask, nullptr, false);

    int32_t score = m_affinePropagate(netData.hiddenOut2, m_outputLayer.biases, m_outputLayer.weights);
    return score / fv_scale;
}

void NNUE::incrementAccumulator(
    const Accumulator& prevAccumulator, // Accumulator from the boardstate before the move
    Accumulator& newAccumulator,        // Uninitialized accumulator
    const Arcanum::Color perspective,   // Color for which the move was made
    const Arcanum::Board& board,        // Board after the move
    const Arcanum::Move& move           // Move being made
) {
    constexpr int registerWidth = 256 / (8*sizeof(AccData)); // Number of AccData (int16_t) in AVX2 register (256 bit)
    static_assert(ftOuts % registerWidth == 0);
    constexpr int numChunks = ftOuts / registerWidth;
    __m256i regs[numChunks]; // Temp accumulators in AVX register

    if(move.moveInfo & MOVE_INFO_CASTLE_MASK)
    {
        m_initializeAccumulatorPerspective(newAccumulator, perspective, board);

        // Find the rook positions
        uint32_t activated, deactivated;
        Arcanum::Color nPerspective = Arcanum::Color(1^perspective);
        uint8_t kingSquare = orientSquare(nPerspective, lsb64(board.getTypedPieces(Arcanum::Piece::W_KING, nPerspective)));
        Arcanum::Piece rookPiece = Arcanum::Piece(Arcanum::Piece::W_ROOK + Arcanum::Piece::B_PAWN * perspective);
        if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_QUEEN)        { activated = findex(nPerspective,  3, rookPiece, kingSquare); deactivated = findex(nPerspective,  0, rookPiece, kingSquare); }
        else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_QUEEN)   { activated = findex(nPerspective, 59, rookPiece, kingSquare); deactivated = findex(nPerspective, 56, rookPiece, kingSquare); }
        else if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_KING)    { activated = findex(nPerspective,  5, rookPiece, kingSquare); deactivated = findex(nPerspective,  7, rookPiece, kingSquare); }
        else /*move.moveInfo & MOVE_INFO_CASTLE_BLACK_KING*/    { activated = findex(nPerspective, 61, rookPiece, kingSquare); deactivated = findex(nPerspective, 63, rookPiece, kingSquare); }
        
        for(int i = 0; i < numChunks; i++)
        {
            // Copy previous accumulator
            __m256i value = _mm256_load_si256((__m256i*)&prevAccumulator.acc[nPerspective][i * registerWidth]);
            // Activate and deactivate features features
            value = _mm256_add_epi16(value, _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[activated * 256 + i * registerWidth])));
            value = _mm256_sub_epi16(value, _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[deactivated * 256 + i * registerWidth])));
            // Store new accumulator
            _mm256_store_si256((__m256i*)&newAccumulator.acc[nPerspective][i * registerWidth], value);
        }

        return;
    }

    // If the move is a king move, the whole accumulator 
    // for the moving color has to be updated.
    // if the move does not capture anything, the other perspective does not need to be updated.
    if(move.moveInfo & MOVE_INFO_KING_MOVE && !(move.moveInfo & MOVE_INFO_CAPTURE_MASK))
    {
        m_initializeAccumulatorPerspective(newAccumulator, perspective, board);

        // Copy the accumulator of the other perspective
        for(int i = 0; i < numChunks; i++)
        {
            __m256i value = _mm256_load_si256((__m256i*)&prevAccumulator.acc[1^perspective][i * registerWidth]);
            _mm256_store_si256((__m256i*)&newAccumulator.acc[1^perspective][i * registerWidth], value);
        }
        return;

    }
    
    if(move.moveInfo & MOVE_INFO_KING_MOVE)
    {
        m_initializeAccumulatorPerspective(newAccumulator, perspective, board);
        
        // Copy the accumulator of the other perspective
        for(int i = 0; i < numChunks; i++)
        {
            regs[i] = _mm256_load_si256((__m256i*)&prevAccumulator.acc[1^perspective][i * registerWidth]);
        }

        std::array<uint32_t, 2> deactivated;
        uint32_t activated; // Unused. When the king moves, no features are activated for the other perspective
        m_calculateChangedIndices(activated, deactivated, Arcanum::Color(1^perspective), move, board);

        // Deactivated features
        // Only the captured feature is removed, as the moved king does not deactivate a feature for the other perspective
        uint32_t feature = deactivated[1];
        for(int i = 0; i < numChunks; i++)
        {
            regs[i] = _mm256_sub_epi16(regs[i], _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[feature * 256 + i * registerWidth])));
        }

        for(int i = 0; i < numChunks; i++)
        {
            _mm256_store_si256((__m256i*)&newAccumulator.acc[1^perspective][i * registerWidth], regs[i]);
        }

        return;
    }

    // For each perspective
    for(int p = 0; p < 2; p++)
    {
        std::array<uint32_t, 2> deactivated;
        uint32_t activated;
        uint32_t numDeactivated = m_calculateChangedIndices(activated, deactivated, Arcanum::Color(p), move, board);
        
        // Copy the accumulator of the other perspective
        for(int i = 0; i < numChunks; i++)
        {
            regs[i] = _mm256_load_si256((__m256i*)&prevAccumulator.acc[p][i * registerWidth]);
        }

        // Activated features
        for(int i = 0; i < numChunks; i++)
        {
            regs[i] = _mm256_add_epi16(regs[i], _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[activated * 256 + i * registerWidth])));
        }

        // Deactivated features
        for(uint32_t d = 0; d < numDeactivated; d++)
        {
            uint32_t feature = deactivated[d];
            for(int i = 0; i < numChunks; i++)
            {
                regs[i] = _mm256_sub_epi16(regs[i], _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[feature * 256 + i * registerWidth])));
            }
        }

        for(uint32_t i = 0; i < numChunks; i++)
        {
            _mm256_store_si256((__m256i*)&newAccumulator.acc[p][i * registerWidth], regs[i]);
        }
    }
}

bool NNUE::isLoaded() { return m_loaded; }

int32_t NNUE::m_affinePropagate(Clipped* input, Bias* biases, Weight* weights)
{
    __m256i *iv = (__m256i *)input;
    __m256i *row = (__m256i *)ALIGN64(weights);
    __m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
    prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
    __m128i sum = _mm_add_epi32(
        _mm256_castsi256_si128(prod), _mm256_extracti128_si256(prod, 1));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
    return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
}

static inline bool nextIdx(uint32_t *idx, uint32_t *offset, Mask2 *v, const Mask *mask, uint32_t inDim)
{
    while (*v == 0) {
        *offset += 8 * sizeof(Mask2);
        if (*offset >= inDim) return false;
        memcpy(v, (char *)mask + (*offset / 8), sizeof(Mask2));
    }

    *idx = *offset + lsb64(*v);
    *v &= *v - 1;

    return true;
}

void NNUE::m_affineTransform(
    Clipped *input,
    void *output,
    uint32_t inDim,
    uint32_t outDim,
    const Bias* biases,
    const Weight* weights,
    const Mask *inMask,
    Mask *outMask,
    const bool pack8Mask
) {
      const __m256i kZero = _mm256_setzero_si256();
    __m256i out0 = _mm256_load_si256(&((__m256i*) biases)[0]);
    __m256i out1 = _mm256_load_si256(&((__m256i*) biases)[1]);
    __m256i out2 = _mm256_load_si256(&((__m256i*) biases)[2]);
    __m256i out3 = _mm256_load_si256(&((__m256i*) biases)[3]);
    __m256i first, second;
    uint32_t idx; 
    Mask2 v;

    memcpy(&v, inMask, sizeof(Mask2));
    for(uint32_t offset = 0; offset < inDim;)
    {
        if (!nextIdx(&idx, &offset, &v, inMask, inDim))
            break;

        first = ((__m256i *)ALIGN64(weights))[idx];
        uint16_t factor = input[idx];

        if (nextIdx(&idx, &offset, &v, inMask, inDim)) {
            second = ((__m256i *)ALIGN64(weights))[idx];
            factor |= input[idx] << 8;
        } else {
            second = kZero;
        }

        __m256i mul = _mm256_set1_epi16(factor), prod, signs;
        prod = _mm256_maddubs_epi16(mul, _mm256_unpacklo_epi8(first, second));
        signs = _mm256_cmpgt_epi16(kZero, prod);
        out0 = _mm256_add_epi32(out0, _mm256_unpacklo_epi16(prod, signs));
        out1 = _mm256_add_epi32(out1, _mm256_unpackhi_epi16(prod, signs));
        prod = _mm256_maddubs_epi16(mul, _mm256_unpackhi_epi8(first, second));
        signs = _mm256_cmpgt_epi16(kZero, prod);
        out2 = _mm256_add_epi32(out2, _mm256_unpacklo_epi16(prod, signs));
        out3 = _mm256_add_epi32(out3, _mm256_unpackhi_epi16(prod, signs));
    }

    __m256i out16_0 = _mm256_srai_epi16(_mm256_packs_epi32(out0, out1), shift);
    __m256i out16_1 = _mm256_srai_epi16(_mm256_packs_epi32(out2, out3), shift);

    __m256i *outVec = (__m256i *)output;
    outVec[0] = _mm256_packs_epi16(out16_0, out16_1);
    if (pack8Mask)
        outMask[0] = _mm256_movemask_epi8(_mm256_cmpgt_epi8(outVec[0], kZero));
    else
        outVec[0] = _mm256_max_epi8(outVec[0], kZero);
}

int NNUE::evaluateBoard(const Arcanum::Board& board)
{
    NN::Accumulator accumulator;
    initializeAccumulator(accumulator, board);
    return evaluate(accumulator, board.getTurn());
}

// Calculates the activated and deactivated indices of a selected perspective
// There can be at most 1 activated index, and 2 deactivated indices in case of captures
// The returned value is the number of deactivated indices
uint32_t NNUE::m_calculateChangedIndices(uint32_t& activated, std::array<uint32_t, 2>& deactivated, const Arcanum::Color perspective, const Arcanum::Move& move, const Arcanum::Board board)
{
    uint8_t kingSquare = orientSquare(perspective, lsb64(board.getTypedPieces(Arcanum::Piece::W_KING, perspective)));
    // It is assumed that the move is already made, and that the color of the moved piece is the opposite of the board turn
    Arcanum::Piece movedPiece = Arcanum::Piece(lsb64(move.moveInfo & MOVE_INFO_MOVE_MASK) + Arcanum::B_PAWN * (1^board.getTurn()));
    if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
    {
        Arcanum::Piece promotedPiece = Arcanum::Piece(lsb64(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11 + Arcanum::B_PAWN * (1^board.getTurn()));
        activated = findex(perspective, move.to, promotedPiece, kingSquare);
    }
    else
    {
        activated = findex(perspective, move.to, movedPiece, kingSquare);
    }

    deactivated[0] = findex(perspective, move.from, movedPiece, kingSquare);

    if(!(move.moveInfo & MOVE_INFO_CAPTURE_MASK))
        return 1; // If there is no capture, there is only 1 deactivated index

    // It is assumed that the move is already made, and that the color of the captured piece is the board turn
    Arcanum::Piece capturedPiece = Arcanum::Piece(lsb64(move.moveInfo & MOVE_INFO_CAPTURE_MASK) - 16 + Arcanum::B_PAWN * board.getTurn());
    if(move.moveInfo & MOVE_INFO_ENPASSANT)
    {   
        // getTurn returns the color of the captured piece in this case
        uint8_t targetSquare = board.getTurn() == Arcanum::Color::BLACK ? (move.to - 8) : (move.to + 8);
        deactivated[1] = findex(perspective, targetSquare, capturedPiece, kingSquare);
    }
    else
        deactivated[1] = findex(perspective, move.to, capturedPiece, kingSquare);

    return 2;
}

// Note on a normal chess board 30 is the maximum number of pieces without the king
// In the case where there are more pieces, eg. in a custom setup, this will not work. 
uint32_t NNUE::m_calculateActiveIndices(std::array<uint32_t, 30>& indicies, const Arcanum::Color perspective, const Arcanum::Board& board)
{
    int idxCounter = 0;

    uint8_t kingSquare = orientSquare(perspective, lsb64(board.getTypedPieces(Arcanum::Piece::W_KING, perspective)));

    // Pawns
    Arcanum::bitboard_t whitePawns = board.getTypedPieces(Arcanum::Piece::W_PAWN, Arcanum::Color::WHITE);
    while (whitePawns) { indicies[idxCounter++] = findex(perspective, popLsb64(&whitePawns), Arcanum::W_PAWN, kingSquare); }
    Arcanum::bitboard_t blackPawns = board.getTypedPieces(Arcanum::Piece::W_PAWN, Arcanum::Color::BLACK);
    while (blackPawns) { indicies[idxCounter++] = findex(perspective, popLsb64(&blackPawns), Arcanum::B_PAWN, kingSquare); }
    
    // Rooks
    Arcanum::bitboard_t whiteRooks = board.getTypedPieces(Arcanum::Piece::W_ROOK, Arcanum::Color::WHITE);
    while (whiteRooks) { indicies[idxCounter++] = findex(perspective, popLsb64(&whiteRooks), Arcanum::W_ROOK, kingSquare); }
    Arcanum::bitboard_t blackRooks = board.getTypedPieces(Arcanum::Piece::W_ROOK, Arcanum::Color::BLACK);
    while (blackRooks) { indicies[idxCounter++] = findex(perspective, popLsb64(&blackRooks), Arcanum::B_ROOK, kingSquare); }

    // Knights
    Arcanum::bitboard_t whiteKnights = board.getTypedPieces(Arcanum::Piece::W_KNIGHT, Arcanum::Color::WHITE);
    while (whiteKnights) { indicies[idxCounter++] = findex(perspective, popLsb64(&whiteKnights), Arcanum::W_KNIGHT, kingSquare); }
    Arcanum::bitboard_t blackKnights = board.getTypedPieces(Arcanum::Piece::W_KNIGHT, Arcanum::Color::BLACK);
    while (blackKnights) { indicies[idxCounter++] = findex(perspective, popLsb64(&blackKnights), Arcanum::B_KNIGHT, kingSquare); }

    // Bishops
    Arcanum::bitboard_t whiteBishops = board.getTypedPieces(Arcanum::Piece::W_BISHOP, Arcanum::Color::WHITE);
    while (whiteBishops) { indicies[idxCounter++] = findex(perspective, popLsb64(&whiteBishops), Arcanum::W_BISHOP, kingSquare); }
    Arcanum::bitboard_t blackBishops = board.getTypedPieces(Arcanum::Piece::W_BISHOP, Arcanum::Color::BLACK);
    while (blackBishops) { indicies[idxCounter++] = findex(perspective, popLsb64(&blackBishops), Arcanum::B_BISHOP, kingSquare); }

    // Queens
    Arcanum::bitboard_t whiteQueens = board.getTypedPieces(Arcanum::Piece::W_QUEEN, Arcanum::Color::WHITE);
    while (whiteQueens) { indicies[idxCounter++] = findex(perspective, popLsb64(&whiteQueens), Arcanum::W_QUEEN, kingSquare); }
    Arcanum::bitboard_t blackQueens = board.getTypedPieces(Arcanum::Piece::W_QUEEN, Arcanum::Color::BLACK);
    while (blackQueens) { indicies[idxCounter++] = findex(perspective, popLsb64(&blackQueens), Arcanum::B_QUEEN, kingSquare); }

    if(idxCounter > 30)
        ERROR("Too many pieces on the board")

    return idxCounter;
}

void NNUE::initializeAccumulator(Accumulator& accumulator, const Arcanum::Board& board)
{
    m_initializeAccumulatorPerspective(accumulator, Arcanum::Color::WHITE, board);
    m_initializeAccumulatorPerspective(accumulator, Arcanum::Color::BLACK, board);
}

void NNUE::m_initializeAccumulatorPerspective(Accumulator& accumulator, const Arcanum::Color perspective, const Arcanum::Board& board)
{
    constexpr uint32_t registerWidth = 256 / 16; // Number of int16_t in AVX2 register
    constexpr uint32_t numRegs = 256 / registerWidth;
    __m256i regs[numRegs]; // Temp accumulators in AVX register

    // Calculate the active feature indices
    std::array<uint32_t, 30> indicies;
    uint32_t numIndices = m_calculateActiveIndices(indicies, perspective, board);

    // Load bias to registers
    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.biases[i * registerWidth]));
    }

    // Add the active features
    for(uint32_t i = 0; i < numIndices; i++){
        uint32_t featureIndex = indicies[i];
        for(uint32_t j = 0; j < numRegs; j++)
        {
            regs[j] = _mm256_add_epi16(regs[j], _mm256_load_si256((__m256i*) ALIGN64(&m_featureTransformer.weights[featureIndex * 256 + j * registerWidth])));
        }
    }

    // Write the registers to the the accumulator
    for(uint32_t i = 0; i < numRegs; i++)
    {
        _mm256_store_si256((__m256i*)&accumulator.acc[perspective][i * registerWidth], regs[i]);
    }
}
 
void NNUE::m_loadHeader(std::ifstream& stream)
{
    LOG("NNUE Version:        " << ifstreamGet<uint32_t>(stream));
    LOG("NNUE Hash:           " << ifstreamGet<uint32_t>(stream));
    uint32_t descSize = ifstreamGet<uint32_t>(stream);
    LOG("Description Size:    " << descSize)

    std::string description;
    description.resize(descSize);
    stream.read(description.data(), descSize);
    LOG("Description : " << description)
}

void NNUE::m_loadWeights(std::ifstream& stream)
{
    // Allocate space for the transformer
    m_featureTransformer.biases  = static_cast<FTBias*>(Memory::alignedMalloc(ftOuts * sizeof(FTBias), 64));
    m_featureTransformer.weights = static_cast<FTWeight*>(Memory::alignedMalloc(ftIns * ftOuts * sizeof(FTWeight), 64));

    ifstreamGet<uint32_t>(stream); // Read unused hash
    // Read the transformer
    for(uint32_t i = 0; i < ftOuts; i++)
        m_featureTransformer.biases[i] = ifstreamGet<FTBias>(stream);
    for(uint32_t i = 0; i < ftIns*ftOuts; i++)
        m_featureTransformer.weights[i] = ifstreamGet<FTWeight>(stream);

    // Read network
    ifstreamGet<uint32_t>(stream); // Read unused hash
    for(uint32_t i = 0; i < l1outs; i++)
        m_hiddenLayer1.biases[i] = ifstreamGet<Bias>(stream);
    for(uint32_t r = 0; r < l1outs; r++)
        for(uint32_t c = 0; c < 2*ftOuts; c++)
            m_hiddenLayer1.weights[windex(r, c, 2*ftOuts)] = ifstreamGet<Weight>(stream);

    for(uint32_t i = 0; i < l2outs; i++)
        m_hiddenLayer2.biases[i] = ifstreamGet<Bias>(stream);
    for(uint32_t r = 0; r < l2outs; r++)
        for(uint32_t c = 0; c < l1outs; c++)
            m_hiddenLayer2.weights[windex(r, c, l1outs)] = ifstreamGet<Weight>(stream);
    
    for(uint32_t i = 0; i < lastouts; i++)
        m_outputLayer.biases[i] = ifstreamGet<Bias>(stream);
    for(uint32_t c = 0; c < l2outs; c++)
        m_outputLayer.weights[c] = ifstreamGet<Weight>(stream);

    m_permuteBiases(m_hiddenLayer1.biases);
    m_permuteBiases(m_hiddenLayer2.biases);
}

void NNUE::m_permuteBiases(Bias* biases)
{
    __m128i *b128 = (__m128i *)biases;
    __m128i tmp[8];
    tmp[0] = b128[0];
    tmp[1] = b128[4];
    tmp[2] = b128[1];
    tmp[3] = b128[5];
    tmp[4] = b128[2];
    tmp[5] = b128[6];
    tmp[6] = b128[3];
    tmp[7] = b128[7];
    memcpy(b128, tmp, 8 * sizeof(__m128i));
}