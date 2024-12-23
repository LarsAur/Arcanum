#pragma once

#include <board.hpp>
#include <nnue.hpp>
#include <tuning/matrix.hpp>
#include <tuning/dataloader.hpp>

namespace Arcanum
{
    class NNUETrainer
    {
        private:

            static constexpr uint32_t FTSize  = 768;
            static constexpr uint32_t L1Size  = 256;
            static constexpr uint32_t L2Size  = 32;
            static constexpr uint32_t RegSize = 256 / 32; // Number of floats in an AVX2 register
            static constexpr float ReluClipValue = 256.0f;

            static const char* NNUE_MAGIC;

            struct Net
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
                Matrix<L1Size, 1>  acc;           // FT Output
                Matrix<L2Size, 1>  l1Out;         // L1 Output post ReLu
                Matrix<1, 1>       out;           // Scalar output
            };

            struct AdamMoments
            {
                Net m;
                Net v;
                Net mHat;
                Net vHat;
            };

            Trace m_trace;
            Net m_net;

            Net m_gradient;
            AdamMoments m_moments;

            static float m_sigmoid(float v);
            static float m_sigmoidPrime(float sigmoid);

            float m_predict(const Board& board);
            void m_initAccumulator(const Board& board);
            void m_findFeatureSet(const Board& board, NNUE::FeatureSet& featureSet);

            void m_applyGradient(uint32_t timestep);
            void m_backPropagate(const Board& board, float cpTarget, DataParser::Result result, float& totalLoss);

            void m_storeNet(std::string filename, Net& net);
            void m_loadNet(std::string filename, Net& net);
            void m_loadNetFromStream(std::istream& stream, Net& net);
        public:
            void randomizeNet();
            void train(std::string dataset, std::string outputPath, uint64_t batchSize, uint32_t startEpoch, uint32_t endEpoch);
            void load(std::string filename);
            void store(std::string filename);
    };
}