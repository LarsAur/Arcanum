#include <board.hpp>
#include <zobrist.hpp>
#include <utils.hpp>
#include <sstream>
#include <fen.hpp>

using namespace Arcanum;

Board::Board(const std::string fen)
{
    if(!FEN::setFEN(*this, fen))
    {
        ERROR("Exit due to FEN error")
        exit(EXIT_FAILURE);
    }
}

Board::Board(const Board& board)
{
    m_hash = board.m_hash;
    m_pawnHash = board.m_pawnHash;
    m_materialHash = board.m_materialHash;
    m_turn = board.m_turn;
    m_rule50 = board.m_rule50;
    m_fullMoves = board.m_fullMoves;
    m_castleRights = board.m_castleRights;
    m_enPassantSquare = board.m_enPassantSquare;
    m_enPassantTarget = board.m_enPassantTarget;
    m_bbEnPassantSquare = board.m_bbEnPassantSquare;
    m_bbEnPassantTarget = board.m_bbEnPassantTarget;
    m_numNonReversableMovesPerformed = board.m_numNonReversableMovesPerformed;

    memcpy(m_pieces, board.m_pieces, sizeof(Piece) * 64);
    m_bbAllPieces = board.m_bbAllPieces;

    m_bbColoredPieces[WHITE]            = board.m_bbColoredPieces[WHITE];
    m_bbTypedPieces[W_PAWN][WHITE]      = board.m_bbTypedPieces[W_PAWN][WHITE];
    m_bbTypedPieces[W_ROOK][WHITE]      = board.m_bbTypedPieces[W_ROOK][WHITE];
    m_bbTypedPieces[W_KNIGHT][WHITE]    = board.m_bbTypedPieces[W_KNIGHT][WHITE];
    m_bbTypedPieces[W_BISHOP][WHITE]    = board.m_bbTypedPieces[W_BISHOP][WHITE];
    m_bbTypedPieces[W_QUEEN][WHITE]     = board.m_bbTypedPieces[W_QUEEN][WHITE];
    m_bbTypedPieces[W_KING][WHITE]      = board.m_bbTypedPieces[W_KING][WHITE];

    m_bbColoredPieces[BLACK]            = board.m_bbColoredPieces[BLACK];
    m_bbTypedPieces[W_PAWN][BLACK]      = board.m_bbTypedPieces[W_PAWN][BLACK];
    m_bbTypedPieces[W_ROOK][BLACK]      = board.m_bbTypedPieces[W_ROOK][BLACK];
    m_bbTypedPieces[W_KNIGHT][BLACK]    = board.m_bbTypedPieces[W_KNIGHT][BLACK];
    m_bbTypedPieces[W_BISHOP][BLACK]    = board.m_bbTypedPieces[W_BISHOP][BLACK];
    m_bbTypedPieces[W_QUEEN][BLACK]     = board.m_bbTypedPieces[W_QUEEN][BLACK];
    m_bbTypedPieces[W_KING][BLACK]      = board.m_bbTypedPieces[W_KING][BLACK];

    m_checkedCache = board.m_checkedCache;
    m_moveset = MoveSet::NOT_GENERATED; // Moves are not copied over
}

// Calulate slider blockers and pinners
// Pinners and blockers are required for both sides because they are used by see().
inline void Board::m_findPinnedPieces()
{
    for(uint8_t i = 0; i < 2; i++)
    {
        bitboard_t snipers;
        bitboard_t occupancy;
        Color c = Color(i);
        m_blockers[c] = 0LL;
        m_pinners[c^1]  = 0LL;
        square_t kingIdx = LS1B(m_bbTypedPieces[Piece::W_KING][c]);

        snipers =  getRookMoves(0LL, kingIdx)
                    & (m_bbTypedPieces[Piece::W_ROOK][c^1]   | m_bbTypedPieces[Piece::W_QUEEN][c^1]);
        snipers |= getBishopMoves(0LL, kingIdx)
                    & (m_bbTypedPieces[Piece::W_BISHOP][c^1] | m_bbTypedPieces[Piece::W_QUEEN][c^1]);

        occupancy = m_bbAllPieces ^ snipers;

        while (snipers)
        {
            square_t sniperIdx = popLS1B(&snipers);
            bitboard_t blockingSquares = getBetweens(kingIdx, sniperIdx) & occupancy;

            if(CNTSBITS(blockingSquares) == 1)
            {
                m_blockers[c]  |= blockingSquares;
                m_pinners[c^1] |= sniperIdx;

                // This does not have to be reset or initialized.
                // It is assumed that the square is known to contain a blocker before lookup.
                // Only update for the corresponding turn, as it is only used by m_attemptAddPseudoLegalMove
                // Does not matter if there are more snipers on the same line
                if(i == m_turn)
                {
                    square_t blockerIdx = LS1B(blockingSquares);
                    m_pinnerBlockerIdxPairs[blockerIdx] = sniperIdx;
                }
            }
        }
    }
}

inline bool Board::m_isLegalEnpassant(Move move, square_t kingIdx)
{
    bitboard_t bbFrom = (0b1LL << move.from);
    bitboard_t bbTo = (0b1LL << move.to);
    Color opponent = Color(m_turn ^ 1);

    // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
    if(move.moveInfo & MoveInfoBit::ENPASSANT)
    {
        if(RANK(m_enPassantTarget) == RANK(kingIdx))
        {
            bitboard_t kingRookMoves = getRookMoves((m_bbAllPieces & ~m_bbEnPassantTarget & ~bbFrom) | bbTo, kingIdx);
            if(kingRookMoves & (m_bbTypedPieces[W_ROOK][opponent] | m_bbTypedPieces[W_QUEEN][opponent]))
                return false;
        }
    }

    if(!(bbFrom & m_blockers[m_turn]))
    {
        return true;
    }

    // Checking that if a blocker is moved, the piece is still blocking.
    // Have to check for blockers still blocking after move
    // TODO: This can be replaced by a lookup table similar to between by containing the entire line
    square_t pinnerIdx = m_pinnerBlockerIdxPairs[move.from];
    if((getBetweens(pinnerIdx, kingIdx) | (1LL << pinnerIdx)) & bbTo)
    {
        return true;
    }

    return false;
}

