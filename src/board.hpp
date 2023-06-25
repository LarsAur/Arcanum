#pragma once

#include <chessutils.hpp>
#include <string>
#include <unordered_map>
#include <vector>
    
namespace ChessEngine2
{
    typedef uint64_t hash_t;
    typedef int16_t eval_t;

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
    #define MOVE_INFO_MOVE_MASK (MOVE_INFO_PAWN_MOVE | MOVE_INFO_ROOK_MOVE | MOVE_INFO_KNIGHT_MOVE | MOVE_INFO_BISHOP_MOVE | MOVE_INFO_QUEEN_MOVE | MOVE_INFO_KING_MOVE)
    #define MOVE_INFO_DOUBLE_MOVE 64
    #define MOVE_INFO_ENPASSANT 128
    #define MOVE_INFO_CASTLE_WHITE_QUEEN 256
    #define MOVE_INFO_CASTLE_WHITE_KING 512
    #define MOVE_INFO_CASTLE_BLACK_QUEEN 1024
    #define MOVE_INFO_CASTLE_BLACK_KING 2048
    #define MOVE_INFO_CASTLE_MASK (MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_CASTLE_BLACK_QUEEN | MOVE_INFO_CASTLE_BLACK_KING)
    #define MOVE_INFO_PROMOTE_ROOK 4096
    #define MOVE_INFO_PROMOTE_KNIGHT 8192
    #define MOVE_INFO_PROMOTE_BISHOP 16384
    #define MOVE_INFO_PROMOTE_QUEEN 32768
    #define MOVE_INFO_PROMOTE_MASK (MOVE_INFO_PROMOTE_ROOK | MOVE_INFO_PROMOTE_KNIGHT | MOVE_INFO_PROMOTE_BISHOP | MOVE_INFO_PROMOTE_QUEEN)
    // These are not added before the move is made
    #define MOVE_INFO_CAPTURE_PAWN 65536
    #define MOVE_INFO_CAPTURE_ROOK 131072
    #define MOVE_INFO_CAPTURE_KNIGHT 262144
    #define MOVE_INFO_CAPTURE_BISHOP 524288
    #define MOVE_INFO_CAPTURE_QUEEN 1048576
    #define MOVE_INFO_CAPTURE_MASK (MOVE_INFO_CAPTURE_PAWN | MOVE_INFO_CAPTURE_ROOK | MOVE_INFO_CAPTURE_KNIGHT | MOVE_INFO_CAPTURE_QUEEN | MOVE_INFO_CAPTURE_BISHOP)

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

        bool operator==(const Move& move)
        {
            return (from == move.from) && (to == move.to);
        }
    } Move;


    class Board
    {
        private:
            Color m_turn;
            uint16_t m_halfMoves;
            uint16_t m_fullMoves;
            uint8_t m_castleRights;
            // set to 64 for invalid enpassant
            uint8_t m_enPassantSquare; // Square moved to when capturing
            uint8_t m_enPassantTarget; // Square of the captured piece
            bitboard_t m_bbEnPassantSquare; // Square moved to when capturing
            bitboard_t m_bbEnPassantTarget; // Square moved to when capturing

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

            // Tests if the king will be checked before adding the move
            bool attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights, bool wasChecked);
        public:
            Board(const Board& board);
            Board(std::string fen);
            ~Board();
            void performMove(Move move);
            void addBoardToHistory();
            hash_t getHash();

            bool isChecked(Color color);
            bool isSlidingChecked(Color color);
            bool isDiagonalChecked(Color color);
            bool isStraightChecked(Color color);

            eval_t evaluate();

            Color getTurn();
            bitboard_t getOponentAttacks();
            Move* getLegalMoves();
            Move* getLegalCaptureAndCheckMoves();
            uint8_t getNumLegalMoves();
            std::string getBoardString();

            static std::unordered_map<hash_t, uint8_t>* getBoardHistory();
    };

    class Zobrist
    {
        private:
            hash_t m_tables[6][2][64];
            hash_t m_enPassantTable[48]; // Only 16 is actually used
            hash_t m_blackToMove;

            void m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor);
        public:
            Zobrist();
            ~Zobrist();

            void getHashs(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            void getUpdatedHashs(const Board &board, Move move, uint8_t oldEnPassentSquare, uint8_t newEnPassentSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
    };
}