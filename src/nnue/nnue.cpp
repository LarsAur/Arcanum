#include <nnue/nnue.hpp>
#include <nnue/linalg.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <math.h>

using namespace NN;
using namespace Arcanum;

#define ALIGN64(p) __builtin_assume_aligned((p), 64)

NNUE::NNUE() : m_net(nullptr)
{}

NNUE::~NNUE()
{
    delete m_net;
    delete m_floatNet.ftWeights;
    delete m_floatNet.ftBiases;
    delete m_floatNet.l1Weights;
    delete m_floatNet.l1Biases;
    delete m_floatNet.l2Weights;
    delete m_floatNet.l2Biases;
    delete m_floatNet.l3Weights;
    delete m_floatNet.l3Bias;
}

void NNUE::load(std::string filename)
{
    if(m_net)
    {
        delete m_net;
        m_net = nullptr;
    }

    m_net = new Net;

    std::string path = getWorkPath();

    std::ifstream is(filename, std::ios::in | std::ios::binary);

    std::streampos bytesize = is.tellg();
    is.seekg(0, std::ios::end);
    bytesize = is.tellg() - bytesize;
    is.seekg(0);

    if(bytesize < 108068)
    {
        ERROR("Unsufficient number of bytes in file " << bytesize << " requires " << 108068)
        exit(-1);
    }

    LOG("Reading " << bytesize << " bytes")

    is.read((char*) m_net->ftWeights, 384 * 128 * sizeof(int16_t));
    is.read((char*) m_net->ftBiases,        128 * sizeof(int16_t));
    is.read((char*) m_net->l1Weights, 256 *  32 * sizeof(int8_t));
    is.read((char*) m_net->l1Biases,         32 * sizeof(int32_t));
    is.read((char*) m_net->l2Weights,  32 *  32 * sizeof(int8_t));
    is.read((char*) m_net->l2Biases,         32 * sizeof(int32_t));
    is.read((char*) m_net->l3Weights,        32 * sizeof(int8_t));
    is.read((char*) m_net->l3Bias,            1 * sizeof(int32_t));

    is.close();

    // #define RANDOM(_set, _size, _type) for(uint32_t i = 0; i < (_size); i++) _set[i] = 1;//_type(rand());

    // RANDOM(m_net->ftWeights, 384 * 128, int16_t);
    // RANDOM(m_net->ftBiases,        128, int16_t);
    // RANDOM(m_net->l1Weights, 256 *  32, int8_t);
    // RANDOM(m_net->l1Biases,         32, int32_t);
    // RANDOM(m_net->l2Weights,  32 *  32, int8_t);
    // RANDOM(m_net->l2Biases,         32, int32_t);
    // RANDOM(m_net->l3Weights,        32, int8_t);
    // RANDOM(m_net->l3Bias,            1, int32_t);

    // #undef RANDOM
}

void NNUE::setNet(Net& net)
{
    if(!m_net) m_net = new Net;
    memcpy(m_net->ftWeights, net.ftWeights, 384 * 128 * sizeof(int16_t));
    memcpy(m_net->ftBiases,  net.ftBiases,        128 * sizeof(int16_t));
    memcpy(m_net->l1Weights, net.l1Weights, 256 *  32 * sizeof(int8_t));
    memcpy(m_net->l1Biases,  net.l1Biases,         32 * sizeof(int32_t));
    memcpy(m_net->l2Weights, net.l2Weights,  32 *  32 * sizeof(int8_t));
    memcpy(m_net->l2Biases,  net.l2Biases,         32 * sizeof(int32_t));
    memcpy(m_net->l3Weights, net.l3Weights,        32 * sizeof(int8_t));
    memcpy(m_net->l3Bias,    net.l3Bias,            1 * sizeof(int32_t));
}

