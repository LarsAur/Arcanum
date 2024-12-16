#include <tuning/binpack.hpp>
#include <utils.hpp>
#include <fen.hpp>
#include <bitboard.hpp>

using namespace Arcanum;

BinpackParser::BinpackParser()
{
    m_currentMoveTextCount = 0;
    m_numBitsInBuffer = 0;
    m_bitBuffer = 0;
}

bool BinpackParser::open(std::string path)
{
    m_ifs.open(path, std::ios::binary);

    if(!m_ifs.is_open())
    {
        ERROR("Unable to open " << path)
        return false;
    }

    m_currentChuckStart = m_ifs.tellg();
    return true;
}

Board* BinpackParser::getNextBoard()
{
    if(m_currentChuckStart - m_ifs.tellg() == 0)
    {
        m_parseBlock();
    }

    if(m_currentMoveTextCount == 0)
    {
        m_parseChain();
        return &m_currentBoard;
    }

    m_parseNextMoveAndScore();
    return &m_currentBoard;
}

int16_t BinpackParser::getCurrentScore()
{
    return m_currentScore;
}

 int16_t BinpackParser::m_unsignedToSigned(uint16_t u)
{
    std::int16_t s;
    u = (u << 15) | (u >> 1);
    if (u & 0x8000) u ^= 0x7FFF;
    std::memcpy(&s, &u, sizeof(int16_t));
    return s;
}

uint16_t BinpackParser::m_bigToLittleEndian(uint16_t b)
{
    return (b >> 8) | (b << 8);
}

void BinpackParser::m_parseBlock()
{
    m_currentChuckStart = m_ifs.tellg();

    char header[4];
    m_ifs.read(header, 4);
    if(strncmp(header, "BINP", 4))
    {
        ERROR("Did not find BINP at the start of block")
        return;
    }

    // Chunk size is not documented, but it is implemented by SF
    // https://github.com/official-stockfish/Stockfish/blob/tools/src/extra/nnue_data_binpack_format.h#L6796-L6800
    // Chunk size seems to be stored as little endian
    m_ifs.read(reinterpret_cast<char*>(&m_currentChuckSize), 4);
}

// Reads the next N bits of the input file
// The leftover bits are stored in the bitBuffer to be read in the next read
uint8_t BinpackParser::m_getNextNBits(uint8_t numBits)
{
    // If there are not enough bits in the buffer
    // A byte has to be read from file and added to the buffer
    if(m_numBitsInBuffer < numBits)
    {
        uint8_t rbyte;
        m_ifs.read(reinterpret_cast<char*>(&rbyte), 1);

        // Insert the byte into the MSBs of the buffer
        // It is assumed that the 'empty' bits in the buffer is 0 bits due to how the bits are removed
        m_bitBuffer |= uint16_t(rbyte) << (8 - m_numBitsInBuffer);
        m_numBitsInBuffer += 8;
    }

    // Read N bits out of the buffer
    uint8_t bits = m_bitBuffer >> (16 - numBits);

    // Remove the bits from the buffer
    m_bitBuffer = m_bitBuffer << numBits;
    m_numBitsInBuffer -= numBits;

    // DEBUG(unsigned(numBits) << " " << std::bitset<8>(bits))
    return bits;
}

// Returns the index of the Nth set bit in the bitboard
square_t BinpackParser::m_getNthSetBitIndex(bitboard_t bb, uint8_t n)
{
    // Remove all set indices before the occBbIndex
    for(uint32_t i = 0; i < n; i++)
    {
        popLS1B(&bb);
    }

    return LS1B(bb);
}

// Returns the minimum number of bits required to represent the value
uint8_t BinpackParser::m_getMinRepBits(uint8_t value)
{
    return MS1B(value) + 1;
}


void BinpackParser::m_parseChain()
{
    m_parseStem();
    m_parseMovetextCount();
}

void BinpackParser::m_parseStem()
{
    m_parsePos();
    m_parseMove();
    m_parseScore();
    m_parsePlyAndResult();
    m_parseRule50();
}

