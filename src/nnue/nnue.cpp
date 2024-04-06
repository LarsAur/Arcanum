#include <nnue/nnue.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <math.h>
#include <eval.hpp>
#include <thread>
#include <mutex>

using namespace NN;
using namespace Arcanum;

NNUE::NNUE()
{
    m_randomizeWeights();
}

NNUE::~NNUE()
{

}

void NNUE::load(std::string filename)
{
    std::string path = getWorkPath();
    std::stringstream ss;
    ss << path << filename << ".fnnue";
    std::ifstream is(ss.str(), std::ios::in | std::ios::binary);

    LOG("Loading NNUE " << ss.str())
    if(!is.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    is.read((char*) m_net.ftWeights.data(), 768 * 128 * sizeof(float));
    is.read((char*) m_net.ftBiases.data(),        128 * sizeof(float));
    is.read((char*) m_net.l1Weights.data(), 128 *  16 * sizeof(float));
    is.read((char*) m_net.l1Biases.data(),         16 * sizeof(float));
    is.read((char*) m_net.l2Weights.data(),  16 *  16 * sizeof(float));
    is.read((char*) m_net.l2Biases.data(),         16 * sizeof(float));
    is.read((char*) m_net.l3Weights.data(),        16 * sizeof(float));
    is.read((char*) m_net.l3Bias.data(),            1 * sizeof(float));

    is.close();

    LOG("Finished loading NNUE " << ss.str())
}

void NNUE::store(std::string filename)
{
    std::string path = getWorkPath();

    std::stringstream ss;
    ss << path << filename << ".fnnue";
    LOG("Storing nnue in " << ss.str())
    std::ofstream fstream(ss.str(), std::ios::out | std::ios::binary);

    if(!fstream.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    fstream.write((char*) m_net.ftWeights.data(), 768 * 128 * sizeof(float));
    fstream.write((char*) m_net.ftBiases.data(),        128 * sizeof(float));
    fstream.write((char*) m_net.l1Weights.data(), 128 *  16 * sizeof(float));
    fstream.write((char*) m_net.l1Biases.data(),         16 * sizeof(float));
    fstream.write((char*) m_net.l2Weights.data(),  16 *  16 * sizeof(float));
    fstream.write((char*) m_net.l2Biases.data(),         16 * sizeof(float));
    fstream.write((char*) m_net.l3Weights.data(),        16 * sizeof(float));
    fstream.write((char*) m_net.l3Bias.data(),            1 * sizeof(float));

    fstream.close();

    LOG("Finished storing nnue in " << ss.str())
}

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
inline uint32_t NNUE::m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece)
{
    if(color == BLACK)
        square ^= 0x3F;

    return (((uint32_t(piece) << 6) | uint32_t(square)) << 1) | color;
}

void NNUE::m_calculateFeatures(const Arcanum::Board& board, uint8_t* numFeatures, uint32_t* features)
{
    *numFeatures = 0;
    for(uint32_t color = 0; color < 2; color++)
    {
        for(uint32_t type = 0; type < 6; type++)
        {
            Arcanum::bitboard_t pieces = board.getTypedPieces(Piece(type), Color(color));
            while(pieces)
            {
                square_t idx = popLS1B(&pieces);
                uint32_t findex = m_getFeatureIndex(idx, Color(color), Piece(type));
                features[(*numFeatures)++] = findex;
            }
        }
    }
}

void NNUE::m_initAccumulatorPerspective(Accumulator* acc, Arcanum::Color perspective, uint8_t numFeatures, uint32_t* features)
{
    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 128 / regSize;
    __m256 regs[numRegs];

    float* biasesPtr         = m_net.ftBiases.data();
    float* weightsPtr        = m_net.ftWeights.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_ps(biasesPtr + regSize*i);
    }

    for(uint32_t i = 0; i < numFeatures; i++)
    {
        // XOR to the the correct index for the perspective
        uint32_t findex = features[i] ^ perspective;

        for(uint32_t j = 0; j < numRegs; j++)
        {
            __m256 weights = _mm256_load_ps(weightsPtr + regSize*j + findex*regSize*numRegs);
            regs[j] = _mm256_add_ps(regs[j], weights);
        }
    }

    for(uint32_t i = 0; i < numRegs; i++)
    {
        _mm256_store_ps(acc->acc[perspective] + regSize*i, regs[i]);
    }
}

