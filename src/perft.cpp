#include <perft.hpp>

using namespace Arcanum;

void Test::findNumMovesAtDepth(Board& board, uint32_t depth, uint64_t *count)
{
    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();

    if(numLegalMoves == 0)
    {
        return;
    }

    if(depth == 1)
    {
        *count += numLegalMoves;
        return;
    }

    board.generateCaptureInfo();
    for(int i = 0; i < numLegalMoves; i++)
    {
        Board newBoard = Board(board);
        newBoard.performMove(legalMoves[i]);
        findNumMovesAtDepth(newBoard, depth - 1, count);
    }
}

void Test::perft(std::string fen, uint32_t depth)
{
    Board board = Board(fen);
    perft(board, depth);
}

void Test::perft(Board& board, uint32_t depth)
{
    uint64_t count = 0LL;
    Move* legalMoves = board.getLegalMoves();
    uint8_t numLegalMoves = board.getNumLegalMoves();

    for(uint8_t i = 0; i < numLegalMoves; i++)
    {
        uint64_t localCount = 0LL;

        if(depth == 1)
            localCount = 1;
        else
        {
            Board newBoard = Board(board);
            newBoard.performMove(legalMoves[i]);
            findNumMovesAtDepth(newBoard, depth - 1, &localCount);
        }

        count += localCount;
        std::cout << legalMoves[i] << ": " << localCount << std::endl;
    }

    std::cout << std::endl << "Nodes searched: " << count << std::endl;
}