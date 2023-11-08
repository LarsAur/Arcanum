#include <board.hpp>
#include <utils.hpp>
#include <sstream>

using namespace Arcanum;

static std::unordered_map<hash_t, uint8_t, HashFunction> s_boardHistory;

std::unordered_map<hash_t, uint8_t, HashFunction>* Board::getBoardHistory()
{
    return &s_boardHistory;
}

static Zobrist s_zobrist;

Board::Board(const std::string fen)
{
    // -- Create empty board
    m_bbAllPieces = 0LL;
    for(int c = 0; c < NUM_COLORS; c++)
    {
        m_bbColoredPieces[c] = 0LL;
        for(int t = 0; t < 6; t++)
        {
            m_bbTypedPieces[t][c] = 0LL;
        }
    }

    for(int i = 0; i < 64; i++)
    {
        m_pieces[i] = NO_PIECE;
    }

    // -- Create board from FEN
    int file = 0, rank = 7;
    int fenPosition = 0;
    while (!(file > 7 && rank == 0))
    {
        char chr = fen[fenPosition++];

        // -- Move to next rank
        if(chr == '/')
        {
            file = 0;
            rank--;
            continue;
        }

        if (chr > '0' && chr <= '8')
        {
            file += chr - '0';
            continue;
        }

        Color color = BLACK;
        if(chr >= 'A' && chr <= 'Z')
        {
            color = WHITE;
            chr -= 'A' - 'a';
        }

        uint8_t square = ((rank << 3) | file);
        bitboard_t bbSquare = 0b1LL << square;
        file++;

        m_bbColoredPieces[color] |= bbSquare;
        m_bbAllPieces |= bbSquare;

        switch (chr)
        {
        case 'p':
            m_bbTypedPieces[W_PAWN][color] |= bbSquare;
            m_pieces[square] = Piece(W_PAWN + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        case 'r':
            m_bbTypedPieces[W_ROOK][color] |= bbSquare;
            m_pieces[square] = Piece(W_ROOK + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        case 'n':
            m_bbTypedPieces[W_KNIGHT][color] |= bbSquare;
            m_pieces[square] = Piece(W_KNIGHT + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        case 'b':
            m_bbTypedPieces[W_BISHOP][color] |= bbSquare;
            m_pieces[square] = Piece(W_BISHOP + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        case 'k':
            m_bbTypedPieces[W_KING][color] = bbSquare;
            m_pieces[square] = Piece(W_KING + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        case 'q':
            m_bbTypedPieces[W_QUEEN][color] |= bbSquare;
            m_pieces[square] = Piece(W_QUEEN + (B_PAWN - W_PAWN) * uint8_t(color));
            break;
        default:
            ERROR("Unknown piece: " << chr)
            exit(-1);
        }
    }

    // Skip space
    char chr = fen[fenPosition++];
    if(chr != ' ')
    {   
        ERROR("Missing space after board"); 
        exit(-1);
    }

    // Read turn
    chr = fen[fenPosition++];
    if(chr == 'w') m_turn = WHITE;
    else if(chr == 'b') m_turn = BLACK;
    else {
        ERROR("Illegal turn: " << chr); 
        exit(-1);
    }

    // Skip space
    chr = fen[fenPosition++];
    if(chr != ' ')
    {   
        ERROR("Missing space after turn"); 
        exit(-1);
    }

    // Read castle rights
    m_castleRights = 0;
    chr = fen[fenPosition++];
    if(chr != '-')
    {
        int safeGuard = 0;
        while(chr != ' ' && safeGuard < 5)
        {
            switch (chr)
            {
            case 'K': m_castleRights |= WHITE_KING_SIDE; break;
            case 'Q': m_castleRights |= WHITE_QUEEN_SIDE; break;
            case 'k': m_castleRights |= BLACK_KING_SIDE; break;
            case 'q': m_castleRights |= BLACK_QUEEN_SIDE; break;
            default:
                ERROR("Illegal castle right: " << chr); 
                exit(-1);
                break;
            }

            safeGuard++;
            chr = fen[fenPosition++];
        }   
    }
    else
    {
        chr = fen[fenPosition++];
        if(chr != ' ')
        {
            ERROR("Missing space after castle rights"); 
            exit(-1);
        }
    }

    // Read enpassant square
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;
    chr = fen[fenPosition++];
    if(chr != '-')
    {
        int file = chr - 'a';
        int rank = fen[fenPosition++] - '1';

        if(file < 0 || file > 7 || rank < 0 || rank > 7)
        {
            ERROR("Illegal enpassant square"); 
            exit(-1);
        }

        m_enPassantSquare = (rank << 3) | file;
        m_bbEnPassantSquare = 1LL << m_enPassantSquare;
        if(rank == 2)
        {
            m_enPassantTarget = m_enPassantSquare + 8;
            m_bbEnPassantTarget = 1LL << m_enPassantTarget;
        } 
        else
        {
            m_enPassantTarget = m_enPassantSquare - 8;
            m_bbEnPassantTarget = 1LL << m_enPassantTarget;
        }
    }

    // Skip space
    chr = fen[fenPosition++];
    if(chr != ' ')
    {   
        ERROR("Missing space after enpassant square");
        exit(-1);
    }

    chr = fen[fenPosition++];
    if(chr < '0' || chr > '9')
    {   
        ERROR("Number of half moves is not a number: " << chr) 
        exit(-1);
    }

    // Read half moves
    m_rule50 = atoi(fen.c_str() + fenPosition - 1);

    // Skip until space
    while(fen[fenPosition++] != ' ' && fenPosition < (int) fen.length());

    if(fenPosition == (int) fen.length())
    {
        ERROR("Missing number of full moves") 
        exit(-1);
    }

    chr = fen[fenPosition];
    if(chr < '0' || chr > '9')
    {   
        ERROR("Number of full moves is not a number") 
        exit(-1);
    }

    // Read full moves
    m_fullMoves = atoi(fen.c_str() + fenPosition);

    s_zobrist.getHashs(*this, m_hash, m_pawnHash, m_materialHash);
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
}

inline bool Board::m_attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights)
{
    bitboard_t bbFrom = (0b1LL << move.from);
    bitboard_t bbTo = (0b1LL << move.to);
    Color opponent = Color(m_turn ^ 1);

    // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
    if(move.moveInfo & MOVE_INFO_ENPASSANT)
    {
        if((m_enPassantTarget >> 3) == (kingIdx >> 3))
        {
            bitboard_t kingRookMoves = getRookMoves((m_bbAllPieces & ~m_bbEnPassantTarget & ~bbFrom) | bbTo, kingIdx);
            if(kingRookMoves & (m_bbTypedPieces[W_ROOK][opponent] | m_bbTypedPieces[W_QUEEN][opponent]))
                return false;
        }
    }

    if(bbFrom & kingDiagonals)
    {
        // Queens and bishops after removing potential capture 
        bitboard_t queenAndBishops = (m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_BISHOP][opponent]) & ~bbTo;
        // Move the piece and calculate potential diagonal attacks on the king
        bitboard_t diagonalAttacks = getBishopMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((diagonalAttacks & queenAndBishops) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
            return true;
        }

        return false;
    } 
    
    if(bbFrom & kingStraights)
    {
        // Queens and rooks after removing potential capture 
        bitboard_t queenAndRooks = (m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_ROOK][opponent]) & ~bbTo;
        // Move the piece and calculate potential straight attacks on the king
        bitboard_t straightAttacks = getRookMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((straightAttacks & queenAndRooks) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
            return true;
        }

        return false;
    }

    m_legalMoves[m_numLegalMoves++] = move;
    return true;
}

Move* Board::getLegalMovesFromCheck()
{
    m_numLegalMoves = 0;
    Color opponent = Color(m_turn^1);
    bitboard_t bbKing = m_bbTypedPieces[W_KING][m_turn];
    uint8_t kingIdx = LS1B(bbKing);
    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);
    
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
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
    }

    // If there are more than one attacker, the only solution is to move the king
    // If there is only one attacker it is also possible to block or capture
    if(CNTSBITS(attackers) > 1)
    {
        return m_legalMoves;
    }

    // If the attacking piece is a pawn or knight, it is not possible to block
    uint8_t attackerIdx = LS1B(attackers);
    if(knightAttackers | pawnAttackers)
    {
        // -- Knight captures
        bitboard_t capturingKnights = getKnightAttacks(attackerIdx) & m_bbTypedPieces[W_KNIGHT][m_turn];
        while (capturingKnights)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingKnights), attackerIdx, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights);

        // -- Pawn captures 
        bitboard_t capturingPawns;
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(attackers);
        else
            capturingPawns = getWhitePawnAttacks(attackers);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        while (capturingPawns)
        {
            uint8_t pawnIdx = popLS1B(&capturingPawns);
            // Promotion
            if(attackers & 0xff000000000000ffLL)
            {
                // If one promotion move is legal, all are legal
                bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
                if(added)
                {
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                    m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
                }
            }
            else
            {
                m_attemptAddPseudoLegalMove(Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
            }
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
            m_attemptAddPseudoLegalMove(Move(LS1B(capturingPawns), m_enPassantSquare, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_ENPASSANT), kingIdx, kingDiagonals, kingStraights); 
        }

        // -- Rook + Queen captures
        bitboard_t capturingRookMoves = getRookMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingRooks = capturingRookMoves & m_bbTypedPieces[W_ROOK][m_turn];
        bitboard_t capturingRQueens = capturingRookMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingRooks)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRooks), attackerIdx, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights);
        while (capturingRQueens)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRQueens), attackerIdx, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);

        // -- Bishop + Queen captures
        bitboard_t capturingBishopMoves = getBishopMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingBishops = capturingBishopMoves & m_bbTypedPieces[W_BISHOP][m_turn];
        bitboard_t capturingBQueens = capturingBishopMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingBishops)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBishops), attackerIdx, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights);
        while (capturingBQueens)
            m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBQueens), attackerIdx, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);

        return m_legalMoves;
    }

    // -- The attacking piece is a sliding piece (Rook, Bishop or Queen)
    
    // Create a blocking mask
    bitboard_t blockingMask = attackers;
    if(bishopAttackers)
        blockingMask |= kingBishopAttacks & getBishopMoves(m_bbAllPieces, attackerIdx);
    else // rookAttackers
        blockingMask |= kingRookAttacks & getRookMoves(m_bbAllPieces, attackerIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= blockingMask;
        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= blockingMask;
        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= blockingMask;

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= blockingMask;

        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Forward move
    while(pawnMoves)
    {
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);
        
        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & (blockingMask & ~attackers) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & (blockingMask & ~attackers);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
    }

    return m_legalMoves;
}