void NNUE::initializeAccumulator(Accumulator& acc, const Arcanum::Board& board)
{
    m_initializeAccumulatorBias(acc);

    m_numActiveIndices[WHITE] = 0;
    m_numActiveIndices[BLACK] = 0;

    for(uint32_t color = 0; color < 2; color++)
    {
        for(uint32_t type = 0; type < 6; type++)
        {
            Arcanum::bitboard_t pieces = board.getTypedPieces(Piece(type), Color(color));
            while(pieces)
            {
                square_t idx = popLS1B(&pieces);
                uint32_t findex = m_getFeatureIndex(idx, Color(color), Piece(type));
                m_activeIndices[color][m_numActiveIndices[color]++] = findex;
                m_setFeature(acc, findex, Color(color));
            }
        }
    }
}

void NNUE::incrementAccumulator(Accumulator& accIn, Accumulator& accOut, Arcanum::Color, const Arcanum::Board& board, const Arcanum::Move& move)
{

}

Arcanum::eval_t NNUE::evaluate(Accumulator& acc, Arcanum::Color turn)
{
    constexpr  uint32_t num256Chunks = 16 * 128 / 256;
    alignas(8) const uint32_t permuateIdx[8] = {0, 1, 4, 5, 2, 3, 6, 7};

    const __m256i zero = _mm256_setzero_si256();

    // Transform the accumulator to int8_t netdata
    // The data is transformed from a vector of 16-bit values to a vector of 8-bit values
    const Arcanum::Color perspectives[2] = {turn, Arcanum::Color(turn^1)};
    for(uint32_t i = 0; i < 2; i++)
    {
        const Arcanum::Color p = perspectives[i];
        for(uint32_t j = 0; j < num256Chunks / 2; j++)
        {
            uint32_t index = j + i*(num256Chunks/2);
            __m256i s0 = ((__m256i*) acc.acc[p])[j * 2];
            __m256i s1 = ((__m256i*) acc.acc[p])[j * 2 + 1];
            // Saturate8 (clamp) all int16_t values
            // The int8_t values are packed as follows
            // 8 x s0_0, 8 x s1_0, 8 x s0_1, 8 x s1_1
            __m256i packed = _mm256_packs_epi16(s0, s1);

            // Shuffle the values back into order
            // 8 x s0_0, 8 x s0_1, 8 x s1_0, 8 x s1_1
            __m256i shuffled =  _mm256_permutevar8x32_epi32(packed, _mm256_load_si256((__m256i*)permuateIdx));

            // Clamp the values between 0 and 127 (ReLU)
            shuffled = _mm256_max_epi8(shuffled, zero);

            _mm256_store_si256(&((__m256i*) m_clampedAcc)[index], shuffled);
        }
    }

    m_affineTransform<256, 32>(m_clampedAcc, m_hiddenOut1, m_net->l1Biases, m_net->l1Weights);
    m_affineTransform<32, 32>(m_hiddenOut1, m_hiddenOut2, m_net->l2Biases, m_net->l2Weights);

    int32_t score = m_affinePropagate(m_hiddenOut2, m_net->l3Bias, m_net->l3Weights);

    return turn == WHITE ? score : -score;
}

int32_t NNUE::m_affinePropagate(int8_t* input, int32_t* biases, int8_t* weights)
{
    __m256i *iv = (__m256i *)input;
    __m256i *row = (__m256i *)weights;
    __m256i prod = _mm256_maddubs_epi16(iv[0], row[0]);
    prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));
    __m128i sum = _mm_add_epi32(
        _mm256_castsi256_si128(prod), _mm256_extracti128_si256(prod, 1));
    sum = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x1b));
    return _mm_cvtsi128_si32(sum) + _mm_extract_epi32(sum, 1) + biases[0];
}

