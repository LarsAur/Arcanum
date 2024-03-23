#include <nnue/nnue.hpp>
#include <nnue/linalg.hpp>
#include <memory.hpp>
#include <utils.hpp>
#include <math.h>

using namespace NN;
using namespace Arcanum;

#define ALIGN64(p) __builtin_assume_aligned((p), 64)

void NNUE::allocateFloatNet(FloatNet& net)
{
    net.ftWeights  = new Matrixf(128, 768);
    net.ftBiases   = new Matrixf(128, 1);
    net.l1Weights  = new Matrixf(16, 128);
    net.l1Biases   = new Matrixf(16, 1);
    net.l2Weights  = new Matrixf(16, 16);
    net.l2Biases   = new Matrixf(16, 1);
    net.l3Weights  = new Matrixf(1, 16);
    net.l3Bias     = new Matrixf(1, 1);
}

void NNUE::freeFloatNet(FloatNet& net)
{
    delete net.ftWeights;
    delete net.ftBiases;
    delete net.l1Weights;
    delete net.l1Biases;
    delete net.l2Weights;
    delete net.l2Biases;
    delete net.l3Weights;
    delete net.l3Bias;
}

void NNUE::allocateTrace(Trace& trace)
{
    trace.input              = new Matrixf(768, 1);
    trace.accumulator        = new Matrixf(128, 1);
    trace.hiddenOut1         = new Matrixf(16, 1);
    trace.hiddenOut2         = new Matrixf(16, 1);
    trace.out                = new Matrixf(1, 1);
}

void NNUE::freeTrace(Trace& trace)
{
    delete trace.input;
    delete trace.accumulator;
    delete trace.hiddenOut1;
    delete trace.hiddenOut2;
    delete trace.out;
}


NNUE::NNUE()
{
    allocateTrace(m_trace);
    allocateFloatNet(m_floatNet);
    m_randomizeWeights();
}

NNUE::~NNUE()
{
    freeTrace(m_trace);
    freeFloatNet(m_floatNet);
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

    is.read((char*) m_floatNet.ftWeights->data(), 768 * 128 * sizeof(float));
    is.read((char*) m_floatNet.ftBiases->data(),        128 * sizeof(float));
    is.read((char*) m_floatNet.l1Weights->data(), 128 *  16 * sizeof(float));
    is.read((char*) m_floatNet.l1Biases->data(),         16 * sizeof(float));
    is.read((char*) m_floatNet.l2Weights->data(),  16 *  16 * sizeof(float));
    is.read((char*) m_floatNet.l2Biases->data(),         16 * sizeof(float));
    is.read((char*) m_floatNet.l3Weights->data(),        16 * sizeof(float));
    is.read((char*) m_floatNet.l3Bias->data(),            1 * sizeof(float));

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

    fstream.write((char*) m_floatNet.ftWeights->data(), 768 * 128 * sizeof(float));
    fstream.write((char*) m_floatNet.ftBiases->data(),        128 * sizeof(float));
    fstream.write((char*) m_floatNet.l1Weights->data(), 128 *  16 * sizeof(float));
    fstream.write((char*) m_floatNet.l1Biases->data(),         16 * sizeof(float));
    fstream.write((char*) m_floatNet.l2Weights->data(),  16 *  16 * sizeof(float));
    fstream.write((char*) m_floatNet.l2Biases->data(),         16 * sizeof(float));
    fstream.write((char*) m_floatNet.l3Weights->data(),        16 * sizeof(float));
    fstream.write((char*) m_floatNet.l3Bias->data(),            1 * sizeof(float));

    fstream.close();

    LOG("Finished storing nnue in " << ss.str())
}

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
inline uint32_t NNUE::m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece)
{
    if(color == BLACK)
        square ^= 0x3F;

    return ((uint32_t(piece) << 6) | uint32_t(square)) << 1 | color;
}

