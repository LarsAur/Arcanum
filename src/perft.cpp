#include <perft.hpp>

using namespace Arcanum;

uint64_t Arcanum::findNumMovesAtDepth(Board& board, uint32_t depth)
{
    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();

    if(numLegalMoves == 0)
    {
        return 0LL;
    }

    if(depth == 1)
    {
        return numLegalMoves;
    }

    uint64_t total = 0LL;
    board.generateCaptureInfo();
    for(int i = 0; i < numLegalMoves; i++)
    {
        Board newBoard = Board(board);
        newBoard.performMove(legalMoves[i]);
        total += findNumMovesAtDepth(newBoard, depth - 1);
    }

    return total;
}

void Arcanum::perft(Board& board, uint32_t depth)
{
    uint64_t count = 0LL;
    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();

    for(uint8_t i = 0; i < numLegalMoves; i++)
    {
        uint64_t localCount = 0LL;

        if(depth == 1)
        {
            localCount = 1;
        }
        else
        {
            Board newBoard = Board(board);
            newBoard.performMove(legalMoves[i]);
            localCount = findNumMovesAtDepth(newBoard, depth - 1);
        }

        count += localCount;
        std::cout << legalMoves[i] << ": " << localCount << std::endl;
    }

    std::cout << std::endl << "Nodes searched: " << count << std::endl;
}