template<uint32_t dimIn, uint32_t dimOut>
void NNUE::m_affineTransform(int8_t *input, int8_t *output, int32_t* biases, int8_t* weights)
{
    constexpr uint32_t numRegs32 = 32 * dimOut / 256; // Number of 256 bit registers required to keep 32-bit data

    __m256i regs[numRegs32];

    // Load the biases into the registers
    for(uint32_t i = 0; i < numRegs32; i++)
    {
        regs[i] = _mm256_load_si256(((__m256i*)biases) + i);
    }

    const __m256i zero = _mm256_setzero_si256();

    for(uint32_t i = 0; i < dimIn / 2; i++)
    {
        // Load alternating input[2*i] and input[2*i + 1] into factor
        // This is done as _mm256_maddubs_epi16 is calculated as (a_1*b_1 + a_2*b_2)
        // Where a is the weights and b is input.
        uint16_t factors16 = uint16_t((input[2*i + 1] << 8) | input[2*i]);

        // Skip this calculation if the input is zero
        // These calculations have no effect on the result, and ReLU may clamp a lot of inputs to zero.
        if(factors16 == 0) continue;
        __m256i factor = _mm256_set1_epi16(factors16);

        for(uint32_t j = 0; j < dimOut / 32; j++)
        {
            // Load the weights
            __m256i weights1 = _mm256_load_si256((__m256i*) (weights + j * 32 + (2*i + 0) * dimOut));
            __m256i weights2 = _mm256_load_si256((__m256i*) (weights + j * 32 + (2*i + 1) * dimOut));

            // Alternate the weights
            __m256i weightPackLo = _mm256_unpacklo_epi8(weights1, weights2);
            __m256i weightPackHi = _mm256_unpackhi_epi8(weights1, weights2);

            // Calulate the products
            // Each prodXX contains 16 partial product sums
            __m256i prodLo = _mm256_maddubs_epi16(factor, weightPackLo);
            __m256i prodHi = _mm256_maddubs_epi16(factor, weightPackHi);

            // Sign extend these products to be 32-bit
            __m256i signs = _mm256_cmpgt_epi16(zero, prodLo);
            __m256i prodLoLo = _mm256_unpacklo_epi16(prodLo, signs);
            __m256i prodLoHi = _mm256_unpackhi_epi16(prodLo, signs);

                    signs = _mm256_cmpgt_epi16(zero, prodHi);
            __m256i prodHiLo = _mm256_unpacklo_epi16(prodHi, signs);
            __m256i prodHiHi = _mm256_unpackhi_epi16(prodHi, signs);

            // Add the 32-bit products to the 32-bit accumulator registers
            regs[4*j + 0] = _mm256_add_epi32(regs[4*j + 0], prodLoLo);
            regs[4*j + 1] = _mm256_add_epi32(regs[4*j + 1], prodLoHi);
            regs[4*j + 2] = _mm256_add_epi32(regs[4*j + 2], prodHiLo);
            regs[4*j + 3] = _mm256_add_epi32(regs[4*j + 3], prodHiHi);
        }
    }

    alignas(8) const uint32_t permuateIdx[8] = {0, 4, 1, 5, 2, 6, 3, 7};

    for(uint32_t j = 0; j < dimOut / 32; j++)
    {
        // Pack the 32-bit accumulator to 16-bit
        __m256i out16_0 = _mm256_packs_epi32(regs[4*j + 0], regs[4*j + 1]);
        __m256i out16_1 = _mm256_packs_epi32(regs[4*j + 2], regs[4*j + 3]);

        // Right shift by 6
        out16_0 = _mm256_srai_epi16(out16_0, 6);
        out16_1 = _mm256_srai_epi16(out16_1, 6);

        // Pack the 16-bit accumulator to 8-bit
        __m256i out8    = _mm256_packs_epi16(out16_0, out16_1);

        // Shuffle the result
        out8 = _mm256_permutevar8x32_epi32(out8, _mm256_load_si256((__m256i*)permuateIdx));

        // Clamp the signed 8-bit values between 0 and 127
                out8    = _mm256_max_epi8(out8, zero);

        // Store the output
        _mm256_store_si256((__m256i*)(output + j*32), out8);
    }
}

Arcanum::eval_t NNUE::evaluateBoard(const Arcanum::Board& board)
{
    Accumulator acc;
    initializeAccumulator(acc, board);
    return evaluate(acc, board.getTurn());
}

