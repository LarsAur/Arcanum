#pragma once

#include <chessutils.hpp>
#include <string>
#include <vector>

namespace ChessEngine2
{
    static std::string startFEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    typedef enum Color
    {
        WHITE,
        BLACK,
        NUM_COLORS,
    } Color;

    typedef enum PieceType
    {
        PAWN,
        ROOK,
        KNIGHT,
        BISHOP,
        KING,
        QUEEN,
        NUM_PIECES,
    } PieceType;

    typedef enum CastleRights
    {
        WHITE_KING_SIDE = 1,
        WHITE_QUEEN_SIDE = 2,
        BLACK_KING_SIDE = 4,
        BLACK_QUEEN_SIDE = 8,
    } CastleRights;

    #define MOVE_INFO_PAWN_MOVE 1
    #define MOVE_INFO_DOUBLE_MOVE 2
    #define MOVE_INFO_ENPASSANT 4
    #define MOVE_INFO_CASTLE_WHITE_QUEEN 8
    #define MOVE_INFO_CASTLE_WHITE_KING 16
    #define MOVE_INFO_CASTLE_BLACK_QUEEN 32
    #define MOVE_INFO_CASTLE_BLACK_KING 64
    // TODO Promo and king, rook moves
    #define MOVE_INFO_KING 128
    #define MOVE_INFO_ROOK 256
    #define MOVE_INFO_PROMOTE_ROOK 512
    #define MOVE_INFO_PROMOTE_BISHOP 1024
    #define MOVE_INFO_PROMOTE_KNIGHT 2048
    #define MOVE_INFO_PROMOTE_QUEEN 4096

    typedef struct Move
    {
        uint8_t from;
        uint8_t to;
        uint16_t moveInfo;

        Move(uint8_t _from, uint8_t _to, uint16_t _moveInfo = 0)
        {
            from = _from;
            to = _to;
            moveInfo = _moveInfo;
        }
    } Move;

    class Board
    {
        private:
            Color m_turn;
            uint16_t m_halfMoves;
            uint16_t m_fullMoves;
            uint8_t m_castleRights;
            // set to >63 for invalid enpassant
            uint8_t m_enPassantSquare; // Square moved to when capturing
            uint8_t m_enPassantTarget; // Square of the captured piece

            bitboard_t m_bbAllPieces;
            bitboard_t m_bbPieces[NUM_COLORS];
            bitboard_t m_bbPawns[NUM_COLORS];
            bitboard_t m_bbKing[NUM_COLORS];
            bitboard_t m_bbKnights[NUM_COLORS];
            bitboard_t m_bbBishops[NUM_COLORS];
            bitboard_t m_bbQueens[NUM_COLORS];
            bitboard_t m_bbRooks[NUM_COLORS];

            std::vector<Move> m_legalMoves;
            // Tests if the king will be checked before adding the move
        void attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights, bool wasChecked);
            // void attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights);
            void attemptAddPseudoLegalKingMove(Move move);
        public:
            Board(const Board& board);
            Board(std::string fen);
            ~Board();
            void performMove(Move move);

            bool isChecked(Color color);
            bool isSlidingChecked(Color color);
            bool isDiagonalChecked(Color color);
            bool isStraightChecked(Color color);

            bitboard_t getOponentAttacks();
            std::vector<Move>* getLegalMoves();
            std::string getBoardString();
            
    };
}