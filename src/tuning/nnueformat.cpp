#include <tuning/nnueformat.hpp>
#include <utils.hpp>
#include <fstream>

using namespace Arcanum;

#ifdef ENABLE_INCBIN
#define INCBIN_PREFIX
#include <incbin/incbin.hpp>
INCBIN(char, EmbeddedNNUE, TOSTRING(DEFAULT_NNUE));
#endif

static const char* NNUEMagic = "Arcanum FNNUE v6";
static const char* NNUEDescription = "768->1024->1 Quantizable";

NNUEParser::NNUEParser() :
    m_data(nullptr),
    m_dataAllocated(false),
    m_offset(0),
    m_size(0)
{};

NNUEParser::~NNUEParser()
{
    if(m_dataAllocated)
    {
        delete[] m_data;
    }
}

uint32_t NNUEParser::m_getU32()
{
    uint32_t u32 = *reinterpret_cast<const uint32_t*>(m_data + m_offset);
    m_offset += sizeof(uint32_t);
    return u32;
}

bool NNUEParser::m_readHeader()
{
    DEBUG("Parsing NNUE header")

    uint32_t expMagicSize = strlen(NNUEMagic);

    uint32_t magicSize = m_getU32();
    if(magicSize != expMagicSize)
    {
        ERROR("Mismatching NNUE magic size " << magicSize << " != " << expMagicSize)
        return false;
    }

    std::string magic(m_data + m_offset, expMagicSize);
    m_offset += sizeof(char) * expMagicSize;
    if(magic != NNUEMagic)
    {
        ERROR("Mismatching NNUE magic " << magic << " != " << NNUEMagic)
        return false;
    }

    uint32_t metadataSize = m_getU32();
    std::string metadata(m_data + m_offset, metadataSize);
    m_offset += sizeof(char) * metadataSize;

    DEBUG("Magic: " << magic);
    DEBUG("Metadata: " << metadata)

    return true;
}

bool NNUEParser::load(const std::string& filename)
{
    DEBUG("Reading NNUE: " << filename)
    #ifdef ENABLE_INCBIN
    if(filename == TOSTRING(DEFAULT_NNUE))
    {
        m_offset = 0;
        m_dataAllocated = false;
        m_size = EmbeddedNNUESize;
        m_data = EmbeddedNNUEData;
    } else
    #endif
    {
        std::string path = getWorkPath();
        path += filename;
        std::ifstream ifs (path, std::ios::in | std::ios::binary);
        if(!ifs.is_open())
        {
            ERROR("Unable to open " << path)
            return false;
        }
        // Find the size of the file
        ifs.seekg(0, std::ios::end);
        m_size = ifs.tellg();
        ifs.seekg(0, std::ios::beg);

        // Read the data
        m_offset = 0;
        m_dataAllocated = true;
        m_data = new char[m_size];
        ifs.read(const_cast<char*>(m_data), m_size);
        ifs.close();
    }

    return m_readHeader();
}

NNUEEncoder::NNUEEncoder()
{

}

NNUEEncoder::~NNUEEncoder()
{
    close();
}

void NNUEEncoder::m_writeHeader()
{
    // Get the current time as a string
    constexpr size_t DataTimeLength = 100;
    char dateTime[DataTimeLength];
    time_t now = std::time(nullptr);
    strftime(dateTime, DataTimeLength, "%c", std::localtime(&now));

    // Set the full meta data string
    std::string metadata = std::string(dateTime) + " " + std::string(NNUEDescription);

    // Calculate the data lengths
    uint32_t magicSize = std::strlen(NNUEMagic);
    uint32_t metadataSize = metadata.length();

    // Write the magic
    m_ofs.write(reinterpret_cast<const char*>(&magicSize), sizeof(uint32_t));
    m_ofs.write(NNUEMagic, magicSize);

    // Write the metadata
    m_ofs.write(reinterpret_cast<const char*>(&metadataSize), sizeof(uint32_t));
    m_ofs.write(metadata.c_str(), metadataSize);
}

bool NNUEEncoder::open(const std::string& filename)
{
    m_path = getWorkPath() + filename;
    m_ofs = std::ofstream(m_path, std::ios::out | std::ios::binary);
    if(!m_ofs)
    {
        ERROR("Unable to open " << m_path)
        return false;
    }

    m_writeHeader();
    return true;
}

void NNUEEncoder::write(float* src, uint32_t rows, uint32_t cols)
{
    m_ofs.write(reinterpret_cast<char*>(src), rows*cols*sizeof(float));
}

void NNUEEncoder::close()
{
    if(m_ofs.is_open())
    {
        INFO("Finished writing NNUE to " << m_path)
        m_ofs.close();
    }
}