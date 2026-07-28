#include <tuning/dataloader.hpp>
#include <analysis.hpp>
#include <utils.hpp>
#include <stdint.h>
#include <sstream>

using namespace Arcanum;

Analyser::Analyser()
{
    m_evaluations = new uint32_t[EvalIndices];
    ASSERT_OR_EXIT(m_evaluations != nullptr, "Failed to allocate memory for evaluations array");
    clear();
}

Analyser::~Analyser()
{
    delete[] m_evaluations;
}

void Analyser::clear()
{
    for(uint32_t i = 0; i < 2; i++)
    {
        for(uint32_t j = 0; j < 6; j++)
        {
            for(uint32_t k = 0; k < 64; k++)
            {
                m_piecePositions[i][j][k] = 0;
            }
        }
    }

    memset(m_rule50Counts, 0, sizeof(m_rule50Counts));
    memset(m_pieceCounts, 0, sizeof(m_pieceCounts));
    memset(m_evaluations, 0, EvalIndices * sizeof(uint32_t));
    m_analysedPositions = 0;
    m_checkedPositions = 0;
}

void Analyser::analyseDataset(std::string inputPath)
{
    DataLoader loader;
    if(!loader.open(inputPath))
    {
        ERROR("Failed to open input file: " << inputPath)
        return;
    }

    INFO("Analysing dataset: " << inputPath)
    while(!loader.eof())
    {
        Board* board = loader.getNextBoard();
        eval_t eval = loader.getScore();

        m_analysePosition(board);
        m_analyseEvaluation(eval);
        m_analysedPositions++;
    }
    INFO("Finished analysing dataset: " << inputPath << " (" << m_analysedPositions << " positions analysed)")
}

void Analyser::printResults()
{
    INFO("Analysed Positions: " << m_analysedPositions)
    INFO("Checked Positions: " << m_checkedPositions)
    INFO("Piece Counts:")
    for(uint32_t i = 2; i < 33; i++)
    {
        INFO("  " << i << " pieces: " << m_pieceCounts[i])
    }

    INFO("Piece Positions:")
    for(uint32_t opponent = 0; opponent < 2; opponent++)
    {
        for(uint32_t pieceType = 0; pieceType < 6; pieceType++)
        {
            INFO("Opponent: " << opponent << " PieceType:" << pieceType << std::endl << m_getPiecePositions(opponent, static_cast<Piece>(pieceType)))
        }
    }

    INFO("Rule 50 Counts:")
    for(uint32_t i = 0; i < 101; i++)
    {
        uint32_t count = m_rule50Counts[i];
        if(count > 0)
        {
            INFO("  HalfMoveClock: " << i << ", Count: " << count)
        }
    }

    INFO("Evaluation Counts:")
    for(uint32_t i = 0; i < EvalIndices; i++)
    {
        uint32_t count = m_evaluations[i];
        if(count > 0)
        {
            eval_t eval = m_indexToEval(i);
            INFO("  Eval: " << eval << ", Count: " << count)
        }
    }
}

inline uint16_t Analyser::m_evalToIndex(eval_t eval) const
{
    return static_cast<uint16_t>(static_cast<int32_t>(eval) + (EvalIndices / 2));
}

inline eval_t Analyser::m_indexToEval(uint16_t index) const
{
    return static_cast<eval_t>(static_cast<int32_t>(index) - (EvalIndices / 2));
}

void Analyser::m_analyseEvaluation(eval_t eval)
{
    uint16_t evalIndex = m_evalToIndex(eval);
    m_evaluations[evalIndex]++;
}

void Analyser::m_analysePosition(Board* board)
{
    bitboard_t allPieces = board->getAllPieces();
    uint32_t numPieces = CNTSBITS(allPieces);
    Color turn = board->getTurn();

    while(allPieces)
    {
        square_t square = popLS1B(&allPieces);
        Piece pieceType = board->getPieceAt(square);
        bool isOpponent = board->getColorAt(square) != turn;
        m_piecePositions[isOpponent][pieceType][square]++;
    }

    m_checkedPositions += board->isChecked();
    m_pieceCounts[numPieces]++;

    uint16_t halfMoves = board->getHalfMoves();
    if(halfMoves > 100)
    {
        halfMoves = 100;
    }

    m_rule50Counts[halfMoves]++;
}

std::string Analyser::m_getPiecePositions(bool opponent, Piece pieceType) const
{
    std::stringstream ss;
    for(uint32_t square = 0; square < 64; square++)
    {
        uint32_t count = m_piecePositions[opponent][pieceType][square];
        ss << std::setw(10) << std::left << count;
        if((square + 1) % 8 == 0)
        {
            ss << std::endl;
        }
    }
    return ss.str();
}