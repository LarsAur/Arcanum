#include <fen.hpp>
#include <zobrist.hpp>

using namespace Arcanum;

bool FEN::m_setPosition(Board& board, std::istringstream& is)
{
    // -- Create empty board
    board.m_bbAllPieces = 0LL;
    for(int c = 0; c < Color::NUM_COLORS; c++)
    {
        board.m_bbColoredPieces[c] = 0LL;
        for(int t = 0; t < 6; t++)
        {
            board.m_bbTypedPieces[t][c] = 0LL;
        }
    }

    for(int i = 0; i < 64; i++)
    {
        board.m_pieces[i] = Piece::NO_PIECE;
    }

    // -- Create board from FEN
    int file = 0, rank = 7;
    char chr;

    // Loop through all the squares rank by rank.
    // A8 -> H8, A7 -> H7 ..., A1 -> H1
    while (!(file > 7 && rank == 0))
    {
        is.get(chr);

        // Verify that the EOF is not hit
        if(is.eof())
        {
            ERROR("Not enough characters in FEN string")
            return false;
        }

        // Move to next rank
        if(chr == '/')
        {
            file = 0;
            rank--;
            continue;
        }

        // Verify that the file is still within the board
        // I.e. There are no missing '/'
        if(file >= 8)
        {
            ERROR("Too many squares on the rank in FEN string: " << file)
            return false;
        }

        // Skip squares N squares if the char is a digit N
        if(std::isdigit(chr))
        {
            if(chr <= '0' || chr > '8')
            {
                ERROR("Invalid empty squares in FEN string: " << chr)
                return false;
            }

            file += chr - '0';
            continue;
        }

        // At this point, the char should represent a piece
        // Get the color of the piece
        Color color = BLACK;
        if(std::isupper(chr))
        {
            color = WHITE;
            chr = std::tolower(chr);
        }

        square_t square = SQUARE(file, rank);
        bitboard_t bbSquare = SQUARE_BB(file, rank);

        board.m_bbColoredPieces[color] |= bbSquare;
        board.m_bbAllPieces |= bbSquare;

        switch (chr)
        {
        case 'p':
            board.m_bbTypedPieces[Piece::PAWN][color] |= bbSquare;
            board.m_pieces[square] = Piece::PAWN;
            break;
            case 'r':
            board.m_bbTypedPieces[Piece::ROOK][color] |= bbSquare;
            board.m_pieces[square] = Piece::ROOK;
            break;
            case 'n':
            board.m_bbTypedPieces[Piece::KNIGHT][color] |= bbSquare;
            board.m_pieces[square] = Piece::KNIGHT;
            break;
            case 'b':
            board.m_bbTypedPieces[Piece::BISHOP][color] |= bbSquare;
            board.m_pieces[square] = Piece::BISHOP;
            break;
            case 'k':
            board.m_bbTypedPieces[Piece::KING][color] = bbSquare;
            board.m_pieces[square] = Piece::KING;
            break;
            case 'q':
            board.m_bbTypedPieces[Piece::QUEEN][color] |= bbSquare;
            board.m_pieces[square] = Piece::QUEEN;
            break;
        default:
            ERROR("Unknown piece: " << chr << " in FEN string")
            return false;
        }

        // Move to the next square
        file++;
    }

    return true;
}

bool FEN::m_setTurn(Board& board, std::istringstream& is)
{
    char chr;
    is.get(chr);

    // Verify that the EOF is not hit
    if(is.eof())
    {
        ERROR("Not enough characters in FEN string")
        return false;
    }

    if(chr == 'w')
    {
        board.m_turn = Color::WHITE;
    }
    else if(chr == 'b')
    {
        board.m_turn = Color::BLACK;
    }
    else
    {
        ERROR("Illegal turn: " << chr);
        return false;
    }

    return true;
}

