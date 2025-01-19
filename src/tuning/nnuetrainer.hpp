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

            static constexpr uint32_t RegSize = 256 / 32; // Number of floats in an AVX2 register
            static constexpr float ReluClipValue = 1.0f;
            static constexpr float SigmoidFactor = 1.0f / 400.0f;
            static constexpr float Lambda = 1.0f; // Weighting between wdlTarget and cpTarget in loss function 1.0 = 100% cpTarget 0.0 = 100% wdlTarget

            static const char* NNUE_MAGIC;

            struct Net
            {
                Matrix<NNUE::L1Size, NNUE::FTSize> ftWeights;
                Matrix<NNUE::L1Size, 1>            ftBiases;
                Matrix<NNUE::L2Size, NNUE::L1Size> l1Weights[NNUE::NumOutputBuckets];
                Matrix<NNUE::L2Size, 1>            l1Biases[NNUE::NumOutputBuckets];
                Matrix<1, NNUE::L2Size>            l2Weights[NNUE::NumOutputBuckets];
                Matrix<1, 1>                       l2Biases[NNUE::NumOutputBuckets];
            };

            // Intermediate results in the net
            struct Trace
            {
                Matrix<NNUE::L1Size, 1> acc;
                Matrix<NNUE::L2Size, 1> l1Out;
                Matrix<1, 1>            out;
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
            bool m_loadNet(std::string filename, Net& net);
            bool m_loadNetFromStream(std::istream& stream, Net& net);
            #ifdef ENABLE_INCBIN
            bool m_loadIncbin();
            #endif
        public:
            void randomizeNet();
            void train(std::string dataset, std::string outputPath, uint64_t batchSize, uint32_t startEpoch, uint32_t endEpoch);
            bool load(std::string filename);
            void store(std::string filename);
            Net* getNet();
    };
}