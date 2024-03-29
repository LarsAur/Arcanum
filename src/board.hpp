#pragma once

#include <types.hpp>
#include <chessutils.hpp>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Arcanum
{
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

        Move(){}

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

        std::string toString() const
        {
            std::stringstream ss;
            ss << getArithmeticNotation(from);
            ss << getArithmeticNotation(to);

            if(moveInfo & PROMOTE_QUEEN)       ss << "q";
            else if(moveInfo & PROMOTE_ROOK)   ss << "r";
            else if(moveInfo & PROMOTE_BISHOP) ss << "b";
            else if(moveInfo & PROMOTE_KNIGHT) ss << "n";
            return ss.str();
        }

        friend inline std::ostream& operator<<(std::ostream& os, const Move& move)
        {
            os << getArithmeticNotation(move.from);
            os << getArithmeticNotation(move.to);

            if(move.moveInfo & PROMOTE_QUEEN)       os << "q";
            else if(move.moveInfo & PROMOTE_ROOK)   os << "r";
            else if(move.moveInfo & PROMOTE_BISHOP) os << "b";
            else if(move.moveInfo & PROMOTE_KNIGHT) os << "n";

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
            uint8_t m_numNonReversableMovesPerformed; // Number of non-reversable moves performed (captures and pawn moves, castle not included)
            // set to 64 for invalid enpassant
            square_t m_enPassantSquare; // Square moved to when capturing
            square_t m_enPassantTarget; // Square of the captured piece
            bitboard_t m_bbEnPassantSquare; // Square moved to when capturing
            bitboard_t m_bbEnPassantTarget; // Square of the captured piece

            bitboard_t m_bbAllPieces;
            bitboard_t m_bbColoredPieces[NUM_COLORS];
            bitboard_t m_bbTypedPieces[6][NUM_COLORS];
            bitboard_t m_blockers[NUM_COLORS]; // Pieces blocking the king from a sliding piece
            bitboard_t m_pinners[NUM_COLORS];  // Pieces which targets the king with only one opponent piece blocking
            square_t m_pinnerBlockerIdxPairs[64]; // Array containing the idx of the pinner

            Piece m_pieces[64];
            uint8_t m_numLegalMoves = 0;
            Move m_legalMoves[218];
            hash_t m_hash;
            hash_t m_materialHash;
            hash_t m_pawnHash;

            friend class Zobrist;
            friend class Evaluator;

            // Tests if the king will be checked before adding the move
            bool m_attemptAddPseudoLegalEnpassant(Move move, square_t kingIdx);
            bool m_attemptAddPseudoLegalMove(Move move, square_t kingIdx);
            bool m_isLegalEnpassant(Move move, square_t kingIdx);
            bool m_isLegalMove(Move move, square_t kingIdx);
            bitboard_t m_getLeastValuablePiece(const bitboard_t mask, const Color color, Piece& piece) const;
            void m_findPinnedPieces();
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
            uint8_t getNumNonReversableMovesPerformed() const;
            uint8_t getCastleRights() const;
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
            bitboard_t getColoredPieces(Color color) const;
            square_t getEnpassantSquare() const;
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
            bitboard_t attackersTo(square_t square) const;
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
            void getUpdatedHashs(const Board &board, Move move, square_t oldEnPassentSquare, square_t newEnPassentSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
    };
}