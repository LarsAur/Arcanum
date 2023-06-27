#include <board.hpp>
#include <utils.hpp>
#include <sstream>

using namespace ChessEngine2;

static std::unordered_map<hash_t, uint8_t> s_boardHistory;

std::unordered_map<hash_t, uint8_t>* Board::getBoardHistory()
{
    return &s_boardHistory;
}

static Zobrist s_zobrist;

Board::Board(std::string fen)
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
            std::cout << "Unknown piece: " << chr << std::endl; 
            exit(-1);
        }
    }

    // Skip space
    char chr = fen[fenPosition++];
    if(chr != ' ')
    {   
        std::cout << "Missing space after board" << std::endl; 
        exit(-1);
    }

    // Read turn
    chr = fen[fenPosition++];
    if(chr == 'w') m_turn = WHITE;
    else if(chr == 'b') m_turn = BLACK;
    else {
        std::cout << "Illegal turn: " << chr << std::endl; 
        exit(-1);
    }

    // Skip space
    chr = fen[fenPosition++];
    if(chr != ' ')
    {   
        std::cout << "Missing space after turn" << std::endl; 
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
                std::cout << "Illegal castle right: " << chr << std::endl; 
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
            std::cout << "Missing space after castle rights" << std::endl; 
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
            std::cout << "Illegal enpassant square" << std::endl; 
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
        std::cout << "Missing space after enpassant square" << std::endl; 
        exit(-1);
    }

    chr = fen[fenPosition++];
    if(chr < '0' || chr > '9')
    {   
        std::cout << chr << std::endl; 
        std::cout << "Number of half moves is not a number" << std::endl; 
        exit(-1);
    }

    // Read half moves
    m_halfMoves = atoi(fen.c_str() + fenPosition);

    // Skip until space
    while(fen[fenPosition++] != ' ' && fenPosition < (int) fen.length());

    if(fenPosition == (int) fen.length())
    {
        std::cout << "Missing number of full moves" << std::endl; 
        exit(-1);
    }

    chr = fen[fenPosition];
    if(chr < '0' || chr > '9')
    {   
        std::cout << "Number of full moves is not a number" << std::endl; 
        exit(-1);
    }

    // Read full moves
    m_fullMoves = atoi(fen.c_str() + fenPosition);

    s_zobrist.getHashs(*this, m_hash, m_pawnHash, m_materialHash);
}