inline bool Board::m_attemptAddPseudoLegalEnpassant(Move move, square_t kingIdx)
{
    if(m_isLegalEnpassant(move, kingIdx))
    {
        m_legalMoves[m_numLegalMoves++] = move;
        return true;
    }

    return false;
}

inline bool Board::m_isLegalMove(Move move, square_t kingIdx)
{
    bitboard_t bbFrom = (0b1LL << move.from);
    bitboard_t bbTo = (0b1LL << move.to);

    if(!(bbFrom & m_blockers[m_turn]))
    {
        return true;
    }

    // Checking that if a blocker is moved, the piece is still blocking.
    // Have to check for blockers still blocking after move
    // TODO: This can be replaced by a lookup table similar to between by containing the entire line
    square_t pinnerIdx = m_pinnerBlockerIdxPairs[move.from];
    if((getBetweens(pinnerIdx, kingIdx) | (1LL << pinnerIdx)) & bbTo)
    {
        return true;
    }

    return false;
}

inline bool Board::m_attemptAddPseudoLegalMove(Move move, square_t kingIdx)
{
    if(m_isLegalMove(move, kingIdx))
    {
        m_legalMoves[m_numLegalMoves++] = move;
        return true;
    }

    return false;
}

Move* Board::getLegalMovesFromCheck()
{
    m_findPinnedPieces();
    m_numLegalMoves = 0;
    Color opponent = Color(m_turn^1);
    bitboard_t bbKing = m_bbTypedPieces[W_KING][m_turn];
    square_t kingIdx = LS1B(bbKing);

    // -- Check if there are more than one checking piece

    // -- Pawns
    bitboard_t opponentPawns = m_bbTypedPieces[W_PAWN][opponent];
    bitboard_t kingPawnAttacks;
    if(m_turn == Color::WHITE)
        kingPawnAttacks = getWhitePawnAttacks(bbKing);
    else
        kingPawnAttacks = getBlackPawnAttacks(bbKing);
    bitboard_t pawnAttackers = opponentPawns & kingPawnAttacks;

    // -- Knight
    bitboard_t knightAttackers = getKnightAttacks(kingIdx) & m_bbTypedPieces[W_KNIGHT][opponent];

    // -- Rooks + Queen
    bitboard_t rqPieces = m_bbTypedPieces[W_ROOK][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    bitboard_t kingRookAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    bitboard_t rookAttackers = kingRookAttacks & rqPieces;

    // -- Bishop + Queen
    bitboard_t bqPieces = m_bbTypedPieces[W_BISHOP][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    bitboard_t kingBishopAttacks = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t bishopAttackers = kingBishopAttacks & bqPieces;

    bitboard_t attackers = knightAttackers | rookAttackers | bishopAttackers | pawnAttackers;

    // Add king moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | getOpponentAttacks());
    while(kMoves)
    {
        square_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MoveInfoBit::KING_MOVE);
    }

    // If there are more than one attacker, the only solution is to move the king
    // If there is only one attacker it is also possible to block or capture
    if(CNTSBITS(attackers) > 1)
    {
        return m_legalMoves;
    }

    // If the attacking piece is a pawn or knight, it is not possible to block
    square_t attackerIdx = LS1B(attackers);
    if(knightAttackers | pawnAttackers)
    {
        // -- Knight captures
        bitboard_t capturingKnights = getKnightAttacks(attackerIdx) & m_bbTypedPieces[W_KNIGHT][m_turn];
        while (capturingKnights)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingKnights), attackerIdx, MoveInfoBit::KNIGHT_MOVE), kingIdx);

        // -- Pawn captures
        bitboard_t capturingPawns;
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(attackers);
        else
            capturingPawns = getWhitePawnAttacks(attackers);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        while (capturingPawns)
        {
            square_t pawnIdx = popLS1B(&capturingPawns);
            // Promotion
            if(attackers & 0xff000000000000ffLL)
            {
                // If one promotion move is legal, all are legal
                bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
                if(added)
                {
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
                }
            }
            else
            {
                m_attemptAddPseudoLegalMove(Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE), kingIdx);
            }
        }

        // -- Enpassant
        // If There is an enpassant square and the king is in check, the enpassant pawn must be the pawn making the check
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(m_bbEnPassantSquare);
        else
            capturingPawns = getWhitePawnAttacks(m_bbEnPassantSquare);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        while(capturingPawns)
        {
            m_attemptAddPseudoLegalEnpassant(Move(popLS1B(&capturingPawns), m_enPassantSquare, MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::PAWN_MOVE | MoveInfoBit::ENPASSANT), kingIdx);
        }

        // -- Rook + Queen captures
        bitboard_t capturingRookMoves = getRookMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingRooks = capturingRookMoves & m_bbTypedPieces[W_ROOK][m_turn];
        bitboard_t capturingRQueens = capturingRookMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingRooks)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRooks), attackerIdx, MoveInfoBit::ROOK_MOVE), kingIdx);
        while (capturingRQueens)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRQueens), attackerIdx, MoveInfoBit::QUEEN_MOVE), kingIdx);

        // -- Bishop + Queen captures
        bitboard_t capturingBishopMoves = getBishopMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingBishops = capturingBishopMoves & m_bbTypedPieces[W_BISHOP][m_turn];
        bitboard_t capturingBQueens = capturingBishopMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingBishops)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBishops), attackerIdx, MoveInfoBit::BISHOP_MOVE), kingIdx);
        while (capturingBQueens)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBQueens), attackerIdx, MoveInfoBit::QUEEN_MOVE), kingIdx);

        return m_legalMoves;
    }

    // -- The attacking piece is a sliding piece (Rook, Bishop or Queen)

    // Create a blocking mask, consisting of all squares in which pieces can move to block attackers
    bitboard_t blockingMask = attackers | getBetweens(attackerIdx, kingIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= blockingMask;
        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= blockingMask;
        while(knightMoves)
        {
            square_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target, MoveInfoBit::KNIGHT_MOVE), kingIdx);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= blockingMask;

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= blockingMask;

        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx);
        }
    }

    // Pawn moves
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= blockingMask & ~attackers;
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= blockingMask & ~attackers;
        pawnMovesOrigin = getWhitePawnMoves(pawnMoves);
    }

    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksRight &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksRight &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    // Forward move
    while(pawnMoves)
    {
        square_t target = popLS1B(&pawnMoves);
        square_t pawnIdx = popLS1B(&pawnMovesOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE), kingIdx);
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & (blockingMask & ~attackers) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & (blockingMask & ~attackers);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::DOUBLE_MOVE | MoveInfoBit::PAWN_MOVE), kingIdx);
    }

    return m_legalMoves;
}

