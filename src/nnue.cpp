#include <nnue.hpp>

using namespace Arcanum;

const char* NNUE::QNNUE_MAGIC = "Arcanum QNNUE";

// Calculate the feature indices of the board with the white perspective
// To the the feature indices of the black perspective, xor the indices with 1
uint32_t NNUE::getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective)
{
    if(pieceColor == BLACK)
    {
        pieceSquare = ((7 - RANK(pieceSquare)) << 3) | FILE(pieceSquare);
    }

    return (((uint32_t(pieceType) << 6) | uint32_t(pieceSquare)) << 1) | (pieceColor ^ perspective);
}


void NNUE::load(const std::string filename)
{
    LOG("Loading QNNUE: " << filename)

    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.is_open())
    {
        ERROR("Unable to open file: " << filename);
        return;
    }

    // Check the NNUE magic string
    std::string magic;
    magic.resize(strlen(NNUE::QNNUE_MAGIC));
    ifs.read(magic.data(), strlen(NNUE::QNNUE_MAGIC));
    if(NNUE::QNNUE_MAGIC != magic)
    {
        ERROR("Invalid magic number in file: " << filename);
        ifs.close();
        return;
    }

    // Read the net
    ifs.read((char*) m_net.ftWeights, sizeof(NNUE::Net::ftWeights));
    ifs.read((char*) m_net.ftBiases,  sizeof(NNUE::Net::ftBiases));
    ifs.read((char*) m_net.l1Weights, sizeof(NNUE::Net::l1Weights));
    ifs.read((char*) m_net.l1Biases,  sizeof(NNUE::Net::l1Biases));
    ifs.read((char*) m_net.l2Weights, sizeof(NNUE::Net::l2Weights));
    ifs.read((char*) m_net.l2Biases,  sizeof(NNUE::Net::l2Biases));

    LOG("Finished loading QNNUE: " << filename)
}