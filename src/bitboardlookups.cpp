#include <bitboardlookups.hpp>
#include <intrinsics.hpp>
#include <algorithm>

using namespace Arcanum;

bitboard_t BitboardLookups::betweens[64][64];
bitboard_t BitboardLookups::knightMoves[64];
bitboard_t BitboardLookups::kingMoves[64];
#ifdef USE_BMI2
bitboard_t BitboardLookups::rookOccupancyMask[64];
bitboard_t BitboardLookups::rookMoves[64][1 << 12];
bitboard_t BitboardLookups::bishopOccupancyMask[64];
bitboard_t BitboardLookups::bishopMoves[64][1 << 12];
#else
bitboard_t BitboardLookups::rookFileMoves[8 * (1 << 6)];
bitboard_t BitboardLookups::rookRankMoves[8 * (1 << 6)];
bitboard_t BitboardLookups::bishopMoves[8 * (1 << 6)];
bitboard_t BitboardLookups::diagonal[64];
bitboard_t BitboardLookups::antiDiagonal[64];
#endif

void generateBetweensLookups()
{
    for(square_t from = 0; from < 64; from++)
    {
        uint8_t fromFile = FILE(from);
        uint8_t fromRank = RANK(from);

        for(square_t to = 0; to < 64; to++)
        {
            uint8_t toFile = FILE(to);
            uint8_t toRank = RANK(to);

            // If not on the same rank, file or diagonal, set to zero.
            BitboardLookups::betweens[from][to] = 0LL;

            // Check if to and from are on the same rank or file
            if(toFile == fromFile)
            {
                uint8_t start = std::min(toRank, fromRank);
                uint8_t end   = std::max(toRank, fromRank);
                for(uint8_t i = start + 1; i < end; i++)
                    BitboardLookups::betweens[from][to] |= SQUARE_BB(toFile, i);
            }
            else if(toRank == fromRank)
            {
                uint8_t start = std::min(toFile, fromFile);
                uint8_t end   = std::max(toFile, fromFile);
                for(uint8_t i = start + 1; i < end; i++)
                    BitboardLookups::betweens[from][to] |= SQUARE_BB(i, toRank);
            }
            // Check if to and from are on the same diagonal
            else if(std::abs(toFile - fromFile) == std::abs(toRank - fromRank))
            {
                uint8_t x1, y1, y2; // x1 is always less than 'x2'
                if(fromFile < toFile)
                {
                    x1 = fromFile;   y1 = fromRank;
                    y2 = toRank;
                }
                else
                {
                    x1 = toFile;       y1 = toRank;
                    y2 = fromRank;
                }

                if(y1 < y2) // Up right
                {
                    uint8_t d = y2 - y1;
                    for(uint8_t i = 1; i < d; i++)
                        BitboardLookups::betweens[from][to] |= SQUARE_BB(x1 + i, y1 + i);
                }
                else // Down right
                {
                    uint8_t d = y1 - y2;
                    for(uint8_t i = 1; i < d; i++)
                        BitboardLookups::betweens[from][to] |= SQUARE_BB(x1 + i, y1 - i);
                }
            }
        }
    }
}

void generateKnightLookups()
{
    // Source: https://www.chessprogramming.org/Knight_Pattern
    for(bitboard_t i = 0; i < 64; i++)
    {
        // Get what file the knight is on
        uint8_t file = FILE(i);
        bool A  = file == 0;
        bool AB = file <= 1;
        bool H  = file == 7;
        bool GH = file >= 6;

        // Knight bitboard
        bitboard_t kbb = 1LL << i;

        // Knight attack bitboard
        // Note: The moves going to rank -1 and rank 8 are removed due to overflow
        bitboard_t kabb = (!H  ? (kbb << 17) : 0) |
                          (!GH ? (kbb << 10) : 0) |
                          (!GH ? (kbb >>  6) : 0) |
                          (!H  ? (kbb >> 15) : 0) |
                          (!A  ? (kbb << 15) : 0) |
                          (!AB ? (kbb <<  6) : 0) |
                          (!AB ? (kbb >> 10) : 0) |
                          (!A  ? (kbb >> 17) : 0);

        BitboardLookups::knightMoves[i] = kabb;
    }
}

void generateKingLookups()
{
    for(bitboard_t i = 0; i < 64; i++)
    {
        // Get what file the king is on
        bool A = FILE(i) == 0;
        bool H = FILE(i) == 7;

        // King bitboard
        bitboard_t kbb = 1LL << i;

        // King move bitboard
        bitboard_t kmbb = (!H  ? (kbb << 1) : 0) |
                          (!H  ? (kbb << 9) : 0) |
                          (!H  ? (kbb >> 7) : 0) |
                          (!A  ? (kbb >> 1) : 0) |
                          (!A  ? (kbb >> 9) : 0) |
                          (!A  ? (kbb << 7) : 0) |
                          ((kbb << 8) | (kbb >> 8));

        BitboardLookups::kingMoves[i] = kmbb;
    }
}

