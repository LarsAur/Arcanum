#pragma once

#include <inttypes.h>
#include <chessutils.hpp>
#include <utils.hpp>

namespace NN
{
    class Matrixf
    {
        private:
            // Column major matrix entries
            float* m_data;
            uint32_t m_cols;
            uint32_t m_rows;
        public:
            Matrixf(uint32_t rows, uint32_t cols);
            ~Matrixf();

            void multiply(Matrixf& matrixIn, Matrixf& matrixOut);
            void vectorMultTransposedSparseVector(Matrixf& tvector, Matrixf& matrixOut);
            void scale(float scalar);
            void pow(float exp);
            void add(Matrixf& matrix);
            void addScalar(float scalar);
            void hadamardInverse(Matrixf& matrixIn);
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

    template<class T> static inline void Log(const __m256 & value)
    {
        const size_t n = sizeof(__m256i) / sizeof(T);
        T buffer[n];
        _mm256_store_ps(buffer, value);
        for (int i = 0; i < n; i++)
            std::cout << buffer[i] << " ";
    }

    template <unsigned int in, unsigned int out>
    void feedForwardReLu(Matrixf* weights, Matrixf* biases, Matrixf* input, Matrixf* output)
    {
        float* inputPtr   = input->data();
        float* biasesPtr  = biases->data();
        float* weightsPtr = weights->data();
        float* outputPtr  = output->data();

        constexpr uint32_t regSize = 256 / 32;
        constexpr uint32_t numRegs = out / regSize;
        __m256 regs[numRegs];
        const __m256 zero = _mm256_setzero_ps();

        for(uint32_t i = 0; i < numRegs; i++)
        {
            regs[i] = _mm256_load_ps(biasesPtr + i*regSize);
        }

        for(uint32_t i = 0; i < in; i++)
        {
            // // Skip iteration if input is zero
            // // This is valuable as the activation function is ReLu
            // // which sets a large number of inputs to zero
            const float fac = inputPtr[i];

            __m256 factor = _mm256_set1_ps(fac);
            for(uint32_t r = 0; r < numRegs; r++)
            {
                __m256 weight = _mm256_load_ps(weightsPtr + r*regSize + i*numRegs*regSize);
                regs[r] = _mm256_fmadd_ps(weight, factor, regs[r]); // a*b +c
            }
        }

        for(uint32_t i = 0; i < numRegs; i++)
        {
            regs[i] = _mm256_max_ps(zero, regs[i]);
            _mm256_store_ps(outputPtr + i*regSize, regs[i]);
        }
    }

    template <unsigned int in>
    void lastLevelFeedForward(Matrixf* weights, Matrixf* biases, Matrixf* input, Matrixf* output)
    {
        float* inputPtr   = input->data();
        float* weightsPtr = weights->data();
        float* outputPtr  = output->data();

        constexpr uint32_t regSize = 256 / 32;
        constexpr uint32_t numRegs = in / regSize;

        __m256 res = _mm256_setzero_ps();
        for(uint32_t r = 0; r < numRegs; r++)
        {
            __m256 weight = _mm256_load_ps(weightsPtr + r*regSize);
            __m256 inputs = _mm256_load_ps(inputPtr   + r*regSize);
            res = _mm256_fmadd_ps(weight, inputs, res); // a*b +c
        }

        outputPtr[0] = biases->data()[0];

        alignas(64) float buffer[regSize];
        _mm256_store_ps(buffer, res);
        for (uint32_t i = 0; i < regSize; i++)
            outputPtr[0] += buffer[i];
    }
}