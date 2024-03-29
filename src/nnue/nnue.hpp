#pragma once

#include <board.hpp>
#include <string>
#include <array>

/**
 * The code related to NNUE is an adaptation of the NNUE-probe library created by dshawul on Github.
 * https://github.com/dshawul/nnue-probe
**/

namespace NN
{
    typedef int16_t AccData;
    typedef int16_t FTWeight;
    typedef int16_t FTBias;
    typedef int8_t Weight;
    typedef int32_t Bias;
    typedef int8_t Clipped;
    typedef uint32_t Mask;
    typedef uint64_t Mask2;

    constexpr uint32_t ftIns = 41024;
    constexpr uint32_t ftOuts = 256;
    constexpr uint32_t l1outs = 32;
    constexpr uint32_t l2outs = 32;
    constexpr uint32_t lastouts = 1;
    constexpr uint32_t shift = 6;
    constexpr int32_t fv_scale = 16;

    struct Accumulator
    {
        alignas(64) AccData acc[2][ftOuts];
    };

    class NNUE
    {
        private:
            struct NetData {
                alignas(64) Clipped input[2*ftOuts];
                Clipped hiddenOut1[32];
                Clipped hiddenOut2[32];
            };

            template <int In, int Out>
            struct LinearLayer
            {
                alignas(64) Weight weights[In * Out];
                alignas(64) Bias   biases[Out];
            };

            struct FeatureTransformer
            {
                FTWeight *weights;
                FTBias   *biases;
            };

            FeatureTransformer m_featureTransformer;
            LinearLayer<2*ftOuts, 32> m_hiddenLayer1;
            LinearLayer<32, 32> m_hiddenLayer2;
            LinearLayer<32, 1> m_outputLayer;
            bool m_loaded;
            void m_affineTransform(
                Clipped *input,
                void *output,
                uint32_t inDim,
                uint32_t outDim,
                const Bias* biases,
                const Weight* weights,
                const Mask *inMask,
                Mask *outMask,
                const bool pack8Mask
            );
            int32_t m_affinePropagate(Clipped* input, Bias* biases, Weight* weights);
            void m_permuteBiases(Bias* biases);
            uint32_t m_calculateActiveIndices(std::array<uint32_t, 30>& indicies, const Arcanum::Color perspective, const Arcanum::Board& board);
            uint32_t m_calculateChangedIndices(uint32_t& activated, std::array<uint32_t, 2>& deactivated, const Arcanum::Color perspective, const Arcanum::Move& move, const Arcanum::Board board);
            void m_initializeAccumulatorPerspective(Accumulator& accumulator, const Arcanum::Color perspective, const Arcanum::Board& board);
            void m_loadHeader(std::ifstream& stream);
            void m_loadWeights(std::ifstream& stream);
        public:
            NNUE();
            ~NNUE();

            // Loads a .nnue file given a full path
            bool loadFullPath(std::string fullPath);
            // Loads a .nnue file in a position relative to the executable
            bool loadRelative(std::string filename);
            void initializeAccumulator(Accumulator& accumulator, const Arcanum::Board& board);
            void incrementAccumulator(
                const Accumulator& prevAccumulator,
                Accumulator& newAccumulator,
                const Arcanum::Color perspective,
                const Arcanum::Board& board,
                const Arcanum::Move& move
            );
            int evaluateBoard(const Arcanum::Board& board);
            int evaluate(const Accumulator& accumulator, Arcanum::Color turn);
            bool isLoaded();
    };
}