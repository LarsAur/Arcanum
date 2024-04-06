#pragma once

#include <board.hpp>
#include <nnue/linalg.hpp>

namespace NN
{
    struct Accumulator
    {
        alignas(64) float acc[2][128];
    };

    struct FloatNet
    {
        Matrixf* ftWeights;
        Matrixf* ftBiases;
        Matrixf* l1Weights;
        Matrixf* l1Biases;
        Matrixf* l2Weights;
        Matrixf* l2Biases;
        Matrixf* l3Weights;
        Matrixf* l3Bias;
    };

    // Intermediate results in the net
    struct Trace
    {
        Matrixf* input;         // Only used by backprop
        Matrixf* accumulator;   // Post ReLU accumulator
        Matrixf* hiddenOut1;
        Matrixf* hiddenOut2;
        Matrixf* out;           // Scalar output
    };

    class NNUE
    {
        private:
            Trace m_trace;
            FloatNet m_floatNet;

            uint32_t m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece);
            float m_predict(Accumulator* acc, Arcanum::Color perspective, Trace& trace);
            float m_predict(Accumulator* acc, Arcanum::Color perspective);
            void m_calculateFeatures(const Arcanum::Board& board, uint8_t* numFeatures, uint32_t* features);
            void m_initAccumulatorPerspective(Accumulator* acc, Arcanum::Color perspective, uint8_t numFeatures, uint32_t* features);
            void m_reluAccumulator(Accumulator* acc, Arcanum::Color perspective, Trace& trace);
            void m_randomizeWeights();
            void m_applyGradient(uint32_t timestep, FloatNet& gradient, FloatNet& momentum1, FloatNet& momentum2);
            void m_test();
            void m_backPropagate(const Arcanum::Board& board, float result, FloatNet& nabla, float& totalError, FloatNet& net, Trace& trace);
        public:
            NNUE();
            ~NNUE();

            static void allocateFloatNet(FloatNet& net);
            static void freeFloatNet(FloatNet& net);
            static void allocateTrace(Trace& trace);
            static void freeTrace(Trace& trace);

            void train(uint32_t epochs, uint32_t batchSize, std::string dataset);
            void load(std::string filename);
            void store(std::string filename);

            void initAccumulator(Accumulator* acc, const Arcanum::Board& board);
            void incAccumulator(Accumulator* accIn, Accumulator* accOut, const Arcanum::Board& board, const Arcanum::Move& move);

            Arcanum::eval_t evaluateBoard(const Arcanum::Board& board);
            Arcanum::eval_t evaluate(Accumulator* acc, Arcanum::Color turn);
    };
}