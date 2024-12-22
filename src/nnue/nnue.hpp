#pragma once

#include <board.hpp>
#include <nnue/matrix.hpp>
#include <tuning/dataloader.hpp>

namespace Arcanum
{
    static constexpr uint32_t FTSize  = 768;
    static constexpr uint32_t L1Size  = 256;
    static constexpr uint32_t L2Size  = 32;
    static constexpr uint32_t RegSize = 256 / 32; // Number of floats in an AVX2 register
    struct Accumulator
    {
        alignas(64) float acc[2][L1Size];
    };

    struct FloatNet
    {
        Matrix<L1Size, FTSize>  ftWeights;
        Matrix<L1Size, 1>       ftBiases;
        Matrix<L2Size, L1Size>  l1Weights;
        Matrix<L2Size, 1>       l1Biases;
        Matrix<1, L2Size>       l2Weights;
        Matrix<1, 1>            l2Biases;
    };

    // Intermediate results in the net
    struct Trace
    {
        Matrix<L1Size, 1>  accumulator;   // Post ReLu accumulator
        Matrix<L2Size, 1>  l1Out;         // L1 Output post ReLu
        Matrix<1, 1>       out;           // Scalar output
    };

    class NNUE
    {
        private:
            Trace m_trace;
            FloatNet m_net;

            float m_sigmoid(float v);
            float m_sigmoidPrime(float sigmoid);
            uint32_t m_getFeatureIndex(square_t square, Color color, Piece piece);
            float m_predict(Accumulator* acc, Color perspective, Trace& trace);
            float m_predict(Accumulator* acc, Color perspective);
            void m_calculateFeatures(const Board& board, uint8_t* numFeatures, uint32_t* features);
            void m_initAccumulatorPerspective(Accumulator* acc, Color perspective, uint8_t numFeatures, uint32_t* features);
            void m_reluAccumulator(Accumulator* acc, Color perspective, Trace& trace);
            void m_randomizeWeights();
            void m_applyGradient(uint32_t timestep, FloatNet& gradient, FloatNet& momentum1, FloatNet& momentum2, FloatNet& mHat, FloatNet& vHat);
            void m_test();
            void m_backPropagate(const Board& board, float cpTarget, DataParser::Result result, FloatNet& gradient, float& totalLoss, FloatNet& net, Trace& trace);

            void m_storeNet(std::string filename, FloatNet& net);
            void m_loadNet(std::string filename, FloatNet& net);
            void m_loadNetFromStream(std::istream& stream, FloatNet& net);
            #ifdef ENABLE_INCBIN
            void m_loadIncbin();
            #endif
        public:
            static const char* NNUE_MAGIC;

            NNUE();
            ~NNUE();

            void train(std::string dataset, std::string outputPath, uint64_t batchSize, uint32_t startEpoch, uint32_t endEpoch, bool randomize);
            void load(std::string filename);
            void store(std::string filename);

            void initAccumulator(Accumulator* acc, const Board& board);
            void incAccumulator(Accumulator* accIn, Accumulator* accOut, const Board& board, const Move& move);

            eval_t evaluateBoard(const Board& board);
            eval_t evaluate(Accumulator* acc, Color turn);
    };
}