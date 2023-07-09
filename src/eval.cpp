#include <board.hpp>

using namespace ChessEngine2;

Eval::Eval(uint8_t pawnEvalIndicies, uint8_t materialEvalIndicies)
{
    // Create eval lookup tables
    m_pawnEvalTableSize = 1LL << (pawnEvalIndicies - 1);
    m_materialEvalTableSize = 1LL << (materialEvalIndicies - 1);
    m_pawnEvalTableMask = m_pawnEvalTableSize - 1;
    m_materialEvalTableMask = m_materialEvalTableSize - 1;
    m_pawnEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_pawnEvalTableSize]);
    m_materialEvalTable = std::unique_ptr<evalEntry_t[]>(new evalEntry_t[m_materialEvalTableSize]);

    // Initialize the tables
    for(uint64_t i = 0; i < m_pawnEvalTableSize; i++)
    {
        m_pawnEvalTable[i].hash = 0LL;
        m_pawnEvalTable[i].value = 0;
    }

    for(uint64_t i = 0; i < m_materialEvalTableSize; i++)
    {
        m_materialEvalTable[i].hash = 0LL;
        m_materialEvalTable[i].value = 0;
    }
}

// Evaluates positive value for WHITE
eval_t Eval::evaluate(Board& board)
{
    // Check for stalemate and checkmate
    // TODO: Create function hasLegalMoves
    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0)
    {
        if(board.isChecked(board.m_turn))
        {
            // subtract number of full moves from the score to have incentive for fastest checkmate
            // If there are more checkmates and this is not done, it might be "confused" and move between
            return board.m_turn == WHITE ? (-INT16_MAX + board.m_fullMoves) : (INT16_MAX - board.m_fullMoves);
        }
        return 0;
    }

    eval_t eval = m_getPawnEval(board);
    eval += m_getMaterialEval(board);

    eval_t mobility = board.m_numLegalMoves;
    board.m_turn = Color(board.m_turn ^ 1);
    board.m_numLegalMoves = 0;
    board.getLegalMoves();
    eval_t opponentMobility = board.m_numLegalMoves;
    board.m_turn = Color(board.m_turn ^ 1);
    eval_t mobilityScore = (board.m_turn == WHITE ? (mobility - opponentMobility) : (opponentMobility - mobility));
    eval += mobilityScore;

    return eval;

}

inline eval_t Eval::m_getPawnEval(Board& board)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_pawnHash & m_pawnEvalTableMask;
    evalEntry_t* evalEntry = &m_pawnEvalTable[evalIdx];
    if(evalEntry->hash == board.m_pawnHash)
    {
        // return evalEntry->value;
    }

    // Evaluate the pawns on the board
    bitboard_t wPawns = board.m_bbTypedPieces[W_PAWN][WHITE]; 
    bitboard_t bPawns = board.m_bbTypedPieces[W_PAWN][BLACK]; 

    eval_t pawnScore = 0;
    while (wPawns)
    {
        pawnScore += popLS1B(&wPawns) >> 3;
    }
    
    while (bPawns)
    {
        pawnScore -= (7 - (popLS1B(&bPawns) >> 3));
    }

    pawnScore = pawnScore << 3;

    if(evalEntry->value != pawnScore && evalEntry->hash == board.m_pawnHash)
    {
        CE2_DEBUG("IDX: " << evalIdx)
        CE2_DEBUG(board.getBoardString())
        CE2_DEBUG(evalEntry->boardString)
    }

    // Write the pawn evaluation to the table
    evalEntry->hash = board.m_pawnHash;
    evalEntry->value = pawnScore;
    evalEntry->boardString = board.getBoardString();

    return pawnScore;
}

inline eval_t Eval::m_getMaterialEval(Board& board)
{
    // Check the pawn eval table
    uint64_t evalIdx = board.m_materialHash & m_materialEvalTableMask;
    evalEntry_t* evalEntry = &m_materialEvalTable[evalIdx];
    if(evalEntry->hash == board.m_materialHash)
    {
        // return evalEntry->value;
    }

    // Evaluate the piece count
    eval_t pieceScore;
    pieceScore  = 100 * (CNTSBITS(board.m_bbTypedPieces[W_PAWN][WHITE])   - CNTSBITS(board.m_bbTypedPieces[W_PAWN][BLACK]));
    pieceScore += 300 * (CNTSBITS(board.m_bbTypedPieces[W_KNIGHT][WHITE]) - CNTSBITS(board.m_bbTypedPieces[W_KNIGHT][BLACK]));
    pieceScore += 300 * (CNTSBITS(board.m_bbTypedPieces[W_BISHOP][WHITE]) - CNTSBITS(board.m_bbTypedPieces[W_BISHOP][BLACK]));
    pieceScore += 500 * (CNTSBITS(board.m_bbTypedPieces[W_ROOK][WHITE])   - CNTSBITS(board.m_bbTypedPieces[W_ROOK][BLACK]));
    pieceScore += 900 * (CNTSBITS(board.m_bbTypedPieces[W_QUEEN][WHITE])  - CNTSBITS(board.m_bbTypedPieces[W_QUEEN][BLACK]));

    // Write the pawn evaluation to the table
    evalEntry->hash = board.m_materialHash;
    evalEntry->value = pieceScore;

    return pieceScore;
}