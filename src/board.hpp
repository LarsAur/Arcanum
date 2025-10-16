#pragma once

#include <move.hpp>
#include <bitboard.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace Arcanum
{
    constexpr uint8_t MAX_MOVE_COUNT = 218;

    typedef enum CastleRights : uint8_t
    {
        WHITE_QUEEN_SIDE = 1,
        WHITE_KING_SIDE = 2,
        BLACK_QUEEN_SIDE = 4,
        BLACK_KING_SIDE = 8,
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
            bitboard_t m_bbOpponentAttacks;
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

            friend class Zobrist;
            friend class Evaluator;
            friend class FEN;
            friend class BinpackParser;
            friend class BinpackEncoder;

            // Tests if the king will be checked before adding the move
            bool m_attemptAddPseudoLegalEnpassant(Move move);
            bool m_attemptAddPseudoLegalMove(Move move);
            bool m_isLegalEnpassant(Move move) const;
            bool m_isLegalMove(Move move) const;
            bitboard_t m_getLeastValuablePiece(const bitboard_t mask, const Color color, Piece& piece) const;
            void m_findPinnedPieces();

            template <MoveInfoBit MoveType, bool CapturesOnly>
            void m_generateMoves();

            template <bool CapturesOnly>
            void m_generatePawnMoves();

            template <MoveInfoBit MoveType>
            bool m_hasMove();

        public:
            Board();
            Board(const Board& board);
            explicit Board(const std::string fen, bool strict = true);
            Board& operator=(const Board& other) = default;
            void performMove(const Move move);
            void generateCaptureInfo();
            Move generateMoveWithInfo(square_t from, square_t to, uint32_t promoteInfo);
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
            Color getColorAt(square_t square) const;
            square_t getEnpassantSquare() const;
            square_t getEnpassantTarget() const;
            Move* getLegalMovesFromCheck();
            Move* getLegalMoves();
            Move* getLegalCaptureMoves();
            uint8_t numOfficers(Color turn) const;
            bool hasOfficers(Color turn) const;
            uint8_t getNumLegalMoves() const;
            uint8_t getNumPieces() const;
            uint8_t getNumColoredPieces(Color color) const;
            Move getMoveFromArithmetic(std::string& arithmetic);
            bitboard_t attackersTo(square_t square, bitboard_t occupancy) const;
            bool see(const Move& move, eval_t threshold = 0) const;
            std::string fen() const;
            bool isMaterialDraw() const;
            bool hasEasyCapture(Color turn) const;
    };
}