Move* Board::getLegalMoves()
{
    if(isChecked(m_turn))
    {
        return getLegalMovesFromCheck();
    }

    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t opponentAttacks = getOpponentAttacks();

    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);
    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn];

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn];

        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn];

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn];

        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Forward move
    while(pawnMoves)
    {
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);
        
        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & ~(m_bbAllPieces) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & ~(m_bbAllPieces);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks);
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
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
                m_legalMoves[m_numLegalMoves++] = Move(4, 2, MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_KING_MOVE);

        if(m_castleRights & WHITE_KING_SIDE)
            if(!((m_bbAllPieces | opponentAttacks) & whiteKingCastleMask))
                m_legalMoves[m_numLegalMoves++] = Move(4, 6, MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_KING_MOVE);
    }
    else
    {
        if(m_castleRights & BLACK_QUEEN_SIDE)
            if(!(m_bbAllPieces & blackQueenCastlePieceMask) && !(opponentAttacks & blackQueenCastleAttackMask))
                m_legalMoves[m_numLegalMoves++] = Move(60, 58, MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_KING_MOVE);

        if(m_castleRights & BLACK_KING_SIDE)
            if(!((m_bbAllPieces | opponentAttacks) & blackKingCastleMask))
                m_legalMoves[m_numLegalMoves++] = Move(60, 62, MOVE_INFO_CASTLE_BLACK_KING | MOVE_INFO_KING_MOVE);
    }
    
    return m_legalMoves;
}

