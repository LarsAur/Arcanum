#pragma once

#include <move.hpp>
#include <bitboard.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace Arcanum
{
    constexpr uint8_t MAX_MOVE_COUNT = 218;

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

    class Board
    {
        private:
            Color m_turn;
            uint16_t m_fullMoves;
            uint8_t m_rule50;
            uint8_t m_castleRights;
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
            square_t m_kingIdx;

            Piece m_pieces[64];
            uint8_t m_numLegalMoves = 0;
            Move m_legalMoves[MAX_MOVE_COUNT];
            hash_t m_hash;
            hash_t m_materialHash;
            hash_t m_pawnHash;

            enum MoveSet
            {
                NOT_GENERATED,
                CAPTURES,
                CAPTURES_AND_CHECKS,
                ALL,
            };
            MoveSet m_moveset; // Which set moves are generated
            MoveSet m_captureInfoGenerated;

            // Values are assigned so that checked = true and not_checked = false
            enum CheckedCacheState
            {
                UNKNOWN=-1,
                NOT_CHECKED=0,
                CHECKED=1,
            };
            CheckedCacheState m_checkedCache;

            friend class Zobrist;
            friend class Evaluator;
            friend class FEN;

            // Tests if the king will be checked before adding the move
            bool m_attemptAddPseudoLegalEnpassant(Move move);
            bool m_attemptAddPseudoLegalMove(Move move);
            bool m_isLegalEnpassant(Move move);
            bool m_isLegalMove(Move move);
            bitboard_t m_getLeastValuablePiece(const bitboard_t mask, const Color color, Piece& piece) const;
            void m_findPinnedPieces();

            template <MoveInfoBit MoveType, bool CapturesOnly>
            void m_generateMoves();

        public:
            Board(const Board& board);
            Board(const std::string fen);
            void performMove(const Move move);
            void generateCaptureInfo();
            void performNullMove();
            hash_t getHash() const;
            hash_t getPawnHash() const;
            hash_t getMaterialHash() const;
            uint16_t getFullMoves() const;
            uint16_t getHalfMoves() const;
            uint8_t getCastleRights() const;
            bool isChecked();
            bool hasLegalMove();
            bool hasLegalMoveFromCheck();
            Color getTurn() const;
            bitboard_t getOpponentAttacks();
            bitboard_t getOpponentPawnAttacks();
            bitboard_t getTypedPieces(Piece type, Color color) const;
            bitboard_t getColoredPieces(Color color) const;
            Piece getPieceAt(square_t square) const;
            square_t getEnpassantSquare() const;
            Move* getLegalMovesFromCheck();
            Move* getLegalMoves();
            Move* getLegalCaptureMoves();
            uint8_t numOfficers(Color turn) const;
            bool hasOfficers(Color turn) const;
            uint8_t getNumLegalMoves() const;
            uint8_t getNumPieces() const;
            uint8_t getNumColoredPieces(Color color) const;
            Move getMoveFromArithmetic(std::string& arithmetic);
            bitboard_t attackersTo(square_t square) const;
            bool see(const Move& move) const;
    };

    template <MoveInfoBit MoveType, bool CapturesOnly>
    inline void Board::m_generateMoves()
    {
        Piece type;
        switch (MoveType)
        {
            case MoveInfoBit::PAWN_MOVE:   type = W_PAWN;   break;
            case MoveInfoBit::ROOK_MOVE:   type = W_ROOK;   break;
            case MoveInfoBit::KNIGHT_MOVE: type = W_KNIGHT; break;
            case MoveInfoBit::BISHOP_MOVE: type = W_BISHOP; break;
            case MoveInfoBit::QUEEN_MOVE:  type = W_QUEEN;  break;
            case MoveInfoBit::KING_MOVE:   type = W_KING;   break;
        }

        bitboard_t pieces = m_bbTypedPieces[type][m_turn];

        while (pieces)
        {
            square_t pieceIdx = popLS1B(&pieces);
            bitboard_t moves;
            switch (MoveType)
            {
                case MoveInfoBit::PAWN_MOVE:    return;
                case MoveInfoBit::ROOK_MOVE:    moves = getRookMoves(m_bbAllPieces, pieceIdx);   break;
                case MoveInfoBit::KNIGHT_MOVE:  moves = getKnightMoves(pieceIdx);                break;
                case MoveInfoBit::BISHOP_MOVE:  moves = getBishopMoves(m_bbAllPieces, pieceIdx); break;
                case MoveInfoBit::QUEEN_MOVE:   moves = getQueenMoves(m_bbAllPieces, pieceIdx);  break;
                case MoveInfoBit::KING_MOVE:    return;
            }

            // Filter the allowed target squares
            if constexpr (CapturesOnly)
                moves &= m_bbColoredPieces[m_turn^1]; // All opponent pieces
            else
                moves &= ~m_bbColoredPieces[m_turn];  // All squares except own pieces

            while(moves)
            {
                square_t target = popLS1B(&moves);
                m_attemptAddPseudoLegalMove(Move(pieceIdx, target, MoveType));
            }
        }
    }

}