#include <uci.hpp>
#include <iostream>  
using namespace ChessEngine2;

static inline std::string getUCINotation(Move move)
{
    std::stringstream ss;
    ss << ChessEngine2::getArithmeticNotation(move.from)  << ChessEngine2::getArithmeticNotation(move.to);

    if(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)       ss << "q";
    else if(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)   ss << "r";
    else if(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP) ss << "b";
    else if(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT) ss << "n";

    return ss.str();
}

void UCI::loop()
{
    Board board = Board(startFEN);
    Searcher searcher = Searcher();
    std::string token, cmd;

    CE2_LOG("Entering UCI loop")
    do
    {
        if(!getline(std::cin, cmd))
        {
            cmd = "quit";
        }

        token.clear();
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if(strcmp(token.c_str(), "uci") == 0)
        {
            std::cout << "id name ChessEngine2" << std::endl; 
            std::cout << "id author Lars Murud Aurud" << std::endl; 
            std::cout << "uciok" << std::endl;
        } 
        else if (strcmp(token.c_str(), "setoption") == 0) CE2_WARNING("Missing setoption")
        else if (strcmp(token.c_str(), "go") == 0) go(board, searcher, is);
        else if (strcmp(token.c_str(), "position") == 0) position(board, is);
        else if (strcmp(token.c_str(), "ucinewgame") == 0) CE2_WARNING("ucinewgame")
        else if (strcmp(token.c_str(), "isready") == 0) std::cout << "readyok" << std::endl;

        // Custom
        else if (strcmp(token.c_str(), "ischeckmate") == 0) ischeckmate(board);


    } while(strcmp(token.c_str(), "quit") != 0);

    CE2_LOG("Exiting UCI loop")
}

void UCI::go(Board& board, Searcher& searcher, std::istringstream& is)
{
    std::string token;
    Move bestMove = Move(0,0);
    int32_t depth = -1;
    int32_t moveTime = -1;

    while(is >> token)
    {
        if(!strcmp(token.c_str(), "searchmoves"))    CE2_WARNING("searchmoves")
        else if(!strcmp(token.c_str(), "wtime"))     CE2_WARNING("wtime")
        else if(!strcmp(token.c_str(), "btime"))     CE2_WARNING("btime")
        else if(!strcmp(token.c_str(), "winc"))      CE2_WARNING("winc")
        else if(!strcmp(token.c_str(), "binc"))      CE2_WARNING("binc")
        else if(!strcmp(token.c_str(), "movestogo")) CE2_WARNING("movestogo")
        else if(!strcmp(token.c_str(), "depth"))     is >> depth;
        else if(!strcmp(token.c_str(), "nodes"))     CE2_WARNING("nodes")
        else if(!strcmp(token.c_str(), "movetime"))  is >> moveTime;
        else if(!strcmp(token.c_str(), "perft"))     CE2_WARNING("perft")
        else if(!strcmp(token.c_str(), "infinite"))  CE2_WARNING("infinite")
        else if(!strcmp(token.c_str(), "ponder"))    CE2_WARNING("ponder")
    }

    if(depth > 0)
    {
        CE2_LOG("Searching at depth " << depth);
        bestMove = searcher.getBestMove(board, depth, 4);
    } else if (moveTime > 0)
    {
        CE2_LOG("Searching for " << moveTime << "ms");
        bestMove = searcher.getBestMoveInTime(board, moveTime, 4);
    }

    std::cout << "bestmove " << getUCINotation(bestMove) << std::endl; 
}

void UCI::position(Board& board, std::istringstream& is)
{
    Move m;
    std::string token, fen;

    is >> token;
    if(strcmp(token.c_str(), "startpos") == 0)
    {
        fen = startFEN;
        is >> token;
    }
    else if(token == "fen")
    {
        while (is >> token && strcmp(token.c_str(), "moves") != 0)
            fen += token + " ";
    }

    CE2_LOG("Loading FEN: " << fen)
    board = Board(fen);
    board.addBoardToHistory();

    // Parse any moves
    while (is >> token)
    {
        // TODO: This can be improved to make the move based on only the uci
        Move* moves = board.getLegalMoves();
        uint8_t numLegalMoves = board.getNumLegalMoves();
        for(int i = 0; i < numLegalMoves; i++)
        {
            if(strcmp(token.c_str(), getUCINotation(moves[i]).c_str()) == 0)
            {
                CE2_DEBUG("Perform move " << getUCINotation(moves[i]));
                board.performMove(moves[i]);
                board.addBoardToHistory();
                break;
            }
        }
    }
}

// Returns "checkmate 'winner'" if checkmate
// Returns "stalemate" if stalemate
// Returns "nocheckmate" if there is no checkmate
void UCI::ischeckmate(Board& board)
{
    auto boardHistory = Board::getBoardHistory();
    auto it = boardHistory->find(board.getHash());
    if(it != boardHistory->end())
    {
        if(it->second == 3) // The check id done after the board is added to history
        {
            std::cout << "stalemate" << std::endl;
            return;
        }
    }

    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0)
    {
        if(board.isChecked(board.getTurn()))
        {
            std::cout << "checkmate " << (board.getTurn() == WHITE ? "black" : "white") << std::endl;
            return;
        }
        std::cout << "stalemate" << std::endl;
        return;
    }
    std::cout << "nocheckmate" << std::endl;
}