#include <binpack.hpp>

using namespace Arcanum;

// Threshold for when chunks are written to file
constexpr uint64_t TargetChunkSize = 1 * 1024 * 1024; // 1 MB

BinpackEncoder::BinpackEncoder()
{

}

bool BinpackEncoder::open(std::string path)
{
    m_ofs.open(path, std::ios::binary | std::ios::app);

    if(!m_ofs.is_open())
    {
        ERROR("Unable to open " << path)
        return false;
    }

    m_buffer.reserve(TargetChunkSize * 1.1f); // Reserve of 110% of the target chunk size

    return true;
}

void BinpackEncoder::close()
{
    // Write any remaining data in the buffer to file
    if(m_buffer.size() > 0)
    {
        m_writeBlock();
    }

    m_ofs.close();
}

void BinpackEncoder::addPosition(
    const Board& board,
    const Move& move,
    eval_t score,
    GameResult result
)
{
    m_reservedMoveVector.push_back(move);
    m_reservedScoreVector.push_back(score);
    addGame(board, m_reservedMoveVector, m_reservedScoreVector, result);
    m_reservedMoveVector.clear();
    m_reservedScoreVector.clear();
}

void BinpackEncoder::addGame(
    const Board& startBoard,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
    GameResult result
)
{
    // Write the chunk/block if it is larger than the target chunk size
    if(m_buffer.size() >= TargetChunkSize)
    {
        m_writeBlock();
    }

    Board board = Board(startBoard);
    // Write chain (stem + movetextcount)
    m_writeStem(board, moves[0], scores[0], result);
    m_writeMovetextCount(moves.size());

    // Perform the first move
    board.performMove(moves[0]);

    m_numBitsInBitBuffer = 0;
    m_bitBuffer = 0;

    // Write all remaining moves and scores
    for(uint32_t i = 1; i < moves.size(); i++)
    {
        m_writeEncodedMove(board, moves[i]);
        m_writeVEncodedScore(scores[i-1], scores[i]);
        board.performMove(moves[i]);
    }

    // Flush the bit-buffer after writing all moves.
    // This is to be ready to write new games
    m_flushBitbuffer();
}

// Write N bits to the bitbuffer
// When the bitbuffer has 8 or more bits, the byte is written to the buffer
void BinpackEncoder::m_writeNbits(uint8_t bits, uint8_t numBits)
{
    m_bitBuffer = (m_bitBuffer << numBits) | bits;
    m_numBitsInBitBuffer += numBits;

    if(m_numBitsInBitBuffer >= 8)
    {
        // Write the 8 MSBs of the bit-buffer
        // Note that this is not the MSB of the 16-bit buffer,
        // but the 8 most significant bits of the bits in the bit-buffer
        uint8_t toWrite = m_bitBuffer >> (m_numBitsInBitBuffer - 8);
        m_writeBytesToBuffer(&toWrite, 1);
        m_numBitsInBitBuffer -= 8;
    }
}

// Flushes the remaining part of the bit-buffer to the buffer
// even if there are less than 8-bits in the buffer
// If the bit-buffer is empty, nothing is written to the buffer
void BinpackEncoder::m_flushBitbuffer()
{
    if(m_numBitsInBitBuffer == 0)
    {
        return;
    }

    // There can be not more than 7 bits in the buffer
    // Note that we want the bits to be in the MSB part of the byte
    uint8_t toWrite = m_bitBuffer << (8 - m_numBitsInBitBuffer);
    m_writeBytesToBuffer(&toWrite, 1);
}

uint8_t BinpackEncoder::m_getMinRepBits(uint8_t value)
{
    return MS1B(value) + 1;
}

// Source: https://github.com/official-stockfish/Stockfish/blob/tools/docs/binpack.md
uint16_t BinpackEncoder::m_signedToUnsigned(int16_t s)
{
    uint16_t u;
    std::memcpy(&u, &s, sizeof(std::uint16_t));
    if (u & 0x8000) u ^= 0x7FFF;
    u = (u << 1) | (u >> 15);
    return u;
}

uint16_t BinpackEncoder::m_littleToBigEndian(uint16_t little)
{
    return (little >> 8) | (little << 8);
}

void BinpackEncoder::m_writeBytesToBuffer(void* src, uint32_t numBytes)
{
    if(m_buffer.capacity() < m_buffer.size() + numBytes)
    {
        WARNING("Buffer capacity is too small")
    }

    for(uint32_t i = 0; i < numBytes; i++)
    {
        m_buffer.push_back(((char*) src)[i]);
    }
}

