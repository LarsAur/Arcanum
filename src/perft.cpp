#include <perft.hpp>

using namespace Arcanum;

void Test::findNumMovesAtDepth(Board& board, uint32_t depth, uint64_t *count, bool top)
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

        if(top)
        {
            uint64_t _count = 0;
            findNumMovesAtDepth(newBoard, depth - 1, &_count, false);
            *count += _count;

            std::cout << legalMoves[i] << ": " << _count << std::endl;
        }
        else
        {
            findNumMovesAtDepth(newBoard, depth - 1, count, false);
        }
    }

    if(top)
    {
        std::cout << std::endl << "Nodes searched: " << *count << std::endl;
    }
}

void Test::perft(std::string fen, uint32_t depth)
{
    Board board = Board(fen);
    uint64_t count = 0LL;
    findNumMovesAtDepth(board, depth, &count, true);
}

void Test::perft(Board& board, uint32_t depth)
{
    uint64_t count = 0LL;
    findNumMovesAtDepth(board, depth, &count, true);
}