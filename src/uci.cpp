#include <uci.hpp>
#include <search.hpp>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <string>
#include <fstream>
#include <thread>
#include <syzygy.hpp>

using namespace Arcanum;

namespace UCI
{
    #ifdef ENABLE_UCI_PRINT
    const static std::string logFileName = "uci.log";
    #define _UCI_PRINT(_str) {std::ofstream fileStream(logFileName, std::ofstream::out | std::ifstream::app); \
    fileStream << _str << std::endl; \
    fileStream.close();}
    #else
    #define _UCI_PRINT(_str) ;
    #endif

    #define UCI_ERROR(_str)   _UCI_PRINT("[ERROR]  " << _str)
    #define UCI_WARNING(_str) _UCI_PRINT("[WARNING]" << _str)
    #define UCI_LOG(_str)     _UCI_PRINT("[LOG]    " << _str)
    #define UCI_DEBUG(_str)   _UCI_PRINT("[DEBUG]  " << _str)
    #define UCI_OUT(_str) {std::cout << _str << std::endl; UCI_LOG("To stdout: " << _str)}

    #ifndef ARCANUM_VERSION
    #define ARCANUM_VERSION dev_build
    #endif
    #define STRINGIFY(s) #s
    #define TOSTRING(x) STRINGIFY(x)

    static std::thread searchThread;
    static bool isSearching;

    void go(Board& board, Searcher& searcher, std::istringstream& is);
    void newgame(Searcher& Searcher, Evaluator& evaluatorm, Board& board);
    void setoption(Searcher& searcher, Evaluator& evaluator, std::istringstream& is);
    void position(Board& board, Searcher& searcher, std::istringstream& is);
    int64_t allocateTime(uint32_t time, uint32_t inc, uint32_t toGo, uint32_t moveNumber);
    void ischeckmate(Board& board, Searcher& searcher);
    void eval(Board& board, Evaluator& evaluator);
    void drawBoard(Board& board);
}

// Source: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
void UCI::loop()
{
    isSearching = false;

    // Create Log file
    #ifdef ENABLE_UCI_PRINT
    std::ofstream fileStream(logFileName, std::ofstream::trunc);
    fileStream << "Created log file: " << logFileName << std::endl;
    fileStream.close();
    #endif

    Board board = Board(startFEN);
    Searcher searcher = Searcher();
    Evaluator evaluator = Evaluator();
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
        std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c){ return std::tolower(c); });

        if(token == "uci")
        {
            UCI_OUT(std::string("id name Arcanum ").append(TOSTRING(ARCANUM_VERSION)))
            UCI_OUT("id author Lars Murud Aurud")
            UCI_OUT("option name Hash type spin default 32 min 0 max 8196")
            UCI_OUT("option name ClearHash type button")
            UCI_OUT("option name SyzygyPath type string default <empty>")
            UCI_OUT("option name NNUEPath type string default " << Evaluator::nnuePathDefault)
            UCI_OUT("uciok")
        }
        else if (token == "setoption" ) setoption(searcher, evaluator, is);
        else if (token == "go"        ) go(board, searcher, is);
        else if (token == "position"  ) position(board, searcher, is);
        else if (token == "ucinewgame") newgame(searcher, evaluator, board);
        else if (token == "isready"   ) UCI_OUT("readyok")
        else if (token == "stop"      ) searcher.stop();

        // Custom
        else if (token == "eval"       ) eval(board, evaluator);
        else if (token == "ischeckmate") ischeckmate(board, searcher);
        else if (token == "d"          ) drawBoard(board);

    } while(token != "quit");

    TBFree();
    UCI_LOG("Exiting UCI loop")
}

void UCI::newgame(Searcher& searcher, Evaluator& evaluator, Board& board)
{
    if(isSearching) return;

    searcher.clearTT();
    searcher.clearHistory();
    board = Board(startFEN);
    searcher.addBoardToHistory(board);
}

void UCI::setoption(Searcher& searcher, Evaluator& evaluator, std::istringstream& is)
{
    #define SET_LOWER_CASE(_str) std::transform((_str).begin(), (_str).end(), (_str).begin(), [](unsigned char c){ return std::tolower(c); });

    if(isSearching) return;

    std::string name, valueToken, nameToken;
    is >> std::skipws >> nameToken; // 'name'
    SET_LOWER_CASE(nameToken)

    if(nameToken != "name") return;

    is >> std::skipws >> name;      // name of the option
    SET_LOWER_CASE(name)

    // Process button options
    if(name == "clearhash") searcher.clearTT();

    is >> std::skipws >> valueToken; // 'value'
    SET_LOWER_CASE(valueToken)

    if(valueToken != "value") return;

    // Process non-button options
    if(name == "hash")
    {
        uint32_t mbSize;
        is >> std::skipws >> mbSize;
        searcher.resizeTT(mbSize);
    }
    else if(name == "syzygypath")
    {
        std::string str;
        is >> std::skipws >> str;
        if(!TBInit(str))
        {
            UCI_ERROR("Failed to set SyzygyPath " << str)
            exit(-1);
        }
    }
    else if(name == "nnuepath")
    {
        std::string path;
        is >> std::skipws >> path;
        Evaluator::nnue.load(path);
    }
}

