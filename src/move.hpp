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

    #define MOVE_INFO_MOVE_MASK     (Arcanum::PAWN_MOVE | Arcanum::ROOK_MOVE | Arcanum::KNIGHT_MOVE | Arcanum::BISHOP_MOVE | Arcanum::QUEEN_MOVE | Arcanum::KING_MOVE)
    #define MOVE_INFO_CASTLE_MASK   (Arcanum::CASTLE_WHITE_QUEEN | Arcanum::CASTLE_WHITE_KING | Arcanum::CASTLE_BLACK_QUEEN | Arcanum::CASTLE_BLACK_KING)
    #define MOVE_INFO_PROMOTE_MASK  (Arcanum::PROMOTE_ROOK | Arcanum::PROMOTE_KNIGHT | Arcanum::PROMOTE_BISHOP | Arcanum::PROMOTE_QUEEN)
    #define MOVE_INFO_CAPTURE_MASK  (Arcanum::CAPTURE_PAWN | Arcanum::CAPTURE_ROOK | Arcanum::CAPTURE_KNIGHT | Arcanum::CAPTURE_BISHOP | Arcanum::CAPTURE_QUEEN)

    #define MOVED_PIECE(_moveInfo)    ((_moveInfo) & MOVE_INFO_MOVE_MASK)
    #define CASTLE_SIDE(_moveInfo)    ((_moveInfo) & MOVE_INFO_CASTLE_MASK)
    #define PROMOTED_PIECE(_moveInfo) ((_moveInfo) & MOVE_INFO_PROMOTE_MASK)
    #define CAPTURED_PIECE(_moveInfo) ((_moveInfo) & MOVE_INFO_CAPTURE_MASK)

    typedef struct Move
    {
        square_t from;
        square_t to;
        uint32_t moveInfo;

        Move(){};
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

    #define NULL_MOVE Move(0,0)
}