Board::Board(const Board& board)
{
    m_hash = board.m_hash;
    m_turn = board.m_turn;
    m_halfMoves = board.m_halfMoves;
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

Board::~Board()
{

}

inline bool Board::m_attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights, bool wasChecked)
{
    bitboard_t bbFrom = (0b1LL << move.from);
    bitboard_t bbTo = (0b1LL << move.to);
    Color oponent = Color(m_turn ^ 1);
    bool added = false;

    // TODO: Remove when having a separate isChecked generator
    if(wasChecked)
    {
        bitboard_t bbAllPieces = m_bbAllPieces;

        bitboard_t bbPiecesWhite  = m_bbColoredPieces[WHITE];
        bitboard_t bbPawnsWhite   = m_bbTypedPieces[W_PAWN][WHITE];
        bitboard_t bbKnightsWhite = m_bbTypedPieces[W_KNIGHT][WHITE];
        bitboard_t bbBishopsWhite = m_bbTypedPieces[W_BISHOP][WHITE];
        bitboard_t bbQueensWhite  = m_bbTypedPieces[W_QUEEN][WHITE];
        bitboard_t bbRooksWhite   = m_bbTypedPieces[W_ROOK][WHITE];

        bitboard_t bbPiecesBlack  = m_bbColoredPieces[BLACK];
        bitboard_t bbPawnsBlack   = m_bbTypedPieces[W_PAWN][BLACK];
        bitboard_t bbKnightsBlack = m_bbTypedPieces[W_KNIGHT][BLACK];
        bitboard_t bbBishopsBlack = m_bbTypedPieces[W_BISHOP][BLACK];
        bitboard_t bbQueensBlack  = m_bbTypedPieces[W_QUEEN][BLACK];
        bitboard_t bbRooksBlack   = m_bbTypedPieces[W_ROOK][BLACK];

        m_bbAllPieces = (m_bbAllPieces | bbTo) & ~bbFrom;
        m_bbColoredPieces[m_turn] = (m_bbColoredPieces[m_turn] | bbTo) & ~bbFrom;

        // TODO: Get piece type as input to enable use of switchcase
        // Move the piece
        if (m_bbTypedPieces[W_PAWN][m_turn] & bbFrom)
        {
            m_bbTypedPieces[W_PAWN][m_turn]   = (m_bbTypedPieces[W_PAWN][m_turn] & ~(bbFrom)) | bbTo;
        }
        else if (m_bbTypedPieces[W_KNIGHT][m_turn] & bbFrom)
        {
            m_bbTypedPieces[W_KNIGHT][m_turn] = (m_bbTypedPieces[W_KNIGHT][m_turn] & ~(bbFrom)) | bbTo;
        }
        else if (m_bbTypedPieces[W_BISHOP][m_turn] & bbFrom)
        {
            m_bbTypedPieces[W_BISHOP][m_turn] = (m_bbTypedPieces[W_BISHOP][m_turn] & ~(bbFrom)) | bbTo;
        } 
        else if (m_bbTypedPieces[W_ROOK][m_turn] & bbFrom)
        {
            m_bbTypedPieces[W_ROOK][m_turn]   = (m_bbTypedPieces[W_ROOK][m_turn] & ~(bbFrom)) | bbTo;
        }
        else if (m_bbTypedPieces[W_QUEEN][m_turn] & bbFrom)
        {
            m_bbTypedPieces[W_QUEEN][m_turn]  = (m_bbTypedPieces[W_QUEEN][m_turn] & ~(bbFrom)) | bbTo;
        }

        Color oponent = Color(m_turn ^ 1);
        
        // Remove potential captures
        bitboard_t bbEnPassantTarget        = (move.moveInfo & MOVE_INFO_ENPASSANT) ? m_bbEnPassantTarget : 0LL;
        m_bbAllPieces                      &= ~bbEnPassantTarget;
        m_bbColoredPieces[oponent]         &= ~(bbTo | bbEnPassantTarget);
        m_bbTypedPieces[W_PAWN][oponent]   &= ~(bbTo | bbEnPassantTarget);
        m_bbTypedPieces[W_KNIGHT][oponent] &= ~bbTo;
        m_bbTypedPieces[W_BISHOP][oponent] &= ~bbTo;
        m_bbTypedPieces[W_QUEEN][oponent]  &= ~bbTo;
        m_bbTypedPieces[W_ROOK][oponent]   &= ~bbTo;

        if(!isChecked(m_turn))
        {
            m_legalMoves[m_numLegalMoves++] = move;
            added = true;
        }

        m_bbAllPieces       = bbAllPieces;

        m_bbColoredPieces[WHITE]          = bbPiecesWhite ;
        m_bbTypedPieces[W_PAWN][WHITE]    = bbPawnsWhite  ;
        m_bbTypedPieces[W_KNIGHT][WHITE]  = bbKnightsWhite;
        m_bbTypedPieces[W_BISHOP][WHITE]  = bbBishopsWhite;
        m_bbTypedPieces[W_QUEEN][WHITE]   = bbQueensWhite ;
        m_bbTypedPieces[W_ROOK][WHITE]    = bbRooksWhite  ;

        m_bbColoredPieces[BLACK]          = bbPiecesBlack ;
        m_bbTypedPieces[W_PAWN][BLACK]    = bbPawnsBlack  ;
        m_bbTypedPieces[W_KNIGHT][BLACK]  = bbKnightsBlack;
        m_bbTypedPieces[W_BISHOP][BLACK]  = bbBishopsBlack;
        m_bbTypedPieces[W_QUEEN][BLACK]   = bbQueensBlack ;
        m_bbTypedPieces[W_ROOK][BLACK]    = bbRooksBlack  ;

        return added;
    }

    if(bbFrom & kingDiagonals)
    {
        // Queens and bishops after removing potential capture 
        bitboard_t queenAndBishops = (m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_BISHOP][oponent]) & ~bbTo;
        // Move the piece and calculate potential diagonal attacks on the king
        bitboard_t diagonalAttacks = getBishopMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((diagonalAttacks & queenAndBishops) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
            return true;
        }
    } 
    else if(bbFrom & kingStraights)
    {
        // Queens and rooks after removing potential capture 
        bitboard_t queenAndRooks = (m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_ROOK][oponent]) & ~bbTo;
        // Move the piece and calculate potential straight attacks on the king
        bitboard_t straightAttacks = getRookMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((straightAttacks & queenAndRooks) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
            return true;
        }
    }
    // It is safe to add the move if it is not potentially a discoverd check
    else 
    {
        m_legalMoves[m_numLegalMoves++] = move;
        return true;
    }

    return false;
}