// Generates the set of legal captures, checks or moves to get out of check 
// If in check, the existing function for generating legal moves will be used
// Note: Does not include castle
Move* Board::getLegalCaptureAndCheckMoves()
{
    m_numLegalMoves = 0;

    // If in check, the existing function for generating legal moves will be used
    if(isChecked(m_turn))
    {
        return getLegalMovesFromCheck();
    }

    // Everything below is generating moves when not in check, thus we can filter for capturing moves
    Color opponent = Color(m_turn ^ 1);
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);
    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);

    // Positions where pieces can move to set the opponent in check
    uint8_t opponentKingIdx = LS1B(m_bbTypedPieces[W_KING][opponent]);
    bitboard_t knightCheckPositions = getKnightAttacks(opponentKingIdx);
    bitboard_t rookCheckPositions = getRookMoves(m_bbAllPieces, opponentKingIdx);
    bitboard_t bishopCheckPositions = getBishopMoves(m_bbAllPieces, opponentKingIdx);
    
    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | bishopCheckPositions | rookCheckPositions);

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | knightCheckPositions);
        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target,  MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | bishopCheckPositions);

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[opponent] | rookCheckPositions);
        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);
        
        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    bitboard_t opponentAttacks = getOpponentAttacks();
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks) & m_bbColoredPieces[opponent];
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
    }

    return m_legalMoves;
}