Move* Board::getLegalMoves()
{
    // Safeguard against repeated calls to generate moves
    if(m_moveset == MoveSet::ALL)
    {
        return m_legalMoves;
    }
    m_moveset = MoveSet::ALL;

    if(isChecked())
    {
        return getLegalMovesFromCheck();
    }

    m_findPinnedPieces();
    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t opponentAttacks = getOpponentAttacks();

    square_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn];

        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn];

        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target, MoveInfoBit::KNIGHT_MOVE), kingIdx);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn];

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn];

        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx);
        }
    }

    // Pawn moves
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMovesOrigin = getWhitePawnMoves(pawnMoves);
    }

    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    // Forward move
    while(pawnMoves)
    {
        square_t target = popLS1B(&pawnMoves);
        square_t pawnIdx = popLS1B(&pawnMovesOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE), kingIdx);
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & ~(m_bbAllPieces) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & ~(m_bbAllPieces);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::DOUBLE_MOVE | MoveInfoBit::PAWN_MOVE), kingIdx);
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks);
    while(kMoves)
    {
        square_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MoveInfoBit::KING_MOVE);
    }

    // Castle
    static constexpr bitboard_t whiteQueenCastlePieceMask   = 0x0ELL;
    static constexpr bitboard_t whiteQueenCastleAttackMask  = 0x0CLL;
    static constexpr bitboard_t whiteKingCastleMask         = 0x60LL;
    static constexpr bitboard_t blackQueenCastlePieceMask   = 0x0E00000000000000LL;
    static constexpr bitboard_t blackQueenCastleAttackMask  = 0x0C00000000000000LL;
    static constexpr bitboard_t blackKingCastleMask         = 0x6000000000000000LL;

    // The following code assumes that the king is not in check
    // It works by checking that the squares which the rook and the king moves over are free,
    // and that the squares which the king moves over and steps into are not attacked by the opponent
    // Note that for queen-side castle the squares which are required to be free, and the squares which are
    // required to not be attacked are different.
    // The fact that the rook and king are in the correct position is handled by the castle-rights flags
    if(m_turn == WHITE)
    {
        if(m_castleRights & WHITE_QUEEN_SIDE)
            if(!(m_bbAllPieces & whiteQueenCastlePieceMask) && !(opponentAttacks & whiteQueenCastleAttackMask))
                m_legalMoves[m_numLegalMoves++] = Move(4, 2, MoveInfoBit::CASTLE_WHITE_QUEEN | MoveInfoBit::KING_MOVE);

        if(m_castleRights & WHITE_KING_SIDE)
            if(!((m_bbAllPieces | opponentAttacks) & whiteKingCastleMask))
                m_legalMoves[m_numLegalMoves++] = Move(4, 6, MoveInfoBit::CASTLE_WHITE_KING | MoveInfoBit::KING_MOVE);
    }
    else
    {
        if(m_castleRights & BLACK_QUEEN_SIDE)
            if(!(m_bbAllPieces & blackQueenCastlePieceMask) && !(opponentAttacks & blackQueenCastleAttackMask))
                m_legalMoves[m_numLegalMoves++] = Move(60, 58, MoveInfoBit::CASTLE_BLACK_QUEEN | MoveInfoBit::KING_MOVE);

        if(m_castleRights & BLACK_KING_SIDE)
            if(!((m_bbAllPieces | opponentAttacks) & blackKingCastleMask))
                m_legalMoves[m_numLegalMoves++] = Move(60, 62, MoveInfoBit::CASTLE_BLACK_KING | MoveInfoBit::KING_MOVE);
    }

    return m_legalMoves;
}

// Generates the set of legal captures, checks or moves to get out of check
// If in check, the existing function for generating legal moves will be used
// Note: Does not include castle
Move* Board::getLegalCaptureAndCheckMoves()
{
    // Safeguard against repeated calls to generate moves
    if(m_moveset == MoveSet::CAPTURES_AND_CHECKS)
    {
        return m_legalMoves;
    }
    m_moveset = MoveSet::CAPTURES_AND_CHECKS;

    // If in check, the existing function for generating legal moves will be used
    if(isChecked())
    {
        return getLegalMovesFromCheck();
    }

    m_findPinnedPieces();
    m_numLegalMoves = 0;

    // Everything below is generating moves when not in check, thus we can filter for capturing moves
    Color opponent = Color(m_turn ^ 1);
    square_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);

    // Positions where pieces can move to set the opponent in check
    square_t opponentKingIdx = LS1B(m_bbTypedPieces[W_KING][opponent]);
    bitboard_t knightCheckPositions = getKnightAttacks(opponentKingIdx);
    bitboard_t rookCheckPositions = getRookMoves(m_bbAllPieces, opponentKingIdx);
    bitboard_t bishopCheckPositions = getBishopMoves(m_bbAllPieces, opponentKingIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | bishopCheckPositions | rookCheckPositions);

        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | knightCheckPositions);
        while(knightMoves)
        {
            square_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target,  MoveInfoBit::KNIGHT_MOVE), kingIdx);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | bishopCheckPositions);

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | rookCheckPositions);
        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx);
        }
    }

    // Pawn moves
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMoves &= getBlackPawnAttacks(m_bbTypedPieces[W_KING][opponent]);
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMoves &= getWhitePawnAttacks(m_bbTypedPieces[W_KING][opponent]);
        pawnMovesOrigin = getWhitePawnMoves(pawnMoves);
    }

    // Forward move
    while(pawnMoves)
    {
        square_t target = popLS1B(&pawnMoves);
        square_t pawnIdx = popLS1B(&pawnMovesOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE), kingIdx);
        }
    }

    // Pawn captues
    pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    bitboard_t opponentAttacks = getOpponentAttacks();
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks) & m_bbColoredPieces[opponent];
    while(kMoves)
    {
        square_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MoveInfoBit::KING_MOVE);
    }

    return m_legalMoves;
}

