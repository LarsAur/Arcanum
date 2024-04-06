#pragma once

#include <types.hpp>
#include <board.hpp>
#include <nnue/matrix.hpp>

namespace NN
{
    struct Accumulator
    {
        alignas(64) float acc[2][128];
    };

    struct FloatNet
    {
        Matrix<128, 768> ftWeights;
        Matrix<128, 1> ftBiases;
        Matrix<16, 128> l1Weights;
        Matrix<16, 1> l1Biases;
        Matrix<16, 16> l2Weights;
        Matrix<16, 1> l2Biases;
        Matrix<1, 16> l3Weights;
        Matrix<1, 1> l3Bias;
    };

    // Intermediate results in the net
    struct Trace
    {
        Matrix<768, 1>  input;         // Only used by backprop
        Matrix<128, 1>  accumulator;   // Post ReLU accumulator
        Matrix<16, 1>   hiddenOut1;
        Matrix<16, 1>   hiddenOut2;
        Matrix<1, 1>    out;           // Scalar output
    };

    class NNUE
    {
        private:
            Trace m_trace;
            FloatNet m_net;

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

            void train(uint32_t epochs, uint32_t batchSize, std::string dataset);
            void load(std::string filename);
            void store(std::string filename);

            void initAccumulator(Accumulator* acc, const Arcanum::Board& board);
            void incAccumulator(Accumulator* accIn, Accumulator* accOut, const Arcanum::Board& board, const Arcanum::Move& move);

            Arcanum::eval_t evaluateBoard(const Arcanum::Board& board);
            Arcanum::eval_t evaluate(Accumulator* acc, Arcanum::Color turn);
    };
}