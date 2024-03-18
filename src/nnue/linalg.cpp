#include <nnue/linalg.hpp>
#include <utils.hpp>
#include <sstream>
#include <iomanip>
#include <memory.hpp>

using namespace NN;

Matrixf::Matrixf(uint32_t rows, uint32_t cols)
{
    m_data = static_cast<float*>(Memory::alignedMalloc(cols * rows * sizeof(float), 64));
    m_rows = rows;
    m_cols = cols;

    if(m_data == nullptr)
    {
        ERROR("Unable to allocate matrix (" << rows << "x" << cols << ")")
    }

    setZero();
}

Matrixf::~Matrixf()
{
    Memory::alignedFree(m_data);
}

void Matrixf::setZero()
{
    for(uint32_t i = 0; i < m_rows * m_cols; i++)
    {
        m_data[i] = 0.0f;
    }
}

void Matrixf::multiply(Matrixf& matrixIn, Matrixf& matrixOut)
{
    // -- Verify the dimensions match
    matrixOut.setZero();
    if(m_cols != matrixIn.m_rows)
    {
        ERROR("Input dimension does not match for "
        << m_rows << "x" << m_cols << " * "
        << matrixIn.m_rows << "x" << matrixIn.m_cols)
    }

    if(m_rows != matrixOut.m_rows)
    {
        ERROR("Output dimension does not match for "
        << m_rows << "x" << m_cols << " * "
        << matrixOut.m_rows << "x" << matrixOut.m_cols)
    }

    if(matrixIn.m_cols != matrixOut.m_cols)
    {
        ERROR("Input/Output dimension does not match for "
        << matrixIn.m_rows << "x" << matrixIn.m_cols << " * "
        << matrixOut.m_rows << "x" << matrixOut.m_cols)
    }

    // -- Perform multiplication

    for(uint32_t row = 0; row < m_rows; row++)
    {
        for(uint32_t col = 0; col < matrixIn.m_cols; col++)
        {
            for(uint32_t i = 0; i < m_cols; i++)
            {
                matrixOut.m_data[col * matrixOut.m_rows + row] += m_data[i * m_rows + row] * matrixIn.m_data[col * matrixIn.m_rows + i];
            }
        }
    }
}

// Multiply this vector by the transpose of a sparse binary (0 or 1) vector to produce a matrix
// The input vector should not be transposed
// TODO: For now only for 256 x 768
void Matrixf::vectorMultTransposedSparseVector(Matrixf& tvector, Matrixf& matrixOut)
{
    if(m_cols != 1)
    {
        ERROR("This is not a vector")
    }

    if(tvector.m_cols != 1)
    {
        ERROR("Input is not a vector")
    }

    if(matrixOut.m_cols != tvector.m_rows || matrixOut.m_rows != m_rows)
    {
        ERROR("Output matrix dimensions does not match")
    }

    for(uint32_t i = 0; i < 256; i+=32)
    {

        __m256 weights = _mm256_load_ps(m_data + i);

        for(uint32_t j = 0; j < 768; j++)
        {
            float f = tvector.m_data[j];
            if(f == 0)
            {
                _mm256_store_ps(matrixOut.m_data + matrixOut.m_rows * j + i, _mm256_setzero_ps());
            }
            else
            {
                // __m256 factor = _mm256_set1_ps(1);
                // __m256 res = _mm256_mul_ps(weights, factor);
                _mm256_store_ps(matrixOut.m_data + matrixOut.m_rows * j + i, weights);
            }
        }
    }
}

void Matrixf::scale(float scalar)
{
    for(uint32_t i = 0; i < m_cols * m_rows; i++)
    {
        m_data[i] *= scalar;
    }
}

