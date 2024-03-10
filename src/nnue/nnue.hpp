#pragma once

#include <board.hpp>
#include <nnue/linalg.hpp>

namespace NN
{
    struct Accumulator
    {
        alignas(64) int16_t acc[2][128];

        std::string toString() const
        {
            std::stringstream ss;
            ss << "\nWHITE:";
            for(uint8_t i = 0; i < 128; i++)
            {
                if(i % 32 == 0) ss << "\n";
                ss << unsigned(acc[0][i]) << " ";
            }

            ss << "\n";

            ss << "BLACK:";
            for(uint8_t i = 0; i < 128; i++)
            {
                if(i % 32 == 0) ss << "\n";
                ss << signed(acc[1][i]) << " ";
            }

            return ss.str();
        }
    };

    struct Net
    {
        alignas(64) int16_t ftWeights[384 * 128];
        alignas(64) int16_t ftBiases[128];

        alignas(64) int8_t l1Weights[256 * 32];
        alignas(64) int32_t l1Biases[32];

        alignas(64) int8_t l2Weights[32 * 32];
        alignas(64) int32_t l2Biases[32];

        alignas(64) int8_t l3Weights[32];
        alignas(64) int32_t l3Bias[1];
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

    class NNUE
    {
        private:
            // Intermediate results in the net
            alignas(64) int8_t m_clampedAcc[2 * 128];
            alignas(64) int8_t m_hiddenOut1[32];
            alignas(64) int8_t m_hiddenOut2[32];

            uint8_t m_numActiveIndices[2];
            int32_t m_activeIndices[2][16];
            Net* m_net;
            FloatNet m_floatNet;

            uint32_t m_getFeatureIndex(Arcanum::square_t square, Arcanum::Color color, Arcanum::Piece piece);
            void m_initializeAccumulatorBias(Accumulator& acc);
            void m_setFeature(Accumulator& acc, uint32_t findex, Arcanum::Color color);
            void m_clearFeature(Accumulator& acc, uint32_t findex, Arcanum::Color color);
            template<uint32_t dimIn, uint32_t dimOut>
            void m_affineTransform(int8_t *input, int8_t *output, int32_t* biases, int8_t* weights);
            int32_t m_affinePropagate(int8_t* input, int32_t* biases, int8_t* weights);

        public:
            NNUE();
            ~NNUE();

            void randomizeFNNUE();
            void quantize();
            void backPropagate(Arcanum::Board& board, float result);
            void loadQnnue(std::string filename);
            void loadFnnue(std::string filename);
            void store(std::string filename);
            void initializeAccumulator(Accumulator& acc, const Arcanum::Board& board);
            void incrementAccumulator(Accumulator& accIn, Accumulator& accOut, Arcanum::Color color, const Arcanum::Board& board, const Arcanum::Move& move);
            Arcanum::eval_t evaluate(Accumulator& acc, Arcanum::Color turn);
            Arcanum::eval_t evaluateBoard(const Arcanum::Board& board);
    };
}