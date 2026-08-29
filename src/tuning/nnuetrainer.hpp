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
        std::string initialNet;
        uint64_t batchSize;
        uint32_t startEpoch;
        uint32_t endEpoch;
        uint64_t epochSize; // How often the net is saved and how gamma is applied. The whole dataset is used independent of "epochSize"
        bool useFullDataset;
        uint64_t validationSize;
        float alpha;  // Learning rate
        float lambda; // Weighting between wdlTarget and cpTarget in loss function 1.0 = 100% cpTarget 0.0 = 100% wdlTarget
        float gamma;  // Scaling for learning rate. Applied every gammaSteps epoch. alpha = alpha * gamma. Set to 1 to disable
        uint32_t gammaSteps; // Number of epochs between applying gamma.
        bool filter; // If true, checked positions, positions with captures as best move, or positions with very high evals are filtered out.
        uint32_t numThreads; // Number of threads to use for training. Each thread will have its own copy of gradients and traces. The batch size is divided between the threads.
    };

    class NNUETrainer
    {
        private:

            static constexpr uint32_t RegSize = 256 / 32; // Number of floats in an AVX2 register
            static constexpr float ReluClipValue = 1.0f;

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
            };

            std::vector<Trace> m_traces;
            std::vector<Net> m_gradients;
            std::vector<BackPropagationData> m_backPropData;
            std::vector<float> m_losses;
            Net m_net;
            AdamMoments m_moments;

            TrainingParameters m_params;

            static float m_sigmoid(float v);
            static float m_sigmoidPrime(float sigmoid);
            static std::string m_getOutputFilename(const std::string& base, uint32_t epoch);
            static void m_logLoss(float epochLoss, uint64_t epochPosCount, float validationLoss, float validationQLoss, const std::string& prefix, const std::string& filename);

            float m_predict(const Board& board, Trace& trace, bool mirrored);
            void m_initAccumulator(const Board& board, Trace& trace, bool mirrored);
            void m_findFeatureSet(const Board& board, NNUE::FeatureSet& featureSet, bool mirrored);

            bool m_runBatch(DataLoader& loader);
            void m_applyGradient(uint32_t timestep, Net& gradient);
            float m_backPropagate(const Board& board, float cpTarget, GameResult result, Trace& trace, BackPropagationData& backPropData, Net& gradient, bool mirrored);
            std::tuple<float, float> m_getValidationLoss(const std::string& filename);
        public:
            bool store(const std::string& filename);
            bool load(const std::string& filename);
            void randomizeNet();
            void train(TrainingParameters params);
            Net* getNet();
    };
}