Move* Board::getLegalCaptureMoves()
{
    // If in check, the existing function for generating legal moves will be used
    if(isChecked(m_turn))
    {
        return getLegalMovesFromCheck();
    }

    m_numLegalMoves = 0;
    // Everything below is generating moves when not in check, thus we can filter for capturing moves
    Color opponent = Color(m_turn ^ 1);
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);
    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);
    
    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];
        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target,  MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn] & m_bbColoredPieces[opponent];
        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        }
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    bitboard_t opponentAttacks = getOpponentAttacks();
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks) & m_bbColoredPieces[opponent];
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
    }

    return m_legalMoves;
}

// Returns true if a legal move is found
// The point of this function is to have 
// a faster check than having to generate
// all the legal moves when checking for 
// checkmate and stalemate at evaluation
// Note that this function can edit the 
// m_numLegalMoves and should only be called 
// when the legal moves are not already calculated
bool Board::hasLegalMove()
{
    if(isChecked(m_turn))
    {
        return hasLegalMoveFromCheck();
    }

    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t opponentAttacks = getOpponentAttacks();

    // TODO: We can check if the piece is blocking a check, this way we know we cannot move the piece and will not have to test all the moves
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | opponentAttacks);
    if(kMoves) return true;

    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn];

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            if(m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn];

        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            if(m_attemptAddPseudoLegalMove(Move(knightIdx, target, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn];

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            if(m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn];

        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            if(m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
        bool added = m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        if(added) return true;
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
        bool added = m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights);
        if(added) return true;
    }

    // Forward move
    while(pawnMoves)
    {
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);
        // It is not required to check if the move is promotion, we only require to know if the piece can be moved
        bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
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
        bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights);
        if(added) return true;
    }

    // NOTE: It is not required to check for castling as it is only legal if the king kan already move

    return false;
} 

