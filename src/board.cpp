#include <board.hpp>
#include <sstream>

using namespace ChessEngine2;

Board::Board(std::string fen)
{
    // -- Create empty board
    m_bbAllPieces = 0LL;
    for(int c = 0; c < NUM_COLORS; c++)
    {
        m_bbPieces[c] = 0LL;
        m_bbPawns[c] = 0LL;
        m_bbKing[c] = 0LL;
        m_bbKnights[c] = 0LL;
        m_bbBishops[c] = 0LL;
        m_bbQueens[c] = 0LL;
        m_bbRooks[c] = 0LL;
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

        bitboard_t piece = 0b1LL << ((rank << 3) | file);
        file++;

        m_bbPieces[color] |= piece;
        m_bbAllPieces |= piece;

        switch (chr)
        {
        case 'p':
            m_bbPawns[color] |= piece;
            break;
        case 'r':
            m_bbRooks[color] |= piece;
            break;
        case 'n':
            m_bbKnights[color] |= piece;
            break;
        case 'b':
            m_bbBishops[color] |= piece;
            break;
        case 'k':
            m_bbKing[color] = piece;
            break;
        case 'q':
            m_bbQueens[color] |= piece;
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
}

Board::~Board()
{

}

inline void Board::attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights, bool wasChecked)
{
    bitboard_t bbFrom = (0b1LL << move.from);
    bitboard_t bbTo = (0b1LL << move.to);
    Color oponent = Color(m_turn ^ 1);

    // TODO: Remove when having a separate isChecked generator
    if(wasChecked)
    {
        bitboard_t bbAllPieces = m_bbAllPieces;

        bitboard_t bbPiecesWhite  = m_bbPieces[WHITE];
        bitboard_t bbPawnsWhite   = m_bbPawns[WHITE];
        bitboard_t bbKingWhite    = m_bbKing[WHITE];
        bitboard_t bbKnightsWhite = m_bbKnights[WHITE];
        bitboard_t bbBishopsWhite = m_bbBishops[WHITE];
        bitboard_t bbQueensWhite  = m_bbQueens[WHITE];
        bitboard_t bbRooksWhite   = m_bbRooks[WHITE];

        bitboard_t bbPiecesBlack  = m_bbPieces[BLACK];
        bitboard_t bbPawnsBlack   = m_bbPawns[BLACK];
        bitboard_t bbKingBlack    = m_bbKing[BLACK];
        bitboard_t bbKnightsBlack = m_bbKnights[BLACK];
        bitboard_t bbBishopsBlack = m_bbBishops[BLACK];
        bitboard_t bbQueensBlack  = m_bbQueens[BLACK];
        bitboard_t bbRooksBlack   = m_bbRooks[BLACK];

        uint8_t castleRights = m_castleRights;
        m_bbAllPieces = (m_bbAllPieces | bbTo) & ~bbFrom;
        m_bbPieces[m_turn] = (m_bbPieces[m_turn] | bbTo) & ~bbFrom;

        // Move the piece
        m_bbPawns[m_turn]   = (m_bbPawns[m_turn] & bbFrom) ? ((m_bbPawns[m_turn] & ~(bbFrom)) | bbTo) : m_bbPawns[m_turn];
        m_bbKing[m_turn]    = (m_bbKing[m_turn] & bbFrom) ? ((m_bbKing[m_turn] & ~(bbFrom)) | bbTo) : m_bbKing[m_turn];
        m_bbKnights[m_turn] = (m_bbKnights[m_turn] & bbFrom) ? ((m_bbKnights[m_turn] & ~(bbFrom)) | bbTo) : m_bbKnights[m_turn];
        m_bbBishops[m_turn] = (m_bbBishops[m_turn] & bbFrom) ? ((m_bbBishops[m_turn] & ~(bbFrom)) | bbTo) : m_bbBishops[m_turn];
        m_bbQueens[m_turn]  = (m_bbQueens[m_turn] & bbFrom) ? ((m_bbQueens[m_turn] & ~(bbFrom)) | bbTo) : m_bbQueens[m_turn];
        m_bbRooks[m_turn]   = (m_bbRooks[m_turn] & bbFrom) ? ((m_bbRooks[m_turn] & ~(bbFrom)) | bbTo) : m_bbRooks[m_turn];

        Color oponent = Color(m_turn ^ 1);
        
        // Remove potential captures
        m_bbAllPieces        &= ~((move.moveInfo & MOVE_INFO_ENPASSANT) ? m_bbEnPassantTarget : 0LL);
        m_bbPieces[oponent]  &= ~(bbTo | ((move.moveInfo & MOVE_INFO_ENPASSANT) ? m_bbEnPassantTarget : 0LL));
        m_bbPawns[oponent]   &= ~(bbTo | ((move.moveInfo & MOVE_INFO_ENPASSANT) ? m_bbEnPassantTarget : 0LL));
        m_bbKing[oponent]    &= ~bbTo;
        m_bbKnights[oponent] &= ~bbTo;
        m_bbBishops[oponent] &= ~bbTo;
        m_bbQueens[oponent]  &= ~bbTo;
        m_bbRooks[oponent]   &= ~bbTo;

        if(!isChecked(m_turn))
        {
            m_legalMoves[m_numLegalMoves++] = move;
        }

        m_bbAllPieces       = bbAllPieces;

        m_bbPieces[WHITE]   = bbPiecesWhite ;
        m_bbPawns[WHITE]    = bbPawnsWhite  ;
        m_bbKing[WHITE]     = bbKingWhite   ;
        m_bbKnights[WHITE]  = bbKnightsWhite;
        m_bbBishops[WHITE]  = bbBishopsWhite;
        m_bbQueens[WHITE]   = bbQueensWhite ;
        m_bbRooks[WHITE]    = bbRooksWhite  ;

        m_bbPieces[BLACK]   = bbPiecesBlack ;
        m_bbPawns[BLACK]    = bbPawnsBlack  ;
        m_bbKing[BLACK]     = bbKingBlack   ;
        m_bbKnights[BLACK]  = bbKnightsBlack;
        m_bbBishops[BLACK]  = bbBishopsBlack;
        m_bbQueens[BLACK]   = bbQueensBlack ;
        m_bbRooks[BLACK]    = bbRooksBlack  ;
        m_castleRights      = castleRights  ;

        return;
    }

    if(bbFrom & kingDiagonals)
    {
        // Queens and bishops after removing potential capture 
        bitboard_t queenAndBishops = (m_bbQueens[oponent] | m_bbBishops[oponent]) & ~bbTo;
        // Move the piece and calculate potential diagonal attacks on the king
        bitboard_t diagonalAttacks = getBishopMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((diagonalAttacks & queenAndBishops) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
        }
    } 
    else if(bbFrom & kingStraights)
    {
        // Queens and rooks after removing potential capture 
        bitboard_t queenAndRooks = (m_bbQueens[oponent] | m_bbRooks[oponent]) & ~bbTo;
        // Move the piece and calculate potential straight attacks on the king
        bitboard_t straightAttacks = getRookMoves((m_bbAllPieces | bbTo) & ~bbFrom, kingIdx);
        
        if ((straightAttacks & queenAndRooks) == 0)
        {
            m_legalMoves[m_numLegalMoves++] = move;
        }
    }
    // It is safe to add the move if it is not potentially a discoverd check
    else 
    {
        m_legalMoves[m_numLegalMoves++] = move;
    }
}

void Board::attemptAddPseudoLegalKingMove(Move move, bitboard_t oponentAttacks)
{
    bitboard_t bbTo = (0b1LL << move.to);

    if((oponentAttacks & bbTo) == 0LL)
    {
        m_legalMoves[m_numLegalMoves++] = move;
    }
}

Move* Board::getLegalMoves()
{
    m_numLegalMoves = 0;

    // Create bitboard for where the king would be attacked
    bitboard_t oponentAttacks = getOponentAttacks();

    // Use a different function for generating moves when in check
    bool wasChecked = (m_bbKing[m_turn] & oponentAttacks) != 0LL;
    if(wasChecked)
    {
        // TODO: Generate moves when in check
        // return &m_legalMoves;
    }

    // King moves
    uint8_t kingIdx = LS1B(m_bbKing[m_turn]);
    bitboard_t kMoves = getKingMoves(kingIdx);
    kMoves &= ~(m_bbPieces[m_turn] | oponentAttacks);
    while(kMoves)
    {
        uint8_t target = popLS1B(&kMoves);
        m_legalMoves[m_numLegalMoves++] = Move(kingIdx, target, MOVE_INFO_KING);
    }

    // TODO: We can check if the piece is blocking a check, this way we know we cannot move the piece and will not have to test all the moves
    bitboard_t kingDiagonals = diagonal[kingIdx] | antiDiagonal[kingIdx];
    bitboard_t kingStraights = (0xffLL << (kingIdx & ~0b111)) | (0x0101010101010101LL << (kingIdx & 0b111));

    // Pawn moves 
    // TODO: make one large if statement
    bitboard_t pawns = m_bbPawns[m_turn];
    bitboard_t pawnMoves = m_turn == WHITE ? getWhitePawnMoves(pawns) : getBlackPawnMoves(pawns);
    pawnMoves &= ~m_bbAllPieces;
    bitboard_t pawnMovesOrigin = m_turn == WHITE ? getBlackPawnMoves(pawnMoves) : getWhitePawnMoves(pawnMoves);

    // Forward move
    while(pawnMoves)
    {
        uint8_t target = popLS1B(&pawnMoves);
        uint8_t pawnIdx = popLS1B(&pawnMovesOrigin);
        
        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // TODO: If one promotion move is legal, all are legal
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
        else
        {
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // TODO: make one large if statement
    bitboard_t pawnAttacksLeft   = m_turn == WHITE ? getWhitePawnAttacksLeft(pawns) : getBlackPawnAttacksLeft(pawns);
    bitboard_t pawnAttacksRight  = m_turn == WHITE ? getWhitePawnAttacksRight(pawns) : getBlackPawnAttacksRight(pawns);
    pawnAttacksLeft  &= m_bbPieces[m_turn ^ 1] | m_bbEnPassantSquare;
    pawnAttacksRight &= m_bbPieces[m_turn ^ 1] | m_bbEnPassantSquare;
    // bitboard_t pawnAttacksLeftOrigin   = m_turn == WHITE ? getBlackPawnAttacksRight(pawnAttacksLeft) : getWhitePawnAttacksRight(pawnAttacksLeft); // TODO: Origins does not have to check for board edges (does not need bitmask)
    // bitboard_t pawnAttacksRightOrigin  = m_turn == WHITE ? getBlackPawnAttacksLeft(pawnAttacksRight) : getWhitePawnAttacksLeft(pawnAttacksRight);
    bitboard_t pawnAttacksLeftOrigin   = m_turn == WHITE ? pawnAttacksLeft >> 7 : pawnAttacksLeft << 9; // TODO: Origins does not have to check for board edges (does not need bitmask)
    bitboard_t pawnAttacksRightOrigin  = m_turn == WHITE ? pawnAttacksRight >> 9 : pawnAttacksRight << 7;
    
    while (pawnAttacksLeft)
    {
        uint8_t target = popLS1B(&pawnAttacksLeft);
        uint8_t pawnIdx = popLS1B(&pawnAttacksLeftOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // TODO: If one promotion move is legal, all are legal
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK),  kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
        else
        {
            // TODO: optimize
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, ((target == m_enPassantSquare) ? MOVE_INFO_ENPASSANT : 0) | MOVE_INFO_PAWN_MOVE);
            attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
        }
    }

    while (pawnAttacksRight)
    {
        uint8_t target = popLS1B(&pawnAttacksRight);
        uint8_t pawnIdx = popLS1B(&pawnAttacksRightOrigin);

        if((0b1LL << target) & 0xff000000000000ffLL)
        {
            // TODO: If one promotion move is legal, all are legal
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_QUEEN), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_ROOK),  kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_BISHOP), kingIdx, kingDiagonals, kingStraights, wasChecked);
            attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_PAWN_MOVE | MOVE_INFO_PROMOTE_KNIGHT), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
        else
        {
            // TODO:
            // Note: The captured piece in enpassant cannot uncover a check, except if the king is on the side of both the attacking and captured pawn while there is a rook/queen in the same rank
            Move move = Move(pawnIdx, target, ((target == m_enPassantSquare) ? MOVE_INFO_ENPASSANT : 0) | MOVE_INFO_PAWN_MOVE);
            attemptAddPseudoLegalMove(move, kingIdx, kingDiagonals, kingStraights, wasChecked || ((move.moveInfo & MOVE_INFO_ENPASSANT) && ((m_enPassantTarget >> 3) == (kingIdx >> 3))));
        }
    }

    // Double move
    bitboard_t doubleMoves       = m_turn == WHITE ? (pawns << 16) & 0xff000000 & ~(m_bbAllPieces << 8) & ~(m_bbAllPieces) : (pawns >> 16) & 0xff00000000 & ~(m_bbAllPieces >> 8) & ~(m_bbAllPieces);
    bitboard_t doubleMovesOrigin = m_turn == WHITE ? doubleMoves >> 16 : doubleMoves << 16;
    while (doubleMoves)
    {
        int target = popLS1B(&doubleMoves);
        int pawnIdx = popLS1B(&doubleMovesOrigin);
        attemptAddPseudoLegalMove(Move(pawnIdx, target, MOVE_INFO_DOUBLE_MOVE | MOVE_INFO_PAWN_MOVE), kingIdx, kingDiagonals, kingStraights, wasChecked);
    }

    // Rook moves
    bitboard_t rooks = m_bbRooks[m_turn];
    while (rooks)
    {
        uint8_t rookIdx = popLS1B(&rooks);
        bitboard_t rookMoves = getRookMoves(m_bbAllPieces, rookIdx);
        rookMoves &= ~m_bbPieces[m_turn];

        while(rookMoves)
        {
            uint8_t target = popLS1B(&rookMoves);
            attemptAddPseudoLegalMove(Move(rookIdx, target), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Knight moves
    bitboard_t knights = m_bbKnights[m_turn];

    while (knights)
    {
        uint8_t knightIdx = popLS1B(&knights);
        bitboard_t knightMoves = getKnightAttacks(knightIdx);
        knightMoves &= ~m_bbPieces[m_turn];

        while(knightMoves)
        {
            uint8_t target = popLS1B(&knightMoves);
            attemptAddPseudoLegalMove(Move(knightIdx, target), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Bishop moves
    bitboard_t bishops = m_bbBishops[m_turn];
    while (bishops)
    {
        uint8_t bishopIdx = popLS1B(&bishops);
        bitboard_t bishopMoves = getBishopMoves(m_bbAllPieces, bishopIdx);
        bishopMoves &= ~m_bbPieces[m_turn];

        while(bishopMoves)
        {
            uint8_t target = popLS1B(&bishopMoves);
            attemptAddPseudoLegalMove(Move(bishopIdx, target), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
    }

    // Queen moves
    bitboard_t queens = m_bbQueens[m_turn];
    while (queens)
    {
        uint8_t queenIdx = popLS1B(&queens);
        bitboard_t queenMoves = getQueenMoves(m_bbAllPieces, queenIdx);
        queenMoves &= ~m_bbPieces[m_turn];

        while(queenMoves)
        {
            uint8_t target = popLS1B(&queenMoves);
            attemptAddPseudoLegalMove(Move(queenIdx, target), kingIdx, kingDiagonals, kingStraights, wasChecked);
        }
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
                if(!( (m_bbKnights[BLACK] & whiteQueensideKnightMask) | ((m_bbPawns[BLACK] | m_bbKing[BLACK]) & whiteQueensidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 2) | getBishopMoves(m_bbAllPieces, 3) | getBishopMoves(m_bbAllPieces, 4);
                    if(! ((m_bbBishops[BLACK] | m_bbQueens[BLACK]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 2) | getRookMoves(m_bbAllPieces, 3) | getRookMoves(m_bbAllPieces, 4);
                        if(! ((m_bbRooks[BLACK] | m_bbQueens[BLACK]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(4, 2, MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_KING);
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
                if(!( (m_bbKnights[BLACK] & whiteKingsideKnightMask) | ((m_bbPawns[BLACK] | m_bbKing[BLACK]) & whiteKingsidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 4) | getBishopMoves(m_bbAllPieces, 5) | getBishopMoves(m_bbAllPieces, 6);
                    if(! ((m_bbBishops[BLACK] | m_bbQueens[BLACK]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 4) | getRookMoves(m_bbAllPieces, 5) | getRookMoves(m_bbAllPieces, 6);
                        if(! ((m_bbRooks[BLACK] | m_bbQueens[BLACK]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(4, 6, MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_KING);
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
                if(!( (m_bbKnights[WHITE] & blackQueensideKnightMask) | ((m_bbPawns[WHITE] | m_bbKing[WHITE]) & blackQueensidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 58) | getBishopMoves(m_bbAllPieces, 59) | getBishopMoves(m_bbAllPieces, 60);
                    if(! ((m_bbBishops[WHITE] | m_bbQueens[WHITE]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 58) | getRookMoves(m_bbAllPieces, 59) | getRookMoves(m_bbAllPieces, 60);
                        if(! ((m_bbRooks[WHITE] | m_bbQueens[WHITE]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(60, 58, MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_KING);
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
                if(!( (m_bbKnights[WHITE] & blackKingsideKnightMask) | ((m_bbPawns[WHITE] | m_bbKing[WHITE]) & blackKingsidePawnMask)))
                {
                    // If not blocked by checking pawns or knighs the king, check for sliding checks
                    bitboard_t bishopMask = getBishopMoves(m_bbAllPieces, 60) | getBishopMoves(m_bbAllPieces, 61) | getBishopMoves(m_bbAllPieces, 62);
                    if(! ((m_bbBishops[WHITE] | m_bbQueens[WHITE]) & bishopMask) )
                    {
                        bitboard_t rookMask = getRookMoves(m_bbAllPieces, 60) | getRookMoves(m_bbAllPieces, 61) | getRookMoves(m_bbAllPieces, 62);
                        if(! ((m_bbRooks[WHITE] | m_bbQueens[WHITE]) & rookMask) )
                        {
                            m_legalMoves[m_numLegalMoves++] = Move(60, 62, MOVE_INFO_CASTLE_BLACK_KING | MOVE_INFO_KING);
                        }
                    }
                }
            }
        }
    }
    
    return m_legalMoves;
}

uint8_t Board::getNumLegalMoves()
{
    return m_numLegalMoves;
}

Board::Board(const Board& board)
{
    m_turn = board.m_turn;
    m_halfMoves = board.m_halfMoves;
    m_fullMoves = board.m_fullMoves;
    m_castleRights = board.m_castleRights;
    m_enPassantSquare = board.m_enPassantSquare;
    m_enPassantTarget = board.m_enPassantTarget;
    m_bbEnPassantSquare = board.m_bbEnPassantSquare;
    m_bbEnPassantTarget = board.m_bbEnPassantTarget;

    m_bbAllPieces = board.m_bbAllPieces;
    
    m_bbPieces[WHITE] = board.m_bbPieces[WHITE];
    m_bbPawns[WHITE] = board.m_bbPawns[WHITE];
    m_bbKing[WHITE] = board.m_bbKing[WHITE];
    m_bbKnights[WHITE] = board.m_bbKnights[WHITE];
    m_bbBishops[WHITE] = board.m_bbBishops[WHITE];
    m_bbQueens[WHITE] = board.m_bbQueens[WHITE];
    m_bbRooks[WHITE] = board.m_bbRooks[WHITE];

    m_bbPieces[BLACK] = board.m_bbPieces[BLACK];
    m_bbPawns[BLACK] = board.m_bbPawns[BLACK];
    m_bbKing[BLACK] = board.m_bbKing[BLACK];
    m_bbKnights[BLACK] = board.m_bbKnights[BLACK];
    m_bbBishops[BLACK] = board.m_bbBishops[BLACK];
    m_bbQueens[BLACK] = board.m_bbQueens[BLACK];
    m_bbRooks[BLACK] = board.m_bbRooks[BLACK];
}

void Board::performMove(Move move)
{
    bitboard_t bbFrom = 0b1LL << move.from;
    bitboard_t bbTo = 0b1LL << move.to;

    if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_QUEEN)
    {
        m_bbRooks[m_turn] = (m_bbRooks[m_turn] & ~0x01LL) | 0x08LL;
        m_bbPieces[m_turn] = (m_bbPieces[m_turn] & ~0x01LL) | 0x08LL;
        m_bbAllPieces = (m_bbAllPieces & ~0x01LL) | 0x08LL;
    }else if(move.moveInfo & MOVE_INFO_CASTLE_WHITE_KING)
    {
        m_bbRooks[m_turn] = (m_bbRooks[m_turn] & ~0x80LL) | 0x20LL;
        m_bbPieces[m_turn] = (m_bbPieces[m_turn] & ~0x80LL) | 0x20LL;
        m_bbAllPieces = (m_bbAllPieces & ~0x80LL) | 0x20LL;
    } else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_QUEEN)
    {
        m_bbRooks[m_turn] = (m_bbRooks[m_turn] & ~0x0100000000000000LL) | 0x0800000000000000LL;
        m_bbPieces[m_turn] = (m_bbPieces[m_turn] & ~0x0100000000000000LL) | 0x0800000000000000LL;
        m_bbAllPieces = (m_bbAllPieces & ~0x0100000000000000LL) | 0x0800000000000000LL;
    }else if(move.moveInfo & MOVE_INFO_CASTLE_BLACK_KING)
    {
        m_bbRooks[m_turn] = (m_bbRooks[m_turn] & ~0x8000000000000000LL) | 0x2000000000000000LL;
        m_bbPieces[m_turn] = (m_bbPieces[m_turn] & ~0x8000000000000000LL) | 0x2000000000000000LL;
        m_bbAllPieces = (m_bbAllPieces & ~0x8000000000000000LL) | 0x2000000000000000LL;
    }

    if(move.moveInfo & MOVE_INFO_KING)
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

    m_bbAllPieces = (m_bbAllPieces | bbTo) & ~bbFrom;
    m_bbPieces[m_turn] = (m_bbPieces[m_turn] | bbTo) & ~bbFrom;

    // Move the piece
    m_bbPawns[m_turn]   = (m_bbPawns[m_turn] & bbFrom) ? ((m_bbPawns[m_turn] & ~(bbFrom)) | (bbTo & 0x00ffffffffffff00LL)) : m_bbPawns[m_turn]; // Bitmask prevents setting a pawn on the promotion square
    m_bbKing[m_turn]    = (m_bbKing[m_turn] & bbFrom) ? ((m_bbKing[m_turn] & ~(bbFrom)) | bbTo) : m_bbKing[m_turn];
    m_bbKnights[m_turn] = (m_bbKnights[m_turn] & bbFrom) ? ((m_bbKnights[m_turn] & ~(bbFrom)) | bbTo) : m_bbKnights[m_turn];
    m_bbBishops[m_turn] = (m_bbBishops[m_turn] & bbFrom) ? ((m_bbBishops[m_turn] & ~(bbFrom)) | bbTo) : m_bbBishops[m_turn];
    m_bbQueens[m_turn]  = (m_bbQueens[m_turn] & bbFrom) ? ((m_bbQueens[m_turn] & ~(bbFrom)) | bbTo) : m_bbQueens[m_turn];
    m_bbRooks[m_turn]   = (m_bbRooks[m_turn] & bbFrom) ? ((m_bbRooks[m_turn] & ~(bbFrom)) | bbTo) : m_bbRooks[m_turn];

    if(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)
    {
        m_bbQueens[m_turn] |= bbTo;
    } 
    else if(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP)
    {
        m_bbBishops[m_turn] |= bbTo;
    } 
    else if(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)
    {
        m_bbRooks[m_turn] |= bbTo;
    } 
    else if(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT)
    {
        m_bbKnights[m_turn] |= bbTo;
    }

    m_turn = Color(m_turn ^ 1);
    
    // Remove potential captures
    bitboard_t enpassantMask = (move.moveInfo & MOVE_INFO_ENPASSANT) ? m_bbEnPassantTarget : 0LL;
    m_bbAllPieces       &= ~enpassantMask;
    m_bbPieces[m_turn]  &= ~(bbTo | enpassantMask); 
    m_bbPawns[m_turn]   &= ~(bbTo | enpassantMask); 
    m_bbKing[m_turn]    &= ~bbTo;
    m_bbKnights[m_turn] &= ~bbTo;
    m_bbBishops[m_turn] &= ~bbTo;
    m_bbQueens[m_turn]  &= ~bbTo;
    m_bbRooks[m_turn]   &= ~bbTo;

    // Required to reset
    m_enPassantSquare = 64;
    m_enPassantTarget = 64;
    m_bbEnPassantSquare = 0LL;
    m_bbEnPassantTarget = 0LL;
    // TODO: Also include an enpassant bitboard, will be 0 if non available
    if(move.moveInfo & MOVE_INFO_DOUBLE_MOVE)
    {
        m_enPassantTarget = move.to;
        m_enPassantSquare = (move.to + move.from) >> 1; // Average of the two squares is the middle
        m_bbEnPassantSquare = 1LL << m_enPassantSquare; 
        m_bbEnPassantTarget = 1LL << m_enPassantTarget; 
    }

    m_fullMoves += (m_turn == WHITE); // Note turn is flipped
    m_halfMoves++;
}

// Generates a bitboard of all attacks of oponents
// The moves does not check if the move will make the oponent become checked, or if the attack is on its own pieces
// Used for checking if the king is in check after king moves.
inline bitboard_t Board::getOponentAttacks()
{
    Color oponent = Color(m_turn ^ 1);

    // Pawns
    bitboard_t attacks = oponent == WHITE ? getWhitePawnAttacks(m_bbPawns[WHITE]) : getBlackPawnAttacks(m_bbPawns[BLACK]);

    // King
    attacks |= getKingMoves(LS1B(m_bbKing[oponent]));

    // Knight
    bitboard_t tmpKnights = m_bbKnights[oponent];
    while (tmpKnights)
    {
        attacks |= getKnightAttacks(popLS1B(&tmpKnights));
    }

    // Remove the king from the occupied mask such that when it moves, the previous king position will not block
    bitboard_t allPiecesNoKing = m_bbAllPieces & ~m_bbKing[m_turn];
    
    // Queens and bishops
    bitboard_t tmpBishops = m_bbBishops[oponent] | m_bbQueens[oponent];
    while (tmpBishops)
    {
        attacks |= getBishopMoves(allPiecesNoKing, popLS1B(&tmpBishops));
    }

    // Queens and rooks
    bitboard_t tmpRooks = m_bbRooks[oponent] | m_bbQueens[oponent];
    while (tmpRooks)
    {
        // Remove the king from the occupied mask such that when it moves, the previous king position will not block
        attacks |= getRookMoves(allPiecesNoKing, popLS1B(&tmpRooks));
    }

    return attacks;
}

bool Board::isChecked(Color color)
{   
    uint8_t kingIdx = LS1B(m_bbKing[color]);
    Color oponent = Color(color ^ 1);
    
    // Pawns
    // Get the position of potentially attacking pawns
    bitboard_t pawnAttackPositions = color == WHITE ? getWhitePawnAttacks(m_bbKing[color]) : getBlackPawnAttacks(m_bbKing[color]);
    if(pawnAttackPositions & m_bbPawns[oponent])
        return true;

    // Knights
    bitboard_t knightAttackPositions = getKnightAttacks(kingIdx);
    if(knightAttackPositions & m_bbKnights[oponent])
        return true;

    return isSlidingChecked(color);
}

inline bool Board::isSlidingChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbKing[color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbQueens[oponent] | m_bbBishops[oponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);

    if(diagonalAttacks & queenAndBishops)
        return true;

    bitboard_t queenAndRooks   = m_bbQueens[oponent] | m_bbRooks[oponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);
    if(straightAttacks & queenAndRooks)
        return true;

    return false;
}

inline bool Board::isDiagonalChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbKing[color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndBishops = m_bbQueens[oponent] | m_bbBishops[oponent];
    bitboard_t diagonalAttacks = getBishopMoves(m_bbAllPieces, kingIdx);
    
    return (diagonalAttacks & queenAndBishops) != 0;
}

inline bool Board::isStraightChecked(Color color)
{
    uint8_t kingIdx = LS1B(m_bbKing[color]);
    Color oponent = Color(color ^ 1);

    bitboard_t queenAndRooks   = m_bbQueens[oponent] | m_bbRooks[oponent];
    bitboard_t straightAttacks = getRookMoves(m_bbAllPieces, kingIdx);

    return (straightAttacks & queenAndRooks) != 0;
}

std::string Board::getBoardString()
{
    std::stringstream ss;
    // for(int y = 0; y < 8; y++)
    // {
    //     for(int x = 0; x < 8; x++)
    //     {
    //         Piece *piece = m_board[x + y * 8];
    //         if(piece)
    //         {
    //             ss.put(piece->getChar());
    //         }
    //     }
    // }

    return ss.str();
}