Move* Board::getLegalCaptureMoves()
{
    // Safeguard against repeated calls to generate moves
    if(m_moveset == MoveSet::CAPTURES)
    {
        return m_legalMoves;
    }
    m_moveset = MoveSet::CAPTURES;

    // If in check, the existing function for generating legal moves will be used
    if(isChecked())
    {
        return getLegalMovesFromCheck();
    }

    m_findPinnedPieces();
    m_numLegalMoves = 0;
    // Everything below is generating moves when not in check, thus we can filter for capturing moves
    Color opponent = Color(m_turn ^ 1);
    square_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];

        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];
        while(knightMoves)
        {
            square_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target,  MoveInfoBit::KNIGHT_MOVE), kingIdx);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];
        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx);
        }
    }

    // Pawn captues
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[opponent] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN), kingIdx);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
            m_attemptAddPseudoLegalEnpassant(move, kingIdx);
        }
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    bitboard_t opponentAttacks = getOpponentAttacks();
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks) & m_bbColoredPieces[opponent];
    while(kMoves)
    {
        square_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MoveInfoBit::KING_MOVE);
    }

    return m_legalMoves;
}

// Returns true if a legal move is found
// The point of this function is to have
// a faster check than having to generate
// all the legal moves when checking for
// checkmate and stalemate at evaluation
bool Board::hasLegalMove()
{
    if((m_moveset == Board::MoveSet::ALL) || (m_numLegalMoves > 0))
        return m_numLegalMoves > 0;

    if(isChecked())
        return hasLegalMoveFromCheck();

    m_findPinnedPieces();

    // Create bitboard for where the king would be attacked
    bitboard_t opponentAttacks = getOpponentAttacks();
    square_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks);
    if(kMoves) return true;

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn];

        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            if(m_isLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx)) return true;
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn];

        while(knightMoves)
        {
            square_t target = popLS1B(&knightMoves);
            if(m_isLegalMove(Move(knightIdx, target, MoveInfoBit::KNIGHT_MOVE), kingIdx)) return true;
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn];

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            if(m_isLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx)) return true;
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn];

        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            if(m_isLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx)) return true;
        }
    }

    // Pawn moves
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMovesOrigin = getWhitePawnMoves(pawnMoves);
    }

    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[m_turn ^ 1] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
        bool added = m_isLegalEnpassant(move, kingIdx);
        if(added) return true;
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
        bool added = m_isLegalEnpassant(move, kingIdx);
        if(added) return true;
    }

    // Forward move
    while(pawnMoves)
    {
        square_t target = popLS1B(&pawnMoves);
        square_t pawnIdx = popLS1B(&pawnMovesOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        bool added = m_isLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE), kingIdx);
        if(added) return true;
    }

    // NOTE: Double moves have to be checked, as they could block a check
    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & ~(m_bbAllPieces) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & ~(m_bbAllPieces);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        bool added = m_isLegalMove(Move(pawnIdx, target, MoveInfoBit::DOUBLE_MOVE | MoveInfoBit::PAWN_MOVE), kingIdx);
        if(added) return true;
    }

    // NOTE: It is not required to check for castling as it is only legal if the king can already move

    return false;
}