bool Board::hasLegalMoveFromCheck()
{
    m_numLegalMoves = 0;
    Color opponent = Color(m_turn^1);
    bitboard_t bbKing = m_bbTypedPieces[W_KING][m_turn];
    uint8_t kingIdx = LS1B(bbKing);
    bitboard_t kingDiagonals = getBishopMoves(m_bbAllPieces, kingIdx);
    bitboard_t kingStraights = getRookMoves(m_bbAllPieces, kingIdx);
    
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
    uint8_t attackerIdx = LS1B(attackers);
    if(knightAttackers | pawnAttackers)
    {
        // -- Knight captures
        bitboard_t capturingKnights = getKnightAttacks(attackerIdx) & m_bbTypedPieces[W_KNIGHT][m_turn];
        while (capturingKnights)
            if(m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingKnights), attackerIdx, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;

        // -- Pawn captures 
        bitboard_t capturingPawns;
        if(m_turn == Color::WHITE)
            capturingPawns = getBlackPawnAttacks(attackers);
        else
            capturingPawns = getWhitePawnAttacks(attackers);
        capturingPawns = m_bbTypedPieces[W_PAWN][m_turn] & capturingPawns;
        while (capturingPawns)
        {
            uint8_t pawnIdx = popLS1B(&capturingPawns);
            // Dont care if it is a promotion, only checking if the move can be made
            if(m_attemptAddPseudoLegalMove(Move(pawnIdx, attackerIdx, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
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
            bool added = m_attemptAddPseudoLegalMove(Move(LS1B(capturingPawns), m_enPassantSquare, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_ENPASSANT), kingIdx, kingDiagonals, kingStraights); 
            if(added) return true;
        }

        // -- Rook + Queen captures
        bitboard_t capturingRookMoves = getRookMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingRooks = capturingRookMoves & m_bbTypedPieces[W_ROOK][m_turn];
        bitboard_t capturingRQueens = capturingRookMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingRooks)
            if(m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRooks), attackerIdx, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        while (capturingRQueens)
            if(m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingRQueens), attackerIdx, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;

        // -- Bishop + Queen captures
        bitboard_t capturingBishopMoves = getBishopMoves(m_bbAllPieces, attackerIdx);
        bitboard_t capturingBishops = capturingBishopMoves & m_bbTypedPieces[W_BISHOP][m_turn];
        bitboard_t capturingBQueens = capturingBishopMoves & m_bbTypedPieces[W_QUEEN][m_turn];
        while (capturingBishops)
            if(m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBishops), attackerIdx, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        while (capturingBQueens)
            if(m_attemptAddPseudoLegalMove(Move(popLS1B(&capturingBQueens), attackerIdx, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;

        return false;
    }

    // -- The attacking piece is a sliding piece (Rook, Bishop or Queen)
    
    // Create a blocking mask
    bitboard_t blockingMask = attackers;
    if(bishopAttackers)
        blockingMask |= kingBishopAttacks & getBishopMoves(m_bbAllPieces, attackerIdx);
    else // rookAttackers
        blockingMask |= kingRookAttacks & getRookMoves(m_bbAllPieces, attackerIdx);

    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= blockingMask;
        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            if(m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= blockingMask;
        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            if(m_attemptAddPseudoLegalMove(Move(knightIdx, target, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= blockingMask;

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            if(m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= blockingMask;

        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            if(m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
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
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
        if(m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights)) return true;
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
        Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : MOVE_INFO_PAWN_MOVE);
        if(m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights)) return true;
    }

    // Forward move
    while(pawnMoves)
    {
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);

        // Dont care if it is a promotion, only checking if the move can be made
        if(m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & (blockingMask & ~attackers) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & (blockingMask & ~attackers);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        if(m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights)) return true;
    }

    return false;
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
            m_legalMoves[i].moveInfo |= (MOVE_INFO_CAPTURE_PAWN << (targetPiece % B_PAWN));
        }
    }   
}

void Board::performMove(const Move move)
{
    bitboard_t bbFrom = 0b1LL << move.from;
    bitboard_t bbTo = 0b1LL << move.to;

    if(move.moveInfo & MOVE_INFO_CASTLE_MASK)
    {
        if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_QUEEN)
        {
            m_bbTypedPieces[W_ROOK][WHITE] = (m_bbTypedPieces[W_ROOK][WHITE] & ~0x01LL) | 0x08LL;
            m_bbColoredPieces[WHITE] = (m_bbColoredPieces[WHITE] & ~0x01LL) | 0x08LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x01LL) | 0x08LL;
            m_pieces[3] = W_ROOK; 
            m_pieces[0] = NO_PIECE; 
        }else if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_KING)
        {
            m_bbTypedPieces[W_ROOK][WHITE] = (m_bbTypedPieces[W_ROOK][WHITE] & ~0x80LL) | 0x20LL;
            m_bbColoredPieces[WHITE] = (m_bbColoredPieces[WHITE] & ~0x80LL) | 0x20LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x80LL) | 0x20LL;
            m_pieces[5] = W_ROOK; 
            m_pieces[7] = NO_PIECE; 
        } else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_QUEEN)
        {
            m_bbTypedPieces[W_ROOK][BLACK] = (m_bbTypedPieces[W_ROOK][BLACK] & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_bbColoredPieces[BLACK] = (m_bbColoredPieces[BLACK] & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x0100000000000000LL) | 0x0800000000000000LL;
            m_pieces[59] = B_ROOK; 
            m_pieces[56] = NO_PIECE; 
        }else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_KING)
        {
            m_bbTypedPieces[W_ROOK][BLACK] = (m_bbTypedPieces[W_ROOK][BLACK] & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_bbColoredPieces[BLACK] = (m_bbColoredPieces[BLACK] & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_bbAllPieces = (m_bbAllPieces & ~0x8000000000000000LL) | 0x2000000000000000LL;
            m_pieces[61] = B_ROOK; 
            m_pieces[63] = NO_PIECE; 
        }
    }

    if(move.moveInfo & MOVE_INFO_KING_MOVE)
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
    else if(move.moveInfo & MOVE_INFO_ENPASSANT)
    {
        m_pieces[m_enPassantTarget] = NO_PIECE;
        m_bbAllPieces &= ~m_bbEnPassantTarget;
        m_bbColoredPieces[opponent] &= ~m_bbEnPassantTarget;
        m_bbTypedPieces[W_PAWN][opponent] &= ~m_bbEnPassantTarget;
    }

    // Move the pieces
    m_bbAllPieces = (m_bbAllPieces | bbTo) & ~bbFrom;
    m_bbColoredPieces[m_turn] = (m_bbColoredPieces[m_turn] | bbTo) & ~bbFrom;
    if(move.moveInfo & MOVE_INFO_PROMOTE_MASK)
    {
        Piece promoteType = Piece(LS1B(move.moveInfo & MOVE_INFO_PROMOTE_MASK) - 11);
        m_bbTypedPieces[W_PAWN][m_turn] = m_bbTypedPieces[W_PAWN][m_turn] & ~(bbFrom);
        m_bbTypedPieces[promoteType][m_turn] = m_bbTypedPieces[promoteType][m_turn] | bbTo;
        m_pieces[move.to] = Piece(promoteType + B_PAWN * m_turn);
        m_pieces[move.from] = NO_PIECE;
    }
    else
    {
        uint8_t pieceIndex = LS1B(move.moveInfo & MOVE_INFO_MOVE_MASK);
        m_bbTypedPieces[pieceIndex][m_turn] = (m_bbTypedPieces[pieceIndex][m_turn] & ~(bbFrom)) | bbTo;
        m_pieces[move.to] = m_pieces[move.from];
        m_pieces[move.from] = NO_PIECE;
    }

    uint8_t oldEnPassantSquare = m_enPassantSquare;
    // Required to reset
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;
    if(move.moveInfo & MOVE_INFO_DOUBLE_MOVE)
    {
        m_enPassantTarget = move.to;
        m_enPassantSquare = (move.to + move.from) >> 1; // Average of the two squares is the middle
        m_bbEnPassantSquare = 1LL << m_enPassantSquare; 
        m_bbEnPassantTarget = 1LL << m_enPassantTarget; 
    }

    s_zobrist.getUpdatedHashs(*this, move, oldEnPassantSquare, m_enPassantSquare, m_hash, m_pawnHash, m_materialHash);

    m_turn = opponent;
    m_fullMoves += (m_turn == WHITE); // Note: turn is flipped
    if((move.moveInfo & MOVE_INFO_CAPTURE_MASK) || (move.moveInfo & MOVE_INFO_PAWN_MOVE))
        m_rule50 = 0;
    else
        m_rule50++;
}

void Board::performNullMove()
{
    uint8_t oldEnPassantSquare = m_enPassantSquare;
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;

    s_zobrist.getUpdatedHashs(*this, Move(0,0), oldEnPassantSquare, 64, m_hash, m_pawnHash, m_materialHash);

    m_turn = Color(m_turn^1);
    m_rule50++;
}

void Board::addBoardToHistory()
{
    auto it = s_boardHistory.find(m_hash);
    if(it == s_boardHistory.end())
    {
        s_boardHistory.emplace(m_hash, 1);
    }
    else
    {
        it->second += 1;
    }
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

bool Board::isChecked(Color color)
{   
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color opponent = Color(color ^ 1);
    
    // Pawns
    // Get the position of potentially attacking pawns
    bitboard_t pawnAttackPositions = color == WHITE ? getWhitePawnAttacks(m_bbTypedPieces[W_KING][color]) : getBlackPawnAttacks(m_bbTypedPieces[W_KING][color]);
    if(pawnAttackPositions & m_bbTypedPieces[W_PAWN][opponent])
        return true;

    // Knights
    bitboard_t knightAttackPositions = getKnightAttacks(kingIdx);
    if(knightAttackPositions & m_bbTypedPieces[W_KNIGHT][opponent])
        return true;

    return isSlidingChecked(color);
}

inline bool Board::isSlidingChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color opponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_BISHOP][opponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);

    if(diagonalAttacks & queenAndBishops)
        return true;

    bitboard_t queenAndRooks   = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_ROOK][opponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    if(straightAttacks & queenAndRooks)
        return true;

    return false;
}

