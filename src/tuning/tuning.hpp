#pragma once

#include <string>
#include <vector>

namespace Tuning
{
    class Tuner
    {
        private:
        std::string m_inputFilePath;
        std::string m_outputFilePath;
        std::string m_trainingDataFilePath;
        std::vector<int16_t> m_weights;
        std::vector<int16_t> m_deltas;
        bool m_changed;

        double m_sigmoid(int16_t eval);
        // Result: 1 White, 0.5 Draw, 0 Black
        double m_squareError(float result, int16_t eval);
        double m_getError();
        void m_loadWeights();
        void m_storeWeights();
        void m_runIteration();
        public:
        Tuner();
        void setInputFile(std::string path);
        void setOutputFile(std::string path);
        void setTrainingDataFilePath(std::string path);
        void start();
    };
}