void BinpackEncoder::m_writeBlock()
{
    DEBUG("Writing chunk: " << m_buffer.size() << " Bytes")

    // Write chunk header
    m_ofs.write("BINP", 4);

    // Write chunk size (little endian)
    uint32_t chunkSize = static_cast<uint32_t>(m_buffer.size());
    m_ofs.write(reinterpret_cast<char*>(&chunkSize), 4);

    // Write chunk
    m_ofs.write(m_buffer.data(), m_buffer.size());
    // Flush the chunk immediatly in case encoding is canceled without closing
    m_ofs.flush();

    // Reset chunk data
    // Note capacity is left unchanged
    m_buffer.clear();
}

void BinpackEncoder::m_writeStem(Board& board, Move& move, eval_t score, GameResult result)
{
    m_writePos(board);
    m_writeMove(move);
    m_writeScore(score);
    m_writePlyAndResult(result, board.getTurn(), board.getFullMoves());
    m_writeRule50(board.getHalfMoves());
}

void BinpackEncoder::m_writePos(Board& board)
{
    // Inverse of the PieceMap in m_parsePos in BinpackParser
    // W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING
    constexpr static uint8_t PieceToNibble[12] = {0, 6, 2, 4, 8, 10, 1, 7, 3, 5, 9, 11};

    constexpr uint32_t PosByteSize = 24;

    unsigned char data[PosByteSize];

    bitboard_t occupancy = board.m_bbAllPieces;

    data[0] = (0xff & (occupancy >> 56));
    data[1] = (0xff & (occupancy >> 48));
    data[2] = (0xff & (occupancy >> 40));
    data[3] = (0xff & (occupancy >> 32));
    data[4] = (0xff & (occupancy >> 24));
    data[5] = (0xff & (occupancy >> 16));
    data[6] = (0xff & (occupancy >> 8 ));
    data[7] = (0xff & (occupancy      ));

    uint8_t* pieceData = reinterpret_cast<uint8_t*>(&data[8]);
    uint8_t nibbleIndex = 0;
    while(occupancy)
    {
        uint8_t nibble = 0;
        square_t square = popLS1B(&occupancy);
        Piece piece = board.getPieceAt(square);

        switch (piece)
        {
        // These pieces have no special cases and can be directly mapped to their nibble
        case Piece::W_KING:
        case Piece::W_KNIGHT:
        case Piece::B_KNIGHT:
        case Piece::W_BISHOP:
        case Piece::B_BISHOP:
        case Piece::W_QUEEN:
        case Piece::B_QUEEN:
            nibble = PieceToNibble[piece];
            break;

        // For pawns, check if there is an enpassant square behind them
        case Piece::W_PAWN:
            nibble = (board.m_enPassantSquare == square - 8) ? 12 : PieceToNibble[Piece::W_PAWN];
            break;
        case Piece::B_PAWN:
            nibble = (board.m_enPassantSquare == square + 8) ? 12 : PieceToNibble[Piece::B_PAWN];
            break;

        // For rooks, castle rights have to be checked
        case Piece::W_ROOK:
            nibble = (
               ((square == Square::H1) && (board.getCastleRights() & CastleRights::WHITE_KING_SIDE))
            || ((square == Square::A1) && (board.getCastleRights() & CastleRights::WHITE_QUEEN_SIDE))
            ) ? 13 : PieceToNibble[Piece::W_ROOK];
            break;

        case Piece::B_ROOK:
            nibble = (
               ((square == Square::H8) && (board.getCastleRights() & CastleRights::BLACK_KING_SIDE))
            || ((square == Square::A8) && (board.getCastleRights() & CastleRights::BLACK_QUEEN_SIDE))
        ) ? 14 : PieceToNibble[Piece::B_ROOK];
            break;

        // Blacks king nibble is selected based on whos turn it is
        case Piece::B_KING:
            nibble = (board.getTurn() == Color::BLACK) ? 15 : PieceToNibble[Piece::B_KING];
            break;

        default:
            ERROR("Unknown piece: " << unsigned(piece))
            break;
        }


        // Write the nibble to the data array
        // First least significant nibble, then most significant nibble
        if(nibbleIndex % 2 == 0)
        {
            pieceData[nibbleIndex / 2] = nibble & 0xf;
        }
        else
        {
            pieceData[nibbleIndex / 2] = pieceData[nibbleIndex / 2] | (nibble << 4);
        }
        nibbleIndex++;
    }

    m_writeBytesToBuffer(data, PosByteSize);
}

