#pragma once

#include <board.hpp>
#include <nnue.hpp>
#include <tuning/matrix.hpp>
#include <tuning/dataloader.hpp>

namespace Arcanum
{
    struct TrainingParameters
    {
        std::string dataset;
        std::string output;
        uint64_t batchSize;
        uint32_t startEpoch;
        uint32_t endEpoch;
        uint64_t epochSize; // How often the net is saved and how gamma is applied. The whole dataset is used independent of "epochSize"
        uint64_t validationSize;
        float alpha;  // Learning rate
        float lambda; // Weighting between wdlTarget and cpTarget in loss function 1.0 = 100% cpTarget 0.0 = 100% wdlTarget
        float gamma;  // Scaling for learning rate. Applied every gammaSteps epoch. alpha = alpha * gamma. Set to 1 to disable
        uint32_t gammaSteps; // Number of epochs between applying gamma.
    };

    class NNUETrainer
    {
        private:

            static constexpr uint32_t RegSize = 256 / 32; // Number of floats in an AVX2 register
            static constexpr float ReluClipValue = 1.0f;

            static const char* NNUE_MAGIC;

            struct Net
            {
                Matrix<NNUE::L1Size, NNUE::FTSize> ftWeights;
                Matrix<NNUE::L1Size, 1>            ftBiases;
                Matrix<1, NNUE::L1Size>            l1Weights[NNUE::NumOutputBuckets];
                Matrix<1, 1>                       l1Biases[NNUE::NumOutputBuckets];
            };

            // Intermediate results in the net
            struct Trace
            {
                Matrix<NNUE::L1Size, 1> acc;
                Matrix<1, 1>            out;
            };

            struct BackPropagationData
            {
                Matrix<NNUE::L1Size, 1> delta1;
                Matrix<1, 1>            delta2;
                Matrix<NNUE::L1Size, 1> accumulatorReLuPrime;
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
            BackPropagationData m_backPropData;

            TrainingParameters m_params;

            static float m_sigmoid(float v);
            static float m_sigmoidPrime(float sigmoid);

            float m_predict(const Board& board);
            void m_initAccumulator(const Board& board);
            void m_findFeatureSet(const Board& board, NNUE::FeatureSet& featureSet);

            void m_applyGradient(uint32_t timestep);
            float m_backPropagate(const Board& board, float cpTarget, GameResult result);
            bool m_shouldFilterPosition(Board& board, Move& move, eval_t eval);
            float m_getValidationLoss();

            void m_storeNet(std::string filename, Net& net);
            bool m_loadNet(std::string filename, Net& net);
            bool m_loadNetFromStream(std::istream& stream, Net& net);
            #ifdef ENABLE_INCBIN
            bool m_loadIncbin();
            #endif
        public:
            void randomizeNet();
            void train(TrainingParameters params);
            bool load(std::string filename);
            void store(std::string filename);
            Net* getNet();
    };
}