uint32_t NNUE::m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece)
{
    if(color == BLACK)
        square ^= 0x3F;

    return (uint32_t(piece) << 6) | uint32_t(square);
}

void NNUE::m_setFeature(Accumulator& acc, uint32_t findex, Arcanum::Color color)
{
    constexpr uint32_t numElementsInReg = 256 / 16; // Number of int16_t in (256 bit) AVX2 register
    constexpr uint32_t numRegs = 128 / numElementsInReg;
    __m256i reg; // Temp accumulators in AVX register
    for(uint32_t i = 0; i < numRegs; i++)
    {
        reg = _mm256_load_si256((__m256i*) &acc.acc[color][i * numElementsInReg]);
        reg = _mm256_add_epi16(reg, _mm256_load_si256((__m256i*) &(m_net->ftWeights[findex * 128 + i * numElementsInReg])));
        _mm256_store_si256((__m256i*) &acc.acc[color][i * numElementsInReg], reg);
    }
}

void NNUE::m_clearFeature(Accumulator& acc, uint32_t findex, Arcanum::Color color)
{
    constexpr uint32_t numElementsInReg = 256 / 16; // Number of int16_t in (256 bit) AVX2 register
    constexpr uint32_t numRegs = 128 / numElementsInReg;
    __m256i reg; // Temp accumulators in AVX register

    for(uint32_t i = 0; i < numRegs; i++)
    {
        reg = _mm256_load_si256((__m256i*) &acc.acc[color][i * numElementsInReg]);
        reg = _mm256_sub_epi16(reg, _mm256_load_si256((__m256i*) &(m_net->ftWeights[findex * 128 + i * numElementsInReg])));
        _mm256_store_si256((__m256i*) &acc.acc[color][i * numElementsInReg], reg);
    }
}

void NNUE::m_initializeAccumulatorBias(Accumulator& acc)
{
    constexpr uint32_t numElementsInReg = 256 / 16; // Number of int16_t in (256 bit) AVX2 register
    constexpr uint32_t numRegs = 128 / numElementsInReg;
    __m256i reg; // Temp accumulators in AVX register

    for(uint32_t i = 0; i < numRegs; i++)
    {
        reg = _mm256_load_si256((__m256i*) &m_net->ftBiases[i * numElementsInReg]);
        _mm256_store_si256((__m256i*) &acc.acc[WHITE][i * numElementsInReg], reg);
        _mm256_store_si256((__m256i*) &acc.acc[BLACK][i * numElementsInReg], reg);
    }
}

void NNUE::quantize()
{
    #define QUANTIZE(_w, _s, _t) for(uint32_t i = 0; i < _s; i++) m_net->_w[i] = static_cast<_t>(std::lroundf(m_floatNet._w->data()[i])); //static_cast<_t>(m_floatNet._w->data()[i]);

    QUANTIZE(ftWeights, 384 * 128, int16_t)
    QUANTIZE(ftBiases,  128,       int16_t)
    QUANTIZE(l1Weights, 256 * 32,  int8_t)
    QUANTIZE(l1Biases,  32,        int32_t)
    QUANTIZE(l2Weights, 32 * 32,   int8_t)
    QUANTIZE(l2Biases,  32,        int32_t)
    QUANTIZE(l3Weights, 32,        int8_t)
    QUANTIZE(l3Bias,    1,         int32_t)

    #undef QUANTIZE
}