// https://github.com/Sopel97/nnue_data_compress/blob/master/src/chess/Position.h#L1166
void BinpackParser::m_parsePos()
{
    constexpr Piece PieceMap[12] = {W_PAWN, B_PAWN, W_KNIGHT, B_KNIGHT, W_BISHOP, B_BISHOP, W_ROOK, B_ROOK, W_QUEEN, B_QUEEN, W_KING, B_KING};
    constexpr uint32_t PosByteSize = 24;

    unsigned char data[PosByteSize];
    m_ifs.read(reinterpret_cast<char*>(data), PosByteSize);

    m_currentBoard = Board();

    // Assume it is white's turn
    // The turn might be set to black's while parsing
    m_currentBoard.m_turn = WHITE;

    bitboard_t occupancy = (
          ((bitboard_t) data[0] << 56)
        | ((bitboard_t) data[1] << 48)
        | ((bitboard_t) data[2] << 40)
        | ((bitboard_t) data[3] << 32)
        | ((bitboard_t) data[4] << 24)
        | ((bitboard_t) data[5] << 16)
        | ((bitboard_t) data[6] <<  8)
        | ((bitboard_t) data[7])
    );

    m_currentBoard.m_bbAllPieces = occupancy;

    uint8_t* pieceState = reinterpret_cast<uint8_t*>(&data[8]);

    uint8_t nibbleIndex = 0;
    while (occupancy)
    {
        uint8_t occIndex = popLS1B(&occupancy);
        bitboard_t bbOcc = 1LL << occIndex;


        // Read the next nibble from pieceState
        uint8_t nibble = pieceState[nibbleIndex / 2];
        if(nibbleIndex % 2 == 0)
        {
            nibble = nibble & 0xf;
        }
        else
        {
            nibble = nibble >> 4;
        }
        nibbleIndex++;

        // Add the piece to the board depending on the nibble value
        switch (nibble)
        {
            case 0:  //  0 : white pawn
            case 1:  //  1 : black pawn
            case 2:  //  2 : white knight
            case 3:  //  3 : black knight
            case 4:  //  4 : white bishop
            case 5:  //  5 : black bishop
            case 6:  //  6 : white rook
            case 7:  //  7 : black rook
            case 8:  //  8 : white queen
            case 9:  //  9 : black queen
            case 10: // 10 : white king
            case 11: // 11 : black king
            {

            // Map the nibble value to the piece value used by Arcanum
            Piece piece = PieceMap[nibble];

            m_currentBoard.m_pieces[occIndex] = piece;

            // Separate the color and type of the piece
            Color color = WHITE;
            if(piece >= B_PAWN)
            {
                piece = Piece(piece - B_PAWN);
                color = BLACK;
            }

            m_currentBoard.m_bbTypedPieces[piece][color] |= bbOcc;
            m_currentBoard.m_bbColoredPieces[color] |= bbOcc;
            }
            break;
            case 12: // 12 : pawn with ep square behind (white or black, depending on rank)
                if(RANK(occIndex) == 3) // White pawn
                {
                    // Square of the captured piece
                    m_currentBoard.m_bbEnPassantTarget = bbOcc;
                    m_currentBoard.m_enPassantTarget = occIndex;

                    // Square moved to when capturing
                    m_currentBoard.m_bbEnPassantTarget = bbOcc - 8;
                    m_currentBoard.m_enPassantTarget = occIndex << 8;

                    m_currentBoard.m_pieces[occIndex] = W_PAWN;
                    m_currentBoard.m_bbTypedPieces[W_PAWN][WHITE] |= bbOcc;
                    m_currentBoard.m_bbColoredPieces[WHITE] |= bbOcc;
                }
                else // Black pawn
                {
                    // Square of the captured piece
                    m_currentBoard.m_bbEnPassantTarget = bbOcc;
                    m_currentBoard.m_enPassantTarget = occIndex;

                    // Square moved to when capturing
                    m_currentBoard.m_bbEnPassantTarget = bbOcc + 8;
                    m_currentBoard.m_enPassantTarget = occIndex >> 8;

                    m_currentBoard.m_pieces[occIndex] = B_PAWN;
                    m_currentBoard.m_bbTypedPieces[W_PAWN][BLACK] |= bbOcc;
                    m_currentBoard.m_bbColoredPieces[BLACK] |= bbOcc;
                }

            break;
            case 13: // 13 : white rook with coresponding castling rights
                if(occIndex == Square::A1)
                {
                    m_currentBoard.m_castleRights |= CastleRights::WHITE_QUEEN_SIDE;
                }
                else
                {
                    m_currentBoard.m_castleRights |= CastleRights::WHITE_KING_SIDE;
                }
                m_currentBoard.m_pieces[occIndex] = W_ROOK;
                m_currentBoard.m_bbTypedPieces[W_ROOK][WHITE] |= bbOcc;
                m_currentBoard.m_bbColoredPieces[WHITE] |= bbOcc;
            break;
            case 14: // 14 : black rook with coresponding castling rights
                if(occIndex == Square::A8)
                {
                    m_currentBoard.m_castleRights |= CastleRights::BLACK_QUEEN_SIDE;
                }
                else
                {
                    m_currentBoard.m_castleRights |= CastleRights::BLACK_KING_SIDE;
                }
                m_currentBoard.m_pieces[occIndex] = B_ROOK;
                m_currentBoard.m_bbTypedPieces[W_ROOK][BLACK] |= bbOcc;
                m_currentBoard.m_bbColoredPieces[BLACK] |= bbOcc;
            break;
            case 15: // 15 : black king and black is side to move
                m_currentBoard.m_pieces[occIndex] = B_KING;
                m_currentBoard.m_bbTypedPieces[W_KING][BLACK] |= bbOcc;
                m_currentBoard.m_bbColoredPieces[BLACK] |= bbOcc;
                m_currentBoard.m_turn = BLACK;
            break;
        }

        m_currentBoard.m_kingIdx = LS1B(m_currentBoard.m_bbTypedPieces[W_KING][m_currentBoard.m_turn]);
    }
}