void NNUE::m_calculateFeatures(const Arcanum::Board& board)
{
    m_numActiveIndices = 0;
    for(uint32_t color = 0; color < 2; color++)
    {
        for(uint32_t type = 0; type < 6; type++)
        {
            Arcanum::bitboard_t pieces = board.getTypedPieces(Piece(type), Color(color));
            while(pieces)
            {
                square_t idx = popLS1B(&pieces);
                uint32_t findex = m_getFeatureIndex(idx, Color(color), Piece(type));
                m_activeIndices[m_numActiveIndices++] = findex;
            }
        }
    }
}

void NNUE::m_initAccumulatorPerspective(Accumulator* acc, Arcanum::Color perspective)
{
    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 128 / regSize;
    __m256 regs[numRegs];

    float* biasesPtr         = m_floatNet.ftBiases->data();
    float* weightsPtr        = m_floatNet.ftWeights->data();

    for(uint32_t i = 0; i < numRegs; i++)
    {
        regs[i] = _mm256_load_ps(biasesPtr + regSize*i);
    }

    for(uint32_t i = 0; i < m_numActiveIndices; i++)
    {
        // XOR to the the correct index for the perspective
        uint32_t findex = m_activeIndices[i] ^ perspective;

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
    m_calculateFeatures(board);
    m_initAccumulatorPerspective(acc, Color::WHITE);
    m_initAccumulatorPerspective(acc, Color::BLACK);
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

    float* weightsPtr        = m_floatNet.ftWeights->data();

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
    m_floatNet.ftWeights->randomize(-1.0f, 1.0f);
    m_floatNet.l1Weights->randomize(-1.0f, 1.0f);
    m_floatNet.l2Weights->randomize(-1.0f, 1.0f);
    m_floatNet.l3Weights->randomize(-1.0f, 1.0f);
}

void NNUE::m_reluAccumulator(Accumulator* acc, Arcanum::Color perspective)
{
    constexpr uint32_t regSize = 256 / 32;
    constexpr uint32_t numRegs = 128 / regSize;
    __m256 zero = _mm256_setzero_ps();
    float* dst = m_trace.accumulator->data();

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

float NNUE::m_predict(Accumulator* acc, Arcanum::Color perspective)
{
    m_reluAccumulator(acc, perspective);
    feedForwardReLu<128, 16>(m_floatNet.l1Weights, m_floatNet.l1Biases, m_trace.accumulator, m_trace.hiddenOut1);
    feedForwardReLu<16, 16>(m_floatNet.l2Weights, m_floatNet.l2Biases, m_trace.hiddenOut1, m_trace.hiddenOut2);
    lastLevelFeedForward<16>(m_floatNet.l3Weights, m_floatNet.l3Bias, m_trace.hiddenOut2, m_trace.out);
    return *m_trace.out->data();
}

// http://neuralnetworksanddeeplearning.com/chap2.html
void NNUE::m_backPropagate(const Arcanum::Board& board, float target, FloatNet& nabla, float& totalError)
{
    constexpr float e = 2.71828182846f;
    constexpr float SIG_FACTOR = 400.0f;

    // -- Run prediction
    Accumulator acc;
    initAccumulator(&acc, board);
    float out = m_predict(&acc, board.getTurn());

    // -- Error Calculation
    float sigmoid       = 1.0f / (1.0f + pow(e, -out / SIG_FACTOR));
    float sigmoidPrime  = sigmoid * (1.0f - sigmoid) * SIG_FACTOR;

    // TODO: Blend with position eval

    float error =       pow(target - sigmoid, 2);
    float errorPrime =  2 * (target - sigmoid);
    totalError += error;

    // -- Create input vector
    m_trace.input->setZero();
    for(uint32_t k = 0; k < m_numActiveIndices; k++)
    {
        // XOR to make the correct the perspective
        uint32_t j = m_activeIndices[k] ^ board.getTurn();
        m_trace.input->set(j, 0, 1.0f);
    }

    // -- Calculation of auxillery coefficients
    Matrixf delta4(1, 1);
    Matrixf delta3(16, 1);
    Matrixf delta2(16, 1);
    Matrixf delta1(128, 1);

    Matrixf hiddenOut2ReLuPrime     (16, 1);
    Matrixf hiddenOut1ReLuPrime     (16, 1);
    Matrixf accumulatorReLuPrime    (128, 1);

    hiddenOut2ReLuPrime.add(*m_trace.hiddenOut2);
    hiddenOut1ReLuPrime.add(*m_trace.hiddenOut1);
    accumulatorReLuPrime.add(*m_trace.accumulator);

    hiddenOut2ReLuPrime.reluPrime();
    hiddenOut1ReLuPrime.reluPrime();
    accumulatorReLuPrime.reluPrime();

    delta4.set(0, 0, sigmoidPrime * errorPrime);

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

    // -- Calculation of gradient

    Matrixf nablaFtWeights (128, 768);
    Matrixf nablaFtBiases  (128, 1);
    Matrixf nablaL1Weights (16, 128);
    Matrixf nablaL1Biases  (16, 1);
    Matrixf nablaL2Weights (16, 16);
    Matrixf nablaL2Biases  (16, 1);
    Matrixf nablaL3Weights (1, 16);
    Matrixf nablaL3Bias    (1, 1);

    m_trace.hiddenOut2->transpose();
    m_trace.hiddenOut1->transpose();
    m_trace.accumulator->transpose();

    delta4.multiply(*m_trace.hiddenOut2, nablaL3Weights);
    nablaL3Bias.add(delta4);

    delta3.multiply(*m_trace.hiddenOut1, nablaL2Weights);
    nablaL2Biases.add(delta3);

    delta2.multiply(*m_trace.accumulator, nablaL1Weights);
    nablaL1Biases.add(delta2);

    // Undo the transpose
    m_trace.hiddenOut2->transpose();
    m_trace.hiddenOut1->transpose();
    m_trace.accumulator->transpose();

    delta1.vectorMultTransposedSparseVector(*m_trace.input, nablaFtWeights);
    nablaFtBiases.add(delta1);

    // Matrixf nablaFtWeights2 (256, 768);
    // m_trace.input->transpose();
    // delta1.multiply(*m_trace.input, nablaFtWeights2);
    // // Undo the transpose
    // m_trace.input->transpose();

    // Accumulate the change

    nabla.ftWeights->add(nablaFtWeights);
    nabla.ftBiases->add(nablaFtBiases);

    nabla.l1Weights->add(nablaL1Weights);
    nabla.l2Weights->add(nablaL2Weights);
    nabla.l3Weights->add(nablaL3Weights);

    nabla.l1Biases->add(nablaL1Biases);
    nabla.l2Biases->add(nablaL2Biases);
    nabla.l3Bias->add(nablaL3Bias);
}

void NNUE::m_applyNabla(FloatNet& nabla, FloatNet& momentum)
{
    constexpr float gamma = 0.5f;

    momentum.ftWeights->scale(gamma);
    momentum.ftBiases->scale(gamma);
    momentum.l1Weights->scale(gamma);
    momentum.l2Weights->scale(gamma);
    momentum.l3Weights->scale(gamma);
    momentum.l1Biases->scale(gamma);
    momentum.l2Biases->scale(gamma);
    momentum.l3Bias->scale(gamma);

    momentum.ftWeights->add(*nabla.ftWeights);
    momentum.ftBiases->add(*nabla.ftBiases);
    momentum.l1Weights->add(*nabla.l1Weights);
    momentum.l2Weights->add(*nabla.l2Weights);
    momentum.l3Weights->add(*nabla.l3Weights);
    momentum.l1Biases->add(*nabla.l1Biases);
    momentum.l2Biases->add(*nabla.l2Biases);
    momentum.l3Bias->add(*nabla.l3Bias);

    m_floatNet.ftWeights->add(*momentum.ftWeights);
    m_floatNet.ftBiases->add(*momentum.ftBiases);
    m_floatNet.l1Weights->add(*momentum.l1Weights);
    m_floatNet.l2Weights->add(*momentum.l2Weights);
    m_floatNet.l3Weights->add(*momentum.l3Weights);
    m_floatNet.l1Biases->add(*momentum.l1Biases);
    m_floatNet.l2Biases->add(*momentum.l2Biases);
    m_floatNet.l3Bias->add(*momentum.l3Bias);
}

void NNUE::train(uint32_t epochs, uint32_t batchSize, std::string dataset)
{
    // -- Initialize nabla
    // Nabla is the accumulator of the changes in a batch
    FloatNet nabla;
    allocateFloatNet(nabla);

    FloatNet momentum;
    allocateFloatNet(momentum);

    std::string header;
    std::string strResult;
    std::string strGames;
    std::string fen;

    for(uint32_t epoch = 181; epoch < epochs; epoch++)
    {
        uint64_t gamesInEpoch = 0LL;
        float epochError = 0;
        std::ifstream is(dataset, std::ios::in);
        while (!is.eof())
        {
            nabla.ftWeights->setZero();
            nabla.ftBiases ->setZero();
            nabla.l1Weights->setZero();
            nabla.l1Biases ->setZero();
            nabla.l2Weights->setZero();
            nabla.l2Biases ->setZero();
            nabla.l3Weights->setZero();
            nabla.l3Bias   ->setZero();

            uint64_t gamesInCurrentBatch = 0LL;
            float error = 0;
            while (!is.eof() && gamesInCurrentBatch < batchSize)
            {
                std::getline(is, header, ' ');
                std::getline(is, strResult, ' ');
                std::getline(is, strGames);

                float result = atof(strResult.c_str());
                uint8_t numGames = atoi(strGames.c_str());
                gamesInCurrentBatch += numGames;

                for(uint8_t i = 0; i < numGames; i++)
                {
                    std::getline(is, fen);
                    Arcanum::Board board = Arcanum::Board(fen);

                    if(board.getTurn() == BLACK)
                    {
                        m_backPropagate(board, 1.0f - result, nabla, error);
                    }
                    else
                    {
                        m_backPropagate(board, result, nabla, error);
                    }
                }
            }

            epochError += error;
            gamesInEpoch += gamesInCurrentBatch;
            DEBUG("Epoch Size = " << gamesInEpoch << " BatchSize = " << gamesInCurrentBatch << " Error = " << error / gamesInCurrentBatch)

            constexpr float rate    = 0.0002f;
            nabla.ftWeights ->scale(rate / gamesInCurrentBatch);
            nabla.ftBiases  ->scale(rate / gamesInCurrentBatch);
            nabla.l1Weights ->scale(rate / gamesInCurrentBatch);
            nabla.l1Biases  ->scale(rate / gamesInCurrentBatch);
            nabla.l2Weights ->scale(rate / gamesInCurrentBatch);
            nabla.l2Biases  ->scale(rate / gamesInCurrentBatch);
            nabla.l3Weights ->scale(rate / gamesInCurrentBatch);
            nabla.l3Bias    ->scale(rate / gamesInCurrentBatch);

            m_applyNabla(nabla, momentum);
        }

        DEBUG("Avg. Error = " << (epochError / gamesInEpoch))

        std::ofstream ofstream("nnue_error.log", std::ios::app);
        std::stringstream filess;
        filess << (epochError / gamesInEpoch) << "\n";
        ofstream.write(filess.str().data(), filess.str().length());
        ofstream.close();

        std::stringstream ss;
        ss << "../nnue/test768x128x16_" << epoch;
        store(ss.str());
        is.close();
        m_test();
    }

    freeFloatNet(nabla);
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