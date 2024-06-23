#include <fen.hpp>
#include <zobrist.hpp>

using namespace Arcanum;

bool FEN::setFEN(Board& board, const std::string fen)
{
    // -- Create empty board
    board.m_bbAllPieces = 0LL;
    for(int c = 0; c < NUM_COLORS; c++)
    {
        board.m_bbColoredPieces[c] = 0LL;
        for(int t = 0; t < 6; t++)
        {
            board.m_bbTypedPieces[t][c] = 0LL;
        }
    }

    for(int i = 0; i < 64; i++)
    {
        board.m_pieces[i] = NO_PIECE;
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

        square_t square = ((rank << 3) | file);
        bitboard_t bbSquare = 0b1LL << square;
        file++;

        board.m_bbColoredPieces[color] |= bbSquare;
        board.m_bbAllPieces |= bbSquare;

        switch (chr)
        {
        case 'p':
            board.m_bbTypedPieces[W_PAWN][color] |= bbSquare;
            board.m_pieces[square] = Piece(W_PAWN + (B_PAWN - W_PAWN) * color);
            break;
        case 'r':
            board.m_bbTypedPieces[W_ROOK][color] |= bbSquare;
            board.m_pieces[square] = Piece(W_ROOK + (B_PAWN - W_PAWN) * color);
            break;
        case 'n':
            board.m_bbTypedPieces[W_KNIGHT][color] |= bbSquare;
            board.m_pieces[square] = Piece(W_KNIGHT + (B_PAWN - W_PAWN) * color);
            break;
        case 'b':
            board.m_bbTypedPieces[W_BISHOP][color] |= bbSquare;
            board.m_pieces[square] = Piece(W_BISHOP + (B_PAWN - W_PAWN) * color);
            break;
        case 'k':
            board.m_bbTypedPieces[W_KING][color] = bbSquare;
            board.m_pieces[square] = Piece(W_KING + (B_PAWN - W_PAWN) * color);
            break;
        case 'q':
            board.m_bbTypedPieces[W_QUEEN][color] |= bbSquare;
            board.m_pieces[square] = Piece(W_QUEEN + (B_PAWN - W_PAWN) * color);
            break;
        default:
            ERROR("Unknown piece: " << chr << " in " << fen)
            return false;
        }
    }

    // Skip space
    char chr = fen[fenPosition++];
    if(chr != ' ')
    {
        ERROR("Missing space after board in " << fen);
        return false;
    }

    // Read turn
    chr = fen[fenPosition++];
    if(chr == 'w') board.m_turn = WHITE;
    else if(chr == 'b') board.m_turn = BLACK;
    else {
        ERROR("Illegal turn: " << chr << " in " << fen);
        return false;
    }

    // Skip space
    chr = fen[fenPosition++];
    if(chr != ' ')
    {
        ERROR("Missing space after turn in " << fen);
        return false;
    }

    // Read castle rights
    board.m_castleRights = 0;
    chr = fen[fenPosition++];
    if(chr != '-')
    {
        int safeGuard = 0;
        while(chr != ' ' && safeGuard < 5)
        {
            switch (chr)
            {
            case 'K': board.m_castleRights |= WHITE_KING_SIDE; break;
            case 'Q': board.m_castleRights |= WHITE_QUEEN_SIDE; break;
            case 'k': board.m_castleRights |= BLACK_KING_SIDE; break;
            case 'q': board.m_castleRights |= BLACK_QUEEN_SIDE; break;
            default:
                ERROR("Illegal castle right: " << chr << " in " << fen);
                return false;
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
            ERROR("Missing space after castle rights in " << fen);
            return false;
        }
    }

    // Read enpassant square
    board.m_enPassantSquare = 64;
    board.m_enPassantTarget = 64;
    board.m_bbEnPassantSquare = 0LL;
    board.m_bbEnPassantTarget = 0LL;
    chr = fen[fenPosition++];
    if(chr != '-')
    {
        int file = chr - 'a';
        int rank = fen[fenPosition++] - '1';

        if(file < 0 || file > 7 || rank < 0 || rank > 7)
        {
            ERROR("Illegal enpassant square " << fen);
            return false;
        }

        board.m_enPassantSquare = (rank << 3) | file;
        board.m_bbEnPassantSquare = 1LL << board.m_enPassantSquare;
        if(rank == 2)
        {
            board.m_enPassantTarget = board.m_enPassantSquare + 8;
            board.m_bbEnPassantTarget = 1LL << board.m_enPassantTarget;
        }
        else
        {
            board.m_enPassantTarget = board.m_enPassantSquare - 8;
            board.m_bbEnPassantTarget = 1LL << board.m_enPassantTarget;
        }
    }

    // Skip space
    chr = fen[fenPosition++];
    if(chr != ' ')
    {
        ERROR("Missing space after enpassant square in " << fen);
        return false;
    }

    chr = fen[fenPosition++];
    if(chr < '0' || chr > '9')
    {
        ERROR("Number of half moves is not a number: " << chr << " in " << fen)
        return false;
    }

    // Read half moves
    board.m_rule50 = atoi(fen.c_str() + fenPosition - 1);

    // Skip until space
    while(fen[fenPosition++] != ' ' && fenPosition < (int) fen.length());

    if(fenPosition == (int) fen.length())
    {
        ERROR("Missing number of full moves in " << fen)
        return false;
    }

    chr = fen[fenPosition];
    if(chr < '0' || chr > '9')
    {
        ERROR("Number of full moves is not a number " << fen)
        return false;
    }

    // Read full moves
    board.m_fullMoves = atoi(fen.c_str() + fenPosition);

    s_zobrist.getHashs(board, board.m_hash, board.m_pawnHash, board.m_materialHash);

    board.m_numNonReversableMovesPerformed = 0;

    // Set cache to unknown
    board.m_checkedCache = Board::CheckedCacheState::UNKNOWN;
    board.m_moveset = Board::MoveSet::NOT_GENERATED;
    board.m_captureInfoGenerated = Board::MoveSet::NOT_GENERATED;

    return true;
}

std::string FEN::getFEN(const Board& board)
{
    std::stringstream ss;
    int emptyCnt = 0;
    for(int rank = 7; rank >= 0; rank--)
    {
        for(int file = 0; file < 8; file++)
        {
            char c = '\0';
            switch (board.m_pieces[file + rank * 8])
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
    ss << ((board.m_turn == Color::WHITE) ? " w " : " b ");

    // Castle
    if(board.m_castleRights & WHITE_KING_SIDE)  ss << "K";
    if(board.m_castleRights & WHITE_QUEEN_SIDE) ss << "Q";
    if(board.m_castleRights & BLACK_KING_SIDE)  ss << "k";
    if(board.m_castleRights & BLACK_QUEEN_SIDE) ss << "q";
    if(board.m_castleRights == 0) ss << "-";

    // Enpassant
    if(board.m_enPassantSquare != 64) ss << " " << squareToString(board.m_enPassantSquare) << " ";
    else ss << " - ";

    // Half moves
    ss << unsigned(board.m_rule50) << " ";
    // Full moves
    ss << board.m_fullMoves;

    return ss.str();
}

std::string FEN::toString(const Board& board)
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
            if(board.m_bbTypedPieces[W_PAWN][WHITE] & mask)
                ss << "P ";
            else if(board.m_bbTypedPieces[W_ROOK][WHITE] & mask)
                ss << "R ";
            else if(board.m_bbTypedPieces[W_KNIGHT][WHITE] & mask)
                ss << "N ";
            else if(board.m_bbTypedPieces[W_BISHOP][WHITE] & mask)
                ss << "B ";
            else if(board.m_bbTypedPieces[W_QUEEN][WHITE] & mask)
                ss << "Q ";
            else if(board.m_bbTypedPieces[W_KING][WHITE] & mask)
                ss << "K ";
            else if(board.m_bbTypedPieces[W_PAWN][BLACK] & mask)
                ss << "p ";
            else if(board.m_bbTypedPieces[W_ROOK][BLACK] & mask)
                ss << "r ";
            else if(board.m_bbTypedPieces[W_KNIGHT][BLACK] & mask)
                ss << "n ";
            else if(board.m_bbTypedPieces[W_BISHOP][BLACK] & mask)
                ss << "b ";
            else if(board.m_bbTypedPieces[W_QUEEN][BLACK] & mask)
                ss << "q ";
            else if(board.m_bbTypedPieces[W_KING][BLACK] & mask)
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