bool FEN::m_setCastleRights(Board& board, std::istringstream& is, bool strict)
{
    char chr;
    board.m_castleRights = 0;

    is.get(chr);

    // Verify that the EOF is not hit
    if(is.eof())
    {
        ERROR("Not enough characters in FEN string")
        return false;
    }

    // No castle rights available
    if(chr == '-')
    {
        return true;
    }

    // Read the castle rights until the next character is a space
    while(true)
    {
        switch (chr)
        {
        case 'K': board.m_castleRights |= CastleRights::WHITE_KING_SIDE;  break;
        case 'Q': board.m_castleRights |= CastleRights::WHITE_QUEEN_SIDE; break;
        case 'k': board.m_castleRights |= CastleRights::BLACK_KING_SIDE;  break;
        case 'q': board.m_castleRights |= CastleRights::BLACK_QUEEN_SIDE; break;
        default:
            ERROR("Illegal castle right: " << chr);
            return false;
        }

        // Exit the loop if there are no more castle rights
        if(is.peek() == ' ')
        {
            break;
        }

        is.get(chr);

        // Verify that the EOF is not hit
        if(is.eof())
        {
            ERROR("Not enough characters in FEN string")
            return false;
        }
    }

    // Check if the castle rights are legal
    // If not in strict mode, fix the castle rights and return true
    // If in strict mode, fail the parsing
    uint8_t prev = board.m_castleRights;
    if(board.m_bbTypedPieces[Piece::KING][WHITE] != (1LL << Square::E1) || (board.m_bbTypedPieces[Piece::ROOK][WHITE] & (1LL << Square::H1)) == 0) board.m_castleRights &= ~CastleRights::WHITE_KING_SIDE;
    if(board.m_bbTypedPieces[Piece::KING][WHITE] != (1LL << Square::E1) || (board.m_bbTypedPieces[Piece::ROOK][WHITE] & (1LL << Square::A1)) == 0) board.m_castleRights &= ~CastleRights::WHITE_QUEEN_SIDE;
    if(board.m_bbTypedPieces[Piece::KING][BLACK] != (1LL << Square::E8) || (board.m_bbTypedPieces[Piece::ROOK][BLACK] & (1LL << Square::H8)) == 0) board.m_castleRights &= ~CastleRights::BLACK_KING_SIDE;
    if(board.m_bbTypedPieces[Piece::KING][BLACK] != (1LL << Square::E8) || (board.m_bbTypedPieces[Piece::ROOK][BLACK] & (1LL << Square::A8)) == 0) board.m_castleRights &= ~CastleRights::BLACK_QUEEN_SIDE;

    if(strict && prev != board.m_castleRights)
    {
        ERROR("The castle rights were illegal: " << unsigned(prev) << " to " << unsigned(board.m_castleRights))
        return false;
    }

    return true;
}

bool FEN::m_setEnpassantTarget(Board& board, std::istringstream& is)
{
    char chr;

    // Read enpassant square
    board.m_enPassantSquare = Square::NONE;
    board.m_enPassantTarget = Square::NONE;
    board.m_bbEnPassantSquare = 0LL;
    board.m_bbEnPassantTarget = 0LL;

    is.get(chr);

    // Verify that the EOF is not hit
    if(is.eof())
    {
        ERROR("Not enough characters in FEN string")
        return false;
    }

    // No enpassant square available
    if(chr == '-')
    {
        return true;
    }

    int32_t file = chr - 'a';

    is.get(chr);

    // Verify that the EOF is not hit
    if(is.eof())
    {
        ERROR("Not enough characters in FEN string")
        return false;
    }

    int32_t rank = chr - '1';

    // Verify that the enpassant square is on a legal rank and file
    if(file < 0 || file > 7 || !(rank == 2 || rank == 5))
    {
        ERROR("Illegal enpassant square");
        return false;
    }

    // Set the enpassant square
    board.m_enPassantSquare = SQUARE(file, rank);
    board.m_bbEnPassantSquare = SQUARE_BB(file, rank);

    // Set the location of the target piece
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

    return true;
}