void NNUE::initAccumulator(Accumulator* acc, const Arcanum::Board& board)
{
    uint8_t numFeatures;
    uint32_t features[32];
    m_calculateFeatures(board, &numFeatures, features);
    m_initAccumulatorPerspective(acc, Color::WHITE, numFeatures, features);
    m_initAccumulatorPerspective(acc, Color::BLACK, numFeatures, features);
}

void NNUE::incAccumulator(Accumulator* accIn, Accumulator* accOut, const Arcanum::Board& board, const Arcanum::Move& move)
{
    int32_t removedFeatures[2] = {-1, -1};
    int32_t addedFeatures[2] = {-1, -1};

    Color opponent    = board.getTurn();
    Color movingColor = Color(board.getTurn() ^ 1); // Accumulator increment is performed after move is performed

    // -- Find the added and removed indices

    Piece movedType = Piece(LS1B(MOVED_PIECE(move.moveInfo)));
    removedFeatures[0] = m_getFeatureIndex(move.from, movingColor, movedType);

    if(PROMOTED_PIECE(move.moveInfo))
    {
        Piece promoteType = Piece(LS1B(PROMOTED_PIECE(move.moveInfo)) - 11);
        addedFeatures[0] = m_getFeatureIndex(move.to, movingColor, promoteType);
    }
    else
    {
        addedFeatures[0] = m_getFeatureIndex(move.to, movingColor, movedType);
    }

    // Handle the moved rook when castling
    if(CASTLE_SIDE(move.moveInfo))
    {
        if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_QUEEN)
        {
            removedFeatures[1] = m_getFeatureIndex(0, WHITE, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(3, WHITE, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_KING)
        {
            removedFeatures[1] = m_getFeatureIndex(7, WHITE, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(5, WHITE, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_QUEEN)
        {
            removedFeatures[1] = m_getFeatureIndex(56, BLACK, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(59, BLACK, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_KING)
        {
            removedFeatures[1] = m_getFeatureIndex(63, BLACK, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(61, BLACK, W_ROOK);
        }
    }

    if(CAPTURED_PIECE(move.moveInfo))
    {
        if(move.moveInfo & MoveInfoBit::ENPASSANT)
        {
            removedFeatures[1] = m_getFeatureIndex(movingColor == WHITE ? (move.to - 8) : (move.to + 8), opponent, W_PAWN);
        }
        else
        {
            Piece captureType = Piece(LS1B(CAPTURED_PIECE(move.moveInfo)) - 16);
            removedFeatures[1] = m_getFeatureIndex(move.to, opponent, captureType);
        }
    }

    // -- Update the accumulators

    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 128 / regSize;
    __m256 regs[numRegs];

    float* weightsPtr        = m_net.ftWeights.data();

    for(uint32_t perspective = 0; perspective < 2; perspective++)
    {
        // -- Load the accumulator into the registers
        for(uint32_t i = 0; i < numRegs; i++)
        {
            regs[i] = _mm256_load_ps(accIn->acc[perspective] + regSize*i);
        }

        // -- Added features
        for(uint32_t i = 0; i < 2; i++)
        {
            int32_t findex = addedFeatures[i];
            // Break in case only one index is added
            if(findex == -1) break;
            // XOR to the the correct index for the perspective
            findex ^= perspective;

            for(uint32_t j = 0; j < numRegs; j++)
            {
                __m256 weights = _mm256_load_ps(weightsPtr + regSize*j + findex*regSize*numRegs);
                regs[j] = _mm256_add_ps(regs[j], weights);
            }
        }

        // -- Removed features
        for(uint32_t i = 0; i < 2; i++)
        {
            int32_t findex = removedFeatures[i];
            // Break in case only one index is added
            if(findex == -1) break;
            // XOR to the the correct index for the perspective
            findex ^= perspective;

            for(uint32_t j = 0; j < numRegs; j++)
            {
                __m256 weights = _mm256_load_ps(weightsPtr + regSize*j + findex*regSize*numRegs);
                regs[j] = _mm256_sub_ps(regs[j], weights);
            }
        }

        // -- Store the output in the new accumulator
        for(uint32_t i = 0; i < numRegs; i++)
        {
            _mm256_store_ps(accOut->acc[perspective] + regSize*i, regs[i]);
        }
    }
}

Arcanum::eval_t NNUE::evaluateBoard(const Arcanum::Board& board)
{
    Accumulator acc;
    initAccumulator(&acc, board);
    return static_cast<eval_t>(m_predict(&acc, board.getTurn()));
}

Arcanum::eval_t NNUE::evaluate(Accumulator* acc, Arcanum::Color turn)
{
    return static_cast<eval_t>(m_predict(acc, turn));
}

void NNUE::m_randomizeWeights()
{
    m_net.ftWeights.randomize(-1.0f, 1.0f);
    m_net.l1Weights.randomize(-1.0f, 1.0f);
    m_net.l2Weights.randomize(-1.0f, 1.0f);
    m_net.l3Weights.randomize(-1.0f, 1.0f);
}

void NNUE::m_reluAccumulator(Accumulator* acc, Arcanum::Color perspective, Trace& trace)
{
    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 128 / regSize;
    __m256 zero = _mm256_setzero_ps();
    float* dst = trace.accumulator.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        // Load accumulator
        __m256 a = _mm256_load_ps(acc->acc[perspective] + regSize*i);
        // ReLu
        a = _mm256_max_ps(zero, a);
        // Score accumulator in the trace
        _mm256_store_ps(dst + regSize*i, a);
    }
}


float NNUE::m_predict(Accumulator* acc, Arcanum::Color perspective, Trace& trace)
{
    m_reluAccumulator(acc, perspective, trace);
    feedForwardReLu<128, 16>(m_net.l1Weights, m_net.l1Biases, trace.accumulator, trace.hiddenOut1);
    feedForwardReLu<16, 16>(m_net.l2Weights, m_net.l2Biases, trace.hiddenOut1, trace.hiddenOut2);
    lastLevelFeedForward<16>(m_net.l3Weights, m_net.l3Bias, trace.hiddenOut2, trace.out);
    return *trace.out.data();
}

float NNUE::m_predict(Accumulator* acc, Arcanum::Color perspective)
{
    return m_predict(acc, perspective, m_trace);
}

// http://neuralnetworksanddeeplearning.com/chap2.html
void NNUE::m_backPropagate(const Arcanum::Board& board, float cpTarget, FloatNet& gradient, float& totalError, FloatNet& net, Trace& trace)
{
    constexpr float e = 2.71828182846f;
    constexpr float SIG_FACTOR = 400.0f;

    // -- Run prediction
    Accumulator acc;
    initAccumulator(&acc, board);
    float out = m_predict(&acc, board.getTurn(), trace);

    // Correct target perspective
    if(board.getTurn() == BLACK)
        cpTarget = -cpTarget;

    // -- Error Calculation
    float sigmoid       = 1.0f / (1.0f + pow(e, -out / SIG_FACTOR));
    float sigmoidPrime  = sigmoid * (1.0f - sigmoid) * SIG_FACTOR;
    float sigmoidTarget = 1.0f / (1.0f + pow(e, -cpTarget / SIG_FACTOR));

    float error =       pow(sigmoidTarget - sigmoid, 2);
    float errorPrime =  2 * (sigmoidTarget - sigmoid);
    totalError += error;

    // -- Create input vector
    uint8_t numFeatures;
    uint32_t features[32];
    m_calculateFeatures(board, &numFeatures, features);
    trace.input.setZero();
    for(uint32_t k = 0; k < numFeatures; k++)
    {
        // XOR to make the correct the perspective
        uint32_t j = features[k] ^ board.getTurn();
        trace.input.set(j, 0, 1.0f);
    }

    // -- Calculation of auxillery coefficients
    Matrix<1, 1> delta4;
    Matrix<16, 1> delta3;
    Matrix<16, 1> delta2;
    Matrix<128, 1> delta1;

    Matrix<16, 1> hiddenOut2ReLuPrime;
    Matrix<16, 1> hiddenOut1ReLuPrime;
    Matrix<128, 1> accumulatorReLuPrime;

    hiddenOut2ReLuPrime.add(trace.hiddenOut2);
    hiddenOut1ReLuPrime.add(trace.hiddenOut1);
    accumulatorReLuPrime.add(trace.accumulator);

    hiddenOut2ReLuPrime.reluPrime();
    hiddenOut1ReLuPrime.reluPrime();
    accumulatorReLuPrime.reluPrime();

    delta4.set(0, 0, sigmoidPrime * errorPrime);

    multiplyTransposeA(net.l3Weights, delta4, delta3);
    // net.l3Weights.transposeMultiply(delta4, delta3);
    delta3.hadamard(hiddenOut2ReLuPrime);

    multiplyTransposeA(net.l2Weights, delta3, delta2);
    // net.l2Weights.transposeMultiply(delta3, delta2);
    delta2.hadamard(hiddenOut1ReLuPrime);

    multiplyTransposeA(net.l1Weights, delta2, delta1);
    // net.l1Weights.transposeMultiply(delta2, delta1);
    delta1.hadamard(accumulatorReLuPrime);

    // -- Calculation of gradient

    Matrix<128, 768> gradientFtWeights;
    Matrix<128, 1>  gradientFtBiases;
    Matrix<16, 128> gradientL1Weights;
    Matrix<16, 1>   gradientL1Biases;
    Matrix<16, 16>  gradientL2Weights;
    Matrix<16, 1>   gradientL2Biases;
    Matrix<1, 16>   gradientL3Weights;
    Matrix<1, 1>    gradientL3Bias;

    multiplyTransposeB(delta4, trace.hiddenOut2, gradientL3Weights);
    // delta4.transposeMultiply(trace.hiddenOut2, gradientL3Weights);
    gradientL3Bias.add(delta4);

    multiplyTransposeB(delta3, trace.hiddenOut1, gradientL2Weights);
    // delta3.transposeMultiply(trace.hiddenOut1, gradientL2Weights);
    gradientL2Biases.add(delta3);

    multiplyTransposeB(delta2, trace.accumulator, gradientL1Weights);
    // delta2.transposeMultiply(trace.accumulator, gradientL1Weights);
    gradientL1Biases.add(delta2);

    vectorMultTransposedSparseVector(delta1, trace.input, gradientFtWeights);
    gradientFtBiases.add(delta1);

    // Accumulate the change

    gradient.ftWeights.add(gradientFtWeights);
    gradient.ftBiases.add(gradientFtBiases);

    gradient.l1Weights.add(gradientL1Weights);
    gradient.l2Weights.add(gradientL2Weights);
    gradient.l3Weights.add(gradientL3Weights);

    gradient.l1Biases.add(gradientL1Biases);
    gradient.l2Biases.add(gradientL2Biases);
    gradient.l3Bias.add(gradientL3Bias);
}

void NNUE::m_applyGradient(uint32_t timestep, FloatNet& gradient, FloatNet& momentum1, FloatNet& momentum2)
{
    // ADAM Optimizer: https://arxiv.org/pdf/1412.6980.pdf
    constexpr float alpha   = 0.001f;
    constexpr float beta1   = 0.9f;
    constexpr float beta2   = 0.999f;
    constexpr float epsilon = 1.0E-8;

    FloatNet m_hat;
    FloatNet v_hat;

    // M_t = B1 * M_t-1 + (1 - B1) * g_t

    momentum1.ftWeights .scale(beta1 / (1.0f - beta1));
    momentum1.ftBiases  .scale(beta1 / (1.0f - beta1));
    momentum1.l1Weights .scale(beta1 / (1.0f - beta1));
    momentum1.l2Weights .scale(beta1 / (1.0f - beta1));
    momentum1.l3Weights .scale(beta1 / (1.0f - beta1));
    momentum1.l1Biases  .scale(beta1 / (1.0f - beta1));
    momentum1.l2Biases  .scale(beta1 / (1.0f - beta1));
    momentum1.l3Bias    .scale(beta1 / (1.0f - beta1));

    momentum1.ftWeights .add(gradient.ftWeights );
    momentum1.ftBiases  .add(gradient.ftBiases  );
    momentum1.l1Weights .add(gradient.l1Weights );
    momentum1.l2Weights .add(gradient.l2Weights );
    momentum1.l3Weights .add(gradient.l3Weights );
    momentum1.l1Biases  .add(gradient.l1Biases  );
    momentum1.l2Biases  .add(gradient.l2Biases  );
    momentum1.l3Bias    .add(gradient.l3Bias    );

    momentum1.ftWeights .scale((1.0f - beta1));
    momentum1.ftBiases  .scale((1.0f - beta1));
    momentum1.l1Weights .scale((1.0f - beta1));
    momentum1.l2Weights .scale((1.0f - beta1));
    momentum1.l3Weights .scale((1.0f - beta1));
    momentum1.l1Biases  .scale((1.0f - beta1));
    momentum1.l2Biases  .scale((1.0f - beta1));
    momentum1.l3Bias    .scale((1.0f - beta1));

    // v_t = B2 * v_t-1 + (1 - B2) * g_t^2

    momentum2.ftWeights .scale(beta2);
    momentum2.ftBiases  .scale(beta2);
    momentum2.l1Weights .scale(beta2);
    momentum2.l2Weights .scale(beta2);
    momentum2.l3Weights .scale(beta2);
    momentum2.l1Biases  .scale(beta2);
    momentum2.l2Biases  .scale(beta2);
    momentum2.l3Bias    .scale(beta2);

    gradient.ftWeights .pow(2);
    gradient.ftBiases  .pow(2);
    gradient.l1Weights .pow(2);
    gradient.l2Weights .pow(2);
    gradient.l3Weights .pow(2);
    gradient.l1Biases  .pow(2);
    gradient.l2Biases  .pow(2);
    gradient.l3Bias    .pow(2);

    gradient.ftWeights .scale(1.0f - beta2);
    gradient.ftBiases  .scale(1.0f - beta2);
    gradient.l1Weights .scale(1.0f - beta2);
    gradient.l2Weights .scale(1.0f - beta2);
    gradient.l3Weights .scale(1.0f - beta2);
    gradient.l1Biases  .scale(1.0f - beta2);
    gradient.l2Biases  .scale(1.0f - beta2);
    gradient.l3Bias    .scale(1.0f - beta2);

    momentum2.ftWeights .add(gradient.ftWeights);
    momentum2.ftBiases  .add(gradient.ftBiases );
    momentum2.l1Weights .add(gradient.l1Weights);
    momentum2.l2Weights .add(gradient.l2Weights);
    momentum2.l3Weights .add(gradient.l3Weights);
    momentum2.l1Biases  .add(gradient.l1Biases );
    momentum2.l2Biases  .add(gradient.l2Biases );
    momentum2.l3Bias    .add(gradient.l3Bias   );

    // M^_t = alpha * M_t / (1 - Beta1^t)

    m_hat.ftWeights .add(momentum1.ftWeights);
    m_hat.ftBiases  .add(momentum1.ftBiases );
    m_hat.l1Weights .add(momentum1.l1Weights);
    m_hat.l2Weights .add(momentum1.l2Weights);
    m_hat.l3Weights .add(momentum1.l3Weights);
    m_hat.l1Biases  .add(momentum1.l1Biases );
    m_hat.l2Biases  .add(momentum1.l2Biases );
    m_hat.l3Bias    .add(momentum1.l3Bias   );

    m_hat.ftWeights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.ftBiases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l1Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l2Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l3Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l1Biases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l2Biases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l3Bias    .scale(alpha / (1.0f - std::pow(beta1, timestep)));

    // v^_t = v_t / (1 - Beta2^t)

    v_hat.ftWeights .add(momentum2.ftWeights);
    v_hat.ftBiases  .add(momentum2.ftBiases );
    v_hat.l1Weights .add(momentum2.l1Weights);
    v_hat.l2Weights .add(momentum2.l2Weights);
    v_hat.l3Weights .add(momentum2.l3Weights);
    v_hat.l1Biases  .add(momentum2.l1Biases );
    v_hat.l2Biases  .add(momentum2.l2Biases );
    v_hat.l3Bias    .add(momentum2.l3Bias   );

    v_hat.ftWeights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.ftBiases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l1Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l2Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l3Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l1Biases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l2Biases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l3Bias    .scale(1.0f / (1.0f - std::pow(beta2, timestep)));

    // sqrt(v^_t) + epsilon

    v_hat.ftWeights.pow(0.5f);
    v_hat.ftBiases .pow(0.5f);
    v_hat.l1Weights.pow(0.5f);
    v_hat.l2Weights.pow(0.5f);
    v_hat.l3Weights.pow(0.5f);
    v_hat.l1Biases .pow(0.5f);
    v_hat.l2Biases .pow(0.5f);
    v_hat.l3Bias   .pow(0.5f);

    v_hat.ftWeights.addScalar(epsilon);
    v_hat.ftBiases .addScalar(epsilon);
    v_hat.l1Weights.addScalar(epsilon);
    v_hat.l2Weights.addScalar(epsilon);
    v_hat.l3Weights.addScalar(epsilon);
    v_hat.l1Biases .addScalar(epsilon);
    v_hat.l2Biases .addScalar(epsilon);
    v_hat.l3Bias   .addScalar(epsilon);

    // Note: Addition instead of subtraction because gradient it is already negated
    // net = net + M^_t / (sqrt(v^_t) + epsilon)

    m_hat.ftWeights .hadamardInverse(v_hat.ftWeights);
    m_hat.ftBiases  .hadamardInverse(v_hat.ftBiases );
    m_hat.l1Weights .hadamardInverse(v_hat.l1Weights);
    m_hat.l2Weights .hadamardInverse(v_hat.l2Weights);
    m_hat.l3Weights .hadamardInverse(v_hat.l3Weights);
    m_hat.l1Biases  .hadamardInverse(v_hat.l1Biases );
    m_hat.l2Biases  .hadamardInverse(v_hat.l2Biases );
    m_hat.l3Bias    .hadamardInverse(v_hat.l3Bias   );

    m_net.ftWeights .add(m_hat.ftWeights);
    m_net.ftBiases  .add(m_hat.ftBiases );
    m_net.l1Weights .add(m_hat.l1Weights);
    m_net.l2Weights .add(m_hat.l2Weights);
    m_net.l3Weights .add(m_hat.l3Weights);
    m_net.l1Biases  .add(m_hat.l1Biases );
    m_net.l2Biases  .add(m_hat.l2Biases );
    m_net.l3Bias    .add(m_hat.l3Bias   );
}

void NNUE::train(uint32_t epochs, uint32_t batchSize, std::string dataset)
{
    constexpr uint8_t numThreads = 1;
    std::thread threads[numThreads];
    FloatNet gradients[numThreads];
    Trace traces[numThreads];
    float errors[numThreads];
    uint64_t counts[numThreads];
    std::mutex mtx = std::mutex();

    FloatNet momentum1;
    FloatNet momentum2;

    for(uint32_t epoch = 121; epoch < 256; epoch++)
    {
        std::ifstream is(dataset, std::ios::in);

        if(!is.is_open())
        {
            ERROR("Unable to open " << dataset)
            exit(-1);
        }

        auto loop = [&](uint8_t id)
        {
            while (true)
            {
                // Acc mutex
                mtx.lock();
                if(is.eof())
                {
                    mtx.unlock();
                    break;
                }

                std::string strCp;
                std::string fen;
                std::getline(is, strCp);
                std::getline(is, fen);
                mtx.unlock();

                float cp = atof(strCp.c_str());
                Arcanum::Board board = Arcanum::Board(fen);

                // The dataset contains some variants of chess
                // where positions can have more than 32 pieces.
                // These have to be filtered out
                if(CNTSBITS(board.getColoredPieces(WHITE) | board.getColoredPieces(BLACK)) > 32 ||
                CNTSBITS(board.getTypedPieces(W_PAWN, WHITE) | board.getTypedPieces(W_PAWN, BLACK)) > 16)
                    continue;

                m_backPropagate(board, cp, gradients[id], errors[id], m_net, traces[id]);

                counts[id] += 1;

                if((id == 0) && (counts[id] % batchSize == 0))
                    DEBUG("Epoch Size = " << counts[id] << " Avg. Error = " << errors[id] / counts[id])
            }
        };

        for(uint8_t i = 0; i < numThreads; i++)
        {
            gradients[i].ftWeights.setZero();
            gradients[i].ftBiases .setZero();
            gradients[i].l1Weights.setZero();
            gradients[i].l1Biases .setZero();
            gradients[i].l2Weights.setZero();
            gradients[i].l2Biases .setZero();
            gradients[i].l3Weights.setZero();
            gradients[i].l3Bias   .setZero();

            errors[i] = 0.0f;
            counts[i] = 0LL;

            DEBUG("Start" << unsigned(i))
            threads[i] = std::thread(loop, i);
        }


        uint64_t epochCount = 0LL;
        float epochError = 0;
        for(uint8_t i = 0; i < numThreads; i++)
        {
            threads[i].join();
            DEBUG("Join" << unsigned(i))

            epochCount += counts[i];
            epochError += errors[i];
        }

        for(uint8_t i = 1; i < numThreads; i++)
        {
            gradients[0].ftWeights .add(gradients[i].ftWeights);
            gradients[0].ftBiases  .add(gradients[i].ftBiases );
            gradients[0].l1Weights .add(gradients[i].l1Weights);
            gradients[0].l1Biases  .add(gradients[i].l1Biases );
            gradients[0].l2Weights .add(gradients[i].l2Weights);
            gradients[0].l2Biases  .add(gradients[i].l2Biases );
            gradients[0].l3Weights .add(gradients[i].l3Weights);
            gradients[0].l3Bias    .add(gradients[i].l3Bias   );
        }

        gradients[0].ftWeights .scale(1.0f / epochCount);
        gradients[0].ftBiases  .scale(1.0f / epochCount);
        gradients[0].l1Weights .scale(1.0f / epochCount);
        gradients[0].l1Biases  .scale(1.0f / epochCount);
        gradients[0].l2Weights .scale(1.0f / epochCount);
        gradients[0].l2Biases  .scale(1.0f / epochCount);
        gradients[0].l3Weights .scale(1.0f / epochCount);
        gradients[0].l3Bias    .scale(1.0f / epochCount);

        m_applyGradient(epoch, gradients[0], momentum1, momentum2);

        DEBUG("Avg. Error = " << (epochError / epochCount))

        std::ofstream ofstream("nnue_error.log", std::ios::app);
        std::stringstream filess;
        filess << (epochError / epochCount) << "\n";
        ofstream.write(filess.str().data(), filess.str().length());
        ofstream.close();

        std::stringstream ss;
        ss << "../nnue/test" << epoch;
        store(ss.str());
        is.close();
        m_test();
    }

}

void NNUE::m_test()
{
    Board b = Board(Arcanum::startFEN);
    eval_t score = evaluateBoard(b);
    Board b1 = Board("1nb1kbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1");
    eval_t score1 = evaluateBoard(b1);
    Board b2 = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/1NB1KBN1 w kq - 0 1");
    eval_t score2 = evaluateBoard(b2);

    LOG("Score (=) = " << score << "Score (+) = " << score1 << "Score (-) = " << score2)
}