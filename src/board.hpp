#pragma once

#include <chessutils.hpp>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Arcanum
{
    typedef uint64_t hash_t;
    static const std::string startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    typedef enum Color
    {
        WHITE,
        BLACK,
        NUM_COLORS,
    } Color;

    typedef enum Piece : uint8_t
    {
        W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING,
        B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING,
        NO_PIECE,
    } Piece;

    typedef enum CastleRights
    {
        WHITE_KING_SIDE = 1,
        WHITE_QUEEN_SIDE = 2,
        BLACK_KING_SIDE = 4,
        BLACK_QUEEN_SIDE = 8,
    } CastleRights;

    #define MOVE_INFO_PAWN_MOVE 1
    #define MOVE_INFO_ROOK_MOVE 2
    #define MOVE_INFO_KNIGHT_MOVE 4
    #define MOVE_INFO_BISHOP_MOVE 8
    #define MOVE_INFO_QUEEN_MOVE 16
    #define MOVE_INFO_KING_MOVE 32
    #define MOVE_INFO_DOUBLE_MOVE 64
    #define MOVE_INFO_ENPASSANT 128
    #define MOVE_INFO_CASTLE_WHITE_QUEEN 256
    #define MOVE_INFO_CASTLE_WHITE_KING 512
    #define MOVE_INFO_CASTLE_BLACK_QUEEN 1024
    #define MOVE_INFO_CASTLE_BLACK_KING 2048
    #define MOVE_INFO_PROMOTE_ROOK 4096
    #define MOVE_INFO_PROMOTE_KNIGHT 8192
    #define MOVE_INFO_PROMOTE_BISHOP 16384
    #define MOVE_INFO_PROMOTE_QUEEN 32768
    #define MOVE_INFO_CAPTURE_PAWN 65536
    #define MOVE_INFO_CAPTURE_ROOK 131072
    #define MOVE_INFO_CAPTURE_KNIGHT 262144
    #define MOVE_INFO_CAPTURE_BISHOP 524288
    #define MOVE_INFO_CAPTURE_QUEEN 1048576
    #define MOVE_INFO_MOVE_MASK (MOVE_INFO_PAWN_MOVE | MOVE_INFO_ROOK_MOVE | MOVE_INFO_KNIGHT_MOVE | MOVE_INFO_BISHOP_MOVE | MOVE_INFO_QUEEN_MOVE | MOVE_INFO_KING_MOVE)
    #define MOVE_INFO_CASTLE_MASK (MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_CASTLE_BLACK_KING)
    #define MOVE_INFO_PROMOTE_MASK (MOVE_INFO_PROMOTE_ROOK | MOVE_INFO_PROMOTE_KNIGHT | MOVE_INFO_PROMOTE_BISHOP | MOVE_INFO_PROMOTE_QUEEN)
    #define MOVE_INFO_CAPTURE_MASK (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_CAPTURE_ROOK | MOVE_INFO_CAPTURE_KNIGHT | MOVE_INFO_CAPTURE_BISHOP | MOVE_INFO_CAPTURE_QUEEN)

    typedef struct Move
    {
        uint8_t from;
        uint8_t to;
        uint32_t moveInfo;

        Move(){}

        Move(uint8_t _from, uint8_t _to, uint32_t _moveInfo = 0)
        {
            from = _from;
            to = _to;
            moveInfo = _moveInfo;
        }

        bool operator==(const Move& move) const
        {
            return (from == move.from) && (to == move.to) && ((moveInfo & MOVE_INFO_PROMOTE_MASK) == (move.moveInfo & MOVE_INFO_PROMOTE_MASK));
        }

        std::string toString() const
        {
            std::stringstream ss;
            ss << getArithmeticNotation(from);
            ss << getArithmeticNotation(to);

            if(moveInfo & MOVE_INFO_PROMOTE_QUEEN)       ss << "q";
            else if(moveInfo & MOVE_INFO_PROMOTE_ROOK)   ss << "r";
            else if(moveInfo & MOVE_INFO_PROMOTE_BISHOP) ss << "b";
            else if(moveInfo & MOVE_INFO_PROMOTE_KNIGHT) ss << "n";
            return ss.str();
        }

        friend inline std::ostream& operator<<(std::ostream& os, const Move& move)
        {
            os << getArithmeticNotation(move.from);
            os << getArithmeticNotation(move.to);

            if(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)       os << "q";
            else if(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)   os << "r";
            else if(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP) os << "b";
            else if(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT) os << "n";

            return os;
        }

    } Move;

    class HashFunction
    {
        public:
        size_t operator()(const hash_t& hash) const {
            return hash;
        }
    };

    class Board
    {
        private:
            Color m_turn;
            uint16_t m_fullMoves;
            uint8_t m_rule50;
            uint8_t m_castleRights;
            // set to 64 for invalid enpassant
            uint8_t m_enPassantSquare; // Square moved to when capturing
            uint8_t m_enPassantTarget; // Square of the captured piece
            bitboard_t m_bbEnPassantSquare; // Square moved to when capturing
            bitboard_t m_bbEnPassantTarget; // Square of the captured piece

            bitboard_t m_bbAllPieces;
            bitboard_t m_bbColoredPieces[NUM_COLORS];
            bitboard_t m_bbTypedPieces[6][NUM_COLORS];

            Piece m_pieces[64];
            uint8_t m_numLegalMoves;
            Move m_legalMoves[218];

            hash_t m_hash;
            hash_t m_materialHash;
            hash_t m_pawnHash;

            friend class Zobrist;
            friend class Evaluator;

            // Tests if the king will be checked before adding the move
            bool m_attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights);
            bitboard_t m_getLeastValuablePiece(const bitboard_t mask, const Color color, Piece& piece) const;
        public:
            Board(const Board& board);
            Board(const std::string fen);
            void performMove(const Move move);
            void addBoardToHistory();
            void generateCaptureInfo();
            void performNullMove();
            hash_t getHash() const;
            hash_t getPawnHash() const;
            hash_t getMaterialHash() const;
            uint16_t getFullMoves() const;
            uint16_t getHalfMoves() const;
            bool isChecked(Color color);
            bool isSlidingChecked(Color color);
            bool isDiagonalChecked(Color color);
            bool isStraightChecked(Color color);
            bool hasLegalMove();
            bool hasLegalMoveFromCheck();
            Color getTurn() const;
            bitboard_t getOpponentAttacks();
            bitboard_t getOpponentPawnAttacks();
            bitboard_t getTypedPieces(Piece type, Color color) const;
            Move* getLegalMovesFromCheck();
            Move* getLegalMoves();
            Move* getLegalCaptureMoves();
            Move* getLegalCaptureAndCheckMoves();
            bool hasOfficers(Color turn) const;
            uint8_t getNumLegalMoves() const;
            uint8_t getNumPiecesLeft() const;
            uint8_t getNumColoredPieces(Color color) const;
            std::string getBoardString() const;
            std::string getFEN() const;
            bitboard_t attackersTo(uint8_t square) const;
            bool see(const Move& move) const;
            static std::unordered_map<hash_t, uint8_t, HashFunction>* getBoardHistory();
    };

    class Zobrist
    {
        private:
            hash_t m_tables[6][2][64];
            hash_t m_enPassantTable[65]; // Only 16 is actually used, index 64 is used to not read out of bounds
            hash_t m_blackToMove;

            void m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor);
        public:
            Zobrist();
            ~Zobrist();

            void getHashs(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            void getUpdatedHashs(const Board &board, Move move, uint8_t oldEnPassentSquare, uint8_t newEnPassentSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
    };
}