// https://github.com/Sopel97/nnue_data_compress/blob/master/src/chess/Chess.h#L1044
void BinpackParser::m_parseMove()
{
    constexpr MoveInfoBit PromoteMap[4] = {MoveInfoBit::PROMOTE_KNIGHT, MoveInfoBit::PROMOTE_BISHOP, MoveInfoBit::PROMOTE_ROOK, MoveInfoBit::PROMOTE_QUEEN};
    char data[2];
    m_ifs.read(data, 2);

    CompressedMoveType type = CompressedMoveType(data[0] >> 6);
    square_t from = data[0] & 0b111111;
    square_t to = (data[1] >> 2) & 0b111111;
    uint32_t promoteBit = type == CompressedMoveType::PROMOTION ? PromoteMap[data[1] & 0b11] : 0;

    m_currentMove = m_currentBoard.generateMoveWithInfo(from, to, promoteBit);
}

void BinpackParser::m_parseScore()
{
    uint16_t uScore;
    m_ifs.read(reinterpret_cast<char*>(&uScore), 2);
    uScore = m_bigToLittleEndian(uScore);
    m_currentScore = m_unsignedToSigned(uScore);
}

void BinpackParser::m_parsePlyAndResult()
{
    constexpr uint16_t PlyMask = (1 << 14) - 1;

    uint16_t plyAndResult;
    m_ifs.read(reinterpret_cast<char*>(&plyAndResult), 2);

    plyAndResult = m_bigToLittleEndian(plyAndResult);

    uint16_t ply = plyAndResult & PlyMask;
    m_currentResult = m_unsignedToSigned(plyAndResult >> 14);

    // DEBUG("Result:" << m_currentResult)
}

void BinpackParser::m_parseRule50()
{
    uint16_t rule50;
    m_ifs.read(reinterpret_cast<char*>(&rule50), 2);
    m_currentBoard.m_rule50 = m_bigToLittleEndian(rule50);
}

void BinpackParser::m_parseMovetextCount()
{
    m_ifs.read(reinterpret_cast<char*>(&m_currentMoveTextCount), 2);
    m_currentMoveTextCount = m_bigToLittleEndian(m_currentMoveTextCount);

    // Erase the bit-buffer to prepare reading moves and scores
    m_bitBuffer = 0;
    m_numBitsInBuffer = 0;
}

