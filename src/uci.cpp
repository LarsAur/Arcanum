#include <uci.hpp>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <string>
#include <fstream>

using namespace Arcanum;

const static std::string logFileName = "uci.log";
#define _UCI_PRINT(_str) {std::ofstream fileStream(logFileName, std::ofstream::out | std::ifstream::app); \
fileStream << _str << std::endl; \
fileStream.close();}

#define UCI_ERROR(_str)   _UCI_PRINT("[ERROR]  " << _str)
#define UCI_WARNING(_str) _UCI_PRINT("[WARNING]" << _str)
#define UCI_LOG(_str)     _UCI_PRINT("[LOG]    " << _str)
#define UCI_DEBUG(_str)   _UCI_PRINT("[DEBUG]  " << _str)
#define UCI_OUT(_str) {std::cout << _str << std::endl; UCI_LOG("To stdout: " << _str)}

// Source: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
void UCI::loop()
{
    // Create Log file
    std::ofstream fileStream(logFileName, std::ofstream::trunc);
    fileStream << "Created log file: " << logFileName << std::endl;
    fileStream.close();

    Board board = Board(startFEN);
    Searcher searcher = Searcher();
    std::string token, cmd;

    UCI_LOG("Entering UCI loop")
    do
    {
        if(!getline(std::cin, cmd))
        {
            cmd = "quit";
        }

        UCI_LOG("From stdin: " << cmd)

        token.clear();
        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if(strcmp(token.c_str(), "uci") == 0)
        {
            UCI_OUT("id name Arcanum") 
            UCI_OUT("id author Lars Murud Aurud") 
            UCI_OUT("uciok")
        } 
        else if (strcmp(token.c_str(), "setoption") == 0) UCI_WARNING("Missing setoption")
        else if (strcmp(token.c_str(), "go") == 0) go(board, searcher, is);
        else if (strcmp(token.c_str(), "position") == 0) position(board, is);
        else if (strcmp(token.c_str(), "ucinewgame") == 0) UCI_WARNING("ucinewgame")
        else if (strcmp(token.c_str(), "isready") == 0) UCI_OUT("readyok")

        // Custom
        else if (strcmp(token.c_str(), "ischeckmate") == 0) ischeckmate(board);


    } while(strcmp(token.c_str(), "quit") != 0);

    UCI_LOG("Exiting UCI loop")
}

void UCI::go(Board& board, Searcher& searcher, std::istringstream& is)
{
    std::string token;
    Move bestMove = Move(0,0);
    int32_t depth = -1;
    int32_t moveTime = -1;

    while(is >> token)
    {
        if(!strcmp(token.c_str(), "searchmoves"))    UCI_WARNING("searchmoves")
        else if(!strcmp(token.c_str(), "wtime"))     UCI_WARNING("wtime")
        else if(!strcmp(token.c_str(), "btime"))     UCI_WARNING("btime")
        else if(!strcmp(token.c_str(), "winc"))      UCI_WARNING("winc")
        else if(!strcmp(token.c_str(), "binc"))      UCI_WARNING("binc")
        else if(!strcmp(token.c_str(), "movestogo")) UCI_WARNING("movestogo")
        else if(!strcmp(token.c_str(), "depth"))     is >> depth;
        else if(!strcmp(token.c_str(), "nodes"))     UCI_WARNING("nodes")
        else if(!strcmp(token.c_str(), "movetime"))  is >> moveTime;
        else if(!strcmp(token.c_str(), "perft"))     UCI_WARNING("perft")
        else if(!strcmp(token.c_str(), "infinite"))  UCI_WARNING("infinite")
        else if(!strcmp(token.c_str(), "ponder"))    UCI_WARNING("ponder")
        else UCI_ERROR("Unknown command")
    }

    if(depth > 0)
    {
        UCI_LOG("Searching at depth " << depth);
        bestMove = searcher.getBestMove(board, depth, 4);
    } else if (moveTime > 0)
    {
        UCI_LOG("Searching for " << moveTime << "ms");
        bestMove = searcher.getBestMoveInTime(board, moveTime, 4);
        if(bestMove == Move(0,0))
        {
            UCI_ERROR("Search did not find any moves")
            exit(EXIT_FAILURE);
        }
    }

    UCI_OUT("info pv " << bestMove)
    UCI_OUT("bestmove " << bestMove) 
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

    UCI_LOG("Loading FEN: " << fen)
    board = Board(fen);
    board.getBoardHistory()->clear();
    board.addBoardToHistory();

    // Parse any moves
    while (is >> token)
    {
        // TODO: This can be improved to make the move based on only the uci
        Move* moves = board.getLegalMoves();
        uint8_t numLegalMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();
        for(int i = 0; i < numLegalMoves; i++)
        {
            if(strcmp(token.c_str(), moves[i].toString().c_str()) == 0)
            {
                board.performMove(moves[i]);
                board.addBoardToHistory();
                break;
            }

            // If none of the moves match the input, it is not legal
            if(i == numLegalMoves - 1)
            {
                UCI_ERROR(token << " is not legal in the position")
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

        // Check for 50 move rule
    if(board.getHalfMoves() >= 100)
    {
        std::cout << "stalemate" << std::endl;
        return;
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