bool Board::hasLegalMoveFromCheck()
{
    if(m_numLegalMoves > 0)
        return true;

    m_findPinnedPieces();
    Color opponent = Color(m_turn^1);
    bitboard_t bbKing = m_bbTypedPieces[W_KING][m_turn];
    square_t kingIdx = LS1B(bbKing);

    // -- Check if there are more than one checking piece

    // -- Pawns
    bitboard_t opponentPawns = m_bbTypedPieces[W_PAWN][opponent];
    bitboard_t kingPawnAttacks;
    if(m_turn == Color::WHITE)
        kingPawnAttacks = getWhitePawnAttacks(bbKing);
    else
        kingPawnAttacks = getBlackPawnAttacks(bbKing);
    bitboard_t pawnAttackers = opponentPawns & kingPawnAttacks;

    // -- Knight
    bitboard_t knightAttackers = getKnightAttacks(kingIdx) & m_bbTypedPieces[W_KNIGHT][opponent];

    // -- Rooks + Queen
    bitboard_t rqPieces = m_bbTypedPieces[W_ROOK][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    bitboard_t kingRookAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    bitboard_t rookAttackers = kingRookAttacks & rqPieces;

    // -- Bishop + Queen
    bitboard_t bqPieces = m_bbTypedPieces[W_BISHOP][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    bitboard_t kingBishopAttacks = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t bishopAttackers = kingBishopAttacks & bqPieces;

    bitboard_t attackers = knightAttackers | rookAttackers | bishopAttackers | pawnAttackers;

    // Add king moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | getOpponentAttacks());
    if(kMoves) return true;

    // If there are more than one attacker, the only solution is to move the king
    // If there is only one attacker it is also possible to block or capture
    if(CNTSBITS(attackers) > 1)
    {
        return false;
    }

    // If the attacking piece is a pawn or knight, it is not possible to block
    square_t attackerIdx = LS1B(attackers);
    if(knightAttackers | pawnAttackers)
    {
        // -- Knight captures
        bitboard_t capturingKnights = getKnightAttacks(attackerIdx) & m_bbTypedPieces[W_KNIGHT][m_turn];
        while (capturingKnights)
            if(m_isLegalMove(Move(popLS1B(&capturingKnights), attackerIdx, MoveInfoBit::KNIGHT_MOVE), kingIdx)) return true;

        // -- Pawn captures
        bitboard_t capturingPawns;
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(attackers);
        else
            capturingPawns = getWhitePawnAttacks(attackers);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        while (capturingPawns)
        {
            square_t pawnIdx = popLS1B(&capturingPawns);
            // Dont care if it is a promotion, only checking if the move can be made
            if(m_isLegalMove(Move(pawnIdx, attackerIdx, MoveInfoBit::PAWN_MOVE), kingIdx)) return true;
        }

        // -- Enpassant
        // If There is an enpassant square and the king is in check, the enpassant pawn must be the pawn making the check
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(m_bbEnPassantSquare);
        else
            capturingPawns = getWhitePawnAttacks(m_bbEnPassantSquare);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        if(capturingPawns)
        {
            bool added = m_isLegalEnpassant(Move(LS1B(capturingPawns), m_enPassantSquare, MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::PAWN_MOVE | MoveInfoBit::ENPASSANT), kingIdx);
            if(added) return true;
        }

        // -- Rook + Queen captures
        bitboard_t capturingRookMoves = getRookMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingRooks = capturingRookMoves & m_bbTypedPieces[W_ROOK][m_turn];
        bitboard_t capturingRQueens = capturingRookMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingRooks)
            if(m_isLegalMove(Move(popLS1B(&capturingRooks), attackerIdx, MoveInfoBit::ROOK_MOVE), kingIdx)) return true;
        while (capturingRQueens)
            if(m_isLegalMove(Move(popLS1B(&capturingRQueens), attackerIdx, MoveInfoBit::QUEEN_MOVE), kingIdx)) return true;

        // -- Bishop + Queen captures
        bitboard_t capturingBishopMoves = getBishopMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingBishops = capturingBishopMoves & m_bbTypedPieces[W_BISHOP][m_turn];
        bitboard_t capturingBQueens = capturingBishopMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingBishops)
            if(m_isLegalMove(Move(popLS1B(&capturingBishops), attackerIdx, MoveInfoBit::BISHOP_MOVE), kingIdx)) return true;
        while (capturingBQueens)
            if(m_isLegalMove(Move(popLS1B(&capturingBQueens), attackerIdx, MoveInfoBit::QUEEN_MOVE), kingIdx)) return true;

        return false;
    }

    // -- The attacking piece is a sliding piece (Rook, Bishop or Queen)

    // Create a blocking mask, consisting of all squares in which pieces can move to block attackers
    bitboard_t blockingMask = attackers | getBetweens(kingIdx, attackerIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        square_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= blockingMask;
        while(queenMoves)
        {
            square_t target = popLS1B(&queenMoves);
            if(m_isLegalMove(Move(queenIdx, target, MoveInfoBit::QUEEN_MOVE), kingIdx)) return true;
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        square_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= blockingMask;
        while(knightMoves)
        {
            square_t target = popLS1B(&knightMoves);
            if(m_isLegalMove(Move(knightIdx, target, MoveInfoBit::KNIGHT_MOVE), kingIdx)) return true;
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        square_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= blockingMask;

        while(bishopMoves)
        {
            square_t target = popLS1B(&bishopMoves);
            if(m_isLegalMove(Move(bishopIdx, target, MoveInfoBit::BISHOP_MOVE), kingIdx)) return true;
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        square_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= blockingMask;

        while(rookMoves)
        {
            square_t target = popLS1B(&rookMoves);
            if(m_isLegalMove(Move(rookIdx, target, MoveInfoBit::ROOK_MOVE), kingIdx)) return true;
        }
    }

    // Pawn moves
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= blockingMask & ~attackers;
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= blockingMask & ~attackers;
        pawnMovesOrigin = getWhitePawnMoves(pawnMoves);
    }

    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin;
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksRight &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7;
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksRight &= (m_bbColoredPieces[opponent] | m_bbEnPassantSquare) & blockingMask;
        pawnAttacksLeftOrigin = pawnAttacksLeft << 9;
        pawnAttacksRightOrigin = pawnAttacksRight << 7;
    }

    while (pawnAttacksLeft)
    {
        square_t target = popLS1B(&pawnAttacksLeft);
        square_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
        if(m_isLegalEnpassant(move, kingIdx)) return true;
    }

    while (pawnAttacksRight)
    {
        square_t target = popLS1B(&pawnAttacksRight);
        square_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE) : MoveInfoBit::PAWN_MOVE);
        if(m_isLegalEnpassant(move, kingIdx)) return true;
    }

    // Forward move
    while(pawnMoves)
    {
        square_t target = popLS1B(&pawnMoves);
        square_t pawnIdx = popLS1B(&pawnMovesOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        if(m_isLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE), kingIdx)) return true;
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & (blockingMask & ~attackers) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & (blockingMask & ~attackers);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        if(m_isLegalMove(Move(pawnIdx, target, MoveInfoBit::DOUBLE_MOVE | MoveInfoBit::PAWN_MOVE), kingIdx)) return true;
    }

    return false;
}

uint8_t Board::numOfficers(Color turn) const
{
    uint8_t numPawns  = CNTSBITS(m_bbTypedPieces[W_PAWN][turn]);
    uint8_t numPieces = CNTSBITS(m_bbColoredPieces[m_turn]);

    // Number of total pieces, subtract pawns and king
    return numPieces - numPawns - 1;
}

bool Board::hasOfficers(Color turn) const
{
    uint8_t numPawns  = CNTSBITS(m_bbTypedPieces[W_PAWN][turn]);
    uint8_t numPieces = CNTSBITS(m_bbColoredPieces[m_turn]);

    // If there are more pieces on the board than the number of pawns and the king
    // There must be at least one officer on the board
    return numPieces > (numPawns + 1);
}