bool FEN::m_setHalfmoveClock(Board& board, std::istringstream& is)
{
    if(!std::isdigit(is.peek()))
    {
        ERROR("The half move clock is not a number")
        return false;
    }

    // Read half moves
    uint16_t rule50;
    is >> rule50;
    board.m_rule50 = uint8_t(rule50);

    return true;
}

bool FEN::m_setFullmoveClock(Board& board, std::istringstream& is)
{
    if(!std::isdigit(is.peek()))
    {
        ERROR("The full move clock is not a number")
        return false;
    }

    // Read full moves
    is >> board.m_fullMoves;

    return true;
}

bool FEN::m_consumeExpectedSpace(std::istringstream& is)
{
    char chr;
    is.get(chr);

    // Verify that the EOF is not hit
    if(is.eof())
    {
        ERROR("Not enough characters in FEN string")
        return false;
    }

    if(chr != ' ')
    {
        ERROR("Missing expected space in FEN string");
        return false;
    }

    return true;
}

bool FEN::setFEN(Board& board, const std::string fen, bool strict)
{
    bool success = true;
    std::istringstream is(fen);

    success &= m_setPosition(board, is);
    success &= m_consumeExpectedSpace(is);
    success &= m_setTurn(board, is);
    success &= m_consumeExpectedSpace(is);
    success &= m_setCastleRights(board, is, strict);
    success &= m_consumeExpectedSpace(is);
    success &= m_setEnpassantTarget(board, is);

    // Some FEN strings does not contain information about the move clocks
    // By disabling strict mode, these are not parsed and instead set to zero
    // Disabling strict mode also handles cases where the castle rights are illegal
    // This can happen if the rooks or kings are not in legal castle positions
    // Simply remove the castle rights and show a warning
    if(strict)
    {
        success &= m_consumeExpectedSpace(is);
        success &= m_setHalfmoveClock(board, is);
        success &= m_consumeExpectedSpace(is);
        success &= m_setFullmoveClock(board, is);
    }
    else
    {
        board.m_rule50 = 0;
        board.m_fullMoves = 0;
    }

    if(!success)
    {
        ERROR("Unable to parse FEN: " << fen);
    }

    Zobrist::zobrist.getHashs(board, board.m_hash, board.m_pawnHash, board.m_materialHash);

    // Set cache to unknown
    board.m_moveset = Board::MoveSet::NOT_GENERATED;
    board.m_captureInfoGenerated = Board::MoveSet::NOT_GENERATED;
    board.m_kingIdx = LS1B(board.m_bbTypedPieces[Piece::KING][board.m_turn]);
    board.m_bbOpponentAttacks = 0LL;

    return success;
}

