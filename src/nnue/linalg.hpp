#pragma once

#include <inttypes.h>

namespace NN
{
    class Matrixf
    {
        private:
            // Column major matrix entries
            float* m_data;
            uint32_t m_cols;
            uint32_t m_rows;

            uint32_t m_idx(uint32_t row, uint32_t col);
        public:
            Matrixf(uint32_t rows, uint32_t cols);
            ~Matrixf();

            void multiply(Matrixf& matrixIn, Matrixf& matrixOut);
            void scale(float scalar);
            void add(Matrixf& matrix);
            void hadamard(Matrixf& matrixIn);
            void hadamard(Matrixf& matrixIn, Matrixf& matrixOut);
            void transpose();
            void setZero();
            float* data();
            void set(uint32_t row, uint32_t col, float value);
            void reluClamp();
            void reluPrime();
            void randomize(const float min, const float max);
            void log();

            template <typename T>
            void copy(T* ptr);
    };

    template <typename T>
    void Matrixf::copy(T* ptr)
    {
        for(uint32_t i = 0; i < m_cols * m_rows; i++)
        {
            m_data[i] = static_cast<float>(ptr[i]);
        }
    }

}