uint8_t Board::getNumLegalMoves() const
{
    return m_numLegalMoves;
}

void Board::generateCaptureInfo()
{
    for(uint8_t i = 0; i < m_numLegalMoves; i++)
    {
        // Set the corresponding capture flag. We do not have to worry about enpassant, as it is already included in the moveInfo
        Piece targetPiece = m_pieces[m_legalMoves[i].to];
        if(targetPiece != NO_PIECE)
        {
            m_legalMoves[i].moveInfo |= (MoveInfoBit::CAPTURE_PAWN << (targetPiece % B_PAWN));
        }
    }
}

void Board::performMove(const Move move)
{
    bitboard_t bbFrom = 0b1LL << move.from;
    bitboard_t bbTo = 0b1LL << move.to;

    if(CASTLE_SIDE(move.moveInfo))
    {
        if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_QUEEN)
        {
            m_bbTypedPieces[W_ROOK][WHITE] = (m_bbTypedPieces[W_ROOK][WHITE] & ~0x01LL) | 0x08LL;
            m_bbColoredPieces[WHITE] = (m_bbColoredPieces[WHITE] & ~0x01LL) | 0x08LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x01LL) | 0x08LL;
            m_pieces[3] = W_ROOK;
            m_pieces[0] = NO_PIECE;
        }else if(move.moveInfo & MoveInfoBit::CASTLE_WHITE_KING)
        {
            m_bbTypedPieces[W_ROOK][WHITE] = (m_bbTypedPieces[W_ROOK][WHITE] & ~0x80LL) | 0x20LL;
            m_bbColoredPieces[WHITE] = (m_bbColoredPieces[WHITE] & ~0x80LL) | 0x20LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x80LL) | 0x20LL;
            m_pieces[5] = W_ROOK;
            m_pieces[7] = NO_PIECE;
        } else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_QUEEN)
        {
            m_bbTypedPieces[W_ROOK][BLACK] = (m_bbTypedPieces[W_ROOK][BLACK] & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_bbColoredPieces[BLACK] = (m_bbColoredPieces[BLACK] & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_pieces[59] = B_ROOK;
            m_pieces[56] = NO_PIECE;
        }else if(move.moveInfo & MoveInfoBit::CASTLE_BLACK_KING)
        {
            m_bbTypedPieces[W_ROOK][BLACK] = (m_bbTypedPieces[W_ROOK][BLACK] & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_bbColoredPieces[BLACK] = (m_bbColoredPieces[BLACK] & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_pieces[61] = B_ROOK;
            m_pieces[63] = NO_PIECE;
        }
    }

    if(move.moveInfo & MoveInfoBit::KING_MOVE)
    {
        if(m_turn == WHITE)
        {
            m_castleRights &= ~(WHITE_KING_SIDE | WHITE_QUEEN_SIDE);
        }
        else
        {
            m_castleRights &= ~(BLACK_KING_SIDE | BLACK_QUEEN_SIDE);
        }
    }

    if(move.to == 0 || move.from == 0)
    {
        m_castleRights &= ~(WHITE_QUEEN_SIDE);
    }

    if(move.to == 7 || move.from == 7)
    {
        m_castleRights &= ~(WHITE_KING_SIDE);
    }

    if(move.to == 56 || move.from == 56)
    {
        m_castleRights &= ~(BLACK_QUEEN_SIDE);
    }

    if(move.to == 63 || move.from == 63)
    {
        m_castleRights &= ~(BLACK_KING_SIDE);
    }

    // Remove potential captures
    Color opponent = Color(m_turn ^ 1);
    if(m_pieces[move.to] != NO_PIECE)
    {
        m_bbColoredPieces[opponent] &= ~bbTo;
        m_bbTypedPieces[m_pieces[move.to] % B_PAWN][opponent] &= ~bbTo;
    }
    else if(move.moveInfo & MoveInfoBit::ENPASSANT)
    {
        m_pieces[m_enPassantTarget] = NO_PIECE;
        m_bbAllPieces &= ~m_bbEnPassantTarget;
        m_bbColoredPieces[opponent] &= ~m_bbEnPassantTarget;
        m_bbTypedPieces[W_PAWN][opponent] &= ~m_bbEnPassantTarget;
    }

    // Move the pieces
    m_bbAllPieces = (m_bbAllPieces | bbTo) & ~bbFrom;
    m_bbColoredPieces[m_turn] = (m_bbColoredPieces[m_turn] | bbTo) & ~bbFrom;
    if(PROMOTED_PIECE(move.moveInfo))
    {
        Piece promoteType = Piece(LS1B(PROMOTED_PIECE(move.moveInfo)) - 11);
        m_bbTypedPieces[W_PAWN][m_turn] = m_bbTypedPieces[W_PAWN][m_turn] & ~(bbFrom);
        m_bbTypedPieces[promoteType][m_turn] = m_bbTypedPieces[promoteType][m_turn] | bbTo;
        m_pieces[move.to] = Piece(promoteType + B_PAWN * m_turn);
        m_pieces[move.from] = NO_PIECE;
    }
    else
    {
        Piece pieceIndex = Piece(LS1B(MOVED_PIECE(move.moveInfo)));
        m_bbTypedPieces[pieceIndex][m_turn] = (m_bbTypedPieces[pieceIndex][m_turn] & ~(bbFrom)) | bbTo;
        m_pieces[move.to] = m_pieces[move.from];
        m_pieces[move.from] = NO_PIECE;
    }

    square_t oldEnPassantSquare = m_enPassantSquare;
    // Required to reset
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;
    if(move.moveInfo & MoveInfoBit::DOUBLE_MOVE)
    {
        m_enPassantTarget = move.to;
        m_enPassantSquare = (move.to + move.from) >> 1; // Average of the two squares is the middle
        m_bbEnPassantSquare = 1LL << m_enPassantSquare;
        m_bbEnPassantTarget = 1LL << m_enPassantTarget;
    }

    s_zobrist.getUpdatedHashs(*this, move, oldEnPassantSquare, m_enPassantSquare, m_hash, m_pawnHash, m_materialHash);

    m_checkedCache = CheckedCacheState::UNKNOWN;
    m_moveset = MoveSet::NOT_GENERATED;
    m_turn = opponent;
    m_fullMoves += (m_turn == WHITE); // Note: turn is flipped
    if(CAPTURED_PIECE(move.moveInfo) || (move.moveInfo & MoveInfoBit::PAWN_MOVE))
    {
        m_numNonReversableMovesPerformed++;
        m_rule50 = 0;
    }
    else
    {
        m_rule50++;
    }
}

void Board::performNullMove()
{
    square_t oldEnPassantSquare = m_enPassantSquare;
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;

    s_zobrist.updateHashsAfterNullMove(m_hash, m_pawnHash, oldEnPassantSquare);

    m_turn = Color(m_turn^1);
    m_rule50++;
}

hash_t Board::getHash() const
{
    return m_hash;
}

hash_t Board::getPawnHash() const
{
    return m_pawnHash;
}

hash_t Board::getMaterialHash() const
{
    return m_materialHash;
}

uint16_t Board::getFullMoves() const
{
    return m_fullMoves;
}

uint16_t Board::getHalfMoves() const
{
    return m_rule50;
}

uint8_t Board::getNumNonReversableMovesPerformed() const
{
    return m_numNonReversableMovesPerformed;
}

uint8_t Board::getCastleRights() const
{
    return m_castleRights;
}

// Generates a bitboard of all attacks of opponents
// The moves does not check if the move will make the opponent become checked, or if the attack is on its own pieces
// Used for checking if the king is in check after king moves.
bitboard_t Board::getOpponentAttacks()
{
    Color opponent = Color(m_turn ^ 1);

    // Pawns
    bitboard_t attacks = getOpponentPawnAttacks();

    // King
    attacks |= getKingMoves(LS1B(m_bbTypedPieces[W_KING][opponent]));

    // Knight
    bitboard_t tmpKnights = m_bbTypedPieces[W_KNIGHT][opponent];
    while (tmpKnights)
    {
        attacks |= getKnightAttacks(popLS1B(&tmpKnights));
    }

    // Remove the king from the occupied mask such that when it moves, the previous king position will not block
    bitboard_t allPiecesNoKing = m_bbAllPieces & ~m_bbTypedPieces[W_KING][m_turn];

    // Queens and bishops
    bitboard_t tmpBishops = m_bbTypedPieces[W_BISHOP][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    while (tmpBishops)
    {
        attacks |= getBishopMoves(allPiecesNoKing, popLS1B(&tmpBishops));
    }

    // Queens and rooks
    bitboard_t tmpRooks = m_bbTypedPieces[W_ROOK][opponent] | m_bbTypedPieces[W_QUEEN][opponent];
    while (tmpRooks)
    {
        attacks |= getRookMoves(allPiecesNoKing, popLS1B(&tmpRooks));
    }

    return attacks;
}

bitboard_t Board::getOpponentPawnAttacks()
{
    bitboard_t attacks = m_turn == BLACK ? getWhitePawnAttacks(m_bbTypedPieces[W_PAWN][WHITE]) : getBlackPawnAttacks(m_bbTypedPieces[W_PAWN][BLACK]);
    return attacks;
}

bitboard_t Board::getTypedPieces(Piece type, Color color) const
{
    return m_bbTypedPieces[type][color];
}

bitboard_t Board::getColoredPieces(Color color) const
{
    return m_bbColoredPieces[color];
}

Piece Board::getPieceAt(square_t square) const
{
    return m_pieces[square];
}

square_t Board::getEnpassantSquare() const
{
    return m_enPassantSquare;
}

bool Board::isChecked()
{
    // Return cached value is available
    if(m_checkedCache != CheckedCacheState::UNKNOWN)
        return m_checkedCache;

    bitboard_t bbKing = m_bbTypedPieces[W_KING][m_turn];
    square_t kingIdx = LS1B(bbKing);
    Color opponent = Color(m_turn ^ 1);

    // Pawns
    // Get the position of potentially attacking pawns
    bitboard_t pawnAttackPositions = m_turn == WHITE ? getWhitePawnAttacks(bbKing) : getBlackPawnAttacks(bbKing);
    if(pawnAttackPositions & m_bbTypedPieces[W_PAWN][opponent])
    {
        m_checkedCache = CheckedCacheState::CHECKED;
        return true;
    }

    // Knights
    bitboard_t knightAttackPositions = getKnightAttacks(kingIdx);
    if(knightAttackPositions & m_bbTypedPieces[W_KNIGHT][opponent])
    {
        m_checkedCache = CheckedCacheState::CHECKED;
        return true;
    }

    // Bishop or Queen
    bitboard_t queenAndBishops = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_BISHOP][opponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);

    if(diagonalAttacks & queenAndBishops)
    {
        m_checkedCache = CheckedCacheState::CHECKED;
        return true;
    }

    // Rook or Queen
    bitboard_t queenAndRooks   = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_ROOK][opponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    if(straightAttacks & queenAndRooks)
    {
        m_checkedCache = CheckedCacheState::CHECKED;
        return true;
    }

    m_checkedCache = CheckedCacheState::NOT_CHECKED;
    return false;
}


Color Board::getTurn() const
{
    return m_turn;
}

uint8_t Board::getNumPiecesLeft() const
{
    return CNTSBITS(m_bbAllPieces);
}

uint8_t Board::getNumColoredPieces(Color color) const
{
    return CNTSBITS(m_bbColoredPieces[color]);
}

bitboard_t Board::attackersTo(const square_t square) const
{
    bitboard_t attackers = 0LL;
    bitboard_t rooks   = m_bbTypedPieces[Piece::W_ROOK][Color::WHITE]   | m_bbTypedPieces[Piece::W_ROOK][Color::BLACK];
    bitboard_t knights = m_bbTypedPieces[Piece::W_KNIGHT][Color::WHITE] | m_bbTypedPieces[Piece::W_KNIGHT][Color::BLACK];
    bitboard_t bishops = m_bbTypedPieces[Piece::W_BISHOP][Color::WHITE] | m_bbTypedPieces[Piece::W_BISHOP][Color::BLACK];
    bitboard_t queens  = m_bbTypedPieces[Piece::W_QUEEN][Color::WHITE]  | m_bbTypedPieces[Piece::W_QUEEN][Color::BLACK];
    bitboard_t kings   = m_bbTypedPieces[Piece::W_KING][Color::WHITE]   | m_bbTypedPieces[Piece::W_KING][Color::BLACK];

    attackers |= getKnightAttacks(square) & knights;
    attackers |= getRookMoves(m_bbAllPieces, square) & (rooks | queens);
    attackers |= getBishopMoves(m_bbAllPieces, square) & (bishops | queens);
    attackers |= getKingMoves(square) & kings;

    attackers |= getBlackPawnAttacks(1LL << square) & m_bbTypedPieces[Piece::W_PAWN][Color::WHITE];
    attackers |= getWhitePawnAttacks(1LL << square) & m_bbTypedPieces[Piece::W_PAWN][Color::BLACK];

    return attackers;
}

bitboard_t Board::m_getLeastValuablePiece(const bitboard_t mask, const Color color, Piece& piece) const
{
    bitboard_t bbPawns = mask & m_bbTypedPieces[Piece::W_PAWN][color];
    if(bbPawns) { piece = W_PAWN; return 1LL << LS1B(bbPawns); }
    bitboard_t bbKnights = mask & m_bbTypedPieces[Piece::W_KNIGHT][color];
    if(bbKnights) { piece = W_KNIGHT; return 1LL << LS1B(bbKnights); }
    bitboard_t bbBishops = mask & m_bbTypedPieces[Piece::W_BISHOP][color];
    if(bbBishops) { piece = W_BISHOP; return 1LL << LS1B(bbBishops); }
    bitboard_t bbRooks = mask & m_bbTypedPieces[Piece::W_ROOK][color];
    if(bbRooks) { piece = W_ROOK; return 1LL << LS1B(bbRooks); }
    bitboard_t bbQueens = mask & m_bbTypedPieces[Piece::W_QUEEN][color];
    if(bbQueens) { piece = W_QUEEN; return 1LL << LS1B(bbQueens); }
    bitboard_t bbKing = mask & m_bbTypedPieces[Piece::W_KING][color];
    if(bbKing) { piece = W_KING; return 1LL << LS1B(bbKing); }

    piece = NO_PIECE;
    return 0LL;
}

// Static exchange evaluation based on the swap algorithm:
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
bool Board::see(const Move& move) const
{
    // Piece values used bu SEE.
    static constexpr uint16_t values[] = {100, 300, 300, 500, 900, 32000};

    // Note: This also works for enpassant
    Piece target = Piece(LS1B(CAPTURED_PIECE(move.moveInfo)) - 16);
    Piece attacker = Piece(LS1B(MOVED_PIECE(move.moveInfo)));

    // It is always ok to capture equal of higher value pieces
    int16_t swap = values[attacker] - values[target];
    if(swap <= 0)
        return true;

    // Knights and kings cannot cause a discovered attack. (Because they are not on any line containing move.to)
    const bitboard_t bishops = m_bbTypedPieces[Piece::W_BISHOP][Color::WHITE] | m_bbTypedPieces[Piece::W_BISHOP][Color::BLACK];
    const bitboard_t rooks   = m_bbTypedPieces[Piece::W_ROOK][Color::WHITE]   | m_bbTypedPieces[Piece::W_ROOK][Color::BLACK];
    const bitboard_t queens  = m_bbTypedPieces[Piece::W_QUEEN][Color::WHITE]  | m_bbTypedPieces[Piece::W_QUEEN][Color::BLACK];

    bitboard_t bbFrom = 1LL << move.from;
    bitboard_t bbTo   = 1LL << move.to;
    bitboard_t occupancy = m_bbAllPieces ^ bbTo ^ bbFrom;
    Color turn = m_turn;
    bitboard_t attackers = attackersTo(move.to); // Attackers and defenders of the square
    bool result = true;

    while (true)
    {
        turn = Color(turn^1);
        attackers &= occupancy;
        bitboard_t currentAttackers = attackers & m_bbColoredPieces[turn];

        // Break when the side to move has no attackers
        if(!currentAttackers)
            break;

        if(m_pinners[turn^1] & occupancy)
        {
            currentAttackers &= ~m_blockers[turn];

            if(!currentAttackers)
                break;
        }

        result ^= 1;

        // Find the least valuable piece to perform the next capture
        Piece lvp;
        bitboard_t bbLvp = m_getLeastValuablePiece(currentAttackers, turn, lvp);
        swap = values[lvp] - swap;

        if(swap < 0)
            break;

        occupancy ^= bbLvp;

        // Note: Knights cannot reveil new attackers
        if(lvp == Piece::W_PAWN)
            attackers |= getBishopMoves(occupancy, move.to) & (bishops | queens);
        else if(lvp == Piece::W_BISHOP)
            attackers |= getBishopMoves(occupancy, move.to) & (bishops | queens);
        else if(lvp == Piece::W_ROOK)
            attackers |= getRookMoves(occupancy, move.to) & (rooks | queens);
        else if(lvp == Piece::W_QUEEN)
        {
            attackers |= getBishopMoves(occupancy, move.to) & (bishops | queens);
            attackers |= getRookMoves(occupancy, move.to)   & (rooks   | queens);
        }
        else if(lvp == Piece::W_KING)
        {
            return (attackers & m_bbColoredPieces[turn^1]) ? result ^ 1 : result;
        }
    }

    return result;
}