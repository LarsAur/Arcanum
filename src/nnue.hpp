#pragma once

#include <types.hpp>
#include <board.hpp>

namespace Arcanum
{

    class NNUE
    {
        private:



        public:

            struct FeatureSet
            {
                uint8_t numFeatures;
                uint32_t features[32];
            };

            static uint32_t getFeatureIndex(square_t pieceSquare, Color pieceColor, Piece pieceType, Color perspective);
    };

}