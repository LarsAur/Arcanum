#pragma once

#include <bitboard.hpp>

namespace Arcanum
{
    enum MoveInfoBit
    {
        PAWN_MOVE           = 1,
        ROOK_MOVE           = 2,
        KNIGHT_MOVE         = 4,
        BISHOP_MOVE         = 8,
        QUEEN_MOVE          = 16,
        KING_MOVE           = 32,
        DOUBLE_MOVE         = 64,
        ENPASSANT           = 128,
        CASTLE_WHITE_QUEEN  = 256,
        CASTLE_WHITE_KING   = 512,
        CASTLE_BLACK_QUEEN  = 1024,
        CASTLE_BLACK_KING   = 2048,
        PROMOTE_ROOK        = 4096,
        PROMOTE_KNIGHT      = 8192,
        PROMOTE_BISHOP      = 16384,
        PROMOTE_QUEEN       = 32768,
        CAPTURE_PAWN        = 65536,
        CAPTURE_ROOK        = 131072,
        CAPTURE_KNIGHT      = 262144,
        CAPTURE_BISHOP      = 524288,
        CAPTURE_QUEEN       = 1048576,
    };

    enum CastleIndex
    {
        CASTLE_WHITE_QUEEN_INDEX = 0,
        CASTLE_WHITE_KING_INDEX  = 1,
        CASTLE_BLACK_QUEEN_INDEX = 2,
        CASTLE_BLACK_KING_INDEX  = 3
    };

    typedef enum Piece : uint8_t
    {
        W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING,
        B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING,
        NO_PIECE,
    } Piece;

    #define MOVE_INFO_MOVE_MASK     (MoveInfoBit::PAWN_MOVE | MoveInfoBit::ROOK_MOVE | MoveInfoBit::KNIGHT_MOVE | MoveInfoBit::BISHOP_MOVE | MoveInfoBit::QUEEN_MOVE | MoveInfoBit::KING_MOVE)
    #define MOVE_INFO_CASTLE_MASK   (MoveInfoBit::CASTLE_WHITE_QUEEN | MoveInfoBit::CASTLE_WHITE_KING | MoveInfoBit::CASTLE_BLACK_QUEEN | MoveInfoBit::CASTLE_BLACK_KING)
    #define MOVE_INFO_PROMOTE_MASK  (MoveInfoBit::PROMOTE_ROOK | MoveInfoBit::PROMOTE_KNIGHT | MoveInfoBit::PROMOTE_BISHOP | MoveInfoBit::PROMOTE_QUEEN)
    #define MOVE_INFO_CAPTURE_MASK  (MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::CAPTURE_ROOK | MoveInfoBit::CAPTURE_KNIGHT | MoveInfoBit::CAPTURE_BISHOP | MoveInfoBit::CAPTURE_QUEEN)

    #define MOVED_PIECE(_moveInfo)    ((_moveInfo) & MOVE_INFO_MOVE_MASK)
    #define CASTLE_SIDE(_moveInfo)    ((_moveInfo) & MOVE_INFO_CASTLE_MASK)
    #define PROMOTED_PIECE(_moveInfo) ((_moveInfo) & MOVE_INFO_PROMOTE_MASK)
    #define CAPTURED_PIECE(_moveInfo) ((_moveInfo) & MOVE_INFO_CAPTURE_MASK)

    enum Square : square_t
    {
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
        NONE,
    };

    typedef struct Move
    {
        square_t from;
        square_t to;
        uint32_t moveInfo;

        Move() = default;
        Move(square_t _from, square_t _to, uint32_t _moveInfo = 0)
        {
            from = _from;
            to = _to;
            moveInfo = _moveInfo;
        }

        bool operator==(const Move& move) const
        {
            return (from == move.from) && (to == move.to) && (PROMOTED_PIECE(moveInfo) == PROMOTED_PIECE(move.moveInfo));
        }

        bool operator!=(const Move& move) const
        {
            return (from != move.from) || (to != move.to) || (PROMOTED_PIECE(moveInfo) != PROMOTED_PIECE(move.moveInfo));
        }

        inline bool isNull() const
        {
            return from == 0 && to == 0;
        }

        inline bool isPromotion() const
        {
            return moveInfo & MOVE_INFO_PROMOTE_MASK;
        }

        inline bool isUnderPromotion() const
        {
            return moveInfo & (MOVE_INFO_PROMOTE_MASK ^ MoveInfoBit::PROMOTE_QUEEN);
        }

        inline Piece promotedPiece() const
        {
            return Piece(LS1B(moveInfo & MOVE_INFO_PROMOTE_MASK) - 11);
        }

        inline bool isCapture() const
        {
            return moveInfo & MOVE_INFO_CAPTURE_MASK;
        }

        inline Piece capturedPiece() const
        {
            return Piece(LS1B(moveInfo & MOVE_INFO_CAPTURE_MASK) - 16);
        }

        inline Piece movedPiece() const
        {
            return Piece(LS1B(moveInfo & MOVE_INFO_MOVE_MASK));
        }

        inline bool isCastle() const
        {
            return moveInfo & MOVE_INFO_CASTLE_MASK;
        }

        inline CastleIndex castleIndex() const
        {
            return CastleIndex(LS1B(moveInfo & MOVE_INFO_CASTLE_MASK) - 8);
        }

        // Move is not a capture or promotion
        inline bool isQuiet() const
        {
            return !((moveInfo) & (MOVE_INFO_CAPTURE_MASK | MOVE_INFO_PROMOTE_MASK));
        }

        inline bool isEnpassant() const
        {
            return moveInfo & ENPASSANT;
        }

        std::string toString() const
        {
            std::stringstream ss;
            ss << squareToString(from);
            ss << squareToString(to);

            if(moveInfo & PROMOTE_QUEEN)       ss << "q";
            else if(moveInfo & PROMOTE_ROOK)   ss << "r";
            else if(moveInfo & PROMOTE_BISHOP) ss << "b";
            else if(moveInfo & PROMOTE_KNIGHT) ss << "n";
            return ss.str();
        }

        friend inline std::ostream& operator<<(std::ostream& os, const Move& move)
        {
            os << squareToString(move.from);
            os << squareToString(move.to);

            if(move.moveInfo & PROMOTE_QUEEN)       os << "q";
            else if(move.moveInfo & PROMOTE_ROOK)   os << "r";
            else if(move.moveInfo & PROMOTE_BISHOP) os << "b";
            else if(move.moveInfo & PROMOTE_KNIGHT) os << "n";

            return os;
        }

    } Move;

    struct PackedMove
    {
        uint16_t _info; // 2 = Promotion bits | 6 bits = from | 6 bits = to

        PackedMove() = default;
        PackedMove(uint16_t info) :
            _info(info)
        {}

        explicit PackedMove(const Move& move)
        {
            uint16_t promotionBits = LS1B((move.moveInfo >> 12) & 0xf) & 0b11;
            _info = (promotionBits << 12) | (move.from << 6) | move.to;
        }

        bool operator==(const Move& move) const
        {
            PackedMove packed(move);

            return packed._info == _info;
        }
    };

    #define PACKED_NULL_MOVE PackedMove(0)
    #define NULL_MOVE Move(0,0)
}