inline bool Board::isDiagonalChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color opponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_BISHOP][opponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);
    
    return (diagonalAttacks & queenAndBishops) != 0;
}

inline bool Board::isStraightChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color opponent = Color(color ^ 1);

    bitboard_t queenAndRooks   = m_bbTypedPieces[W_QUEEN][opponent] | m_bbTypedPieces[W_ROOK][opponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);

    return (straightAttacks & queenAndRooks) != 0;
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

std::string Board::getBoardString() const
{
    std::stringstream ss;
    ss << "    a b c d e f g h " << std::endl;
    ss << "  +-----------------+" << std::endl;

    for(int y = 7; y >= 0; y--)
    {
        ss << y+1 << " | ";
        for(int x = 0; x < 8; x++)
        {
            bitboard_t mask = (1LL << ((y << 3) | x));
            if(m_bbTypedPieces[W_PAWN][WHITE] & mask)
                ss << "P ";
            else if(m_bbTypedPieces[W_ROOK][WHITE] & mask)
                ss << "R ";
            else if(m_bbTypedPieces[W_KNIGHT][WHITE] & mask)
                ss << "N ";
            else if(m_bbTypedPieces[W_BISHOP][WHITE] & mask)
                ss << "B ";
            else if(m_bbTypedPieces[W_QUEEN][WHITE] & mask)
                ss << "Q ";
            else if(m_bbTypedPieces[W_KING][WHITE] & mask)
                ss << "K ";
            else if(m_bbTypedPieces[W_PAWN][BLACK] & mask)
                ss << "p ";
            else if(m_bbTypedPieces[W_ROOK][BLACK] & mask)
                ss << "r ";
            else if(m_bbTypedPieces[W_KNIGHT][BLACK] & mask)
                ss << "n ";
            else if(m_bbTypedPieces[W_BISHOP][BLACK] & mask)
                ss << "b ";
            else if(m_bbTypedPieces[W_QUEEN][BLACK] & mask)
                ss << "q ";
            else if(m_bbTypedPieces[W_KING][BLACK] & mask)
                ss << "k ";
            else 
                ss << (((x + y) % 2 == 0) ? COLOR_GREEN : COLOR_WHITE) << ". " << COLOR_WHITE;
        }
        ss << "| " << y+1 << std::endl;
    }
    ss << "  +-----------------+" << std::endl;
    ss << "    a b c d e f g h " << std::endl;

    return ss.str();
}