// https://github.com/Sopel97/chess_pos_db/blob/master/docs/bcgn/variable_length.md
void BinpackParser::m_parseNextMoveAndScore()
{
    // Perform the current move before parsing the next
    m_currentBoard.performMove(m_currentMove);

    bitboard_t occupancy = m_currentBoard.m_bbColoredPieces[m_currentBoard.m_turn];
    uint8_t occBbIndexBitCount = m_getMinRepBits(CNTSBITS(occupancy) - 1);
    uint8_t occBbIndex = m_getNextNBits(occBbIndexBitCount); // The set bit index in the occupancy bitboard

    uint32_t promoteInfo = 0;
    square_t to;
    square_t from = m_getNthSetBitIndex(occupancy, occBbIndex);
    bitboard_t bbFrom = 1LL << from;
    Piece type = Piece((m_currentBoard.m_pieces[from] % B_PAWN));
    Color opponent = Color(m_currentBoard.m_turn^1);

    if(type == W_PAWN)
    {
        bitboard_t destinations;
        uint8_t promotionRank;
        if(m_currentBoard.m_turn == WHITE)
        {
            promotionRank = 6;
            // Forward move and potential double forward move
            // Note that destination for the first forward move is used to generate the double forward move
            // This gives us the check for the first square in the double move
            destinations = getWhitePawnMoves(bbFrom) & ~m_currentBoard.m_bbAllPieces;
            if(RANK(from) == 1) destinations |= getWhitePawnMoves(destinations) & ~m_currentBoard.m_bbAllPieces;

            // Attacks and enpassant squares
            destinations |= (getWhitePawnAttacks(bbFrom) & (m_currentBoard.m_bbColoredPieces[opponent] | m_currentBoard.m_bbEnPassantSquare));
        }
        else
        {
            promotionRank = 1;
            // Forward move and potential double forward move
            // Note that destination for the first forward move is used to generate the double forward move
            // This gives us the check for the first square in the double move
            destinations = getBlackPawnMoves(bbFrom) & ~m_currentBoard.m_bbAllPieces;
            if(RANK(from) == 6) destinations |= getBlackPawnMoves(destinations) & ~m_currentBoard.m_bbAllPieces;

            // Attacks and enpassant squares
            destinations |= (getBlackPawnAttacks(bbFrom) & (m_currentBoard.m_bbColoredPieces[opponent] | m_currentBoard.m_bbEnPassantSquare));
        }

        if(RANK(from) == promotionRank)
        {
            constexpr MoveInfoBit PromoteMap[4] = {MoveInfoBit::PROMOTE_KNIGHT, MoveInfoBit::PROMOTE_BISHOP, MoveInfoBit::PROMOTE_ROOK, MoveInfoBit::PROMOTE_QUEEN};

            // Note: The destination count is multiplied by 4 to account for all promotion types
            uint8_t destIndexBitCount = m_getMinRepBits(4 * CNTSBITS(destinations) - 1);
            uint8_t moveId = m_getNextNBits(destIndexBitCount);

            // Find promoted piece
            promoteInfo = PromoteMap[moveId % 4];

            // Find the 'to' square
            to = m_getNthSetBitIndex(destinations, moveId / 4);
        }
        else
        {
            uint8_t destIndexBitCount = m_getMinRepBits(CNTSBITS(destinations) - 1);
            uint8_t moveId = m_getNextNBits(destIndexBitCount);

            // Find the 'to' square
            to = m_getNthSetBitIndex(destinations, moveId);
        }
    }
    else if(type == W_KING)
    {
        bitboard_t moves = getKingMoves(from) & ~m_currentBoard.m_bbColoredPieces[m_currentBoard.m_turn];

        uint8_t castleRights = m_currentBoard.m_turn == WHITE
            ? (m_currentBoard.m_castleRights & (CastleRights::WHITE_KING_SIDE | CastleRights::WHITE_QUEEN_SIDE))
            : (m_currentBoard.m_castleRights & (CastleRights::BLACK_KING_SIDE | CastleRights::BLACK_QUEEN_SIDE));

        uint8_t numCastleRights = CNTSBITS(castleRights);
        uint8_t numMoves = CNTSBITS(moves);

        uint8_t indexBitCount = m_getMinRepBits(numCastleRights + numMoves - 1);

        uint8_t moveId = m_getNextNBits(indexBitCount);

        // Check if it is a castling move or normal move
        if(moveId >= numMoves)
        {
            uint8_t castleIndex = moveId - numMoves;

            // Check if castleIndex == 0 and Queen side castle is available
            to = castleIndex == 0 && castleRights & (CastleRights::WHITE_QUEEN_SIDE | CastleRights::BLACK_QUEEN_SIDE)
            ? from - 2
            : from + 2;
        }
        else
        {
            // Find the 'to' square
            to = m_getNthSetBitIndex(moves, moveId);
        }
    }
    else
    {
        bitboard_t moves;
        switch (type)
        {
        case W_ROOK:
            moves = getRookMoves(m_currentBoard.m_bbAllPieces, from);
            break;
        case W_KNIGHT:
            moves = getKnightMoves(from);
            break;
        case W_BISHOP:
            moves = getBishopMoves(m_currentBoard.m_bbAllPieces, from);
            break;
        case W_QUEEN:
            moves = getQueenMoves(m_currentBoard.m_bbAllPieces, from);
            break;
        default:
            ERROR("Unhandled type in move parsing")
            moves = 0LL;
        }
        moves &= ~m_currentBoard.m_bbColoredPieces[m_currentBoard.m_turn];
        uint8_t numBits = m_getMinRepBits(CNTSBITS(moves) - 1);
        uint8_t moveId = m_getNextNBits(numBits);
        to = m_getNthSetBitIndex(moves, moveId);
    }

    m_currentMove = m_currentBoard.generateMoveWithInfo(from, to, promoteInfo);

    // std::cout << m_currentMove << std::endl;
    // std::cout << FEN::getFEN(m_currentBoard) << std::endl;

    m_parseVEncodedScore();

    m_currentMoveTextCount--;
}

void BinpackParser::m_parseVEncodedScore()
{
    constexpr uint32_t BlockSize = 4;
    uint16_t mask = (1 << BlockSize) - 1;
    uint8_t offset = 0;
    uint16_t value = 0;

    while (true)
    {
        uint16_t block = m_getNextNBits(BlockSize + 1);
        value |= (block & mask) << offset;

        // If the first bit of the block is 0 we can stop reading
        if(!(block >> BlockSize)) break;

        offset += BlockSize;
    }

    m_currentScore = -m_currentScore + m_unsignedToSigned(value);
}