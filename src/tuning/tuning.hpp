#pragma once

#include <types.hpp>
#include <string>
#include <vector>

namespace Tuning
{
    class Tuner
    {
        private:
        static constexpr size_t BATCH_SIZE = 10000;
        std::string m_inputFilePath;
        std::string m_outputFilePath;
        std::string m_trainingDataFilePath;
        std::vector<int16_t> m_weights;
        std::vector<int16_t> m_deltas;
        std::streampos m_iterationStartPos;

        double m_sigmoid(Arcanum::eval_t eval);
        // Result: 1 White, 0.5 Draw, 0 Black
        double m_squareError(float result, Arcanum::eval_t eval);
        double m_getError();
        void m_loadWeights();
        void m_storeWeights();
        void m_runIteration();
        void m_calculateNextStartPos();
        public:
        Tuner();
        void setInputFile(std::string path);
        void setOutputFile(std::string path);
        void setTrainingDataFilePath(std::string path);
        void start();
    };
}