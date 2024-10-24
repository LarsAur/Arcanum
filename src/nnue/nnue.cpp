#include <nnue/nnue.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <math.h>
#include <eval.hpp>

using namespace NN;
using namespace Arcanum;

const char* NNUE::NNUE_MAGIC = "Arcanum FNNUE";

NNUE::NNUE()
{

}

NNUE::~NNUE()
{

}

void NNUE::load(std::string filename)
{
    m_loadNet(filename, m_net);
}

void NNUE::m_loadNet(std::string filename, FloatNet& net)
{
    std::string path = getWorkPath();
    std::stringstream ss;
    ss << path << filename;
    std::ifstream stream(ss.str(), std::ios::in | std::ios::binary);

    LOG("Loading NNUE " << ss.str())
    if(!stream.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    // Reading header

    std::string magic;
    std::string metadata;
    uint32_t size;

    magic.resize(strlen(NNUE_MAGIC));
    stream.read(magic.data(), strlen(NNUE_MAGIC));

    if(magic != NNUE_MAGIC)
    {
        ERROR("Mismatching NNUE magic" << magic << " != " << NNUE_MAGIC);
        return;
    }

    stream.read((char*) &size, sizeof(uint32_t));

    metadata.resize(size);
    stream.read(metadata.data(), size);

    DEBUG("Magic:" << magic)
    DEBUG("Metadata:\n" << metadata)

    // Read Net data

    net.ftWeights.readFromStream(stream);
    net.ftBiases.readFromStream(stream);
    net.l1Weights.readFromStream(stream);
    net.l1Biases.readFromStream(stream);
    net.l2Weights.readFromStream(stream);
    net.l2Biases.readFromStream(stream);

    stream.close();

    LOG("Finished loading NNUE " << ss.str())
}

void NNUE::store(std::string filename)
{
    m_storeNet(filename, m_net);
}

void NNUE::m_storeNet(std::string filename, FloatNet& net)
{
    std::string path = getWorkPath();

    std::stringstream ss;
    ss << path << filename;
    LOG("Storing nnue in " << ss.str())
    std::ofstream stream(ss.str(), std::ios::out | std::ios::binary);

    if(!stream.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    // Write header
    time_t now = time(0);
    tm *gmt = gmtime(&now);
    std::string utcstr = asctime(gmt);
    std::string arch = "768->256->32->1";

    std::string metadata = utcstr + arch;
    uint32_t size = metadata.size();

    stream.write(NNUE_MAGIC, strlen(NNUE_MAGIC));
    stream.write((char*) &size, sizeof(uint32_t));
    stream.write(metadata.c_str(), size);

    // Write Net data
    net.ftWeights.writeToStream(stream);
    net.ftBiases.writeToStream(stream);
    net.l1Weights.writeToStream(stream);
    net.l1Biases.writeToStream(stream);
    net.l2Weights.writeToStream(stream);
    net.l2Biases.writeToStream(stream);

    stream.close();

    LOG("Finished storing nnue in " << ss.str())
}

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
inline uint32_t NNUE::m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece)
{
    if(color == BLACK)
        square = ((7 - RANK(square)) << 3) | FILE(square);

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
    constexpr uint32_t numRegs = L1Size / RegSize;
    __m256 regs[numRegs];

    float* biasesPtr         = m_net.ftBiases.data();
    float* weightsPtr        = m_net.ftWeights.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_ps(biasesPtr + RegSize*i);
    }

    for(uint32_t i = 0; i < numFeatures; i++)
    {
        // XOR to the the correct index for the perspective
        uint32_t findex = features[i] ^ perspective;

        for(uint32_t j = 0; j < numRegs; j++)
        {
            __m256 weights = _mm256_load_ps(weightsPtr + RegSize*j + findex*RegSize*numRegs);
            regs[j] = _mm256_add_ps(regs[j], weights);
        }
    }

    for(uint32_t i = 0; i < numRegs; i++)
    {
        _mm256_store_ps(acc->acc[perspective] + RegSize*i, regs[i]);
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
            removedFeatures[1] = m_getFeatureIndex(Square::A1, WHITE, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(Square::D1, WHITE, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_KING)
        {
            removedFeatures[1] = m_getFeatureIndex(Square::H1, WHITE, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(Square::F1, WHITE, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_QUEEN)
        {
            removedFeatures[1] = m_getFeatureIndex(Square::A8, BLACK, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(Square::D8, BLACK, W_ROOK);
        }
        else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_KING)
        {
            removedFeatures[1] = m_getFeatureIndex(Square::H8, BLACK, W_ROOK);
            addedFeatures[1]   = m_getFeatureIndex(Square::F8, BLACK, W_ROOK);
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

    // -- Prefetch the weigths
    #pragma GCC unroll 2
    for(uint32_t perspective = 0; perspective < 2; perspective++)
    {
        for(uint32_t i = 0; i < 2; i++)
        {
            int32_t findex = addedFeatures[i];
            // Break in case only one index is added
            if(findex == -1) break;
            // XOR to the the correct index for the perspective
            findex ^= perspective;
            m_net.ftWeights.prefetchCol(findex);
        }

        for(uint32_t i = 0; i < 2; i++)
        {
            int32_t findex = removedFeatures[i];
            // Break in case only one index is added
            if(findex == -1) break;
            // XOR to the the correct index for the perspective
            findex ^= perspective;
            m_net.ftWeights.prefetchCol(findex);
        }
    }

    // -- Update the accumulators

    constexpr uint32_t numRegs = L1Size / RegSize;
    __m256 regs[numRegs];

    float* weightsPtr        = m_net.ftWeights.data();

    #pragma GCC unroll 2
    for(uint32_t perspective = 0; perspective < 2; perspective++)
    {
        // -- Load the accumulator into the registers
        #pragma GCC unroll 32
        for(uint32_t i = 0; i < numRegs; i++)
        {
            regs[i] = _mm256_load_ps(accIn->acc[perspective] + RegSize*i);
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
                __m256 weights = _mm256_load_ps(weightsPtr + RegSize*j + findex*RegSize*numRegs);
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
                __m256 weights = _mm256_load_ps(weightsPtr + RegSize*j + findex*RegSize*numRegs);
                regs[j] = _mm256_sub_ps(regs[j], weights);
            }
        }

        // -- Store the output in the new accumulator
        #pragma GCC unroll 32
        for(uint32_t i = 0; i < numRegs; i++)
        {
            _mm256_store_ps(accOut->acc[perspective] + RegSize*i, regs[i]);
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
    LOG("Randomizing NNUE")
    m_net.ftWeights.heRandomize();
    m_net.l1Weights.heRandomize();
    m_net.l2Weights.heRandomize();
    m_net.ftBiases.setZero();
    m_net.l1Biases.setZero();
    m_net.l2Biases.setZero();
}

void NNUE::m_reluAccumulator(Accumulator* acc, Arcanum::Color perspective, Trace& trace)
{
    constexpr uint32_t numRegs = L1Size / RegSize;
    __m256 zero = _mm256_setzero_ps();
    float* dst = trace.accumulator.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        // Load accumulator
        __m256 a = _mm256_load_ps(acc->acc[perspective] + RegSize*i);
        // ReLu
        a = _mm256_max_ps(zero, a);
        // Score accumulator in the trace
        _mm256_store_ps(dst + RegSize*i, a);
    }
}

float NNUE::m_predict(Accumulator* acc, Arcanum::Color perspective, Trace& trace)
{
    m_reluAccumulator(acc, perspective, trace);
    feedForwardReLu(m_net.l1Weights, m_net.l1Biases, trace.accumulator, trace.l1Out);
    lastLevelFeedForward(m_net.l2Weights, m_net.l2Biases, trace.l1Out, trace.out);
    return *trace.out.data();
}

float NNUE::m_predict(Accumulator* acc, Arcanum::Color perspective)
{
    return m_predict(acc, perspective, m_trace);
}

// http://neuralnetworksanddeeplearning.com/chap2.html
void NNUE::m_backPropagate(const Arcanum::Board& board, float cpTarget, float wdlTarget, FloatNet& gradient, float& totalLoss, FloatNet& net, Trace& trace)
{
    constexpr float lambda = 0.50f; // Weighting between wdlTarget and cpTarget
    constexpr float e = 2.71828182846f;
    constexpr float SIG_FACTOR = 200.0f;

    #define SIGMOID(_v) (1.0f / (1.0f + pow(e, -(_v) / SIG_FACTOR)))

    // Calculate derivative of sigmoid based on the sigmoid value
    // f'(x) = f(x) * (1 - f(x)) * SIG_FACTOR
    #define SIGMOID_PRIME(_sigmoid) ((_sigmoid) * (1.0f - (_sigmoid)))

    // -- Run prediction
    Accumulator acc;
    initAccumulator(&acc, board);
    float out = m_predict(&acc, board.getTurn(), trace);

    // Correct target perspective
    if(board.getTurn() == BLACK)
    {
        cpTarget = -cpTarget;
        wdlTarget = 1.0f - wdlTarget;
    }

    // Calculate target
    float wdlOutput       = SIGMOID(out);
    float wdlTargetCp     = SIGMOID(cpTarget);
    float target          = wdlTargetCp * lambda + wdlTarget * (1.0f - lambda);

    // Calculate loss
    float loss            = pow(target - wdlOutput, 2);
    totalLoss             += loss;

    // Calculate loss gradients
    float sigmoidPrime    = SIGMOID_PRIME(wdlOutput);
    float lossPrime       = 2 * (target - wdlOutput);

    // -- Create input vector
    uint8_t numFeatures;
    uint32_t features[32];
    m_calculateFeatures(board, &numFeatures, features);
    for(uint32_t k = 0; k < numFeatures; k++)
    {
        // XOR to make the correct the perspective
        features[k] = features[k] ^ board.getTurn();
    }

    // -- Calculation of auxillery coefficients
    Matrix<L1Size, 1> delta1(false);
    Matrix<L2Size, 1> delta2(false);
    Matrix<1, 1>      delta3(false);

    // Calculate derivative of activation functions (Sigma prime)
    Matrix<L2Size, 1> L2ReLuPrime(false);
    L2ReLuPrime.copy(trace.l1Out);
    L2ReLuPrime.reluPrime();

    Matrix<L1Size, 1> accumulatorReLuPrime(false);
    accumulatorReLuPrime.copy(trace.accumulator);
    accumulatorReLuPrime.reluPrime();

    // Calculate deltas (d_l = W_l+1^T * d_l+1) * sigma prime (Z_l)

    delta3.set(0, 0, sigmoidPrime * lossPrime);

    multiplyTransposeA(net.l2Weights, delta3, delta2);
    delta2.hadamard(L2ReLuPrime);

    multiplyTransposeA(net.l1Weights, delta2, delta1);
    delta1.hadamard(accumulatorReLuPrime);

    // Calculation of gradient

    Matrix<1, L2Size>        gradientL2Weights(false);
    Matrix<L2Size, L1Size>   gradientL1Weights(false);

    multiplyTransposeB(delta3, trace.l1Out,       gradientL2Weights);
    multiplyTransposeB(delta2, trace.accumulator, gradientL1Weights);
    calcAndAccFtGradient(numFeatures, features, delta1, gradient.ftWeights);

    // Accumulate the change

    gradient.l2Biases.add(delta3);
    gradient.l1Biases.add(delta2);
    gradient.ftBiases.add(delta1);
    gradient.l1Weights.add(gradientL1Weights);
    gradient.l2Weights.add(gradientL2Weights);
}

void NNUE::m_applyGradient(uint32_t timestep, FloatNet& gradient, FloatNet& momentum1, FloatNet& momentum2, FloatNet& mHat, FloatNet& vHat)
{
    // ADAM Optimizer: https://arxiv.org/pdf/1412.6980.pdf
    constexpr float alpha   = 0.01f;
    constexpr float beta1   = 0.9f;
    constexpr float beta2   = 0.999f;
    constexpr float epsilon = 1.0E-8;

    // M_t = B1 * M_t-1 + (1 - B1) * g_t

    momentum1.ftWeights .scale(beta1 / (1.0f - beta1));
    momentum1.ftBiases  .scale(beta1 / (1.0f - beta1));
    momentum1.l1Weights .scale(beta1 / (1.0f - beta1));
    momentum1.l1Biases  .scale(beta1 / (1.0f - beta1));
    momentum1.l2Weights .scale(beta1 / (1.0f - beta1));
    momentum1.l2Biases  .scale(beta1 / (1.0f - beta1));

    momentum1.ftWeights .add(gradient.ftWeights );
    momentum1.ftBiases  .add(gradient.ftBiases  );
    momentum1.l1Weights .add(gradient.l1Weights );
    momentum1.l1Biases  .add(gradient.l1Biases  );
    momentum1.l2Weights .add(gradient.l2Weights );
    momentum1.l2Biases  .add(gradient.l2Biases  );

    momentum1.ftWeights .scale(1.0f - beta1);
    momentum1.ftBiases  .scale(1.0f - beta1);
    momentum1.l1Weights .scale(1.0f - beta1);
    momentum1.l1Biases  .scale(1.0f - beta1);
    momentum1.l2Weights .scale(1.0f - beta1);
    momentum1.l2Biases  .scale(1.0f - beta1);

    // v_t = B2 * v_t-1 + (1 - B2) * g_t^2

    momentum2.ftWeights .scale(beta2);
    momentum2.ftBiases  .scale(beta2);
    momentum2.l1Weights .scale(beta2);
    momentum2.l1Biases  .scale(beta2);
    momentum2.l2Weights .scale(beta2);
    momentum2.l2Biases  .scale(beta2);

    gradient.ftWeights .pow2();
    gradient.ftBiases  .pow2();
    gradient.l1Weights .pow2();
    gradient.l1Biases  .pow2();
    gradient.l2Weights .pow2();
    gradient.l2Biases  .pow2();

    gradient.ftWeights .scale(1.0f - beta2);
    gradient.ftBiases  .scale(1.0f - beta2);
    gradient.l1Weights .scale(1.0f - beta2);
    gradient.l1Biases  .scale(1.0f - beta2);
    gradient.l2Weights .scale(1.0f - beta2);
    gradient.l2Biases  .scale(1.0f - beta2);

    momentum2.ftWeights .add(gradient.ftWeights);
    momentum2.ftBiases  .add(gradient.ftBiases );
    momentum2.l1Weights .add(gradient.l1Weights);
    momentum2.l1Biases  .add(gradient.l1Biases );
    momentum2.l2Weights .add(gradient.l2Weights);
    momentum2.l2Biases  .add(gradient.l2Biases );

    // M^_t = alpha * M_t / (1 - Beta1^t)

    mHat.ftWeights .copy(momentum1.ftWeights);
    mHat.ftBiases  .copy(momentum1.ftBiases );
    mHat.l1Weights .copy(momentum1.l1Weights);
    mHat.l1Biases  .copy(momentum1.l1Biases );
    mHat.l2Weights .copy(momentum1.l2Weights);
    mHat.l2Biases  .copy(momentum1.l2Biases );

    mHat.ftWeights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    mHat.ftBiases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    mHat.l1Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    mHat.l1Biases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    mHat.l2Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    mHat.l2Biases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));

    // v^_t = v_t / (1 - Beta2^t)

    vHat.ftWeights .copy(momentum2.ftWeights);
    vHat.ftBiases  .copy(momentum2.ftBiases );
    vHat.l1Weights .copy(momentum2.l1Weights);
    vHat.l1Biases  .copy(momentum2.l1Biases );
    vHat.l2Weights .copy(momentum2.l2Weights);
    vHat.l2Biases  .copy(momentum2.l2Biases );

    vHat.ftWeights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    vHat.ftBiases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    vHat.l1Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    vHat.l1Biases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    vHat.l2Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    vHat.l2Biases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));

    // sqrt(v^_t) + epsilon

    vHat.ftWeights.sqrt();
    vHat.ftBiases .sqrt();
    vHat.l1Weights.sqrt();
    vHat.l1Biases .sqrt();
    vHat.l2Weights.sqrt();
    vHat.l2Biases .sqrt();

    vHat.ftWeights.addScalar(epsilon);
    vHat.ftBiases .addScalar(epsilon);
    vHat.l1Weights.addScalar(epsilon);
    vHat.l1Biases .addScalar(epsilon);
    vHat.l2Weights.addScalar(epsilon);
    vHat.l2Biases .addScalar(epsilon);

    // Note: Addition instead of subtraction because gradient it is already negated
    // net = net + M^_t / (sqrt(v^_t) + epsilon)

    mHat.ftWeights .hadamardInverse(vHat.ftWeights);
    mHat.ftBiases  .hadamardInverse(vHat.ftBiases );
    mHat.l1Weights .hadamardInverse(vHat.l1Weights);
    mHat.l1Biases  .hadamardInverse(vHat.l1Biases );
    mHat.l2Weights .hadamardInverse(vHat.l2Weights);
    mHat.l2Biases  .hadamardInverse(vHat.l2Biases );

    m_net.ftWeights .add(mHat.ftWeights);
    m_net.ftBiases  .add(mHat.ftBiases );
    m_net.l1Weights .add(mHat.l1Weights);
    m_net.l1Biases  .add(mHat.l1Biases );
    m_net.l2Weights .add(mHat.l2Weights);
    m_net.l2Biases  .add(mHat.l2Biases );
}

void NNUE::train(std::string dataset, std::string outputPath, uint64_t batchSize, uint32_t startEpoch, uint32_t endEpoch, bool randomize)
{
    FloatNet gradient;
    Trace trace;

    FloatNet momentum1;
    FloatNet momentum2;
    FloatNet mHat;
    FloatNet vHat;

    std::string strWdl;
    std::string strCp;
    std::string fen;

    if(randomize) m_randomizeWeights();

    for(uint32_t epoch = startEpoch; epoch < endEpoch; epoch++)
    {
        std::ifstream is(dataset, std::ios::in);

        if(!is.is_open())
        {
            ERROR("Unable to open " << dataset)
            exit(-1);
        }

        uint64_t epochPosCount = 0LL;
        uint64_t batchPosCount = 0LL;
        float epochLoss = 0.0f;
        float batchLoss = 0.0f;

        gradient.ftWeights.setZero();
        gradient.ftBiases .setZero();
        gradient.l1Weights.setZero();
        gradient.l1Biases .setZero();
        gradient.l2Weights.setZero();
        gradient.l2Biases .setZero();

        while (!is.eof())
        {
            std::getline(is, strWdl);
            std::getline(is, strCp);
            std::getline(is, fen);

            if(fen == "")
                continue;

            // Convert strings to floats and board
            // Normalize the result from [-1, 1] to [0, 1]
            float wdl = (atof(strWdl.c_str()) + 1) / 2.0f;
            float cp = atof(strCp.c_str());
            Arcanum::Board board = Arcanum::Board(fen);

            // Run back propagation
            m_backPropagate(board, cp, wdl, gradient, batchLoss, m_net, trace);

            batchPosCount++;

            if((batchPosCount % batchSize == 0) || is.eof())
            {
                gradient.ftWeights .scale(1.0f / batchPosCount);
                gradient.ftBiases  .scale(1.0f / batchPosCount);
                gradient.l1Weights .scale(1.0f / batchPosCount);
                gradient.l1Biases  .scale(1.0f / batchPosCount);
                gradient.l2Weights .scale(1.0f / batchPosCount);
                gradient.l2Biases  .scale(1.0f / batchPosCount);

                m_applyGradient(epoch, gradient, momentum1, momentum2, mHat, vHat);

                gradient.ftWeights .setZero();
                gradient.ftBiases  .setZero();
                gradient.l1Weights .setZero();
                gradient.l1Biases  .setZero();
                gradient.l2Weights .setZero();
                gradient.l2Biases  .setZero();

                // Aggregate the loss and position count
                epochPosCount += batchPosCount;
                epochLoss     += batchLoss;

                LOG("Avg. Batch Loss = " << std::fixed << std::setprecision(6) << (batchLoss / batchPosCount)
                << " Avg. Epoch Loss = " << std::setprecision(6)               << (epochLoss / epochPosCount)
                << " #Positions = " << epochPosCount)

                batchPosCount = 0;
                batchLoss = 0.0f;
            }
        }

        // Write the epoch loss to a file
        std::ofstream os("loss.log", std::ios::app | std::ios::out);
        std::stringstream ssLoss;
        ssLoss << std::fixed << std::setprecision(6) << (epochLoss / float(epochPosCount)) << "\n";
        os.write(ssLoss.str().c_str(), ssLoss.str().length());
        os.close();

        // Store the net for each epoch
        std::stringstream ssNnueName;
        ssNnueName << outputPath << epoch << ".fnnue";
        store(ssNnueName.str());
        is.close();

        m_test();
    }

}

void NNUE::m_test()
{
    Board b = Board(FEN::startpos);
    eval_t score = evaluateBoard(b);
    Board b1 = Board("1nb1kbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQ - 0 1");
    eval_t score1 = evaluateBoard(b1);
    Board b2 = Board("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/1NB1KBN1 w kq - 0 1");
    eval_t score2 = evaluateBoard(b2);

    LOG("Score (=) = " << score << " Score (+) = " << score1 << " Score (-) = " << score2)
}