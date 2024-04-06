#pragma once

#include <inttypes.h>
#include <chessutils.hpp>
#include <utils.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <memory.hpp>

namespace NN
{

    template <uint32_t rows, uint32_t cols>
    class Matrix
    {
        private:
            // Column major matrix entries
            float* m_data;
        public:

            Matrix()
            {
                m_data = static_cast<float*>(Memory::alignedMalloc(cols * rows * sizeof(float), 64));

                if(m_data == nullptr)
                    ERROR("Unable to allocate matrix (" << rows << "x" << cols << ")")

                setZero();
            }

            ~Matrix()
            {
                Memory::alignedFree(m_data);
            }

            float* data()
            {
                return m_data;
            }

            void setZero()
            {
                for(uint32_t i = 0; i < rows * cols; i++)
                    m_data[i] = 0.0f;
            }

            // TODO: there is room for using AVX for all these
            void scale(float scalar)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] *= scalar;
            }

            void pow(float exp)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = std::pow(m_data[i], exp);
            }

            void add(Matrix<rows, cols>& matrix)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] += matrix.m_data[i];
            }

            void addScalar(float scalar)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] += scalar;
            }

            void hadamardInverse(Matrix<rows, cols>& matrixIn)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] /= matrixIn.m_data[i];
            }

            void hadamard(Matrix<rows, cols>& matrixIn)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = m_data[i] * matrixIn.m_data[i];
            }

            void reluPrime()
            {
                if(cols != 1)
                    ERROR("Should not use relu on Matrix other than vector")

                for(uint32_t row = 0; row < rows; row++)
                    m_data[row] = m_data[row] > 0 ? 1.0f : 0.0f;
            }

            void relu()
            {
                if(cols != 1)
                    ERROR("Should not use relu on Matrix other than vector")

                for(uint32_t row = 0; row < rows; row++)
                    m_data[row] = std::max(m_data[row], 0.0f);
            }

            void set(uint32_t row, uint32_t col, float value)
            {
                if(row >= rows)
                    ERROR("Row out of bounds")
                if(col >= cols)
                    ERROR("Col out of bounds")

                m_data[col * rows + row] = value;
            }

            void randomize(const float min, const float max)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                {
                    float norm = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
                    m_data[i] = min + (norm * (max - min));
                }
            }

            void copy(float* ptr)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = ptr[i];
            }

            void copy(Matrix<rows, cols> matrix)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = matrix.data[i];
            }

            void log()
            {
                for(uint32_t row = 0; row < rows; row++)
                {
                    std::stringstream ss;
                    ss.precision(2);
                    for(uint32_t col = 0; col < cols; col++)
                    {
                        ss << std::setfill(' ') << std::setw(3) << m_data[col * rows + row];
                    }
                    LOG(ss.str())
                }
            }
    };

    // Multiply this vector by the transpose of a sparse binary (0 or 1) vector to produce a matrix
    // The input vector should not be transposed
    // TODO: For now only for 128 x 768
    template <uint32_t rows, uint32_t cols>
    void vectorMultTransposedSparseVector(Matrix<rows, 1>& vector, Matrix<cols, 1>& tvector, Matrix<rows, cols>& matrixOut)
    {
        float* vData   = vector.data();
        float* tvData  = tvector.data();
        float* oData   = matrixOut.data();

        for(uint32_t i = 0; i < 128; i+=32)
        {
            __m256 weights = _mm256_load_ps(vData + i);

            for(uint32_t j = 0; j < 768; j++)
            {
                float f = tvData[j];
                if(f == 0)
                {
                    _mm256_store_ps(oData + rows * j + i, _mm256_setzero_ps());
                }
                else
                {
                    _mm256_store_ps(oData + rows * j + i, weights);
                }
            }
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
    void feedForwardReLu(Matrix<out, in>& weights, Matrix<out,1>& biases, Matrix<in,1>& input, Matrix<out,1>& output)
    {
        float* inputPtr   = input.data();
        float* biasesPtr  = biases.data();
        float* weightsPtr = weights.data();
        float* outputPtr  = output.data();

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
    void lastLevelFeedForward(Matrix<1,in>& weights, Matrix<1,1>& biases, Matrix<in,1>& input, Matrix<1,1>& output)
    {
        float* inputPtr   = input.data();
        float* weightsPtr = weights.data();
        float* outputPtr  = output.data();

        constexpr uint32_t regSize = 256 / 32;
        constexpr uint32_t numRegs = in / regSize;

        __m256 res = _mm256_setzero_ps();
        for(uint32_t r = 0; r < numRegs; r++)
        {
            __m256 weight = _mm256_load_ps(weightsPtr + r*regSize);
            __m256 inputs = _mm256_load_ps(inputPtr   + r*regSize);
            res = _mm256_fmadd_ps(weight, inputs, res); // a*b +c
        }

        outputPtr[0] = biases.data()[0];

        alignas(64) float buffer[regSize];
        _mm256_store_ps(buffer, res);
        for (uint32_t i = 0; i < regSize; i++)
            outputPtr[0] += buffer[i];
    }

    template <uint32_t n, uint32_t m, uint32_t p>
    void multiply(Matrix<n, m>& matA, Matrix<m, p>& matB, Matrix<n, p>& matOut)
    {
        float* aData = matA.data();
        float* bData = matB.data();
        float* oData = matOut.data();

        for(uint32_t row = 0; row < n; row++)
        {
            for(uint32_t col = 0; col < p; col++)
            {
                oData[col * n + row] = 0;

                for(uint32_t i = 0; i < m; i++)
                {
                    oData[col * n + row] += aData[i * n + row] * bData[col * m + i];
                }
            }
        }
    }

    // Matrix A will be transposed
    template <uint32_t n, uint32_t m, uint32_t p>
    void multiplyTransposeA(Matrix<m, n>& matA, Matrix<m, p>& matB, Matrix<n, p>& matOut)
    {
        float* aData = matA.data();
        float* bData = matB.data();
        float* oData = matOut.data();

        for(uint32_t row = 0; row < n; row++)
        {
            for(uint32_t col = 0; col < p; col++)
            {
                oData[col * n + row] = 0;

                for(uint32_t i = 0; i < m; i++)
                {
                    oData[col * n + row] += aData[row * m + i] * bData[col * m + i];
                }
            }
        }
    }

    // Matrix B will be transposed
    template <uint32_t n, uint32_t m, uint32_t p>
    void multiplyTransposeB(Matrix<n, m>& matA, Matrix<p, m>& matB, Matrix<n, p>& matOut)
    {
        float* aData = matA.data();
        float* bData = matB.data();
        float* oData = matOut.data();

        for(uint32_t row = 0; row < n; row++)
        {
            for(uint32_t col = 0; col < p; col++)
            {
                oData[col * n + row] = 0;

                for(uint32_t i = 0; i < m; i++)
                {
                    oData[col * n + row] += aData[i * n + row] * bData[i * n + col];
                }
            }
        }
    }

}