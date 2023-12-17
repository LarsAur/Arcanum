#include <chessutils.hpp>
#include <iostream>
#include <bitset>
#include <board.hpp>

namespace Arcanum
{
    bitboard_t betweens[64][64];
    void initGenerateBetweens()
    {
        for(uint8_t from = 0; from < 64; from++)
        {
            uint8_t fromFile = from & 0b111;
            uint8_t fromRank = from >> 3;
            for(uint8_t to = 0; to < 64; to++)
            {
                uint8_t toFile = to & 0b111;
                uint8_t toRank = to >> 3;

                // If not on the same rank, file or diagonal, set to zero.
                betweens[from][to] = 0LL;

                // Check if to and from are on the same rank or file
                if(toFile == fromFile)
                {
                    uint8_t start = std::min(toRank, fromRank);
                    uint8_t end   = std::max(toRank, fromRank);
                    for(uint8_t i = start + 1; i < end; i++)
                        betweens[from][to] |= (1LL << toFile) << (i << 3);
                }
                else if(toRank == fromRank)
                {
                    uint8_t start = std::min(toFile, fromFile);
                    uint8_t end   = std::max(toFile, fromFile);
                    for(uint8_t i = start + 1; i < end; i++)
                        betweens[from][to] |= (1LL << i) << (toRank << 3);
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
                            betweens[from][to] |= (1LL << (x1 + i)) << ((y1 + i) << 3);
                    }
                    else // Down right
                    {
                        uint8_t d = y1 - y2;
                        for(uint8_t i = 1; i < d; i++)
                            betweens[from][to] |= (1LL << (x1 + i)) << ((y2 - i) << 3);
                    }
                }
            }
        }
    }

    bitboard_t knightAttacks[64];
    void initGenerateKnightAttacks()
    {
        // Source: https://www.chessprogramming.org/Knight_Pattern
        for(bitboard_t i = 0; i < 64; i++)
        {
            // Get what file the knight is on
            int file = i & 0b111;
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

            knightAttacks[i] = kabb;
        }
    }

    bitboard_t kingMoves[64];
    void initGenerateKingMoves()
    {
        for(bitboard_t i = 0; i < 64; i++)
        {
            // Get what file the king is on
            bool A  = (i & 0b111) == 0;
            bool H  = (i & 0b111) == 7;

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

            kingMoves[i] = kmbb;
        }
    }

#ifdef USE_BMI2
    bitboard_t rookOccupancyMask[64];
    bitboard_t rookMoves[64][1 << 12]; // 12 occupancy bits for 6 file and 6 for rank
#else
    // Moves along rank (horizontal)
    bitboard_t rookFileMoves[8 * (1 << 6)];
    // Moves along file (vertical)
    bitboard_t rookRankMoves[8 * (1 << 6)];