std::string FEN::getFEN(const Board& board)
{
    std::string str;
    int emptyCnt = 0;
    for(int rank = 7; rank >= 0; rank--)
    {
        for(int file = 0; file < 8; file++)
        {
            square_t square = SQUARE(file, rank);

            char c = '\0';
            switch (board.m_pieces[square])
            {
            case NO_PIECE: emptyCnt++; break;
            case Piece::PAWN:    c = 'P'; break;
            case Piece::ROOK:    c = 'R'; break;
            case Piece::KNIGHT:  c = 'N'; break;
            case Piece::BISHOP:  c = 'B'; break;
            case Piece::QUEEN:   c = 'Q'; break;
            case Piece::KING:    c = 'K'; break;
            }

            if(board.getColorAt(square) == Color::BLACK)
            {
                c = std::tolower(c);
            }

            if(c != '\0')
            {
                if(emptyCnt > 0)
                {
                    str += std::to_string(emptyCnt);
                }
                str += c;
                emptyCnt = 0;
            }
        }

        if(emptyCnt > 0) str += std::to_string(emptyCnt);
        emptyCnt = 0;
        if(rank != 0) str += "/";
    }

    // Turn
    str += ((board.m_turn == Color::WHITE) ? " w " : " b ");
    // Castle
    if(board.m_castleRights & CastleRights::WHITE_KING_SIDE)  str += "K";
    if(board.m_castleRights & CastleRights::WHITE_QUEEN_SIDE) str += "Q";
    if(board.m_castleRights & CastleRights::BLACK_KING_SIDE)  str += "k";
    if(board.m_castleRights & CastleRights::BLACK_QUEEN_SIDE) str += "q";
    if(board.m_castleRights == 0) str += "-";

    // Enpassant
    if(board.m_enPassantSquare != Square::NONE) str += " " + squareToString(board.m_enPassantSquare) + " ";
    else str += " - ";

    // Half moves
    str += std::to_string(unsigned(board.m_rule50)) + " ";
    // Full moves
    str += std::to_string(board.m_fullMoves);

    return str;
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
            if(board.m_bbTypedPieces[Piece::PAWN][WHITE] & mask)
                ss << "P ";
            else if(board.m_bbTypedPieces[Piece::ROOK][WHITE] & mask)
                ss << "R ";
            else if(board.m_bbTypedPieces[Piece::KNIGHT][WHITE] & mask)
                ss << "N ";
            else if(board.m_bbTypedPieces[Piece::BISHOP][WHITE] & mask)
                ss << "B ";
            else if(board.m_bbTypedPieces[Piece::QUEEN][WHITE] & mask)
                ss << "Q ";
            else if(board.m_bbTypedPieces[Piece::KING][WHITE] & mask)
                ss << "K ";
            else if(board.m_bbTypedPieces[Piece::PAWN][BLACK] & mask)
                ss << "p ";
            else if(board.m_bbTypedPieces[Piece::ROOK][BLACK] & mask)
                ss << "r ";
            else if(board.m_bbTypedPieces[Piece::KNIGHT][BLACK] & mask)
                ss << "n ";
            else if(board.m_bbTypedPieces[Piece::BISHOP][BLACK] & mask)
                ss << "b ";
            else if(board.m_bbTypedPieces[Piece::QUEEN][BLACK] & mask)
                ss << "q ";
            else if(board.m_bbTypedPieces[Piece::KING][BLACK] & mask)
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

void FEN::m_parseVariation(Board& board, std::vector<Move>& variation, std::istringstream& is)
{
    std::string token;
    Board _board = Board(board);

    is >> token;
    while(token != ";" && !is.eof())
    {
        Move move = getMoveFromAlgebraic(token, _board);
        _board.performMove(move);
        variation.push_back(move);
        is >> token;
    }
}

void FEN::m_parseMovelist(Board& board, std::vector<Move>& list, std::istringstream& is)
{
    std::string token;

    is >> token;
    while(token != ";" && !is.eof())
    {
        Move move = getMoveFromAlgebraic(token, board);
        list.push_back(move);
        is >> token;
    }
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

    Board board = Board(desc.fen, false);

    while(is >> token)
    {
        if     (token == "acd" ) is >> desc.acd;
        else if(token == "acn" ) is >> desc.acn;
        else if(token == "acs" ) is >> desc.acs;
        else if(token == "am"  ) m_parseMovelist(board, desc.am, is);
        else if(token == "bm"  ) m_parseMovelist(board, desc.bm, is);
        else if(token == "c"   ) WARNING("Missing EDP comment")
        else if(token == "ce"  ) is >> desc.ce;
        else if(token == "dm"  ) is >> desc.dm;
        else if(token == "eco" ) is >> desc.eco;
        else if(token == "fmvn") is >> desc.fmvn;
        else if(token == "hmvc") is >> desc.hmvc;
        else if(token == "id"  ) is >> desc.id;
        else if(token == "nic" ) is >> desc.nic;
        else if(token == "pm"  ) {is >> token; desc.pm = getMoveFromAlgebraic(token, board); }
        else if(token == "pv"  ) m_parseVariation(board, desc.pv, is);
        else if(token == "rc"  ) is >> desc.rc;
        else if(token == "sm"  ) {is >> token; desc.pm = getMoveFromAlgebraic(token, board); }
        else if(token == "v0"  ) WARNING("Missing EDP variant name")
    }

    return desc;
}