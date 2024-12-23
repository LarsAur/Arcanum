#include <nnue.hpp>

using namespace Arcanum;

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