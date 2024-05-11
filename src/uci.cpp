#include <uci.hpp>
#include <search.hpp>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <string>
#include <fstream>
#include <thread>
#include <syzygy.hpp>
#include <tuning/fengen.hpp>
#include <fen.hpp>
#include <perft.hpp>

using namespace Arcanum;

namespace UCI
{
    #define UCI_OUT(_str) std::cout << _str << std::endl;

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
    void fengen(std::istringstream& is);
    void train(std::istringstream& is);
}

// Source: https://www.wbec-ridderkerk.nl/html/UCIProtocol.html
void UCI::loop()
{
    isSearching = false;
    Board board = Board(FEN::startpos);
    Searcher searcher = Searcher();
    Evaluator evaluator = Evaluator();
    std::string token, cmd;

    LOG("Entering UCI loop")
    do
    {
        if(!getline(std::cin, cmd))
            cmd = "quit";

        if(searchThread.joinable() && !isSearching)
            searchThread.join();

        LOG("From stdin: " << cmd)

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
        else if (token == "fengen"     ) fengen(is);
        else if (token == "train"      ) train(is);

    } while(token != "quit");

    TBFree();
    LOG("Exiting UCI loop")
}

void UCI::newgame(Searcher& searcher, Evaluator& evaluator, Board& board)
{
    if(isSearching) return;

    searcher.clearTT();
    searcher.clearHistory();
    board = Board(FEN::startpos);
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
            ERROR("Failed to set SyzygyPath " << str)
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
    uint32_t perftDepth = 0;
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
        else if(token == "perft"     ) is >> perftDepth;
        else if(token == "infinite"  ) parameters.infinite = true;
        else if(token == "ponder"    ) WARNING("Missing implementation: ponder")
        else if(token == "mate"      ) WARNING("Missing implementation: mate")
        else ERROR("Unknown command: " << token)
    }

    // If perft is used, search will not be performed
    if(perftDepth > 0)
    {
        Test::perft(board, perftDepth);
        return;
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
        fen = FEN::startpos;
        is >> token;
    }
    else if(token == "fen")
    {
        while (is >> token && token != "moves")
            fen += token + " ";
    }

    LOG("Loading FEN: " << fen)
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
                ERROR(token << " is not legal in the position")
            }
        }
    }
}

int64_t UCI::allocateTime(uint32_t time, uint32_t inc, uint32_t toGo, uint32_t moveNumber)
{
    // Ensure there is some margin
    time = std::max(1U, time - 10U);

    // I pulled these numbers and formulas out of a hat :^)
    if(time == 0)
        return 0;

    if(toGo > 0)
        return std::max(1U, (time / (toGo + 1)) + inc);

    if(inc > 0)
    {
        if(moveNumber >= 40)
            return std::max(1U, std::min(time, (time / 8U) + inc));
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
    Evaluator::nnue.logEvalBreakdown(board);
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
            UCI_OUT("stalemate")
            return;
        }
    }

    // Check for 50 move rule
    if(board.getHalfMoves() >= 100)
    {
        UCI_OUT("stalemate")
        return;
    }

    board.getLegalMoves();
    if(board.getNumLegalMoves() == 0)
    {
        if(board.isChecked())
        {
            UCI_OUT("checkmate " << (board.getTurn() == WHITE ? "black" : "white"))
            return;
        }
        UCI_OUT("stalemate")
        return;
    }
    UCI_OUT("nocheckmate")
}

void UCI::drawBoard(Board& board)
{
    UCI_OUT(FEN::toString(board))
    UCI_OUT("FEN: " << FEN::getFEN(board))
    UCI_OUT("Current Turn: " << ((board.getTurn() == Color::WHITE) ? "White" : "Black"))
}

// Parse parameters and call Tuning::fengen
// It is strongly recomended that syzygy is used when generating FENS
// This reduces wrongly played endgames and improves the data quality
void UCI::fengen(std::istringstream& is)
{
    std::string startPosPath;   // Start position epd file path
    std::string outputPath;     // Output path
    std::string numFens;        // Number of fens to generate
    std::string numThreads;     // Number of threads
    std::string depth;          // Search depth

    is >> std::skipws >> startPosPath;
    is >> std::skipws >> outputPath;
    is >> std::skipws >> numFens;
    is >> std::skipws >> numThreads;
    is >> std::skipws >> depth;

    //TODO: Sanitize input. E.g validate int values

    Tuning::fengen(startPosPath, outputPath, atoi(numFens.c_str()), atoi(numThreads.c_str()), atoi(depth.c_str()));
}

void UCI::train(std::istringstream& is)
{
    std::string dataset;        // Dataset created by fengen
    std::string outputPath;     // Output path (_epoch.fnnue will be added as a suffix)
    std::string batchSizeStr;   // Batch size
    std::string startEpochStr;  // Epoch to start at (used for naming output and ADAM time variable)
    std::string endEpochStr;    // Epoch to end at

    is >> std::skipws >> dataset;
    is >> std::skipws >> outputPath;
    is >> std::skipws >> batchSizeStr;
    is >> std::skipws >> startEpochStr;
    is >> std::skipws >> endEpochStr;

    uint64_t batchSize  = atoi(batchSizeStr.c_str());
    uint32_t startEpoch = atoi(startEpochStr.c_str());
    uint32_t endEpoch   = atoi(endEpochStr.c_str());

    if(startEpoch < 1)
    {
        ERROR("Start epoch must be at least 1")
        return;
    }

    // TODO: Add condition for creating a new randomized net

    Evaluator::nnue.train(dataset, outputPath, batchSize, startEpoch, endEpoch);
}