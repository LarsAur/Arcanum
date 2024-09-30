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

    // Set cache to unknown
    board.m_checkedCache = Board::CheckedCacheState::UNKNOWN;
    board.m_moveset = Board::MoveSet::NOT_GENERATED;
    board.m_captureInfoGenerated = Board::MoveSet::NOT_GENERATED;
    board.m_kingIdx = LS1B(board.m_bbTypedPieces[W_KING][board.m_turn]);
    board.m_bbOpponentAttacks = 0LL;
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

// https://en.wikipedia.org/wiki/Algebraic_notation_(chess)
Move getMoveFromAlgebraic(std::string token, Board& board)
{
    uint32_t consumed = 0;
    int8_t fromFile = -1;
    int8_t fromRank = -1;
    Move toMatch = NULL_MOVE;
    std::string _token = token;

    // Remove possible semicolon from EDP
    if(token[token.size() - 1] == ';')
    {
        token = token.substr(0, token.size() - 1);
    }

    // Remove possible checkmate '#' or check '+' symbol
    if(token[token.size() - 1] == '#' || token[token.size() - 1] == '+')
    {
        token = token.substr(0, token.size() - 1);
    }

    {
        // Remove capture character 'x'
        size_t p = token.find('x');
        if(p != std::string::npos)
        {
            token.erase(p, 1);
        }

        // Remove capture character ':'
        p = token.find(':');
        if(p != std::string::npos)
        {
            token.erase(p, 1);
        }
    }

    // Parse king side castling moves
    if(token == "O-O" || token == "0-0")
    {
        toMatch.moveInfo |= board.getTurn() ? MoveInfoBit::CASTLE_BLACK_KING : MoveInfoBit::CASTLE_WHITE_KING;
        toMatch.to = board.getTurn() ? 62 : 6;
        goto find_move;
    }

    // Parse queen side castling moves
    if(token == "O-O-O" || token == "0-0-0")
    {
        toMatch.moveInfo |= board.getTurn() ? MoveInfoBit::CASTLE_BLACK_QUEEN : MoveInfoBit::CASTLE_WHITE_QUEEN;
        toMatch.to = board.getTurn() ? 58 : 2;
        goto find_move;
    }

    // Parse the first character containing the moved piece
    // If no character matches, the move is a pawn move
    switch (token[consumed])
    {
        case 'R': toMatch.moveInfo |= MoveInfoBit::ROOK_MOVE;   consumed++; break;
        case 'N': toMatch.moveInfo |= MoveInfoBit::KNIGHT_MOVE; consumed++; break;
        case 'B': toMatch.moveInfo |= MoveInfoBit::BISHOP_MOVE; consumed++; break;
        case 'Q': toMatch.moveInfo |= MoveInfoBit::QUEEN_MOVE;  consumed++; break;
        case 'K': toMatch.moveInfo |= MoveInfoBit::KING_MOVE;   consumed++; break;
        default:  toMatch.moveInfo |= MoveInfoBit::PAWN_MOVE;   break;
    }

    // Check if the move is ambigous
    if(token[consumed] >= 'a' && token[consumed] <= 'h' && token[consumed+1] >= '1' && token[consumed+1] <= '8')
    {
        if(token.size() > consumed + 3 && token[consumed+2] >= 'a' && token[consumed+2] <= 'h' && token[consumed+3] >= '1' && token[consumed+3] <= '8')
        {
            // Ambigous
            fromFile = token[consumed] - 'a';
            fromRank = token[consumed+1] - '8';
            toMatch.to = SQUARE(token[consumed+2] - 'a', token[consumed+3] - '1');
        }
        else
        {
            // Unambigous
            toMatch.to = SQUARE(token[consumed] - 'a', token[consumed+1] - '1');
        }
    }
    else if(token[consumed] >= 'a' && token[consumed] <= 'h')
    {
        // Ambigous
        fromFile = token[consumed] - 'a';
        if(token[consumed+1] >= 'a' && token[consumed+1] <= 'h' && token[consumed+2] >= '1' && token[consumed+2] <= '8')
        {
            toMatch.to = SQUARE(token[consumed+1] - 'a', token[consumed+2] - '1');
        }
    }
    else if(token[consumed] >= '1' && token[consumed] <= '8')
    {
        // Ambigous
        fromRank = token[consumed] - '1';
        if(token[consumed+1] >= 'a' && token[consumed+1] <= 'h' && token[consumed+2] >= '1' && token[consumed+2] <= '8')
        {
            toMatch.to = SQUARE(token[consumed+1] - 'a', token[consumed+2] - '1');
        }
    }

    // Check for possible promotions
    switch (token[token.size() - 1])
    {
        case 'R': toMatch.moveInfo |= MoveInfoBit::PROMOTE_ROOK;   break;
        case 'N': toMatch.moveInfo |= MoveInfoBit::PROMOTE_KNIGHT; break;
        case 'B': toMatch.moveInfo |= MoveInfoBit::PROMOTE_BISHOP; break;
        case 'Q': toMatch.moveInfo |= MoveInfoBit::PROMOTE_QUEEN;  break;
    }

    // Find the correct move based on the required match info
    find_move:
    Move* moves = board.getLegalMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    board.generateCaptureInfo();

    for(uint8_t i = 0; i < numMoves; i++)
    {
        if(RANK(moves[i].from) != fromRank && fromRank != -1)
            continue;

        if(FILE(moves[i].from) != fromFile && fromFile != -1)
            continue;

        // Verify that all the fields in toMatch is set in the move.
        // And that the destination squares are matching
        if((moves[i].moveInfo & toMatch.moveInfo) == toMatch.moveInfo && (moves[i].to == toMatch.to))
        {
            return moves[i];
        }
    }

    WARNING("No matching move found for " << _token << " in " << FEN::getFEN(board))
    return NULL_MOVE;
}

