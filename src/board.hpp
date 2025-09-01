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
            Board(const std::string fen, bool strict = true);
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

    template <MoveInfoBit MoveType, bool CapturesOnly>
    __attribute__((always_inline))
    inline void Board::m_generateMoves()
    {
        static_assert(MoveType != MoveInfoBit::PAWN_MOVE);
        static_assert(MoveType != MoveInfoBit::KING_MOVE);

        Piece type;
        switch (MoveType)
        {
            case MoveInfoBit::ROOK_MOVE:   type = W_ROOK;   break;
            case MoveInfoBit::KNIGHT_MOVE: type = W_KNIGHT; break;
            case MoveInfoBit::BISHOP_MOVE: type = W_BISHOP; break;
            case MoveInfoBit::QUEEN_MOVE:  type = W_QUEEN;  break;
        }

        bitboard_t pieces = m_bbTypedPieces[type][m_turn];
        while(pieces)
        {
            square_t pieceIdx = popLS1B(&pieces);
            bitboard_t targets;
            switch (MoveType)
            {
                case MoveInfoBit::ROOK_MOVE:    targets = getRookMoves(m_bbAllPieces, pieceIdx);   break;
                case MoveInfoBit::KNIGHT_MOVE:  targets = getKnightMoves(pieceIdx);                break;
                case MoveInfoBit::BISHOP_MOVE:  targets = getBishopMoves(m_bbAllPieces, pieceIdx); break;
                case MoveInfoBit::QUEEN_MOVE:   targets = getQueenMoves(m_bbAllPieces, pieceIdx);  break;
            }

            // Filter the allowed target squares
            if constexpr (CapturesOnly)
            {
                targets &= m_bbColoredPieces[m_turn^1]; // All opponent pieces
            }
            else
            {
                targets &= ~m_bbColoredPieces[m_turn];  // All squares except own pieces
            }

            // Check if the piece is a blocker
            // Note: In theory, the blockers and non-blockers could be separated into
            // two loops, by using m_blockers[m_turn] as a mask. For some reason,
            // creating two loops seems to be a bit slower, so we continue to check if each piece is a blocker
            if((1LL << pieceIdx) & m_blockers[m_turn])
            {
                square_t pinnerIdx = m_pinnerBlockerIdxPairs[pieceIdx];
                targets &= getBetweens(pinnerIdx, m_kingIdx) | (1LL << pinnerIdx);
            }

            while(targets)
            {
                square_t target = popLS1B(&targets);
                m_legalMoves[m_numLegalMoves].from     = pieceIdx;
                m_legalMoves[m_numLegalMoves].to       = target;
                m_legalMoves[m_numLegalMoves].moveInfo = MoveType;
                m_numLegalMoves++;
            }
        }
    }

    template <bool CapturesOnly>
    __attribute__((always_inline))
    inline void Board::m_generatePawnMoves()
    {
        constexpr bitboard_t PromotionSquares = 0xff000000000000ffLL;

        Color opponent = Color(m_turn^1);

        bitboard_t pawns = m_bbTypedPieces[W_PAWN][m_turn];
        bitboard_t bbAttacks, bbOrigins;

        // Left attacks without promotion
        bbAttacks = getPawnAttacksLeft(pawns, m_turn) & m_bbColoredPieces[opponent] & ~PromotionSquares;
        bbOrigins = getPawnAttacksRight(bbAttacks, opponent);
        while (bbAttacks)
        {
            square_t target = popLS1B(&bbAttacks);
            square_t pawnIdx = popLS1B(&bbOrigins);
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE));
        }

        // Left attacks with promotion
        bbAttacks = getPawnAttacksLeft(pawns, m_turn) & m_bbColoredPieces[opponent] & PromotionSquares;
        bbOrigins = getPawnAttacksRight(bbAttacks, opponent);
        while (bbAttacks)
        {
            square_t target = popLS1B(&bbAttacks);
            square_t pawnIdx = popLS1B(&bbOrigins);
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN));
            if(!CapturesOnly && added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }

        // Right attacks without promotion
        bbAttacks = getPawnAttacksRight(pawns, m_turn) & m_bbColoredPieces[opponent] & ~PromotionSquares;
        bbOrigins = getPawnAttacksLeft(bbAttacks, opponent);
        while (bbAttacks)
        {
            square_t target = popLS1B(&bbAttacks);
            square_t pawnIdx = popLS1B(&bbOrigins);
            m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE));
        }

        // Right attacks with promotions
        bbAttacks = getPawnAttacksRight(pawns, m_turn) & m_bbColoredPieces[opponent] & PromotionSquares;
        bbOrigins = getPawnAttacksLeft(bbAttacks, opponent);
        while (bbAttacks)
        {
            square_t target = popLS1B(&bbAttacks);
            square_t pawnIdx = popLS1B(&bbOrigins);
            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN));
            if(!CapturesOnly && added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }

        // Enpassant
        if(m_bbEnPassantSquare)
        {
            bitboard_t enpassantAttackers = getPawnAttacks(m_bbEnPassantSquare, opponent) & pawns;
            while (enpassantAttackers)
            {
                square_t pawnIdx = popLS1B(&enpassantAttackers);
                m_attemptAddPseudoLegalEnpassant(Move(pawnIdx, m_enPassantSquare, MoveInfoBit::CAPTURE_PAWN | MoveInfoBit::ENPASSANT | MoveInfoBit::PAWN_MOVE));
            }
        }

        // Forward moves with promotion
        bitboard_t pawnMoves = getPawnMoves(pawns, m_turn) & ~m_bbAllPieces & PromotionSquares;
        bitboard_t pawnMovesOrigin = getPawnMoves(pawnMoves, opponent);
        while(pawnMoves)
        {
            square_t target = popLS1B(&pawnMoves);
            square_t pawnIdx = popLS1B(&pawnMovesOrigin);

            // If one promotion move is legal, all are legal
            bool added = m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_QUEEN));
            if(!CapturesOnly && added)
            {
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_ROOK);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_BISHOP);
                m_legalMoves[m_numLegalMoves++] = Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE | MoveInfoBit::PROMOTE_KNIGHT);
            }
        }

        if constexpr(!CapturesOnly)
        {
            // Forward moves without promotion
            pawnMoves = getPawnMoves(pawns, m_turn) & ~m_bbAllPieces & ~PromotionSquares;
            pawnMovesOrigin = getPawnMoves(pawnMoves, opponent);
            while(pawnMoves)
            {
                square_t target = popLS1B(&pawnMoves);
                square_t pawnIdx = popLS1B(&pawnMovesOrigin);
                m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::PAWN_MOVE));
            }


            // Double move
            bitboard_t doubleMoves       = getPawnDoubleMoves(pawns, m_turn, m_bbAllPieces);
            bitboard_t doubleMovesOrigin = getPawnDoubleBackwardsMoves(doubleMoves, m_turn);
            while (doubleMoves)
            {
                int target = popLS1B(&doubleMoves);
                int pawnIdx = popLS1B(&doubleMovesOrigin);
                m_attemptAddPseudoLegalMove(Move(pawnIdx, target, MoveInfoBit::DOUBLE_MOVE | MoveInfoBit::PAWN_MOVE));
            }
        }
    }

    template <MoveInfoBit MoveType>
    inline bool Board::m_hasMove()
    {
        static_assert(MoveType != MoveInfoBit::PAWN_MOVE);
        static_assert(MoveType != MoveInfoBit::KING_MOVE);

        Piece type;
        switch (MoveType)
        {
            case MoveInfoBit::ROOK_MOVE:   type = W_ROOK;   break;
            case MoveInfoBit::KNIGHT_MOVE: type = W_KNIGHT; break;
            case MoveInfoBit::BISHOP_MOVE: type = W_BISHOP; break;
            case MoveInfoBit::QUEEN_MOVE:  type = W_QUEEN;  break;
        }

        bitboard_t pieces = m_bbTypedPieces[type][m_turn];

        while (pieces)
        {
            square_t pieceIdx = popLS1B(&pieces);
            bitboard_t targets;
            switch (MoveType)
            {
                case MoveInfoBit::ROOK_MOVE:   targets = getRookMoves(m_bbAllPieces, pieceIdx);   break;
                case MoveInfoBit::KNIGHT_MOVE: targets = getKnightMoves(pieceIdx);                break;
                case MoveInfoBit::BISHOP_MOVE: targets = getBishopMoves(m_bbAllPieces, pieceIdx); break;
                case MoveInfoBit::QUEEN_MOVE:  targets = getQueenMoves(m_bbAllPieces, pieceIdx);  break;
            }

            // Filter the allowed target squares
            targets &= ~m_bbColoredPieces[m_turn];  // All squares except own pieces

            // Check if the piece is a blocker
            // Note: In theory, the blockers and non-blockers could be separated into
            // two loops, by using m_blockers[m_turn] as a mask. For some reason,
            // creating two loops seems to be a bit slower, so we continue to check if each piece is a blocker
            if((1LL << pieceIdx) & m_blockers[m_turn])
            {
                square_t pinnerIdx = m_pinnerBlockerIdxPairs[pieceIdx];
                targets &= getBetweens(pinnerIdx, m_kingIdx) | (1LL << pinnerIdx);
            }

            return targets;
        }

        return false;
    }

}