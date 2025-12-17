#include <uci/uci.hpp>
#include <uci/timeman.hpp>
#include <tuning/fengen.hpp>
#include <tuning/nnuetrainer.hpp>
#include <uci/wdlmodel.hpp>
#include <syzygy.hpp>
#include <fen.hpp>
#include <perft.hpp>
#include <utils.hpp>

using namespace Arcanum;
using namespace Arcanum::Interface;

bool        UCI::isSearching = false;
std::thread UCI::searchThread;
Board       UCI::board(FEN::startpos);
Searcher    UCI::searcher;
std::vector<Option*> Option::options;

SpinOption   UCI::optionHash         = SpinOption("Hash", 32, 0, 2048, []{ UCI::searcher.resizeTT(UCI::optionHash.value); });
ButtonOption UCI::optionClearHash    = ButtonOption("ClearHash", []{ UCI::searcher.clear(); });
StringOption UCI::optionSyzygyPath   = StringOption("SyzygyPath", "<empty>", []{ Syzygy::TBInit(UCI::optionSyzygyPath.value); });
StringOption UCI::optionNNUEPath     = StringOption("NNUEPath", TOSTRING(DEFAULT_NNUE), []{ Evaluator::nnue.load(UCI::optionNNUEPath.value); });
SpinOption   UCI::optionMoveOverhead = SpinOption("MoveOverhead", 10, 0, 5000);
CheckOption  UCI::optionNormalizeScore = CheckOption("NormalizeScore", true);
CheckOption  UCI::optionShowWDL      = CheckOption("UCI_ShowWDL", false);
CheckOption  UCI::optionDatagenMode  = CheckOption("DatagenMode", false, []{ UCI::searcher.setDatagenMode(UCI::optionDatagenMode.value); });

void UCI::listUCI()
{
    UCI_OUT(std::string("id name Arcanum ").append(TOSTRING(ARCANUM_VERSION)))
    UCI_OUT("id author Lars Murud Aurud")
    for(auto option : Option::options)
    {
        option->list();
    }
    UCI_OUT("uciok")
}

void UCI::newgame()
{
    if(UCI::isSearching) return;

    UCI::searcher.clear();
    UCI::searcher.clearHistory();
    UCI::board = Board(FEN::startpos);
    UCI::searcher.addBoardToHistory(board);
}

void UCI::setoption(std::istringstream& is)
{
    if(isSearching) return;

    // input stream should consist of up to 4 tokens:
    // 'name' <name> 'value' <value>
    // For button options, only the first two tokens may be present

    std::string tokens[2];
    std::string name, value;
    is >> std::skipws >> tokens[0];
    is >> std::skipws >> name;
    is >> std::skipws >> tokens[1];
    is >> std::skipws >> value;

    if(tokens[0] != "name") return;

    // Match button options
    for(auto option : Option::options)
    {
        if(option->matches(name) && option->isButton())
        {
            option->set("");
            break;
        }
    }

    if(tokens[1] != "value") return;

    // Match non-button options
    for(auto option : Option::options)
    {
        if(option->matches(name) && !option->isButton())
        {
            option->set(value);
            break;
        }
    }
}