void generateRookLookups()
{
#ifdef USE_BMI2
    // Create rook occupancy mask
    constexpr static bitboard_t fileA = 0x0001010101010100LL; // All all file squares excluding edges
    constexpr static bitboard_t rank1 = 0x000000000000007ELL; // All all rank squares excluding edges
    for(int rank = 0; rank < 8; rank++)
    {
        for(int file = 0; file < 8; file++)
        {
            uint8_t idx = SQUARE(file, rank);
            // Or together the file and rank masks
            BitboardLookups::rookOccupancyMask[idx] =  fileA << file;
            BitboardLookups::rookOccupancyMask[idx] |= rank1 << (rank * 8);
            // Remove the rook square from the  mask, as it does not matter.
            // This makes it a little more flexible allowing for creating a mask even if the rook is not actually there
            BitboardLookups::rookOccupancyMask[idx] &= ~(1LL << idx);
        }
    }

    // Create move bitboard for every combination of occupancy
    for(int rookIdx = 0; rookIdx < 64; rookIdx++)
    {
        uint8_t file = FILE(rookIdx);
        uint8_t rank = RANK(rookIdx);

        // For each combination of occupancy on the file
        for(uint64_t fileOcc = 0; fileOcc < (1 << 6); fileOcc++)
        {
            bitboard_t fileMove = 0LL;
            // Loop over all squares except the rook square
            // Check if there is a blocker
            // Escape after to include potential capture
            // j is bitshift to represent the center squares
            for(int k = file+1; k < 8; k++)
            {
                fileMove |= (1LL << k);
                if((fileOcc << 1) & (1 << k)) break;
            }
            for(int k = file-1; k >= 0; k--)
            {
                fileMove |= (1LL << k);
                if((fileOcc << 1) & (1LL << k)) break;
            }


            for(uint64_t rankOcc = 0; rankOcc < (1 << 6); rankOcc++)
            {
                bitboard_t rankMove = 0LL;
                // Loop over all squares except the rook square
                // Check if there is a blocker
                // Escape after to include potential capture
                // j is bitshift to represent the center squares
                for(int k = rank+1; k < 8; k++)
                {
                    rankMove |= (1LL << (k << 3));
                    if((rankOcc << 1) & (1 << k)) break;
                }

                for(int k = rank-1; k >= 0; k--)
                {
                    rankMove |= (1LL << (k << 3));
                    if((rankOcc << 1) & (1LL << k)) break;
                }

                // Construct occupancy bitboard, which is used to generate the correct index
                // This is needed because the order of the occupancy bits are not contiguous rank file bits
                // The bits are interleaved when using the _pext_u64 intrinsic.
                bitboard_t occupancy = (fileOcc << 1) << (rank * 8); // add file
                for(int m = 0; m < 6; m++)
                {
                    //           Mth bit of rankOcc       To M+1th rank    To file
                    occupancy |= (((rankOcc >> m) & 1) << ((m+1) << 3)) << file;
                }
                occupancy &= ~(1LL << rookIdx); // Remove rook from occupancy
                bitboard_t occupancyIdx = PEXT(occupancy, BitboardLookups::rookOccupancyMask[rookIdx]);
                BitboardLookups::rookMoves[rookIdx][occupancyIdx] = (fileMove << (rank * 8)) | (rankMove << file) ;
            }
        }
    }
#else
    // For each rook positon
    for(int i = 0; i < 8; i++)
    {
        // For each combination of pieces
        for(int j = 0; j < (1 << 6); j++)
        {
            bitboard_t fileMove = 0LL;
            bitboard_t rankMove = 0LL;
            // Loop over all squares except the rook square
            for(int k = i+1; k < 8; k++)
            {
                fileMove |= (1 << k);
                rankMove |= (1LL << (k << 3));
                // Check if there is a blocker
                // Escape after to include potential capture
                // j is bitshift to represent the center squares
                if((j << 1) & (1 << k)) break;
            }

            for(int k = i-1; k >= 0; k--)
            {
                fileMove |= (1 << k);
                rankMove |= (1LL << (k << 3));
                // Check if there is a blocker
                // Escape after to include potential capture
                // j is bitshift to represent the center squares
                if((j << 1) & (1 << k)) break;
            }

            rookFileMoves[(i << 6) | j] = fileMove;
            rookRankMoves[(i << 6) | j] = rankMove;
        }
    }
#endif
}