void BinpackEncoder::m_writeMove(Move& move)
{
    // Rook, Knight, Bishop, Queen
    constexpr static uint8_t PromotionToBits[4] = {0b10, 0b00, 0b01, 0b11};
    char data[2];

    CompressedMoveType type = CompressedMoveType::NORMAL;
    uint8_t promoteBit = 0;
    if(move.isPromotion())
    {
        type = CompressedMoveType::PROMOTION;
        promoteBit = PromotionToBits[move.promotedPiece() - 1]; // -1 as Piece type starts with pawn
    }
    else if(move.isEnpassant())
    {
        type = CompressedMoveType::ENPASSANT;
    }
    else if(move.isCastle())
    {
        type = CompressedMoveType::CASTLE;
    }

    // TODO: Might have to change the to square of castle moves. See m_parseMove() in BinpackParser.

    data[0] = (uint8_t(type) << 6) | move.from;
    data[1] = (move.to << 2) | promoteBit;
    m_writeBytesToBuffer(data, 2);
}

void BinpackEncoder::m_writeScore(eval_t score)
{
    int16_t sScore = score;
    uint16_t uScore = m_signedToUnsigned(sScore);
    uScore = m_littleToBigEndian(uScore);
    m_writeBytesToBuffer(&uScore, 2);
}

void BinpackEncoder::m_writePlyAndResult(GameResult result, Color turn, uint16_t fullmove)
{
    constexpr uint16_t PlyMask = (1 << 14) - 1;

    // Calculate the number of plys based on full moves.
    // Note that full moves starts at 1 and an additional ply is performed when it is blacks turn
    int16_t plyBits = 2 * (fullmove - 1) + (turn == Color::BLACK);

    int16_t resultBits = -1; // Resultbit is -1 if current turn is losing
    if(result == GameResult::DRAW)
    {
        resultBits = 0;
    }
    else if(((result == GameResult::WHITE_WIN) && (turn == Color::WHITE))
    || ((result == GameResult::BLACK_WIN) && (turn == Color::BLACK)))
    {
        // Resultbit is 1 if current turn is winning
        resultBits = 1;
    }

    uint16_t plyAndResult = (m_signedToUnsigned(resultBits) << 14) | (plyBits & PlyMask);
    uint16_t data = m_littleToBigEndian(plyAndResult);
    m_writeBytesToBuffer(&data, 2);
}

void BinpackEncoder::m_writeRule50(uint8_t rule50)
{
    uint16_t data = m_littleToBigEndian(uint16_t(rule50));
    m_writeBytesToBuffer(&data, 2);
}

void BinpackEncoder::m_writeMovetextCount(uint32_t numMoves)
{
    uint16_t data = m_littleToBigEndian(numMoves - 1);
    m_writeBytesToBuffer(&data, 2);
}