void UCI::go(std::istringstream& is)
{
    std::string token;
    SearchParameters parameters = SearchParameters();
    int64_t time[2] = {0, 0};
    int64_t inc[2] = {0, 0};
    int64_t movesToGo = -1;
    int32_t perftDepth = 0;
    bool requireTimeAlloc[2] = {false, false};

    while(is >> token)
    {
        toLowerCase(token);
        if(token == "searchmoves")
        {
            while (is >> token)
            {
                toLowerCase(token);
                Move move = board.getMoveFromArithmetic(token);
                parameters.searchMoves[parameters.numSearchMoves++] = move;
            }
        }
        else if(token == "wtime"     ) { is >> time[Color::WHITE]; requireTimeAlloc[Color::WHITE] = true; }
        else if(token == "btime"     ) { is >> time[Color::BLACK]; requireTimeAlloc[Color::BLACK] = true; }
        else if(token == "winc"      ) { is >> inc[Color::WHITE]; }
        else if(token == "binc"      ) { is >> inc[Color::BLACK]; }
        else if(token == "depth"     ) { is >> parameters.depth;  parameters.useDepth = true; }
        else if(token == "nodes"     ) { is >> parameters.nodes;  parameters.useNodes = true; }
        else if(token == "movetime"  ) { is >> parameters.msTime; parameters.useTime  = true; }
        else if(token == "movestogo" ) { is >> movesToGo;            }
        else if(token == "perft"     ) { is >> perftDepth;           }
        else if(token == "infinite"  ) { parameters.infinite = true; }
        else if(token == "ponder"    ) { WARNING("Missing implementation: ponder")  }
        else if(token == "mate"      ) { WARNING("Missing implementation: mate")    }
        else ERROR("Unknown command: " << token)
    }

    // If perft is used, search will not be performed
    if(perftDepth > 0)
    {
        perft(UCI::board, perftDepth);
        return;
    }

    // Subtract moveOverhead from moveTime
    if(parameters.msTime != 0) parameters.msTime = std::max(parameters.msTime - optionMoveOverhead.value, int64_t(1));

    // Allocate time
    Color turn = board.getTurn();
    if(requireTimeAlloc[turn])
    {
        parameters.useTime = true;
        parameters.msTime = getAllocatedTime(time[turn], inc[turn], movesToGo, parameters.msTime, optionMoveOverhead.value);
    }

    if(!UCI::isSearching)
    {
        // Set isSearching before creating the thread.
        // This is to make sure it is set before returning from go.
        UCI::isSearching = true;
        UCI::searchThread = std::thread([&](SearchParameters _parameters) {
            UCI::searcher.search(board, _parameters);
            // Set isSearching to false when the search is done in the thread
            UCI::isSearching = false;
        }, parameters);
    }
}

void UCI::position(std::istringstream& is)
{
    std::string token, fen;
    is >> token;
    toLowerCase(token);

    if(token == "startpos")
    {
        fen = FEN::startpos;
        is >> token;
    }
    else if(token == "fen")
    {
        // Do not convert token to lower case, because the FEN is case sensitive
        while (is >> token && !strEqCi(token, "moves"))
            fen += token + " ";
    }

    DEBUG("Loading FEN: " << fen)
    searcher.clearHistory();
    board = Board(fen);
    searcher.addBoardToHistory(board);

    // Parse any moves
    while (is >> token)
    {
        toLowerCase(token);
        Move move = board.getMoveFromArithmetic(token);
        if(move.isNull())
        {
            ERROR(token << " is not legal in the position")
        }
        else
        {
            board.performMove(move);
            searcher.addBoardToHistory(board);
        }
    }
}

void UCI::isready()
{
    UCI_OUT("readyok")
}

void UCI::stop()
{
    UCI::searcher.stop();
}

void UCI::eval()
{
    Evaluator evaluator;
    evaluator.initAccumulatorStack(board);
    eval_t score = evaluator.evaluate(board, 0);

    if(optionNormalizeScore.value)
    {
        UCI_OUT(WDLModel::getNormalizedScore(board, score))
    }
    else
    {
        UCI_OUT(score)
    }
}

void UCI::drawboard()
{
    UCI_OUT(FEN::toString(board))
    UCI_OUT("FEN: " << FEN::getFEN(board))
    UCI_OUT("Current Turn: " << ((board.getTurn() == Color::WHITE) ? "White" : "Black"))
}

