#include <chessutils.hpp>
#include <iostream>
#include <bitset>

namespace ChessEngine2
{
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

    // 8 for rook position, 1 << 6 for all bit 
    // combinations for the 6 center squares, edges are dont-care
    // Requires to be shifted to the correct file
    // TODO: Could potentially be an array of uint8_t to be more cache efficient
    
    // Moves along rank (horizontal)
    bitboard_t rookFileMoves[8 * (1 << 6)];
    
    // Moves along file (vertical)
    bitboard_t rookRankMoves[8 * (1 << 6)];
    void initGenerateRookMoves()
    {
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
    }

    // occupancy index is used for one diagonal
    // to get both diagonals, lookup occupancy index of both diagonals 
    // individually and use bitwise or with corresponding diagonal mask
    bitboard_t bishopMoves[8 * (1 << 6)] = {0LL};
    
    // Diagonal bottom left to upper right
    bitboard_t diagonal[64];
    
    // Diagonal bottom upper left to lower right
    bitboard_t antiDiagonal[64];
    void initGenerateBishopMoves()
    {
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
    }
}