#pragma once

#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{

    class NNUE
    {
        public:
            // TODO: Refer to these in the NNUETrainer class
            static const char* QNNUE_MAGIC;
            static constexpr uint32_t FTSize  = 768;
            static constexpr uint32_t L1Size  = 256;
            static constexpr uint32_t L2Size  = 32;
            static constexpr int32_t FTQ = 127; // Quantization factor of the feature transformer
            static constexpr int32_t LQ = 64;   // Quantization factor of the linear layers

            struct Accumulator
            {
                alignas(64) int16_t acc[2][L1Size];
            };

            struct Net
            {
                alignas(64) int16_t ftWeights[L1Size * FTSize];
                alignas(64) int16_t ftBiases[L1Size];
                alignas(64) int8_t  l1Weights[L2Size * L1Size];
                alignas(64) int32_t l1Biases[L2Size];
                alignas(64) float l2Weights[L2Size];
                alignas(64) float l2Biases[1];
            };

            struct FeatureSet
            {
                uint8_t numFeatures;
                uint32_t features[32];
            };

            static uint32_t getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective);

            void load(const std::string filename);

        private:
            Net m_net;
    };

}