EDP FEN::parseEDP(std::string edp)
{
    std::string token;
    EDP desc;

    std::istringstream is(edp);

    // Parse the FEN
    is >> token; desc.fen.append(token);       // Position
    is >> token; desc.fen.append(" " + token); // Turn
    is >> token; desc.fen.append(" " + token); // Castle rights
    is >> token; desc.fen.append(" " + token); // Enpassant move
    desc.fen.append(" 0 1"); // Set default "half move clock" and "full move number" // TODO: Set based on fmvn and hmvc

    Board board = Board(desc.fen);
    Move* moves = board.getLegalMoves();
    board.generateCaptureInfo();

    while(is >> token)
    {
        if     (token == "acd" ) is >> desc.acd;
        else if(token == "acn" ) is >> desc.acn;
        else if(token == "acs" ) is >> desc.acs;
        else if(token == "am"  ) WARNING("Missing EDP am")
        else if(token == "bm"  ) WARNING("Missing EDP bm")
        else if(token == "c"   ) WARNING("Missing EDP comment")
        else if(token == "ce"  ) is >> desc.ce;
        else if(token == "dm"  ) is >> desc.dm;
        else if(token == "eco" ) is >> desc.eco;
        else if(token == "fmvn") is >> desc.fmvn;
        else if(token == "hmvc") is >> desc.hmvc;
        else if(token == "id"  ) is >> desc.id;
        else if(token == "nic" ) is >> desc.nic;
        else if(token == "pm"  ) {is >> token; desc.pm = getMoveFromAlgebraic(token, board); }
        else if(token == "pv"  ) WARNING("Missing EDP pv")
        else if(token == "rc"  ) is >> desc.rc;
        else if(token == "sm"  ) {is >> token; desc.pm = getMoveFromAlgebraic(token, board); }
        else if(token == "v0"  ) WARNING("Missing EDP variant name")
    }

    return desc;
}