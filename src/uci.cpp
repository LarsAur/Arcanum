#include <uci.hpp>
#include <search.hpp>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <string>
#include <fstream>
#include <thread>

using namespace Arcanum;

namespace UCI
{
    const static std::string logFileName = "uci.log";
    #define _UCI_PRINT(_str) {std::ofstream fileStream(logFileName, std::ofstream::out | std::ifstream::app); \
    fileStream << _str << std::endl; \
    fileStream.close();}

    #define UCI_ERROR(_str)   _UCI_PRINT("[ERROR]  " << _str)
    #define UCI_WARNING(_str) _UCI_PRINT("[WARNING]" << _str)
    #define UCI_LOG(_str)     _UCI_PRINT("[LOG]    " << _str)
    #define UCI_DEBUG(_str)   _UCI_PRINT("[DEBUG]  " << _str)
    #define UCI_OUT(_str) {std::cout << _str << std::endl; UCI_LOG("To stdout: " << _str)}

    static std::thread searchThread;
    static bool isSearching;

    void go(Arcanum::Board& board, Arcanum::Searcher& searcher, std::istringstream& is);
    void setoption(std::istringstream& is);
    void position(Arcanum::Board& board, std::istringstream& is);
    void ischeckmate(Arcanum::Board& board);
    void eval(Arcanum::Board& board);
    void drawBoard(Arcanum::Board& board);
}

// Source: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
void UCI::loop()
{
    isSearching = false;

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
            cmd = "quit";

        if(searchThread.joinable() && !isSearching)
            searchThread.join();

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
        else if (strcmp(token.c_str(), "setoption"  ) == 0) UCI_WARNING("Missing setoption")
        else if (strcmp(token.c_str(), "go"         ) == 0) go(board, searcher, is);
        else if (strcmp(token.c_str(), "position"   ) == 0) position(board, is);
        else if (strcmp(token.c_str(), "ucinewgame" ) == 0) UCI_WARNING("ucinewgame")
        else if (strcmp(token.c_str(), "isready"    ) == 0) UCI_OUT("readyok")
        else if (strcmp(token.c_str(), "stop"       ) == 0) searcher.stop();

        // Custom
        else if (strcmp(token.c_str(), "eval") == 0) eval(board);
        else if (strcmp(token.c_str(), "ischeckmate") == 0) ischeckmate(board);
        else if (strcmp(token.c_str(), "d") == 0) drawBoard(board);

    } while(strcmp(token.c_str(), "quit") != 0);

    UCI_LOG("Exiting UCI loop")
}

void UCI::go(Board& board, Searcher& searcher, std::istringstream& is)
{
    std::string token;
    std::stringstream ssInfo;
    SearchParameters parameters = SearchParameters();
    ssInfo << "info ";

    while(is >> token)
    {
        if(!strcmp(token.c_str(), "searchmoves"))
        {
            // TODO: This can be improved to make the move based on only the uci
            Move* moves = board.getLegalMoves();
            int numLegalMoves = board.getNumLegalMoves();
            board.generateCaptureInfo();
            while (is >> token)
                for(int i = 0; i < numLegalMoves; i++)
                    if(strcmp(token.c_str(), moves[i].toString().c_str()) == 0)
                        parameters.searchMoves[numLegalMoves++];
        }
        else if(!strcmp(token.c_str(), "wtime"))     UCI_WARNING("wtime")
        else if(!strcmp(token.c_str(), "btime"))     UCI_WARNING("btime")
        else if(!strcmp(token.c_str(), "winc"))      UCI_WARNING("winc")
        else if(!strcmp(token.c_str(), "binc"))      UCI_WARNING("binc")
        else if(!strcmp(token.c_str(), "movestogo")) UCI_WARNING("movestogo")
        else if(!strcmp(token.c_str(), "depth"))     is >> parameters.depth;
        else if(!strcmp(token.c_str(), "nodes"))     is >> parameters.nodes;
        else if(!strcmp(token.c_str(), "movetime"))  is >> parameters.msTime;
        else if(!strcmp(token.c_str(), "perft"))     UCI_WARNING("perft")
        else if(!strcmp(token.c_str(), "infinite"))  parameters.infinite = true;
        else if(!strcmp(token.c_str(), "ponder"))    UCI_WARNING("ponder")
        else UCI_ERROR("Unknown command")
    }

    // TODO: Time manager

    if(!isSearching)
    {
        // Set isSearching before creating the thread.
        // This is to make sure it is set before returning from go.
        isSearching = true;
        searchThread = std::thread([&] {
            searcher.search(board, parameters);
            // Set isSearching to false when the search is done in the thread
            isSearching = false;
        });
    }
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

void UCI::sendUciInfo(const SearchInfo& info)
{
    std::stringstream ss;

    ss << "info";
    if(info.depth > 0)
        ss << " depth " << info.depth;
    if(info.msTime > 0)
        ss << " time " << info.msTime;
    if(info.nodes > 0)
        ss << " nodes " << info.nodes;
    if(info.mate)
        ss << " score mate " << info.mateDistance;
    else
        ss << " score cp " << info.score;
    if(info.hashfull > 0)
        ss << " hashfull " << info.hashfull;
    if(info.nodes > 0 && info.msTime > 0)
        ss << " nps " << ((1000 * info.nodes) / info.msTime);
    if(info.pvLine.size() > 0)
    {
        ss << " pv";
        for(Move move : info.pvLine)
            ss << " " << move;
    }

    UCI_OUT(ss.str())
}

void UCI::sendUciBestMove(const Arcanum::Move& move)
{
    UCI_OUT("bestmove " << move)
}

// Returns the statc eval score for white
void UCI::eval(Board& board)
{
    Evaluator evaluator = Evaluator();
    UCI_OUT(evaluator.evaluate(board, 0).total)
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

void UCI::drawBoard(Board& board)
{
    std::cout << board.getBoardString() << std::endl;
    std::cout << "FEN: " << board.getFEN() << std::endl;
    std::cout << "Current Turn: " << ((board.getTurn() == Color::WHITE) ? "White" : "Black") << std::endl;
}
