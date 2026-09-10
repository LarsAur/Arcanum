#include <tuning/nnuetrainer.hpp>
#include <tuning/nnueformat.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <eval.hpp>
#include <timer.hpp>
#include <cmath>
#include <thread>

using namespace Arcanum;

#define NET_BINARY_OP(_net1, _op, _net2) \
_net1.ftWeights._op(_net2.ftWeights); \
_net1.ftBiases ._op(_net2.ftBiases ); \
for(uint32_t _i = 0; _i < NNUE::NumOutputBuckets; _i++) \
{ \
_net1.l1Weights[_i]._op(_net2.l1Weights[_i]); \
_net1.l1Biases [_i]._op(_net2.l1Biases [_i]); \
}

#define NET_UNARY_OP(_net1, _op) \
_net1.ftWeights._op; \
_net1.ftBiases ._op; \
for(uint32_t _i = 0; _i < NNUE::NumOutputBuckets; _i++) \
{ \
_net1.l1Weights[_i]._op; \
_net1.l1Biases [_i]._op; \
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

void NNUETrainer::m_findFeatureSet(const Board& board, NNUE::FeatureSet& featureSet, bool mirrored)
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
                idx = mirrored ? FLIP_FILE(idx) : idx;
                uint32_t findex = NNUE::getFeatureIndex(idx, Color(color), Piece(type), perspective);
                featureSet.features[featureSet.numFeatures++] = findex;
            }
        }
    }
}

