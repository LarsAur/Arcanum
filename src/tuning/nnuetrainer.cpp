#include <tuning/nnuetrainer.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <math.h>
#include <eval.hpp>

using namespace Arcanum;

#define NET_BINARY_OP(_net1, _op, _net2) \
_net1.ftWeights._op(_net2.ftWeights); \
_net1.ftBiases ._op(_net2.ftBiases ); \
_net1.l1Weights._op(_net2.l1Weights); \
_net1.l1Biases ._op(_net2.l1Biases ); \
_net1.l2Weights._op(_net2.l2Weights); \
_net1.l2Biases ._op(_net2.l2Biases );

#define NET_UNARY_OP(_net1, _op) \
_net1.ftWeights._op; \
_net1.ftBiases ._op; \
_net1.l1Weights._op; \
_net1.l1Biases ._op; \
_net1.l2Weights._op; \
_net1.l2Biases ._op;

const char* NNUETrainer::NNUE_MAGIC = "Arcanum FNNUE";

void NNUETrainer::load(std::string filename)
{
    m_loadNet(filename, m_net);
}

void NNUETrainer::m_loadNet(std::string filename, Net& net)
{
    std::string path = getWorkPath();
    std::stringstream ss;
    ss << path << filename;
    std::ifstream fStream(ss.str(), std::ios::in | std::ios::binary);

    LOG("Loading FNNUE " << ss.str())
    if(!fStream.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    std::istream& stream = fStream;
    m_loadNetFromStream(stream, net);
    fStream.close();

    LOG("Finished loading FNNUE " << ss.str())
}

void NNUETrainer::m_loadNetFromStream(std::istream& stream, Net& net)
{
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
}

void NNUETrainer::store(std::string filename)
{
    m_storeNet(filename, m_net);
}

void NNUETrainer::m_storeNet(std::string filename, Net& net)
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

void NNUETrainer::m_findFeatureSet(const Board& board, NNUE::FeatureSet& featureSet)
{
    Color perspective = board.getTurn();
    featureSet.numFeatures = 0;
    for(uint32_t color = 0; color < 2; color++)
    {
        for(uint32_t type = 0; type < 6; type++)
        {
            bitboard_t pieces = board.getTypedPieces(Piece(type), Color(color));
            while(pieces)
            {
                square_t idx = popLS1B(&pieces);
                uint32_t findex = NNUE::getFeatureIndex(idx, Color(color), Piece(type), perspective);
                featureSet.features[featureSet.numFeatures++] = findex;
            }
        }
    }
}

void NNUETrainer::m_initAccumulator(const Board& board)
{
    NNUE::FeatureSet featureSet;
    m_findFeatureSet(board, featureSet);
    float* accPtr = m_trace.acc.data();

    constexpr uint32_t numRegs = L1Size / RegSize;
    __m256 regs[numRegs];

    float* biasesPtr         = m_net.ftBiases.data();
    float* weightsPtr        = m_net.ftWeights.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_ps(biasesPtr + RegSize*i);
    }

    for(uint32_t i = 0; i < featureSet.numFeatures; i++)
    {
        uint32_t findex = featureSet.features[i];
        for(uint32_t j = 0; j < numRegs; j++)
        {
            __m256 weights = _mm256_load_ps(weightsPtr + RegSize*j + findex*RegSize*numRegs);
            regs[j] = _mm256_add_ps(regs[j], weights);
        }
    }

    for(uint32_t i = 0; i < numRegs; i++)
    {
        _mm256_store_ps(accPtr + RegSize*i, regs[i]);
    }
}

void NNUETrainer::randomizeNet()
{
    LOG("Randomizing NNUETrainer")
    m_net.ftWeights.heRandomize();
    m_net.l1Weights.heRandomize();
    m_net.l2Weights.heRandomize();
    m_net.ftBiases.setZero();
    m_net.l1Biases.setZero();
    m_net.l2Biases.setZero();
}