void BinpackEncoder::m_writeEncodedMove(Board& board, Move& move)
{
    constexpr static uint8_t PromotionFromRanks[2] = {6, 1};

    bitboard_t occupancy = board.getColoredPieces(board.getTurn());

    // Find the number of 1s before the 'from' square in the occupancy bitboard
    uint8_t fromBitCount = m_getMinRepBits(CNTSBITS(occupancy) - 1);
    bitboard_t bbFrom = 1LL << move.from;
    uint8_t fromIndex = CNTSBITS(occupancy & (bbFrom - 1));
    m_writeNbits(fromIndex, fromBitCount);

    if(move.movedPiece() == W_PAWN)
    {
        uint8_t promotionRank = PromotionFromRanks[board.getTurn()];
        bitboard_t attacks = getPawnAttacks(bbFrom, board.getTurn());
        bitboard_t destinations = getPawnMoves(bbFrom, board.getTurn()) & ~board.m_bbAllPieces;
        destinations |= getPawnDoubleMoves(bbFrom, board.getTurn(), board.m_bbAllPieces);

        // SF Binpacks does not include the enpassant square if the move would cause the king to become checked
        // Thus we have to invalidate the enpassant square if this is the case to not end up with an additional
        // bit in the destionations bitboard. To simplify it, we generate all legal moves on the board and check
        // if an enpassant move is legal, as this check is done in move generation
        // Note that the legal enpassant move does not be the move currently being encoded.
        // TODO: This can be done without generating all legal moves
        bitboard_t bbEnpassantSquare = 0LL;
        if(attacks & board.m_bbEnPassantSquare)
        {
            Move* moves = board.getLegalMoves();
            uint8_t numMoves = board.getNumLegalMoves();

            for(uint8_t i = 0; i < numMoves; i++)
            {
                if(moves[i].moveInfo & MoveInfoBit::ENPASSANT)
                {
                    bbEnpassantSquare = board.m_bbEnPassantSquare;
                    break;
                }
            }
        }

        // Attacks and enpassant squares
        destinations |= (attacks & (board.getColoredPieces(Color(!board.getTurn())) | bbEnpassantSquare));

        if(RANK(move.from) == promotionRank)
        {
            // Lookup for promoted piece to index
            // Note pawn promotion is not possible thus -1
            constexpr static int8_t PromoteIndex[5] = {-1, 2, 0, 1, 3};

            // Note: The destination count is multiplied by 4 to account for all promotion types
            uint8_t numBits = m_getMinRepBits(4 * CNTSBITS(destinations) - 1);
            bitboard_t bbTo = 1LL << move.to;
            uint8_t moveIndex = CNTSBITS(destinations & (bbTo - 1));

            // Find promoted piece
            int8_t promoteIndex = PromoteIndex[move.promotedPiece()];

            // Find the 'to' square
            m_writeNbits(4 * moveIndex + promoteIndex, numBits);
        }
        else
        {
            uint8_t numBits = m_getMinRepBits(CNTSBITS(destinations) - 1);
            bitboard_t bbTo = 1LL << move.to;
            uint8_t moveIndex = CNTSBITS(destinations & (bbTo - 1));
            m_writeNbits(moveIndex, numBits);
        }
    }
    else if(move.movedPiece() == W_KING)
    {
        bitboard_t moves = getKingMoves(move.from) & ~board.getColoredPieces(board.getTurn());

        uint8_t castleRights = board.getTurn() == WHITE
        ? (board.getCastleRights() & (CastleRights::WHITE_KING_SIDE | CastleRights::WHITE_QUEEN_SIDE))
        : (board.getCastleRights() & (CastleRights::BLACK_KING_SIDE | CastleRights::BLACK_QUEEN_SIDE));

        uint8_t numCastleRights = CNTSBITS(castleRights);
        uint8_t numMoves = CNTSBITS(moves);
        uint8_t numBits = m_getMinRepBits(numCastleRights + numMoves - 1);

        if(move.isCastle())
        {
            // If queen side castle is available, but king side castle is performed
            uint8_t castleIndex = numMoves;
            if((castleRights & (CastleRights::WHITE_QUEEN_SIDE | CastleRights::BLACK_QUEEN_SIDE)) && (FILE(move.to) == FILE(Square::G1)))
            {
                castleIndex++;
            }
            m_writeNbits(castleIndex, numBits);
        }
        else
        {
            bitboard_t bbTo = 1LL << move.to;
            uint8_t moveIndex = CNTSBITS(moves & (bbTo - 1));
            m_writeNbits(moveIndex, numBits);
        }
    }
    else
    {
        bitboard_t moves;
        switch (move.movedPiece())
        {
        case W_ROOK:
            moves = getRookMoves(board.m_bbAllPieces, move.from);
            break;
        case W_KNIGHT:
            moves = getKnightMoves(move.from);
            break;
        case W_BISHOP:
            moves = getBishopMoves(board.m_bbAllPieces, move.from);
            break;
        case W_QUEEN:
            moves = getQueenMoves(board.m_bbAllPieces, move.from);
            break;
        default:
            ERROR("Unhandled type in move parsing")
            moves = 0LL;
        }

        moves &= ~board.getColoredPieces(board.getTurn());
        uint8_t numBits = m_getMinRepBits(CNTSBITS(moves) - 1);
        bitboard_t bbTo = 1LL << move.to;
        uint8_t moveIndex = CNTSBITS(moves & (bbTo - 1));
        m_writeNbits(moveIndex, numBits);
    }
}

void BinpackEncoder::m_writeVEncodedScore(eval_t prevScore, eval_t currentScore)
{
    constexpr uint32_t BlockSize = 4;
    constexpr uint16_t BlockMask = (1 << BlockSize) - 1;

    int16_t sDeltaScore = prevScore + currentScore; // current - (-previous)
    uint16_t uDeltaScore = m_signedToUnsigned(sDeltaScore);
    while (true)
    {
        uint8_t block = uDeltaScore & BlockMask;
        uDeltaScore = uDeltaScore >> BlockSize;

        // If there are more bits in uDeltaScore, set the 5th bit to 1
        // If there are no more bits, set the bit to 0 and stop writing
        if(uDeltaScore)
        {
            m_writeNbits((1 << BlockSize) | block, BlockSize + 1);
        }
        else
        {
            m_writeNbits(block, BlockSize + 1);
            break;
        }
    }
}