void generateBishopLookups()
{
    #ifdef USE_BMI2
        // Generate occupancy mask
        for(int i = 0; i < 64; i++)
        {
            int rank = RANK(i);
            int file = FILE(i);

            for(int j = 1; j < 7; j++)
            {
                if(rank + j < 7 && file + j < 7)
                {
                    BitboardLookups::bishopOccupancyMask[i] |= SQUARE_BB(file + j, rank + j);
                }

                if(rank - j >=1 && file - j >=1)
                {
                    BitboardLookups::bishopOccupancyMask[i] |= SQUARE_BB(file - j, rank - j);
                }

                if(rank + j < 7 && file - j >= 1)
                {
                    BitboardLookups::bishopOccupancyMask[i] |= SQUARE_BB(file - j, rank + j);
                }

                if(rank - j >= 1 && file + j < 7)
                {
                    BitboardLookups::bishopOccupancyMask[i] |= SQUARE_BB(file + j, rank - j);
                }
            }
        }

        for(int i = 0; i < 64; i++)
        {
            int rank = RANK(i);
            int file = FILE(i);

            int numBits = CNTSBITS(BitboardLookups::bishopOccupancyMask[i]);

            // For each combination of the occupation mask
            for(int occupancyIdx = 0; occupancyIdx < (1LL << numBits); occupancyIdx++)
            {
                bitboard_t mask = BitboardLookups::bishopOccupancyMask[i];
                bitboard_t occupancy = 0LL;
                for(int k = 0; k < numBits; k++)
                {
                    int maskIdx = popLS1B(&mask);
                    // If bit is set in the occupancyIdx, include it in the occupancy
                    if((occupancyIdx >> k) & 1)
                    {
                        occupancy |= 1LL << maskIdx;
                    }
                }

                // Calculate the bishop moves
                bitboard_t bishopMove = 0LL;

                // Up right
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank + k >= 8 || file + k >= 8) break;

                    bitboard_t bb = SQUARE_BB(file + k, rank + k);
                    bishopMove |= bb;

                    // Check if occupied
                    if(occupancy & bb) break;
                }

                // Down left
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank - k < 0 || file - k < 0) break;

                    bitboard_t bb = SQUARE_BB(file - k, rank - k);
                    bishopMove |= bb;

                    // Check if occupied
                    if(occupancy & bb) break;
                }

                // Up left
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank + k >= 8 || file - k < 0) break;

                    bitboard_t bb = SQUARE_BB(file - k, rank + k);
                    bishopMove |= bb;

                    // Check if occupied
                    if(occupancy & bb) break;
                }

                // Down right
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank - k < 0 || file + k >= 8) break;

                    bitboard_t bb = SQUARE_BB(file + k, rank - k);
                    bishopMove |= bb;

                    // Check if occupied
                    if(occupancy & bb) break;
                }

                BitboardLookups::bishopMoves[i][occupancyIdx] = bishopMove;
            }
        }

    #else
    // Generate diagonals
    for(int i = 0; i < 64; i++)
    {
        int rank = RANK(i);
        int file = FILE(i);
        diagonal[i] = 0LL;
        antiDiagonal[i] = 0LL;

        for(int j = 1; j < 8; j++)
        {
            if(rank + j < 8 && file + j < 8)
            {
                diagonal[i] |= SQUARE_BB(file + k, rank + k);
            }
            if(rank - j >=0 && file - j >=0)
            {
                diagonal[i] |= SQUARE_BB(file - k, rank - k);
            }

            if(rank + j < 8 && file - j >= 0)
            {
                antiDiagonal[i] |= SQUARE_BB(file + k, rank - k);
            }
            if(rank - j >= 0 && file + j < 8)
            {
                antiDiagonal[i] |= SQUARE_BB(file + k, rank - k);
            }
        }
    }

    // Generate bishop moves
    for(int i = 0; i < 64; i++)
    {
        int rank = RANK(i);
        int file = FILE(i);

        for(int j = 0; j < (1 << 6); j++)
        {
            bitboard_t bishopMove = 0LL;

            // Up right
            for(int k = 1; k < 8; k++)
            {
                // Check if outside the board
                if(rank + k >= 8 || file + k >= 8) break;

                bishopMove |= SQUARE_BB(file + k, rank + k);

                // Check if occupied
                if((j << 1) & (1 << (file + k))) break;
            }

            // Down left
            for(int k = 1; k < 8; k++)
            {
                // Check if outside the board
                if(rank - k < 0 || file - k < 0) break;

                bishopMove |= SQUARE_BB(file - k, rank - k);

                // Check if occupied
                if((j << 1) & (1 << (file - k))) break;
            }

            // Up left
            for(int k = 1; k < 8; k++)
            {
                // Check if outside the board
                if(rank + k >= 8 || file - k < 0) break;

                bishopMove |= SQUARE_BB(file - k, rank + k);

                // Check if occupied
                if((j << 1) & (1 << (file - k))) break;
            }

            // Down right
            for(int k = 0; k < 8; k++)
            {
                // Check if outside the board
                if(rank - k < 0 || file + k >= 8) break;

                bishopMove |= SQUARE_BB(file + k, rank - k);

                // Check if occupied
                if((j << 1) & (1 << (file + k))) break;
            }

            bishopMoves[(file << 6) | j] |= bishopMove;
        }
    }
    #endif
}

void Arcanum::BitboardLookups::generateBitboardLookups()
{
    generateBetweensLookups();
    generateKnightLookups();
    generateKingLookups();
    generateRookLookups();
    generateBishopLookups();
}