std::string Board::getFEN() const
{
    std::stringstream ss;
    int emptyCnt = 0;
    for(int rank = 7; rank >= 0; rank--)
    {
        for(int file = 0; file < 8; file++)
        {
            char c = '\0';
            switch (m_pieces[file + rank * 8])
            {
            case NO_PIECE: emptyCnt++; break;
            case W_PAWN:    c = 'P'; break;
            case W_ROOK:    c = 'R'; break;
            case W_KNIGHT:  c = 'N'; break;
            case W_BISHOP:  c = 'B'; break;
            case W_QUEEN:   c = 'Q'; break;
            case W_KING:    c = 'K'; break;
            case B_PAWN:    c = 'p'; break;
            case B_ROOK:    c = 'r'; break;
            case B_KNIGHT:  c = 'n'; break;
            case B_BISHOP:  c = 'b'; break;
            case B_QUEEN:   c = 'q'; break;
            case B_KING:    c = 'k'; break;
            }

            if(c != '\0')
            {
                if(emptyCnt > 0) ss << emptyCnt;
                ss << c;
                emptyCnt = 0;
            }
        }

        if(emptyCnt > 0) ss << emptyCnt;
        emptyCnt = 0;
        if(rank != 0) ss << "/";
    }

    // Turn
    ss << ((m_turn == Color::WHITE) ? " w " : " b ");
    
    // Castle
    if(m_castleRights & WHITE_KING_SIDE)  ss << "K";  
    if(m_castleRights & WHITE_QUEEN_SIDE) ss << "Q"; 
    if(m_castleRights & BLACK_KING_SIDE)  ss << "k";  
    if(m_castleRights & BLACK_QUEEN_SIDE) ss << "q"; 
    if(m_castleRights == 0) ss << "-";

    // Enpassant
    if(m_enPassantSquare != 64) ss << " " << getArithmeticNotation(m_enPassantSquare) << " ";
    else ss << " - ";

    // Half moves
    ss << unsigned(m_rule50) << " ";
    // Full moves
    ss << m_fullMoves;

    return ss.str();
}