void Matrixf::add(Matrixf& matrix)
{
    if((matrix.m_rows != m_rows) || (matrix.m_cols != m_cols))
    {
        ERROR("Cannot add mismatching matrix "
        << matrix.m_rows << "x" << matrix.m_cols << " + "
        << m_rows << "x" << m_cols)
    }

    for(uint32_t i = 0; i < m_cols * m_rows; i++)
    {
        m_data[i] += matrix.m_data[i];
    }
}

void Matrixf::transpose()
{
    float* transpose = static_cast<float*>(Memory::alignedMalloc(m_cols * m_rows * sizeof(float), 64));

    if(transpose == nullptr)
    {
        ERROR("Unable to allocate matrix (" << m_cols << "x" << m_rows << ")")
    }

    for(uint32_t col = 0; col < m_cols; col++)
    {
        for(uint32_t row = 0; row < m_rows; row++)
        {
            transpose[row * m_cols + col] = m_data[col * m_rows + row];
        }
    }

    Memory::alignedFree(m_data);
    m_data = transpose;

    uint32_t tmp = m_rows;
    m_rows = m_cols;
    m_cols = tmp;
}

float* Matrixf::data()
{
    return m_data;
}

void Matrixf::reluPrime()
{
    if(m_cols != 1)
        ERROR("Should not use relu on matrixf other than vector")

    for(uint32_t row = 0; row < m_rows; row++)
    {
        m_data[row] = m_data[row] > 0 ? 1.0f : 0.0f;
    }
}

void Matrixf::reluClamp()
{
    if(m_cols != 1)
        ERROR("Should not use relu on matrixf other than vector")

    for(uint32_t row = 0; row < m_rows; row++)
    {
        // Clamp the values between 0 and 128 emulating int8_t
        m_data[row] = std::max(m_data[row], 0.0f);
    }
}

void Matrixf::randomize(const float min, const float max)
{
    for(uint32_t i = 0; i < m_cols * m_rows; i++)
    {
        float norm = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
        m_data[i] = min + (norm * (max - min));
    }
}

void Matrixf::hadamard(Matrixf& matrixIn, Matrixf& matrixOut)
{
    if((matrixIn.m_rows != m_rows) || (matrixIn.m_cols != m_cols))
    {
        ERROR("Cannot calculate hadamard of mismatching matrix "
        << matrixIn.m_rows << "x" << matrixIn.m_cols << " + "
        << m_rows << "x" << m_cols)
    }

    if((matrixOut.m_rows != m_rows) || (matrixOut.m_cols != m_cols))
    {
        ERROR("Cannot calculate hadamard to mismatching output "
        << matrixOut.m_rows << "x" << matrixOut.m_cols << " + "
        << m_rows << "x" << m_cols)
    }

    for(uint32_t i = 0; i < m_cols * m_rows; i++)
    {
        matrixOut.m_data[i] = m_data[i] * matrixIn.m_data[i];
    }
}

void Matrixf::hadamard(Matrixf& matrixIn)
{
    if((matrixIn.m_rows != m_rows) || (matrixIn.m_cols != m_cols))
    {
        ERROR("Cannot calculate hadamard of mismatching matrix "
        << matrixIn.m_rows << "x" << matrixIn.m_cols << " + "
        << m_rows << "x" << m_cols)
    }

    for(uint32_t i = 0; i < m_cols * m_rows; i++)
    {
        m_data[i] = m_data[i] * matrixIn.m_data[i];
    }
}

void Matrixf::set(uint32_t row, uint32_t col, float value)
{
    if(row >= m_rows)
        ERROR("Row out of bounds")
    if(col >= m_cols)
        ERROR("Col out of bounds")

    m_data[col * m_rows + row] = value;
}

void Matrixf::log()
{
    for(uint32_t row = 0; row < m_rows; row++)
    {
        std::stringstream ss;
        ss.precision(2);
        for(uint32_t col = 0; col < m_cols; col++)
        {
            ss << std::setfill(' ') << std::setw(3) << m_data[col * m_rows + row];
        }
        LOG(ss.str())
    }
}