#endif

    void initGenerateRookMoves()
    {
    #ifdef USE_BMI2
        // Create rook occupancy mask
        constexpr static bitboard_t fileA = 0x0001010101010100LL; // All all squares except edges of file
        constexpr static bitboard_t rank1 = 0x000000000000007ELL; // All all squares except edges of rank
        for(int rank = 0; rank < 8; rank++)
        {
            for(int file = 0; file < 8; file++)
            {
                uint8_t idx = file + rank * 8;
                // Or together the file and rank
                rookOccupancyMask[idx] = ((fileA << file) | (rank1 << (rank * 8))) & ~(1LL << idx);
            }
        }

        // Create move bitboard for every combination of occupancy
        for(int rookIdx = 0; rookIdx < 64; rookIdx++)
        {

            uint8_t file = rookIdx & 0b111;
            uint8_t rank = rookIdx >> 3;

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
                    fileMove |= (1 << k);
                    if((fileOcc << 1) & (1 << k)) break;
                }
                for(int k = file-1; k >= 0; k--)
                {
                    fileMove |= (1 << k);
                    if((fileOcc << 1) & (1 << k)) break;
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
                        if((rankOcc << 1) & (1 << k)) break;
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
                    bitboard_t occupancyIdx = _pext_u64(occupancy, rookOccupancyMask[rookIdx]);
                    rookMoves[rookIdx][occupancyIdx] = (fileMove << (rank * 8)) | (rankMove << file) ;
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

    #ifdef USE_BMI2
    bitboard_t bishopMoves[64][1 << 12];
    bitboard_t bishopOccupancyMask[64];
    #else
    // occupancy index is used for one diagonal
    // to get both diagonals, lookup occupancy index of both diagonals
    // individually and use bitwise or with corresponding diagonal mask
    bitboard_t bishopMoves[8 * (1 << 6)] = {0LL};

    // Diagonal bottom left to upper right
    bitboard_t diagonal[64];

    // Diagonal bottom upper left to lower right
    bitboard_t antiDiagonal[64];
    #endif

    void initGenerateBishopMoves()
    {
        #ifdef USE_BMI2
            // Generate occupancy mask
            for(int i = 0; i < 64; i++)
            {
                int rank = i >> 3;
                int file = i & 0b111;

                for(int j = 1; j < 7; j++)
                {
                    if(rank + j < 7 && file + j < 7)
                    {
                        bishopOccupancyMask[i] |= 0b1LL << (((rank + j) << 3) | (file + j));
                    }
                    if(rank - j >=1 && file - j >=1)
                    {
                        bishopOccupancyMask[i] |= 0b1LL << (((rank - j) << 3) | (file - j));
                    }

                    if(rank + j < 7 && file - j >= 1)
                    {
                        bishopOccupancyMask[i] |= 0b1LL << (((rank + j) << 3) | (file - j));
                    }
                    if(rank - j >= 1 && file + j < 7)
                    {
                        bishopOccupancyMask[i] |= 0b1LL << (((rank - j) << 3) | (file + j));
                    }
                }
            }

            for(int i = 0; i < 64; i++)
            {
                int rank = i >> 3;
                int file = i & 0b111;

                int numBits = CNTSBITS(bishopOccupancyMask[i]);

                // For each combination of the occupation mask
                for(int occupancyIdx = 0; occupancyIdx < (1LL << numBits); occupancyIdx++)
                {
                    bitboard_t mask = bishopOccupancyMask[i];
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

                        int bitboardIdx = 8*(rank + k) + file + k;
                        bishopMove |= 0b1LL << bitboardIdx;

                        // Check if occupied
                        if(occupancy & (1LL << bitboardIdx)) break;
                    }

                    // Down left
                    for(int k = 1; k < 8; k++)
                    {
                        // Check if outside the board
                        if(rank - k < 0 || file - k < 0) break;

                        int bitboardIdx = 8*(rank - k) + file - k;
                        bishopMove |= 0b1LL << bitboardIdx;

                        // Check if occupied
                        if(occupancy & (1LL << bitboardIdx)) break;
                    }

                    // Up left
                    for(int k = 1; k < 8; k++)
                    {
                        // Check if outside the board
                        if(rank + k >= 8 || file - k < 0) break;

                        int bitboardIdx = 8*(rank + k) + file - k;
                        bishopMove |= 0b1LL << bitboardIdx;

                        // Check if occupied
                        if(occupancy & (1LL << bitboardIdx)) break;
                    }

                    // Down right
                    for(int k = 1; k < 8; k++)
                    {
                        // Check if outside the board
                        if(rank - k < 0 || file + k >= 8) break;

                        int bitboardIdx = 8*(rank - k) + file + k;
                        bishopMove |= 0b1LL << bitboardIdx;

                        // Check if occupied
                        if(occupancy & (1LL << bitboardIdx)) break;
                    }

                    bishopMoves[i][occupancyIdx] = bishopMove;
                }
            }

        #else
        // Generate diagonals
        for(int i = 0; i < 64; i++)
        {
            int rank = i >> 3;
            int file = i & 0b111;
            diagonal[i] = 0LL;
            antiDiagonal[i] = 0LL;

            for(int j = 1; j < 8; j++)
            {
                if(rank + j < 8 && file + j < 8)
                {
                    diagonal[i] |= 0b1LL << (((rank + j) << 3) | (file + j));
                }
                if(rank - j >=0 && file - j >=0)
                {
                    diagonal[i] |= 0b1LL << (((rank - j) << 3) | (file - j));
                }

                if(rank + j < 8 && file - j >= 0)
                {
                    antiDiagonal[i] |= 0b1LL << (((rank + j) << 3) | (file - j));
                }
                if(rank - j >= 0 && file + j < 8)
                {
                    antiDiagonal[i] |= 0b1LL << (((rank - j) << 3) | (file + j));
                }
            }
        }

        // Generate bishop moves
        for(int i = 0; i < 64; i++)
        {
            int rank = i >> 3;
            int file = i & 0b111;

            for(int j = 0; j < (1 << 6); j++)
            {
                bitboard_t bishopMove = 0LL;

                // Up right
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank + k >= 8 || file + k >= 8) break;

                    int bitboardIdx = (((rank + k) << 3) | (file + k));
                    bishopMove |= 0b1LL << bitboardIdx;

                    // Check if occupied
                    if((j << 1) & (1 << (file + k))) break;
                }

                // Down left
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank - k < 0 || file - k < 0) break;

                    int bitboardIdx = (((rank - k) << 3) | (file - k));
                    bishopMove |= 0b1LL << bitboardIdx;

                    // Check if occupied
                    if((j << 1) & (1 << (file - k))) break;
                }

                // Up left
                for(int k = 1; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank + k >= 8 || file - k < 0) break;

                    int bitboardIdx = (((rank + k) << 3) | (file - k));
                    bishopMove |= 0b1LL << bitboardIdx;

                    // Check if occupied
                    if((j << 1) & (1 << (file - k))) break;
                }

                // Down right
                for(int k = 0; k < 8; k++)
                {
                    // Check if outside the board
                    if(rank - k < 0 || file + k >= 8) break;

                    int bitboardIdx = (((rank - k) << 3) | (file + k));
                    bishopMove |= 0b1LL << bitboardIdx;

                    // Check if occupied
                    if((j << 1) & (1 << (file + k))) break;
                }

                bishopMoves[(file << 6) | j] |= bishopMove;
            }
        }
        #endif
    }
}