float NNUETrainer::m_predict(const Board& board)
{
    m_initAccumulator(board);
    m_trace.acc.relu();
    feedForwardReLu(m_net.l1Weights, m_net.l1Biases, m_trace.acc, m_trace.l1Out);
    lastLevelFeedForward(m_net.l2Weights, m_net.l2Biases, m_trace.l1Out, m_trace.out);
    return *m_trace.out.data();
}

constexpr float SIGMOID_FACTOR = 200.0f;
inline float NNUETrainer::m_sigmoid(float v)
{
    constexpr float e = 2.71828182846f;
    return 1.0f / (1.0f + pow(e, -v / SIGMOID_FACTOR));
}

inline float NNUETrainer::m_sigmoidPrime(float sigmoid)
{
    // Calculate derivative of sigmoid based on the sigmoid value
    // f'(x) = f(x) * (1 - f(x)) / SIG_FACTOR
    return ((sigmoid) * (1.0f - (sigmoid))) / SIGMOID_FACTOR;
}

// http://neuralnetworksanddeeplearning.com/chap2.html
void NNUETrainer::m_backPropagate(const Board& board, float cpTarget, DataParser::Result result, float& totalLoss)
{
    constexpr float lambda = 0.50f; // Weighting between wdlTarget and cpTarget

    // -- Run prediction
    float out = m_predict(board);

    // Set Win-Draw-Loss target based on result
    // Normalize from [-1, 1] to [0, 1]
    float wdlTarget = (result + 1.0f) / 2.0f;

    // Correct target perspective
    if(board.getTurn() == BLACK)
    {
        wdlTarget = 1.0f - wdlTarget;
    }

    // Calculate target
    float wdlOutput       = m_sigmoid(out);
    float wdlTargetCp     = m_sigmoid(cpTarget);
    float target          = wdlTargetCp * lambda + wdlTarget * (1.0f - lambda);

    // Calculate loss
    float loss            = pow(target - wdlOutput, 2);
    totalLoss             += loss;

    // Calculate loss gradients
    float sigmoidPrime    = m_sigmoidPrime(wdlOutput);
    // Note: The loss gradient should be -2 * (target - wdlOutput),
    //       but the minus is ommitted and the gradient is later added instead of subtracted in m_applyGradient
    float lossPrime       = 2 * (target - wdlOutput);

    // -- Create input vector
    NNUE::FeatureSet featureSet;
    m_findFeatureSet(board, featureSet);

    // -- Calculation of auxillery coefficients
    Matrix<L1Size, 1> delta1(false);
    Matrix<L2Size, 1> delta2(false);
    Matrix<1, 1>      delta3(false);

    // Calculate derivative of activation functions (Sigma prime)
    Matrix<L2Size, 1> L2ReLuPrime(false);
    L2ReLuPrime.copy(m_trace.l1Out);
    L2ReLuPrime.reluPrime();

    Matrix<L1Size, 1> accumulatorReLuPrime(false);
    accumulatorReLuPrime.copy(m_trace.acc);
    accumulatorReLuPrime.reluPrime();

    // Calculate deltas (d_l = W_l+1^T * d_l+1) * sigma prime (Z_l)

    delta3.set(0, 0, sigmoidPrime * lossPrime);

    multiplyTransposeA(m_net.l2Weights, delta3, delta2);
    delta2.hadamard(L2ReLuPrime);

    multiplyTransposeA(m_net.l1Weights, delta2, delta1);
    delta1.hadamard(accumulatorReLuPrime);

    // Calculation of gradient

    Matrix<1, L2Size>        gradientL2Weights(false);
    Matrix<L2Size, L1Size>   gradientL1Weights(false);

    multiplyTransposeB(delta3, m_trace.l1Out, gradientL2Weights);
    multiplyTransposeB(delta2, m_trace.acc, gradientL1Weights);
    calcAndAccFtGradient(featureSet, delta1, m_gradient.ftWeights);

    // Accumulate the change

    m_gradient.l2Biases.add(delta3);
    m_gradient.l1Biases.add(delta2);
    m_gradient.ftBiases.add(delta1);
    m_gradient.l1Weights.add(gradientL1Weights);
    m_gradient.l2Weights.add(gradientL2Weights);
}