void UCI::help()
{
    UCI_OUT("ucinewgame                            - Start a new game")
    UCI_OUT("uci                                   - List uci options and author")
    UCI_OUT("setoption                             - Set uci option")
    UCI_OUT("\tname <name>                         - Option name")
    UCI_OUT("\t[value <value>]                     - Option value")
    UCI_OUT("go                                    - Search the current positions with given restrictions")
    UCI_OUT("\t[wtime <wtime>]                     - White's remaining time (ms)")
    UCI_OUT("\t[btime <btime>]                     - Black's remaining time (ms)")
    UCI_OUT("\t[winc <winc>]                       - White's time increment (ms)")
    UCI_OUT("\t[binc <winc>]                       - Black's time increment (ms)")
    UCI_OUT("\t[movestogo <movestogo>]             - Moves until new time is given")
    UCI_OUT("\t[depth <depth>]                     - Maximum depth to search to")
    UCI_OUT("\t[nodes <nodes>]                     - Maximum number of nodes to search")
    UCI_OUT("\t[movetime <movetime>]               - Maximum time to search (ms)")
    UCI_OUT("\t[infinite]                          - Search until stop command is given")
    UCI_OUT("go perft <depth>                      - Run perft to given depth")
    UCI_OUT("stop                                  - Stop any currently ongoing search")
    UCI_OUT("position                              - Set the current position")
    UCI_OUT("\tfen <FEN> | startpos                - Set to given FEN or the starting position")
    UCI_OUT("\t[moves <list of moves>]             - Perform the moves after setting the position")
    UCI_OUT("isready                               - Ask if the engine is ready to receive new commands. 'readyok' is returned when ready")
    UCI_OUT("eval                                  - Returns the static evaluation for the current position")
    UCI_OUT("d                                     - Show the current board, FEN and turn")
    UCI_OUT("\nFor more details, check out https://www.wbec-ridderkerk.nl/html/UCIProtocol.html")
}

void UCI::loop()
{
    UCI::newgame();

    DEBUG("Entering UCI loop")
    std::string token, cmd;
    do
    {
        if(!getline(std::cin, cmd))
            cmd = "quit";

        // Join the search thread if the search has finished
        if(!isSearching && searchThread.joinable())
            searchThread.join();

        DEBUG("UCI command: " << cmd)

        std::istringstream is(cmd);
        is >> std::skipws >> token;
        toLowerCase(token);

        if      (token == "uci"       ) UCI::listUCI();
        else if (token == "setoption" ) UCI::setoption(is);
        else if (token == "go"        ) UCI::go(is);
        else if (token == "position"  ) UCI::position(is);
        else if (token == "ucinewgame") UCI::newgame();
        else if (token == "isready"   ) UCI::isready();
        else if (token == "stop"      ) UCI::stop();
        else if (token == "eval"      ) UCI::eval();
        else if (token == "d"         ) UCI::drawboard();
        else if (token == "help"      ) UCI::help();
    } while (token != "quit");

    Syzygy::TBFree();
    DEBUG("Exiting UCI loop")
}

void UCI::sendInfo(const SearchInfo& info)
{
    std::stringstream ss;

    ss << "info";
    ss << " depth " << info.depth;
    ss << " seldepth " << info.seldepth;
    ss << " time " << info.msTime;
    ss << " nodes " << info.nodes;
    ss << " hashfull " << info.hashfull;
    ss << " tbhits " << info.tbHits;

    if(info.mate)
    {
        ss << " score mate " << info.mateDistance;
    }
    else
    {
        if(optionNormalizeScore.value)
        {
            ss << " score cp " << WDLModel::getNormalizedScore(info.board, info.score);
        }
        else
        {
            ss << " score cp " << info.score;
        }
    }

    if(optionShowWDL.value)
    {
        WDL wdl = WDLModel::getExpectedWDL(info.board, info.score);
        ss << " wdl " << wdl.win << " " << wdl.draw << " " << wdl.loss;
    }

    if(info.msTime > 0)
    {
        ss << " nps " << ((1000 * info.nodes) / info.msTime);
    }
    else if(info.nsTime > 0)
    {
        ss << " nps " << ((1000000000 * info.nodes) / info.nsTime);
    }

    if(info.pvTable)
        ss << " pv " << info.pvTable->getPvLine();

    UCI_OUT(ss.str())
}

void UCI::sendBestMove(const Move& move)
{
    if(move.isNull())
        ERROR("Illegal Null-Move was reported as the best move")

    UCI_OUT("bestmove " << move)
}