Move* Board::getLegalMoves()
{
    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t oponentAttacks = getOponentAttacks();

    // Use a different function for generating moves when in check
    bool wasChecked = (m_bbTypedPieces[W_KING][m_turn] & oponentAttacks) != 0LL;

    // TODO: We can check if the piece is blocking a check, this way we know we cannot move the piece and will not have to test all the moves
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);
    bitboard_t kingDiagonals = diagonal[kingIdx] | antiDiagonal[kingIdx];
    bitboard_t kingStraights = (0xffLL << (kingIdx & ~0b111)) | (0x0101010101010101LL << (kingIdx & 0b111));

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
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
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
            m_attemptAddPseudoLegalMove(Move(knightIdx, target, MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
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
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
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
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
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
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // TODO: optimize
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE));
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // TODO:
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE));
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
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
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & ~(m_bbAllPieces) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & ~(m_bbAllPieces);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | oponentAttacks);
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
    }

    // Castle
    static const bitboard_t whiteQueenCastleMask = 0x1fLL;
    static const bitboard_t whiteKingCastleMask  = 0xf0LL;
    static const bitboard_t blackQueenCastleMask = 0x1f00000000000000LL;
    static const bitboard_t blackKingCastleMask  = 0xf000000000000000LL;

    // All possible knight positions where knights can block 
    static const bitboard_t whiteQueensideKnightMask = 0x3e7700LL;
    static const bitboard_t whiteKingsideKnightMask  = 0xf8dc00LL;
    static const bitboard_t blackQueensideKnightMask = 0x773e0000000000LL;
    static const bitboard_t blackKingsideKnightMask  = 0xdcf80000000000LL;

    static const bitboard_t whiteQueensidePawnMask = 0x3e00LL;
    static const bitboard_t whiteKingsidePawnMask  = 0xf800LL;
    static const bitboard_t blackQueensidePawnMask = 0x3e000000000000LL;
    static const bitboard_t blackKingsidePawnMask  = 0xf8000000000000LL;

    // Only test for castle if there are castle rights and correct turn
    // TODO: Can check if the king is in check first as both tests checks for it
    if(m_turn == WHITE)
    {
        if(m_castleRights & WHITE_QUEEN_SIDE)
        {
            // Check that the correct spots are free
            // Not checking if it is a rook and king as the castleRight will ensure this
            if((m_bbAllPieces & whiteQueenCastleMask) == 0x11LL)
            {
                if(!( (m_bbTypedPieces[W_KNIGHT][BLACK] & whiteQueensideKnightMask) | ((m_bbTypedPieces[W_PAWN][BLACK] | m_bbTypedPieces[W_KING][BLACK]) & whiteQueensidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 2) | getBishopMoves(m_bbAllPieces, 3) | getBishopMoves(m_bbAllPieces, 4);
                    if(! ((m_bbTypedPieces[W_BISHOP][BLACK] | m_bbTypedPieces[W_QUEEN][BLACK]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 2) | getRookMoves(m_bbAllPieces, 3) | getRookMoves(m_bbAllPieces, 4);
                        if(! ((m_bbTypedPieces[W_ROOK][BLACK] | m_bbTypedPieces[W_QUEEN][BLACK]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(4, 2, MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_KING_MOVE);
                        }
                    }
                }
            }
        }

        if(m_castleRights & WHITE_KING_SIDE)
        {
            // Check that the correct spots are free
            // Not checking if it is a rook and king as the castleRight will ensure this
            if((m_bbAllPieces & whiteKingCastleMask) == 0x90LL)
            {
                if(!( (m_bbTypedPieces[W_KNIGHT][BLACK] & whiteKingsideKnightMask) | ((m_bbTypedPieces[W_PAWN][BLACK] | m_bbTypedPieces[W_KING][BLACK]) & whiteKingsidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 4) | getBishopMoves(m_bbAllPieces, 5) | getBishopMoves(m_bbAllPieces, 6);
                    if(! ((m_bbTypedPieces[W_BISHOP][BLACK] | m_bbTypedPieces[W_QUEEN][BLACK]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 4) | getRookMoves(m_bbAllPieces, 5) | getRookMoves(m_bbAllPieces, 6);
                        if(! ((m_bbTypedPieces[W_ROOK][BLACK] | m_bbTypedPieces[W_QUEEN][BLACK]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(4, 6, MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_KING_MOVE);
                        }
                    }
                }
            }
        }
    }
    else // m_turn == black
    {
        if(m_castleRights & BLACK_QUEEN_SIDE)
        {
            // Check that the correct spots are free
            // Not checking if it is a rook and king as the castleRight will ensure this
            if((m_bbAllPieces & blackQueenCastleMask) == 0x1100000000000000LL)
            {
                if(!( (m_bbTypedPieces[W_KNIGHT][WHITE] & blackQueensideKnightMask) | ((m_bbTypedPieces[W_PAWN][WHITE] | m_bbTypedPieces[W_KING][WHITE]) & blackQueensidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 58) | getBishopMoves(m_bbAllPieces, 59) | getBishopMoves(m_bbAllPieces, 60);
                    if(! ((m_bbTypedPieces[W_BISHOP][WHITE] | m_bbTypedPieces[W_QUEEN][WHITE]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 58) | getRookMoves(m_bbAllPieces, 59) | getRookMoves(m_bbAllPieces, 60);
                        if(! ((m_bbTypedPieces[W_ROOK][WHITE] | m_bbTypedPieces[W_QUEEN][WHITE]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(60, 58, MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_KING_MOVE);
                        }
                    }
                }
            }
        }

        if(m_castleRights & BLACK_KING_SIDE)
        {
            // Check that the correct spots are free
            // Not checking if it is a rook and king as the castleRight will ensure this
            if((m_bbAllPieces & blackKingCastleMask) == 0x9000000000000000LL)
            {
                if(!( (m_bbTypedPieces[W_KNIGHT][WHITE] & blackKingsideKnightMask) | ((m_bbTypedPieces[W_PAWN][WHITE] | m_bbTypedPieces[W_KING][WHITE]) & blackKingsidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 60) | getBishopMoves(m_bbAllPieces, 61) | getBishopMoves(m_bbAllPieces, 62);
                    if(! ((m_bbTypedPieces[W_BISHOP][WHITE] | m_bbTypedPieces[W_QUEEN][WHITE]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 60) | getRookMoves(m_bbAllPieces, 61) | getRookMoves(m_bbAllPieces, 62);
                        if(! ((m_bbTypedPieces[W_ROOK][WHITE] | m_bbTypedPieces[W_QUEEN][WHITE]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(60, 62, MOVE_INFO_CASTLE_BLACK_KING | MOVE_INFO_KING_MOVE);
                        }
                    }
                }
            }
        }
    }
    
    return m_legalMoves;
}

// Generates the set of legal captures, checks or moves to get out of check 
// If in check, the existing function for generating legal moves will be used
// Note: Does not include castle
Move* Board::getLegalCaptureAndCheckMoves()
{
    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t oponentAttacks = getOponentAttacks();

    // Use a different function for generating moves when in check
    bool wasChecked = (m_bbTypedPieces[W_KING][m_turn] & oponentAttacks) != 0LL;
    // If in check, the existing function for generating legal moves will be used
    if(wasChecked)
    {
        return getLegalMoves();
    }

    // Everything below is generating moves when not in check, thus we can filter for capturing moves
    Color oponent = Color(m_turn ^ 1);
    // TODO: We can check if the piece is blocking a check, this way we know we cannot move the piece and will not have to test all the moves
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][m_turn]);
    bitboard_t kingDiagonals = diagonal[kingIdx] | antiDiagonal[kingIdx];
    bitboard_t kingStraights = (0xffLL << (kingIdx & ~0b111)) | (0x0101010101010101LL << (kingIdx & 0b111));

    // Positions where pieces can move to set the oponent in check
    uint8_t oponentKingIdx = LS1B(m_bbTypedPieces[W_KING][oponent]);
    bitboard_t knightCheckPositions = getKnightAttacks(oponentKingIdx);
    bitboard_t rookCheckPositions = getRookMoves(m_bbAllPieces, oponentKingIdx);
    bitboard_t bishopCheckPositions = getBishopMoves(m_bbAllPieces, oponentKingIdx);
    
    // Queen moves
    bitboard_t queens = m_bbTypedPieces[W_QUEEN][m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[oponent] | bishopCheckPositions | rookCheckPositions);

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            m_attemptAddPseudoLegalMove(Move(queenIdx, target, MOVE_INFO_QUEEN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbTypedPieces[W_KNIGHT][m_turn];
    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[oponent] | knightCheckPositions);
        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            m_attemptAddPseudoLegalMove(Move(knightIdx, target,  MOVE_INFO_KNIGHT_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbTypedPieces[W_BISHOP][m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[oponent] | bishopCheckPositions);

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            m_attemptAddPseudoLegalMove(Move(bishopIdx, target, MOVE_INFO_BISHOP_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Rook moves
    bitboard_t rooks = m_bbTypedPieces[W_ROOK][m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbColoredPieces[m_turn] & (m_bbColoredPieces[oponent] | rookCheckPositions);
        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            m_attemptAddPseudoLegalMove(Move(rookIdx, target, MOVE_INFO_ROOK_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Pawn moves 
    bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnMoves, pawnMovesOrigin;
    if(m_turn == WHITE)
    {
        pawnMoves= getWhitePawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMoves &= getBlackPawnAttacks(m_bbTypedPieces[W_KING][oponent]);
        pawnMovesOrigin = getBlackPawnMoves(pawnMoves);
    }
    else
    {
        pawnMoves= getBlackPawnMoves(pawns);
        pawnMoves &= ~m_bbAllPieces;
        pawnMoves &= getWhitePawnAttacks(m_bbTypedPieces[W_KING][oponent]);
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
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Pawn captues
    pawns = m_bbTypedPieces[W_PAWN][m_turn];
    bitboard_t pawnAttacksLeft, pawnAttacksRight, pawnAttacksLeftOrigin, pawnAttacksRightOrigin; 
    if(m_turn == WHITE)
    {
        pawnAttacksLeft = getWhitePawnAttacksLeft(pawns);
        pawnAttacksRight = getWhitePawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[oponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[oponent] | m_bbEnPassantSquare;
        pawnAttacksLeftOrigin = pawnAttacksLeft >> 7; 
        pawnAttacksRightOrigin = pawnAttacksRight >> 9;
    }
    else
    {
        pawnAttacksLeft = getBlackPawnAttacksLeft(pawns);
        pawnAttacksRight = getBlackPawnAttacksRight(pawns);
        pawnAttacksLeft  &= m_bbColoredPieces[oponent] | m_bbEnPassantSquare;
        pawnAttacksRight &= m_bbColoredPieces[oponent] | m_bbEnPassantSquare;
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
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // TODO: optimize
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE));
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            if(added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT);
            }
        }
        else
        {
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, (target == m_enPassantSquare) ? (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_ENPASSANT | MOVE_INFO_PAWN_MOVE) : (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_PAWN_MOVE));
            m_attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
        }
    }

    // King moves
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbColoredPieces[m_turn] | oponentAttacks) & m_bbColoredPieces[oponent];
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING_MOVE);
    }


    return m_legalMoves;
}

uint8_t Board::getNumLegalMoves()
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

void Board::performMove(Move move)
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
    Color oponent = Color(m_turn ^ 1);
    if(m_pieces[move.to] != NO_PIECE)
    {
        m_bbColoredPieces[oponent] &= ~bbTo;
        m_bbTypedPieces[m_pieces[move.to] % B_PAWN][oponent] &= ~bbTo;
    } 
    else if(move.moveInfo & MOVE_INFO_ENPASSANT)
    {
        m_pieces[m_enPassantTarget] = NO_PIECE;
        m_bbAllPieces &= ~m_bbEnPassantTarget;
        m_bbColoredPieces[oponent] &= ~m_bbEnPassantTarget;
        m_bbTypedPieces[W_PAWN][oponent] &= ~m_bbEnPassantTarget;
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

    s_zobrist.getUpdatedHashs(*this, move, oldEnPassantSquare, m_enPassantSquare, m_hash, m_materialHash, m_pawnHash);

    m_turn = oponent;
    m_fullMoves += (m_turn == WHITE); // Note: turn is flipped
    m_halfMoves++;
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

hash_t Board::getHash()
{
    return m_hash;
}

// Generates a bitboard of all attacks of oponents
// The moves does not check if the move will make the oponent become checked, or if the attack is on its own pieces
// Used for checking if the king is in check after king moves.
inline bitboard_t Board::getOponentAttacks()
{
    Color oponent = Color(m_turn ^ 1);

    // Pawns
    bitboard_t attacks = oponent == WHITE ? getWhitePawnAttacks(m_bbTypedPieces[W_PAWN][WHITE]) : getBlackPawnAttacks(m_bbTypedPieces[W_PAWN][BLACK]);

    // King
    attacks |= getKingMoves(LS1B(m_bbTypedPieces[W_KING][oponent]));

    // Knight
    bitboard_t tmpKnights = m_bbTypedPieces[W_KNIGHT][oponent];
    while (tmpKnights)
    {
        attacks |= getKnightAttacks(popLS1B(&tmpKnights));
    }

    // Remove the king from the occupied mask such that when it moves, the previous king position will not block
    bitboard_t allPiecesNoKing = m_bbAllPieces & ~m_bbTypedPieces[W_KING][m_turn];
    
    // Queens and bishops
    bitboard_t tmpBishops = m_bbTypedPieces[W_BISHOP][oponent] | m_bbTypedPieces[W_QUEEN][oponent];
    while (tmpBishops)
    {
        attacks |= getBishopMoves(allPiecesNoKing, popLS1B(&tmpBishops));
    }

    // Queens and rooks
    bitboard_t tmpRooks = m_bbTypedPieces[W_ROOK][oponent] | m_bbTypedPieces[W_QUEEN][oponent];
    while (tmpRooks)
    {
        // Remove the king from the occupied mask such that when it moves, the previous king position will not block
        attacks |= getRookMoves(allPiecesNoKing, popLS1B(&tmpRooks));
    }

    return attacks;
}

bool Board::isChecked(Color color)
{   
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color oponent = Color(color ^ 1);
    
    // Pawns
    // Get the position of potentially attacking pawns
    bitboard_t pawnAttackPositions = color == WHITE ? getWhitePawnAttacks(m_bbTypedPieces[W_KING][color]) : getBlackPawnAttacks(m_bbTypedPieces[W_KING][color]);
    if(pawnAttackPositions & m_bbTypedPieces[W_PAWN][oponent])
        return true;

    // Knights
    bitboard_t knightAttackPositions = getKnightAttacks(kingIdx);
    if(knightAttackPositions & m_bbTypedPieces[W_KNIGHT][oponent])
        return true;

    return isSlidingChecked(color);
}

inline bool Board::isSlidingChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_BISHOP][oponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);

    if(diagonalAttacks & queenAndBishops)
        return true;

    bitboard_t queenAndRooks   = m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_ROOK][oponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    if(straightAttacks & queenAndRooks)
        return true;

    return false;
}

inline bool Board::isDiagonalChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_BISHOP][oponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);
    
    return (diagonalAttacks & queenAndBishops) != 0;
}

inline bool Board::isStraightChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbTypedPieces[W_KING][color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndRooks   = m_bbTypedPieces[W_QUEEN][oponent] | m_bbTypedPieces[W_ROOK][oponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);

    return (straightAttacks & queenAndRooks) != 0;
}

// Evaluates positive value for WHITE
eval_t Board::evaluate()
{
    // Check for stalemate and checkmate
    // TODO: Create function hasLegalMoves
    getLegalMoves();
    if(getNumLegalMoves() == 0)
    {
        if(isChecked(m_turn))
        {
            // subtract number of full moves from the score to have incentive for fastest checkmate
            // If there are more checkmates and this is not done, it might be "confused" and move between
            return m_turn == WHITE ? (-INT16_MAX + m_fullMoves) : (INT16_MAX - m_fullMoves);
        }

        return 0;
    }

    // TODO: Use model parameters
    eval_t pieceScore;
    pieceScore  = 100 * (CNTSBITS(m_bbTypedPieces[W_PAWN][WHITE])    - CNTSBITS(m_bbTypedPieces[W_PAWN][BLACK]));
    pieceScore += 300 * (CNTSBITS(m_bbTypedPieces[W_KNIGHT][WHITE]) - CNTSBITS(m_bbTypedPieces[W_KNIGHT][BLACK]));
    pieceScore += 300 * (CNTSBITS(m_bbTypedPieces[W_BISHOP][WHITE]) - CNTSBITS(m_bbTypedPieces[W_BISHOP][BLACK]));
    pieceScore += 500 * (CNTSBITS(m_bbTypedPieces[W_ROOK][WHITE])   - CNTSBITS(m_bbTypedPieces[W_ROOK][BLACK]));
    pieceScore += 900 * (CNTSBITS(m_bbTypedPieces[W_QUEEN][WHITE])  - CNTSBITS(m_bbTypedPieces[W_QUEEN][BLACK]));

    eval_t mobility = m_numLegalMoves;

    m_turn = Color(m_turn ^ 1);
    m_numLegalMoves = 0;
    getLegalMoves();
    eval_t oponentMobility = m_numLegalMoves;
    m_turn = Color(m_turn ^ 1);

    eval_t mobilityScore = (m_turn == WHITE ? (mobility - oponentMobility) : (oponentMobility - mobility));

    bitboard_t wPawns = m_bbTypedPieces[W_PAWN][WHITE]; 
    bitboard_t bPawns = m_bbTypedPieces[W_PAWN][BLACK]; 

    eval_t pawnScore = 0;
    while (wPawns)
    {
        pawnScore += popLS1B(&wPawns) >> 3;
    }
    
    while (bPawns)
    {
        pawnScore -= (7 - (popLS1B(&bPawns) >> 3));
    }
    
    // return pieceScore;
    return pieceScore + mobilityScore + pawnScore;
}

Color Board::getTurn()
{
    return m_turn;
}

std::string Board::getBoardString()
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
                ss << (((x + y) % 2 == 0) ? CHESS_ENGINE2_COLOR_GREEN : CHESS_ENGINE2_COLOR_WHITE) << ". " << CHESS_ENGINE2_COLOR_WHITE;
        }
        ss << "| " << y+1 << std::endl;
    }
    ss << "  +-----------------+" << std::endl;
    ss << "    a b c d e f g h " << std::endl;

    return ss.str();
}