#include <tuning/nnuetrainer.hpp>
#include <tuning/nnueformat.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <math.h>
#include <eval.hpp>
#include <timer.hpp>

using namespace Arcanum;

#define NET_BINARY_OP(_net1, _op, _net2) \
_net1.ftWeights._op(_net2.ftWeights); \
_net1.ftBiases ._op(_net2.ftBiases ); \
for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++) \
{ \
_net1.l1Weights[i]._op(_net2.l1Weights[i]); \
_net1.l1Biases [i]._op(_net2.l1Biases [i]); \
}

#define NET_UNARY_OP(_net1, _op) \
_net1.ftWeights._op; \
_net1.ftBiases ._op; \
for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++) \
{ \
_net1.l1Weights[i]._op; \
_net1.l1Biases [i]._op; \
}

bool NNUETrainer::load(const std::string& filename)
{
    NNUEParser parser;
    if(!parser.load(filename))
    {
        return false;
    }

    bool status = true;
    status &= parser.read(m_net.ftWeights.data(), NNUE::L1Size, NNUE::FTSize, 1);
    status &= parser.read(m_net.ftBiases.data(),  NNUE::L1Size, 1, 1);
    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        status &= parser.read(m_net.l1Weights[i].data(), 1, NNUE::L1Size, 1);
        status &= parser.read(m_net.l1Biases[i].data(), 1, 1, 1);
    }

    return status;
}

bool NNUETrainer::store(const std::string& filename)
{
    NNUEEncoder encoder;
    if(!encoder.open(filename))
    {
        return false;
    }

    encoder.write(m_net.ftWeights.data(), NNUE::L1Size, NNUE::FTSize);
    encoder.write(m_net.ftBiases.data(), NNUE::L1Size, 1);

    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        encoder.write(m_net.l1Weights[i].data(), 1, NNUE::L1Size);
        encoder.write(m_net.l1Biases[i].data(), 1, 1);
    }

    encoder.close();

    return true;
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

    constexpr uint32_t numRegs = NNUE::L1Size / RegSize;
    __m256 regs[numRegs];

    float* biasesPtr         = m_net.ftBiases.data();
    float* weightsPtr        = m_net.ftWeights.data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_ps(biasesPtr + RegSize*i);
    }

    for(uint32_t i = 0; i < featureSet.numFeatures; i++)
    {
        uint16_t findex = featureSet.features[i];
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
    INFO("Randomizing NNUETrainer")
    m_net.ftWeights.heRandomize();
    m_net.l1Weights[0].heRandomize();
    m_net.ftBiases.setZero();
    m_net.l1Biases[0].setZero();

    for(uint32_t i = 1; i < NNUE::NumOutputBuckets; i++)
    {
        m_net.l1Weights[i].copy(m_net.l1Weights[0]);
        m_net.l1Biases[i].copy(m_net.l1Biases[0]);
    }
}

float NNUETrainer::m_predict(const Board& board)
{
    uint32_t bucket = NNUE::getOutputBucket(board);
    m_initAccumulator(board);
    m_trace.acc.clippedRelu(ReluClipValue);
    lastLevelFeedForward(m_net.l1Weights[bucket], m_net.l1Biases[bucket], m_trace.acc, m_trace.out);
    return *m_trace.out.data() * NNUE::NetworkScale;
}

inline float NNUETrainer::m_sigmoid(float v)
{
    constexpr float e = 2.71828182846f;
    return 1.0f / (1.0f + pow(e, -v));
}

inline float NNUETrainer::m_sigmoidPrime(float sigmoid)
{
    // Calculate derivative of sigmoid based on the sigmoid value
    // f'(x) = f(x) * (1 - f(x))
    return ((sigmoid) * (1.0f - (sigmoid)));
}

// http://neuralnetworksanddeeplearning.com/chap2.html
float NNUETrainer::m_backPropagate(const Board& board, float cpTarget, GameResult result)
{
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
    float wdlOutput       = m_sigmoid(out / NNUE::NetworkScale);
    float wdlTargetCp     = m_sigmoid(cpTarget / NNUE::NetworkScale);
    float target          = wdlTargetCp * m_params.lambda + wdlTarget * (1.0f - m_params.lambda);

    // Calculate loss
    float loss            = pow(target - wdlOutput, 2);

    // Calculate loss gradients
    float sigmoidPrime    = m_sigmoidPrime(wdlOutput);
    // Note: The loss gradient should be -2 * (target - wdlOutput),
    //       but the minus is ommitted and the gradient is later added instead of subtracted in m_applyGradient
    float lossPrime       = 2 * (target - wdlOutput);

    // -- Create input vector
    NNUE::FeatureSet featureSet;
    m_findFeatureSet(board, featureSet);
    uint32_t bucket = NNUE::getOutputBucket(board);

    // Calculate derivative of activation functions (Sigma prime)
    m_backPropData.accumulatorReLuPrime.copy(m_trace.acc);
    m_backPropData.accumulatorReLuPrime.clippedReluPrime(ReluClipValue);

    // Calculate deltas (d_l = W_l+1^T * d_l+1) * sigma prime (Z_l)

    m_backPropData.delta2.set(0, 0, sigmoidPrime * lossPrime);

    multiplyTransposeA(m_net.l1Weights[bucket], m_backPropData.delta2, m_backPropData.delta1);
    m_backPropData.delta1.hadamard(m_backPropData.accumulatorReLuPrime);

    // Calculation of gradient

    multiplyTransposeBAccumulate(m_backPropData.delta2, m_trace.acc, m_gradient.l1Weights[bucket]);
    calcAndAccFtGradient(featureSet, m_backPropData.delta1, m_gradient.ftWeights);

    // Accumulate the change
    m_gradient.l1Biases[bucket].add(m_backPropData.delta2);
    m_gradient.ftBiases.add(m_backPropData.delta1);

    return loss;
}