void NNUE::initBackPropagate()
{
    m_floatNet.ftWeights  = new Matrixf(128, 384);
    m_floatNet.ftBiases   = new Matrixf(128, 1);
    m_floatNet.l1Weights  = new Matrixf(32, 256);
    m_floatNet.l1Biases   = new Matrixf(32, 1);
    m_floatNet.l2Weights  = new Matrixf(32, 32);
    m_floatNet.l2Biases   = new Matrixf(32, 1);
    m_floatNet.l3Weights  = new Matrixf(1, 32);
    m_floatNet.l3Bias     = new Matrixf(1, 1);

    m_floatNet.ftWeights ->randomize(0.1f, 1.0f);
    m_floatNet.ftBiases  ->randomize(0.1f, 1.0f);
    m_floatNet.l1Weights ->randomize(0.1f, 1.0f);
    m_floatNet.l1Biases  ->randomize(0.1f, 1.0f);
    m_floatNet.l2Weights ->randomize(0.1f, 1.0f);
    m_floatNet.l2Biases  ->randomize(0.1f, 1.0f);
    m_floatNet.l3Weights ->randomize(-1.0f, 1.0f);
    m_floatNet.l3Bias    ->randomize(-1.0f, 1.0f);
}

// http://neuralnetworksanddeeplearning.com/chap2.html
void NNUE::backPropagate(Arcanum::Board& board, float result)
{
    constexpr float rate = 0.01f;
    constexpr float e = 2.71828182846f;

    // -- Set inputs

    // Generate the feature set
    Accumulator acc;
    initializeAccumulator(acc, board);

    Matrixf input1(384, 1);
    Matrixf input2(384, 1);

    uint32_t color = board.getTurn();
    for(uint32_t k = 0; k < m_numActiveIndices[color]; k++)
    {
        uint32_t j = m_activeIndices[color][k];
        input1.set(j, 0, 1.0f);
    }

    for(uint32_t k = 0; k < m_numActiveIndices[color^1]; k++)
    {
        uint32_t j = m_activeIndices[color][k];
        input2.set(j, 0, 1.0f);
    }

    // -- Forward feeding
    Matrixf accumulator1(128, 1);
    Matrixf accumulator2(128, 1);
    Matrixf accumulator(256, 1);
    Matrixf hiddenOut1(32, 1);
    Matrixf hiddenOut2(32, 1);
    Matrixf output(1, 1);

    m_floatNet.ftWeights->multiply(input1, accumulator1);
    m_floatNet.ftWeights->multiply(input2, accumulator2);
    accumulator1.add(*m_floatNet.ftBiases);
    accumulator2.add(*m_floatNet.ftBiases);

    // Concatinate the accumulators
    for(uint32_t i = 0; i < 128; i++)
    {
        accumulator.data()[i] = accumulator1.data()[i];
        accumulator.data()[i + 128] = accumulator2.data()[i];
    }
    accumulator.reluClamp();

    m_floatNet.l1Weights->multiply(accumulator, hiddenOut1);
    hiddenOut1.add(*m_floatNet.l1Biases);
    hiddenOut1.scale(1 / 64.0f); // Scale to account for right bitshift of 6
    hiddenOut1.reluClamp();

    m_floatNet.l2Weights->multiply(hiddenOut1, hiddenOut2);
    hiddenOut2.add(*m_floatNet.l2Biases);
    hiddenOut2.scale(1 / 64.0f); // Scale to account for right bitshift of 6
    hiddenOut2.reluClamp();

    m_floatNet.l3Weights->multiply(hiddenOut2, output);
    output.add(*m_floatNet.l3Bias);

    float score = *output.data();
    float sigmoid = 1.0f / (1.0f + pow(e, -score / 200.0f));
    float error = pow(result - sigmoid, 2);
    // Derivative of error
    float derror = -((result - 1) * pow(e, score / 100.0f) + result * pow(e, score / 200.0f)) / (100.0f * pow(pow(e, score / 200.0f) + 1, 3));

    DEBUG("Error = " << error << " DError = " << derror << " Expected = " << result << " Output = " << score)

    // -- Calculation of auxillery coefficients

    Matrixf delta4(1, 1);
    Matrixf delta3(32, 1);
    Matrixf delta2(32, 1);
    Matrixf delta1(256, 1);

    Matrixf delta1_1(128, 1);
    Matrixf delta1_2(128, 1);

    Matrixf hiddenOut2ReLuPrime     (32, 1);
    Matrixf hiddenOut1ReLuPrime     (32, 1);
    Matrixf accumulatorReLuPrime    (256, 1);

    hiddenOut2ReLuPrime.add(hiddenOut2);
    hiddenOut1ReLuPrime.add(hiddenOut1);
    accumulatorReLuPrime.add(accumulator);

    hiddenOut2ReLuPrime.reluPrime();
    hiddenOut1ReLuPrime.reluPrime();
    accumulatorReLuPrime.reluPrime();

    delta4.set(0, 0, -derror); // Negaitve because we are minimizing

    m_floatNet.l3Weights->transpose();
    m_floatNet.l3Weights->multiply(delta4, delta3);
    m_floatNet.l3Weights->transpose();
    delta3.hadamard(hiddenOut2ReLuPrime);

    m_floatNet.l2Weights->transpose();
    m_floatNet.l2Weights->multiply(delta3, delta2);
    m_floatNet.l2Weights->transpose();
    delta2.hadamard(hiddenOut1ReLuPrime);

    m_floatNet.l1Weights->transpose();
    m_floatNet.l1Weights->multiply(delta2, delta1);
    m_floatNet.l1Weights->transpose();
    delta1.hadamard(accumulatorReLuPrime);

    // Split the accumulator
    for(uint32_t i = 0; i < 128; i++)
    {
        delta1_1.set(i, 0, delta1.data()[i]);
        delta1_2.set(i, 0, delta1.data()[i + 128]);
    }

    // -- Calculation of gradient

    Matrixf nablaFtWeights1 (128, 384);
    Matrixf nablaFtWeights2 (128, 384);
    Matrixf nablaFtBiases1  (128, 1);
    Matrixf nablaFtBiases2  (128, 1);
    Matrixf nablaL1Weights (32, 256);
    Matrixf nablaL1Biases  (32, 1);
    Matrixf nablaL2Weights (32, 32);
    Matrixf nablaL2Biases  (32, 1);
    Matrixf nablaL3Weights (1, 32);
    Matrixf nablaL3Bias    (1, 1);

    hiddenOut2.transpose();
    hiddenOut1.transpose();
    accumulator.transpose();

    delta4.multiply(hiddenOut2, nablaL3Weights);
    nablaL3Weights.scale(rate * 64.0f);
    nablaL3Bias.add(delta4);
    nablaL3Bias.scale(rate * 64.0f);

    delta3.multiply(hiddenOut1, nablaL2Weights);
    nablaL2Weights.scale(rate);
    nablaL2Biases.add(delta3);
    nablaL2Biases.scale(rate);

    delta2.multiply(accumulator, nablaL1Weights);
    nablaL1Weights.scale(rate);
    nablaL1Biases.add(delta2);
    nablaL1Biases.scale(rate);

    input1.transpose();
    input2.transpose();

    delta1_1.multiply(input1, nablaFtWeights1);
    nablaFtWeights1.scale(rate * 0.5f);
    nablaFtBiases1.add(delta1_1);
    nablaFtBiases1.scale(rate * 0.5f);

    delta1_2.multiply(input2, nablaFtWeights2);
    nablaFtWeights2.scale(rate * 0.5f);
    nablaFtBiases2.add(delta1_2);
    nablaFtBiases2.scale(rate * 0.5f);

    m_floatNet.ftWeights->add(nablaFtWeights1);
    m_floatNet.ftWeights->add(nablaFtWeights2);
    m_floatNet.ftBiases->add(nablaFtBiases1);
    m_floatNet.ftBiases->add(nablaFtBiases2);

    m_floatNet.l1Weights->add(nablaL1Weights);
    m_floatNet.l2Weights->add(nablaL2Weights);
    m_floatNet.l3Weights->add(nablaL3Weights);

    m_floatNet.l1Biases->add(nablaL1Biases);
    m_floatNet.l2Biases->add(nablaL2Biases);
    m_floatNet.l3Bias->add(nablaL3Bias);
}