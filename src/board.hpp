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
            return (from == move.from) && (to == move.to);
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
            friend class Eval;

            // Tests if the king will be checked before adding the move
            bool m_attemptAddPseudoLegalMove(Move move, uint8_t kingIdx, bitboard_t kingDiagonals, bitboard_t kingStraights, bool wasChecked);
        public:
            Board(const Board& board);
            Board(std::string fen);
            void performMove(Move move);
            void addBoardToHistory();
            hash_t getHash();
            hash_t getPawnHash();
            hash_t getMaterialHash();
            uint16_t getFullMoves();
            uint16_t getHalfMoves();
            bool isChecked(Color color);
            bool isSlidingChecked(Color color);
            bool isDiagonalChecked(Color color);
            bool isStraightChecked(Color color);
            Color getTurn();
            bitboard_t getopponentAttacks();
            bitboard_t getopponentPawnAttacks();
            Move* getLegalMoves();
            Move* getLegalCaptureMoves();
            Move* getLegalCaptureAndCheckMoves();
            uint8_t getNumLegalMoves();
            std::string getBoardString() const;
            void generateCaptureInfo();
            static std::unordered_map<hash_t, uint8_t, HashFunction>* getBoardHistory();
    };

    class Zobrist
    {
        private:
            hash_t m_tables[6][2][64];
            hash_t m_enPassantTable[64]; // Only 16 is actually used
            hash_t m_blackToMove;

            void m_addAllPieces(hash_t &hash, hash_t &materialHash, bitboard_t bitboard, uint8_t pieceType, Color pieceColor);
        public:
            Zobrist();
            ~Zobrist();

            void getHashs(const Board &board, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
            void getUpdatedHashs(const Board &board, Move move, uint8_t oldEnPassentSquare, uint8_t newEnPassentSquare, hash_t &hash, hash_t &pawnHash, hash_t &materialHash);
    };


    typedef int16_t eval_t;
    typedef struct EvalTrace
    {
        #ifdef FULL_TRACE
        bool checkmate;
        bool stalemate;
        eval_t mobility;
        eval_t material;
        eval_t pawns;
        #endif // FULL_TRACE
        eval_t total;

        EvalTrace()
        {
        #ifdef FULL_TRACE
            checkmate = false;
            stalemate = false;
            mobility = 0;
            material = 0;
            pawns = 0;
        #endif // FULL_TRACE
            
            total = 0;
        };

        EvalTrace(eval_t eval)
        {
        #ifdef FULL_TRACE
            checkmate = false;
            stalemate = false;
            mobility = 0;
            material = 0;
            pawns = 0;
        #endif // FULL_TRACE
            total = eval;
        }

        bool operator> (const EvalTrace&) const;
        bool operator>=(const EvalTrace&) const;
        bool operator==(const EvalTrace&) const;
        bool operator<=(const EvalTrace&) const;
        bool operator< (const EvalTrace&) const;

        EvalTrace operator-()
        {
            EvalTrace et;
            #ifdef FULL_TRACE
            et.checkmate = checkmate;
            et.stalemate = stalemate;
            et.mobility = -mobility;
            et.material = -material;
            et.pawns    = -pawns;
            #endif // FULL_TRACE
            et.total    = -total;
            return et;
        }

        std::string toString() const
        {
            std::stringstream ss;
            #ifdef FULL_TRACE
            ss << "Checkmate:" << (checkmate ? "True" : "False") << std::endl;
            ss << "Stalemate:" << (stalemate ? "True" : "False") << std::endl;
            ss << "Mobility: " << mobility << std::endl;
            ss << "Material: " << material << std::endl;
            ss << "Pawns   : " << pawns << std::endl;
            #endif // FULL_TRACE
            ss << "Total   : " << total << std::endl;
            return ss.str();
        }

        friend inline std::ostream& operator<<(std::ostream& os, const EvalTrace& trace)
        {
            #ifdef FULL_TRACE
            os << "Checkmate:" << (trace.checkmate ? "True" : "False") << std::endl;
            os << "Stalemate:" << (trace.stalemate ? "True" : "False") << std::endl;
            os << "Mobility: " << trace.mobility << std::endl;
            os << "Material: " << trace.material << std::endl;
            os << "Pawns   : " << trace.pawns << std::endl;
            #endif // FULL_TRACE
            os << "Total   : " << trace.total << std::endl;
            return os;
        }
    } EvalTrace;

    typedef struct evalEntry_t
    {
        hash_t hash;
        eval_t value;
    } evalEntry_t;

    typedef struct phaseEntry_t
    {
        hash_t hash;
        uint8_t value;
    } phaseEntry_t;

    class Eval
    {
        private:
            uint64_t m_pawnEvalTableSize;
            uint64_t m_materialEvalTableSize;
            uint64_t m_pawnEvalTableMask;
            uint64_t m_materialEvalTableMask;
            std::unique_ptr<evalEntry_t[]> m_pawnEvalTable;
            std::unique_ptr<evalEntry_t[]> m_materialEvalTable;
            std::unique_ptr<phaseEntry_t[]> m_phaseTable;
            uint8_t m_getPhase(Board& board);
            eval_t m_getPawnEval(Board& board, uint8_t phase);
            eval_t m_getMaterialEval(Board& board, uint8_t phase);
            eval_t m_getMobilityEval(Board& board, uint8_t phase);
            
        public:
            Eval(uint8_t pawnEvalIndicies, uint8_t materialEvalIndicies);
            EvalTrace evaluate(Board& board);
    };
}