void NNUETrainer::m_applyGradient()
{
    m_net.ftWeights.adamUpdate(m_params.alpha, m_gradient.ftWeights, m_moments.m.ftWeights, m_moments.v.ftWeights);
    m_net.ftBiases.adamUpdate(m_params.alpha,  m_gradient.ftBiases, m_moments.m.ftBiases, m_moments.v.ftBiases);
    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        m_net.l1Weights[i].adamUpdate(m_params.alpha, m_gradient.l1Weights[i], m_moments.m.l1Weights[i], m_moments.v.l1Weights[i]);
        m_net.l1Biases [i].adamUpdate(m_params.alpha, m_gradient.l1Biases [i], m_moments.m.l1Biases[i],  m_moments.v.l1Biases[i]);
    }

    // Clamp the weights of the linear layers to enable quantization at a later stage
    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        m_net.l1Weights[i].clamp(-127.0f/NNUE::LQ, 127.0f/NNUE::LQ);
    }
}

// Returns true if the position should be skipped / filtered out
bool NNUETrainer::m_shouldFilterPosition(Board& board, Move& move, eval_t eval)
{
    // Filter out very high scoring positions
    if(std::abs(eval) > 10000)
    {
        return true;
    }

    // Filter capture moves
    // Move is null move if the move is not available
    if(!move.isNull() && move.isCapture())
    {
        return true;
    }

    // Filter positions which are checked
    if(board.isChecked())
    {
        return true;
    }

    // Filter positions with only one legal move
    board.getLegalMoves();
    if(board.getNumLegalMoves() == 1)
    {
        return true;
    }

    return false;
}

std::tuple<float, float> NNUETrainer::m_getValidationLoss(const std::string& filename)
{
    if(m_params.validationSize == 0)
    {
        return std::tuple<float, float>(0.0f, 0.0f);
    }

    DataLoader loader;
    if(!loader.open(m_params.dataset))
    {
        ERROR("Unable to open validation dataset " << m_params.dataset)
        return std::tuple<float, float>(0.0f, 0.0f);
    }

    NNUE nnue;
    nnue.load(filename);

    float totalLoss = 0.0f;
    float totalQLoss = 0.0f;

    uint64_t i = 0;
    while(i < m_params.validationSize)
    {
        Board *board = loader.getNextBoard();
        eval_t cp = loader.getScore();
        Move move = loader.getMove();
        GameResult result = loader.getResult();

        if(m_shouldFilterPosition(*board, move, cp))
        {
            continue;
        }

        i++;

        float out = m_predict(*board);
        eval_t qout = nnue.predictBoard(*board);

        // Set Win-Draw-Loss target based on result
        // Normalize from [-1, 1] to [0, 1]
        float wdlTarget = (result + 1.0f) / 2.0f;

        // Correct target perspective
        if(board->getTurn() == BLACK)
        {
            wdlTarget = 1.0f - wdlTarget;
        }

        // Calculate target
        float wdlOutput       = m_sigmoid(out / NNUE::NetworkScale);
        float qwdlOutput      = m_sigmoid(qout / NNUE::NetworkScale);
        float wdlTargetCp     = m_sigmoid(cp / NNUE::NetworkScale);
        float target          = wdlTargetCp * m_params.lambda + wdlTarget * (1.0f - m_params.lambda);

        // Calculate loss
        float loss            = pow(target - wdlOutput, 2);
        float qloss           = pow(target - qwdlOutput, 2);

        totalLoss += loss;
        totalQLoss += qloss;
    }

    return std::tuple<float, float>(totalLoss / m_params.validationSize, totalQLoss / m_params.validationSize);
}

std::string NNUETrainer::m_getOutputFilename(const std::string& base, uint32_t epoch)
{
    std::stringstream ss;
    ss << base << epoch << ".fnnue";
    return ss.str();
}

// Write the epoch loss and validation loss to a file
void NNUETrainer::m_logLoss(float epochLoss, uint64_t epochPosCount, float validationLoss, float validationQLoss, const std::string& prefix, const std::string& filename)
{
    std::ofstream os(filename, std::ios::app | std::ios::out);
    std::stringstream ssLoss;
    ssLoss << prefix << ":"
    << std::fixed << std::setprecision(6)
    << " Epoch loss: " << (epochLoss / epochPosCount)
    << " Validation loss: " << validationLoss
    << " Validation loss (Quantized): " << validationQLoss << "\n";
    os.write(ssLoss.str().c_str(), ssLoss.str().length());
    os.close();
}