void NNUETrainer::m_initAccumulator(const Board& board, Trace& trace, bool mirrored)
{
    NNUE::FeatureSet featureSet;
    m_findFeatureSet(board, featureSet, mirrored);
    float* accPtr = trace.acc.data();

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

float NNUETrainer::predict(const Board& board)
{
    // Ensure there is at least one trace available
    if(m_traces.size() == 0) { m_traces.resize(1); }
    return m_predict(board, m_traces[0], false);
}

float NNUETrainer::m_predict(const Board& board, Trace& trace, bool mirrored)
{
    uint32_t bucket = NNUE::getOutputBucket(board);
    m_initAccumulator(board, trace, mirrored);
    trace.acc.clippedRelu(ReluClipValue);
    lastLevelFeedForward(m_net.l1Weights[bucket], m_net.l1Biases[bucket], trace.acc, trace.out);
    return *trace.out.data() * NNUE::NetworkScale;
}

inline float NNUETrainer::m_sigmoid(float v)
{
    return 1.0f / (1.0f + expf(-v));
}

inline float NNUETrainer::m_sigmoidPrime(float sigmoid)
{
    // Calculate derivative of sigmoid based on the sigmoid value
    // f'(x) = f(x) * (1 - f(x))
    return ((sigmoid) * (1.0f - (sigmoid)));
}

// http://neuralnetworksanddeeplearning.com/chap2.html
float NNUETrainer::m_backPropagate(
    const Board& board,
    float cpTarget,
    GameResult result,
    Trace& trace,
    BackPropagationData& backPropData,
    Net& gradient,
    bool mirrored
)
{
    // -- Run prediction
    float out = m_predict(board, trace, mirrored);

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
    m_findFeatureSet(board, featureSet, mirrored);
    uint32_t bucket = NNUE::getOutputBucket(board);

    // Calculate derivative of activation functions (Sigma prime)
    backPropData.accumulatorReLuPrime.copy(trace.acc);
    backPropData.accumulatorReLuPrime.clippedReluPrime(ReluClipValue);

    // Calculate deltas (d_l = W_l+1^T * d_l+1) * sigma prime (Z_l)

    backPropData.delta2.set(0, 0, sigmoidPrime * lossPrime);

    multiplyTransposeA(m_net.l1Weights[bucket], backPropData.delta2, backPropData.delta1);
    backPropData.delta1.hadamard(backPropData.accumulatorReLuPrime);

    // Calculation of gradient

    multiplyTransposeBAccumulate(backPropData.delta2, trace.acc, gradient.l1Weights[bucket]);
    calcAndAccFtGradient(featureSet, backPropData.delta1, gradient.ftWeights);

    // Accumulate the change
    gradient.l1Biases[bucket].add(backPropData.delta2);
    gradient.ftBiases.add(backPropData.delta1);

    return loss;
}

void NNUETrainer::m_applyGradient(uint32_t timestep, Net& gradient)
{
    m_net.ftWeights.adamUpdate(m_params.alpha, timestep, gradient.ftWeights, m_moments.m.ftWeights, m_moments.v.ftWeights, m_params.batchSize);
    m_net.ftBiases.adamUpdate(m_params.alpha, timestep, gradient.ftBiases, m_moments.m.ftBiases, m_moments.v.ftBiases, m_params.batchSize);
    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        m_net.l1Weights[i].adamUpdate(m_params.alpha, timestep, gradient.l1Weights[i], m_moments.m.l1Weights[i], m_moments.v.l1Weights[i], m_params.batchSize);
        m_net.l1Biases [i].adamUpdate(m_params.alpha, timestep, gradient.l1Biases [i], m_moments.m.l1Biases[i],  m_moments.v.l1Biases[i], m_params.batchSize);
    }

    // Clamp the weights of the linear layers to enable quantization at a later stage
    constexpr float ftWeightClampMin = static_cast<float>(-INT16_MAX)/(NNUE::FTQ * 33);
    constexpr float ftWeightClampMax = static_cast<float>(INT16_MAX)/(NNUE::FTQ * 33);
    m_net.ftWeights.clamp(ftWeightClampMin, ftWeightClampMax);
    m_net.ftBiases.clamp(ftWeightClampMin, ftWeightClampMax);

    for(uint32_t i = 0; i < NNUE::NumOutputBuckets; i++)
    {
        m_net.l1Weights[i].clamp(-127.0f/NNUE::LQ, 127.0f/NNUE::LQ);
    }
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

    Trace trace;
    for (uint32_t i = 0; i < m_params.validationSize; i++)
    {
        Board *board = loader.getNextBoard();
        float cp = static_cast<float>(loader.getScore());
        GameResult result = loader.getResult();

        float out = m_predict(*board, trace, false);
        float qout = static_cast<float>(nnue.predictBoard(*board));

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

bool NNUETrainer::m_runBatch(DataLoader& loader)
{
    bool passed = true;
    std::mutex mtx;

    std::function <void(uint32_t)> threadFunc = [this, &mtx, &loader, &passed](uint32_t threadId)
    {
        // Clear the gradients and loss for this thread
        m_losses[threadId] = 0.0f;
        NET_UNARY_OP(m_gradients[threadId], setZero())

        // The batch size is divided between the threads
        // TODO: This might lead to less than batchSize positions being processed
        // if the batch size is not divisible by the number of threads. This should be fixed by having the last thread process the remaining positions.
        uint32_t threadBatchSize = m_params.batchSize / m_params.numThreads;
        for(uint32_t j = 0; j < threadBatchSize; j+=2)
        {
            // Get the next board and move from the data loader
            mtx.lock();
            if(loader.eof())
            {
                passed = false;
                mtx.unlock();
                break;
            }

            Board board = Board(*loader.getNextBoard());
            eval_t cp = loader.getScore();
            GameResult result = loader.getResult();
            mtx.unlock();

            // Run back propagation with board and mirrored board to augment the dataset
            m_losses[threadId] += m_backPropagate(board, cp, result, m_traces[threadId], m_backPropData[threadId], m_gradients[threadId], false);
            m_losses[threadId] += m_backPropagate(board, cp, result, m_traces[threadId], m_backPropData[threadId], m_gradients[threadId], true);
        }
    };

    // TODO: Make a thread pool instead of creating new threads for each batch
    // Run the batch in parallel across multiple threads
    std::vector<std::thread> threads;
    for(uint32_t i = 1; i < m_params.numThreads; i++)
    {
        threads.emplace_back(threadFunc, i);
    }

    // Run the first thread in the main thread
    threadFunc(0);

    // Wait for all threads to finish
    for(auto& thread : threads)
    {
        thread.join();
    }

    // Aggregate the gradients from all threads into the first gradient
    // TODO: This could also be done in parallel by merging sets of ranges per thread.
    for(uint32_t i = 1; i < m_params.numThreads; i++)
    {
        NET_BINARY_OP(m_gradients[0], add, m_gradients[i])
        m_losses[0] += m_losses[i];
    }

    return passed;
}

void NNUETrainer::train(TrainingParameters params)
{
    ENABLE_FTZ_AND_DAZ();

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

    // Allocate the gradients and backpropagation data
    m_traces.resize(m_params.numThreads);
    m_gradients.resize(m_params.numThreads);
    m_backPropData.resize(m_params.numThreads);
    m_losses.resize(m_params.numThreads);

    NET_UNARY_OP(m_moments.m, setZero())
    NET_UNARY_OP(m_moments.v, setZero())

    bool dataLoaded = false;
    DataLoader loader;

    uint32_t timestep = 0;
    for(uint32_t epoch = m_params.startEpoch; epoch < m_params.endEpoch; epoch++)
    {
        uint64_t epochPosCount = 0LL;
        uint64_t iterationBatchCount = 0LL;
        float epochLoss        = 0.0f;
        float iterationLoss    = 0.0f;

        // Start timers
        Timer epochTimer;
        Timer iterationTimer;
        epochTimer.start();
        iterationTimer.start();

        while (m_params.useFullDataset || (epochPosCount < m_params.epochSize))
        {
            // If the end of the dataset is reached, restart the parser
            if(!dataLoaded || loader.eof())
            {
                loader.close();
                if(!loader.open(m_params.dataset))
                {
                    ERROR("Unable to open dataset " << m_params.dataset)
                    return;
                }
                dataLoaded = true;

                // Skip the validation positions at the beginning of the dataset
                for(uint32_t i = 0; i < m_params.validationSize; i++)
                {
                    loader.getNextBoard();
                }
            }

            bool batchSuccess = m_runBatch(loader);

            // Batch fails if it reaches the end of the dataset
            if(batchSuccess)
            {
                m_applyGradient(++timestep, m_gradients[0]);

                // Aggregate the loss and position count
                epochPosCount += m_params.batchSize;
                epochLoss     += m_losses[0];
                iterationLoss += m_losses[0];
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

            if(m_params.useFullDataset && loader.eof())
            {
                break;
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

    DISABLE_FTZ_AND_DAZ();
}

NNUETrainer::Net* NNUETrainer::getNet()
{
    return &m_net;
}