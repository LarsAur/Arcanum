#pragma once

#include <types.hpp>
#include <utils.hpp>
#include <memory.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <random>

namespace NN
{

    template <uint32_t rows, uint32_t cols>
    class Matrix
    {
        private:
            // Column major matrix entries
            float* m_data;
        public:

            Matrix(bool zero = true)
            {
                m_data = static_cast<float*>(Memory::alignedMalloc(cols * rows * sizeof(float), 64));

                if(m_data == nullptr)
                    ERROR("Unable to allocate matrix (" << rows << "x" << cols << ")")

                if(zero)
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

            void pow2()
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] *= m_data[i];
            }

            void sqrt()
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = std::sqrt(m_data[i]);
            }

            void add(Matrix<rows, cols>& matrix)
            {
                constexpr uint32_t regSize = 256 / 32;
                constexpr uint32_t r = (rows * cols) % regSize;
                constexpr uint32_t m = (rows * cols) - r;

                for(uint32_t i = 0; i < m; i += regSize)
                    _mm256_store_ps(&m_data[i], _mm256_add_ps(_mm256_load_ps(&m_data[i]), _mm256_load_ps(&matrix.m_data[i])));

                for(uint32_t i = m; i < m + r; i++)
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

            void heRandomize()
            {
                // He’s Initialization:
                // https://medium.com/@freshtechyy/weight-initialization-for-deep-neural-network-e0302b6f5bf3
                // https://paperswithcode.com/method/he-initialization
                // Use normal distribution with variance of 2/N.
                // I.e standard deviation of sqrt(2/N).
                // N is number of inputs. I.e #Rows.
                std::default_random_engine generator;
                std::normal_distribution<float> distribution(0.0f, std::sqrt(2.0f / rows));
                for(uint32_t i = 0; i < rows * cols; i++)
                {
                    while(m_data[i] == 0)
                        m_data[i] = distribution(generator);
                }
            }

            void prefetchCol(uint32_t col)
            {
                constexpr uint32_t elementsPerCacheLine = CACHE_LINE_SIZE / sizeof(float);
                float* colStart = m_data + col*rows;

                #pragma GCC unroll 16
                for(uint32_t i = 0; i < rows; i+=elementsPerCacheLine)
                    _mm_prefetch(colStart + i, _MM_HINT_T0);
            }

            void copy(float* ptr)
            {
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = ptr[i];
            }

            void copy(Matrix<rows, cols>& matrix)
            {
                float* data = matrix.data();
                for(uint32_t i = 0; i < cols * rows; i++)
                    m_data[i] = data[i];
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

            void writeToStream(std::ofstream& stream)
            {
                stream.write((char*) m_data, rows * cols * sizeof(float));
            }

            void readFromStream(std::ifstream& stream)
            {
                stream.read((char*) m_data, rows * cols * sizeof(float));
            }
    };

    template <uint32_t rows, uint32_t cols>
    void calcAndAccFtGradient(uint8_t numFeatures, uint32_t* features, Matrix<rows, 1>& delta, Matrix<rows, cols>& gradient)
    {
        constexpr uint32_t regSize = 256 / 32;
        constexpr uint32_t numRegs = rows / regSize;

        float* dData   = delta.data();
        float* gData   = gradient.data();

        __m256 weights[numRegs];

        // Load delta weights
        for(uint32_t i = 0; i < numRegs; i++)
        {
            weights[i] = _mm256_load_ps(dData + i * regSize);
        }

        for(uint8_t k = 0; k < numFeatures; k++)
        {
            uint32_t feature = features[k];

            for(uint32_t i = 0; i < numRegs; i++)
            {
                __m256 segment = _mm256_load_ps(gData + rows * feature + i * regSize);
                segment = _mm256_add_ps(segment, weights[i]);
                _mm256_store_ps(gData + rows * feature + i * regSize, segment);
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