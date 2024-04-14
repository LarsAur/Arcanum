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
    m_loadNet(filename, m_net);
}

void NNUE::m_loadNet(std::string filename, FloatNet& net)
{
    std::string path = getWorkPath();
    std::stringstream ss;
    ss << path << filename;
    std::ifstream is(ss.str(), std::ios::in | std::ios::binary);

    LOG("Loading NNUE " << ss.str())
    if(!is.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    is.read((char*) net.ftWeights.data(), 768 * 256 * sizeof(float));
    is.read((char*) net.ftBiases.data(),        256 * sizeof(float));
    is.read((char*) net.l1Weights.data(),  1 *  256 * sizeof(float));
    is.read((char*) net.l1Biases.data(),          1 * sizeof(float));

    is.close();

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
    std::ofstream fstream(ss.str(), std::ios::out | std::ios::binary);

    if(!fstream.is_open())
    {
        ERROR("Unable to open " << ss.str())
        return;
    }

    fstream.write((char*) net.ftWeights.data(), 768 * 256 * sizeof(float));
    fstream.write((char*) net.ftBiases.data(),        256 * sizeof(float));
    fstream.write((char*) net.l1Weights.data(),   1 * 256 * sizeof(float));
    fstream.write((char*) net.l1Biases.data(),          1 * sizeof(float));

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
    constexpr uint32_t numRegs = 256 / regSize;
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
    constexpr uint32_t numRegs = 256 / regSize;
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
    m_net.ftWeights.heRandomize();
    m_net.l1Weights.heRandomize();
}

void NNUE::m_reluAccumulator(Accumulator* acc, Arcanum::Color perspective, Trace& trace)
{
    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 256 / regSize;
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
    lastLevelFeedForward<256>(m_net.l1Weights, m_net.l1Biases, trace.accumulator, trace.out);
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
    for(uint32_t k = 0; k < numFeatures; k++)
    {
        // XOR to make the correct the perspective
        features[k] = features[k] ^ board.getTurn();
    }

    // -- Calculation of auxillery coefficients
    Matrix<1, 1> delta2(false);
    Matrix<256, 1> delta1(false);

    Matrix<256, 1> accumulatorReLuPrime(false);

    accumulatorReLuPrime.copy(trace.accumulator);

    accumulatorReLuPrime.reluPrime();

    delta2.set(0, 0, sigmoidPrime * errorPrime);

    multiplyTransposeA(net.l1Weights, delta2, delta1);
    delta1.hadamard(accumulatorReLuPrime);

    // -- Calculation of gradient

    Matrix<1, 256>   gradientL1Weights(false);

    multiplyTransposeB(delta2, trace.accumulator, gradientL1Weights);

    calcAndAccFtGradient(numFeatures, features, delta1, gradient.ftWeights);

    // Accumulate the change

    gradient.l1Biases.add(delta2);
    gradient.ftBiases.add(delta1);
    gradient.l1Weights.add(gradientL1Weights);
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
    momentum1.l1Biases  .scale(beta1 / (1.0f - beta1));

    momentum1.ftWeights .add(gradient.ftWeights );
    momentum1.ftBiases  .add(gradient.ftBiases  );
    momentum1.l1Weights .add(gradient.l1Weights );
    momentum1.l1Biases  .add(gradient.l1Biases  );

    momentum1.ftWeights .scale((1.0f - beta1));
    momentum1.ftBiases  .scale((1.0f - beta1));
    momentum1.l1Weights .scale((1.0f - beta1));
    momentum1.l1Biases  .scale((1.0f - beta1));

    // v_t = B2 * v_t-1 + (1 - B2) * g_t^2

    momentum2.ftWeights .scale(beta2);
    momentum2.ftBiases  .scale(beta2);
    momentum2.l1Weights .scale(beta2);
    momentum2.l1Biases  .scale(beta2);

    gradient.ftWeights .pow(2);
    gradient.ftBiases  .pow(2);
    gradient.l1Weights .pow(2);
    gradient.l1Biases  .pow(2);

    gradient.ftWeights .scale(1.0f - beta2);
    gradient.ftBiases  .scale(1.0f - beta2);
    gradient.l1Weights .scale(1.0f - beta2);
    gradient.l1Biases  .scale(1.0f - beta2);

    momentum2.ftWeights .add(gradient.ftWeights);
    momentum2.ftBiases  .add(gradient.ftBiases );
    momentum2.l1Weights .add(gradient.l1Weights);
    momentum2.l1Biases  .add(gradient.l1Biases );

    // M^_t = alpha * M_t / (1 - Beta1^t)

    m_hat.ftWeights .add(momentum1.ftWeights);
    m_hat.ftBiases  .add(momentum1.ftBiases );
    m_hat.l1Weights .add(momentum1.l1Weights);
    m_hat.l1Biases  .add(momentum1.l1Biases );

    m_hat.ftWeights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.ftBiases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l1Weights .scale(alpha / (1.0f - std::pow(beta1, timestep)));
    m_hat.l1Biases  .scale(alpha / (1.0f - std::pow(beta1, timestep)));

    // v^_t = v_t / (1 - Beta2^t)

    v_hat.ftWeights .add(momentum2.ftWeights);
    v_hat.ftBiases  .add(momentum2.ftBiases );
    v_hat.l1Weights .add(momentum2.l1Weights);
    v_hat.l1Biases  .add(momentum2.l1Biases );

    v_hat.ftWeights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.ftBiases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l1Weights .scale(1.0f / (1.0f - std::pow(beta2, timestep)));
    v_hat.l1Biases  .scale(1.0f / (1.0f - std::pow(beta2, timestep)));

    // sqrt(v^_t) + epsilon

    v_hat.ftWeights.pow(0.5f);
    v_hat.ftBiases .pow(0.5f);
    v_hat.l1Weights.pow(0.5f);
    v_hat.l1Biases .pow(0.5f);

    v_hat.ftWeights.addScalar(epsilon);
    v_hat.ftBiases .addScalar(epsilon);
    v_hat.l1Weights.addScalar(epsilon);
    v_hat.l1Biases .addScalar(epsilon);

    // Note: Addition instead of subtraction because gradient it is already negated
    // net = net + M^_t / (sqrt(v^_t) + epsilon)

    m_hat.ftWeights .hadamardInverse(v_hat.ftWeights);
    m_hat.ftBiases  .hadamardInverse(v_hat.ftBiases );
    m_hat.l1Weights .hadamardInverse(v_hat.l1Weights);
    m_hat.l1Biases  .hadamardInverse(v_hat.l1Biases );

    m_net.ftWeights .add(m_hat.ftWeights);
    m_net.ftBiases  .add(m_hat.ftBiases );
    m_net.l1Weights .add(m_hat.l1Weights);
    m_net.l1Biases  .add(m_hat.l1Biases );
}

void NNUE::train(uint32_t epochs, uint32_t batchSize, std::string dataset)
{
    constexpr uint8_t numThreads = 6;
    std::thread threads[numThreads];
    FloatNet gradients[numThreads];
    Trace traces[numThreads];
    float errors[numThreads];
    uint64_t counts[numThreads];
    std::mutex mtx = std::mutex();

    FloatNet momentum1;
    FloatNet momentum2;

    load("../nnue/gen2201");
    m_loadNet("../nnue/momentum1.fnnue", momentum1);
    m_loadNet("../nnue/momentum2.fnnue", momentum2);

    for(uint32_t epoch = 1; epoch < 512; epoch++)
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
        }

        gradients[0].ftWeights .scale(10.0f / epochCount);
        gradients[0].ftBiases  .scale(1.0f / epochCount);
        gradients[0].l1Weights .scale(1.0f / epochCount);
        gradients[0].l1Biases  .scale(1.0f / epochCount);

        m_applyGradient(epoch, gradients[0], momentum1, momentum2);

        DEBUG("Avg. Error = " << (epochError / epochCount))

        std::ofstream ofstream("nnue_error.log", std::ios::app);
        std::stringstream filess;
        filess << (epochError / epochCount) << "\n";
        ofstream.write(filess.str().data(), filess.str().length());
        ofstream.close();

        std::stringstream ss;
        ss << "../nnue/gen2" << epoch;
        store(ss.str());
        is.close();

        m_storeNet("../nnue/momentum1.fnnue", momentum1);
        m_storeNet("../nnue/momentum2.fnnue", momentum2);
        m_storeNet("../nnue/gradient.fnnue", gradients[0]);

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