void NNUETrainer::train(TrainingParameters params)
{
    constexpr uint32_t LoggingInterval = 200; // Number of batches between logging

    m_params = params;

    // Calculate the initial alpha based on the starting epoch.
    m_params.alpha = m_params.alpha * std::pow(m_params.gamma, m_params.startEpoch / m_params.gammaSteps);

    // Load initial net or create a random initial net
    if(m_params.initialNet != "")
    {
        if(!load(m_params.initialNet))
        {
            ERROR("Unable to load initial net " << m_params.initialNet)
            return;
        }
    }
    else
    {
        randomizeNet();
    }

    // Initialize the gradients
    NET_UNARY_OP(m_gradient, setZero())
    NET_UNARY_OP(m_moments.m, setZero())
    NET_UNARY_OP(m_moments.v, setZero())

    DataLoader loader;
    if(!loader.open(m_params.dataset))
    {
        ERROR("Unable to open dataset " << m_params.dataset)
        return;
    }

    for(uint32_t epoch = m_params.startEpoch; epoch < m_params.endEpoch; epoch++)
    {
        uint64_t epochPosCount = 0LL;
        uint64_t batchPosCount = 0LL;
        uint64_t iterationBatchCount = 0LL;
        float epochLoss        = 0.0f;
        float batchLoss        = 0.0f;
        float iterationLoss    = 0.0f;

        // Clear the gradient at the start of the epoch
        NET_UNARY_OP(m_gradient, setZero())

        // Start timers
        Timer epochTimer;
        Timer iterationTimer;
        epochTimer.start();
        iterationTimer.start();

        while (epochPosCount < m_params.epochSize)
        {
            // If the end of the dataset is reached, restart the parser
            if(loader.eof())
            {
                loader.close();
                if(!loader.open(m_params.dataset))
                {
                    ERROR("Unable to open dataset " << m_params.dataset)
                    return;
                }

                // Skip the validation positions at the beginning of the dataset
                uint64_t i = 0;
                while(i < m_params.validationSize)
                {
                    Board *board = loader.getNextBoard();
                    eval_t cp = loader.getScore();
                    Move move = loader.getMove();
                    i += !m_shouldFilterPosition(*board, move, cp);
                }
            }

            Board *board = loader.getNextBoard();
            eval_t cp = loader.getScore();
            GameResult result = loader.getResult();
            Move move = loader.getMove();

            if(m_shouldFilterPosition(*board, move, cp))
            {
                continue;
            }

            // Run back propagation
            batchLoss += m_backPropagate(*board, cp, result);

            // Count the number of positions in the current batch
            batchPosCount++;

            if(batchPosCount >= m_params.batchSize)
            {
                NET_UNARY_OP(m_gradient, scale(1.0f / m_params.batchSize))

                m_applyGradient();

                // Reset the gradient to 0
                NET_UNARY_OP(m_gradient, setZero())

                // Aggregate the loss and position count
                epochPosCount += batchPosCount;
                epochLoss     += batchLoss;
                iterationLoss += batchLoss;
                batchPosCount  = 0;
                batchLoss      = 0.0f;
                iterationBatchCount++;

                if(iterationBatchCount >= LoggingInterval)
                {
                    INFO("Avg. Iteration Loss: " << std::fixed << std::setprecision(6) << (iterationLoss / (iterationBatchCount * m_params.batchSize))
                    << " Avg. Epoch Loss: "     << std::setprecision(6)               << (epochLoss / epochPosCount)
                    << " FENs: " << epochPosCount
                    << " FENs/sec: " << 1000 * iterationBatchCount * m_params.batchSize / iterationTimer.getMs())

                    iterationLoss = 0.0f;
                    iterationBatchCount = 0;
                    iterationTimer.start();
                }
            }

        }
        INFO("Epoch time: " << epochTimer.getMs() << " ms")

        // Get the filename for the current epoch
        std::string netFilename = m_getOutputFilename(m_params.output, epoch);

        // Store the net for each epoch
        store(netFilename);

        // Calculate validation loss
        auto [validationLoss, validationQLoss] = m_getValidationLoss(netFilename);
        INFO("Validation loss: " << validationLoss)
        INFO("Validation loss (Quantized): " << validationQLoss)

        // Log the losses to file
        m_logLoss(epochLoss, epochPosCount, validationLoss, validationQLoss, netFilename, "loss.log");

        // Apply gamma scaling
        if((epoch != 0) && (epoch % m_params.gammaSteps == 0))
        {
            m_params.alpha *= m_params.gamma;
            INFO("Applying gamma scaling. New alpha: " << m_params.alpha)
        }
    }
}

NNUETrainer::Net* NNUETrainer::getNet()
{
    return &m_net;
}