#pragma once

#include <string>
#include <cmath>
#include <utils.hpp>

namespace Arcanum
{
    class NNUEParser
    {
        private:
            const char* m_data;
            bool m_dataAllocated;
            uint32_t m_offset;
            uint32_t m_size;

            uint32_t m_getU32();
            bool m_readHeader();
        public:
            NNUEParser();
            ~NNUEParser();
            bool load(const std::string& filename);

            // Reads the values of a matrix as floats, and quantizes it to the type T
            // The value is multiplied by qFactor before quantizing
            template <typename T>
            bool read(T* dst, uint32_t rows, uint32_t cols, int32_t qFactor)
            {
                const uint32_t bytes = sizeof(float) * rows * cols;
                if(m_offset + bytes > m_size)
                {
                    ERROR("Cannot read " << bytes << " bytes from offset " << m_offset << ". Filesize: " << m_size)
                    return false;
                }

                const float* data = reinterpret_cast<const float*>(m_data + m_offset);
                for(uint32_t i = 0; i < rows * cols; i++)
                {
                    if constexpr (std::is_same<T, float>::value)
                    {
                        dst[i] = qFactor * data[i];
                    }
                    else
                    {
                        dst[i] = static_cast<T>(std::round(qFactor * data[i]));
                    }
                }

                m_offset += bytes;
                return true;
            }

            // Reads the values as floats, and quantizes it to the type T
            // The value is multiplied by qFactor before quantizing
            // The matrix is also transposed
            template <typename T>
            bool readTranspose(T* dst, uint32_t rows, uint32_t cols, int32_t qFactor)
            {
                const uint32_t bytes = sizeof(float) * rows * cols;
                if(m_offset + bytes > m_size)
                {
                    ERROR("Cannot read " << bytes << " bytes from offset " << m_offset << ". Filesize: " << m_size)
                    return false;
                }

                const float* data = reinterpret_cast<const float*>(m_data + m_offset);
                for(uint32_t i = 0; i < rows; i++)
                {
                    for(uint32_t j = 0; j < cols; j++)
                    {
                        // Write in row-major order and read in column-major
                        if constexpr (std::is_same<T, float>::value)
                        {
                            dst[i * cols + j] = qFactor * data[j*rows + i];
                        }
                        else
                        {
                            dst[i * cols + j] = static_cast<T>(std::round(qFactor * data[j*rows + i]));
                        }
                    }
                }

                m_offset += bytes;
                return true;
            }
    };

    class NNUEEncoder
    {
        private:
            std::ofstream m_ofs;
            std::string m_path;
            void m_writeHeader();
        public:
            NNUEEncoder();
            ~NNUEEncoder();
            bool open(const std::string& filename);
            void write(float* src, uint32_t rows, uint32_t cols);
            void close();
    };
}