void NNUETrainer::m_applyGradient(uint32_t timestep)
{
    // ADAM Optimizer: https://arxiv.org/pdf/1412.6980.pdf
    constexpr float alpha   = 0.01f;
    constexpr float beta1   = 0.9f;
    constexpr float beta2   = 0.999f;
    constexpr float epsilon = 1.0E-8;

    // M_t = B1 * M_t-1 + (1 - B1) * g_t

    NET_UNARY_OP(m_moments.m, scale(beta1 / (1.0f - beta1)))
    NET_BINARY_OP(m_moments.m, add, m_gradient)
    NET_UNARY_OP(m_moments.m, scale(1.0f - beta1))

    // v_t = B2 * v_t-1 + (1 - B2) * g_t^2

    NET_UNARY_OP(m_moments.v, scale(beta2))
    NET_UNARY_OP(m_gradient, pow2())
    NET_UNARY_OP(m_gradient, scale(1.0f - beta2))
    NET_BINARY_OP(m_moments.v, add, m_gradient)

    // M^_t = alpha * M_t / (1 - Beta1^t)

    NET_BINARY_OP(m_moments.mHat, copy, m_moments.m)
    NET_UNARY_OP(m_moments.mHat, scale(alpha / (1.0f - std::pow(beta1, timestep))))

    // v^_t = v_t / (1 - Beta2^t)

    NET_BINARY_OP(m_moments.vHat, copy, m_moments.v)
    NET_UNARY_OP(m_moments.vHat, scale(1.0f / (1.0f - std::pow(beta2, timestep))))

    // sqrt(v^_t) + epsilon

    NET_UNARY_OP(m_moments.vHat, sqrt())
    NET_UNARY_OP(m_moments.vHat, addScalar(epsilon))

    // net = net + M^_t / (sqrt(v^_t) + epsilon)

    NET_BINARY_OP(m_moments.mHat, hadamardInverse, m_moments.vHat)
    NET_BINARY_OP(m_net, add, m_moments.mHat)
}

void NNUETrainer::train(std::string dataset, std::string outputPath, uint64_t batchSize, uint32_t startEpoch, uint32_t endEpoch)
{
    // Initialize the gradients and ADAM moments
    NET_UNARY_OP(m_gradient, setZero())
    NET_UNARY_OP(m_moments.m, setZero())
    NET_UNARY_OP(m_moments.v, setZero())
    NET_UNARY_OP(m_moments.mHat, setZero())
    NET_UNARY_OP(m_moments.vHat, setZero())

    for(uint32_t epoch = startEpoch; epoch < endEpoch; epoch++)
    {
        DataLoader loader;
        if(!loader.open(dataset))
        {
            ERROR("Unable to open dataset " << dataset)
            return;
        }

        uint64_t epochPosCount = 0LL;
        uint64_t batchPosCount = 0LL;
        float epochLoss = 0.0f;
        float batchLoss = 0.0f;

        // Clear the gradient at the start of the epoch
        NET_UNARY_OP(m_gradient, setZero())

        while (!loader.eof())
        {
            Board *board = loader.getNextBoard();
            eval_t cp = loader.getScore();
            DataParser::Result result = loader.getResult();

            // Run back propagation
            m_backPropagate(*board, cp, result, batchLoss);

            batchPosCount++;

            if((batchPosCount % batchSize == 0) || loader.eof())
            {
                NET_UNARY_OP(m_gradient, scale(1.0f / batchPosCount))

                m_applyGradient(epoch);

                NET_UNARY_OP(m_gradient, setZero())

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
        loader.close();
    }
}