void UCI::go(Board& board, Searcher& searcher, std::istringstream& is)
{
    std::string token;
    std::stringstream ssInfo;
    SearchParameters parameters = SearchParameters();
    uint32_t time[2] = {0, 0};
    uint32_t inc[2] = {0, 0};
    uint32_t movesToGo = 0;
    ssInfo << "info ";

    while(is >> token)
    {
        if(token == "searchmoves")
        {
            // TODO: This can be improved to make the move based on only the uci
            Move* moves = board.getLegalMoves();
            int numLegalMoves = board.getNumLegalMoves();
            board.generateCaptureInfo();
            while (is >> token)
                for(int i = 0; i < numLegalMoves; i++)
                    if(token == moves[i].toString())
                        parameters.searchMoves[parameters.numSearchMoves++] = moves[i];
        }
        else if(token == "wtime"     ) is >> time[Color::WHITE];
        else if(token == "btime"     ) is >> time[Color::BLACK];
        else if(token == "winc"      ) is >> inc[Color::WHITE];
        else if(token == "binc"      ) is >> inc[Color::BLACK];
        else if(token == "movestogo" ) is >> movesToGo;
        else if(token == "depth"     ) is >> parameters.depth;
        else if(token == "nodes"     ) is >> parameters.nodes;
        else if(token == "movetime"  ) is >> parameters.msTime;
        else if(token == "perft"     ) UCI_WARNING("perft")
        else if(token == "infinite"  ) parameters.infinite = true;
        else if(token == "ponder"    ) UCI_WARNING("ponder")
        else if(token == "mate"      ) UCI_WARNING("mate")
        else UCI_ERROR("Unknown command")
    }

    Color turn = board.getTurn();
    int64_t allocatedTime = allocateTime(time[turn], inc[turn], movesToGo, board.getFullMoves());
    if(parameters.msTime > 0 && allocatedTime > 0)
        parameters.msTime = std::min(parameters.msTime, allocatedTime);
    else if(allocatedTime > 0)
        parameters.msTime = allocatedTime;

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

void UCI::position(Board& board, Searcher& searcher, std::istringstream& is)
{
    Move m;
    std::string token, fen;
    is >> token;

    if(token == "startpos")
    {
        fen = startFEN;
        is >> token;
    }
    else if(token == "fen")
    {
        while (is >> token && token != "moves")
            fen += token + " ";
    }

    UCI_LOG("Loading FEN: " << fen)
    searcher.clearHistory();

    board = Board(fen);
    searcher.addBoardToHistory(board);

    // Parse any moves
    while (is >> token)
    {
        // TODO: This can be improved to make the move based on only the uci
        Move* moves = board.getLegalMoves();
        uint8_t numLegalMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();
        for(int i = 0; i < numLegalMoves; i++)
        {
            if(token == moves[i].toString())
            {
                board.performMove(moves[i]);
                searcher.addBoardToHistory(board);
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

int64_t UCI::allocateTime(uint32_t time, uint32_t inc, uint32_t toGo, uint32_t moveNumber)
{
    // I pulled these numbers and formulas out of a hat :^)
    if(time == 0)
        return 0;

    if(toGo > 0)
        return std::max(1U, (time / (toGo + 1)) + inc);

    if(inc > 0)
    {
        if(moveNumber >= 40)
            return std::max(1U, (time + inc) / 2U);
        return std::max(1U, std::min(time, (time + inc * (40 - moveNumber)) / (45U - moveNumber)));
    }

    if(moveNumber >= 40 || time < 10000)
        return std::max(1U, time / 20U);

    return std::max(1U, time / (50U - moveNumber));
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

void UCI::sendUciBestMove(const Move& move)
{
    UCI_OUT("bestmove " << move)
}

// Returns the statc eval score for white
void UCI::eval(Board& board, Evaluator& evaluator)
{
    evaluator.initAccumulatorStack(board);
    UCI_OUT(evaluator.evaluate(board, 0))
}

// Returns "checkmate 'winner'" if checkmate
// Returns "stalemate" if stalemate
// Returns "nocheckmate" if there is no checkmate
void UCI::ischeckmate(Board& board, Searcher& searcher)
{
    auto history = searcher.getHistory();
    auto it = history.find(board.getHash());
    if(it != history.end())
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
