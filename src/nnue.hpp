#pragma once

#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{

    class NNUE
    {
        public:
            static constexpr uint32_t FTSize  = 768;
            static constexpr uint32_t L1Size  = 512;
            static constexpr uint32_t L2Size  = 16;
            static constexpr int32_t FTQ = 127; // Quantization factor of the feature transformer
            static constexpr int32_t LQ = 64;   // Quantization factor of the linear layers
            static constexpr uint32_t NumOutputBuckets = 8;

            struct Accumulator
            {
                alignas(64) int16_t acc[2][L1Size];
            };

            // Matrices are stored in column-major order
            // except the l1Weights which are transposed during loading
            struct Net
            {
                alignas(64) int16_t ftWeights[L1Size * FTSize];
                alignas(64) int16_t ftBiases[L1Size];
                alignas(64) int8_t  l1Weights[NumOutputBuckets][L2Size * L1Size];
                alignas(64) int32_t l1Biases[NumOutputBuckets][L2Size];
                alignas(64) float l2Weights[NumOutputBuckets][L2Size];
                alignas(64) float l2Biases[NumOutputBuckets][1];
            };

            struct DeltaFeatures
            {
                uint8_t numAdded;
                uint8_t numRemoved;
                uint32_t added[2][2];   // First index is the perspective
                uint32_t removed[2][2]; // First index is the perspective
            };

            struct FeatureSet
            {
                uint8_t numFeatures;
                uint32_t features[32];
            };

            struct FullFeatureSet
            {
                uint8_t numFeatures;
                uint32_t features[2][32];
            };

            static uint32_t getOutputBucket(const Board& board);
            static uint32_t getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective);
            static void findDeltaFeatures(const Board& board, const Move& move, DeltaFeatures& delta);
            static void findFullFeatureSet(const Board& board, FullFeatureSet& featureSet);

            NNUE();
            ~NNUE();
            void load(const std::string filename);
            void initializeAccumulator(Accumulator* acc, const Board& board);
            void incrementAccumulator(Accumulator* acc, Accumulator* nextAcc, const Board& board, const Move& move);
            eval_t predict(const Accumulator* acc, const Board& board);
            eval_t predictBoard(const Board& board);
        private:
            Net* m_net;
            void m_l1AffineRelu(const int8_t* in, int8_t* weights, int32_t* biases, int32_t* out);
            void m_clampAcc(const int16_t* in, int8_t* out);
    };

}