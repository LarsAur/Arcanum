#include <uci/uci.hpp>
#include <uci/timeman.hpp>
#include <tuning/fengen.hpp>
#include <syzygy.hpp>
#include <fen.hpp>
#include <perft.hpp>

using namespace Arcanum;
using namespace Arcanum::Interface;

bool        UCI::isSearching = false;
std::thread UCI::searchThread;
Board       UCI::board(FEN::startpos);
Searcher    UCI::searcher;

SpinOption   UCI::optionHash         = SpinOption("Hash", 32, 0, 8196, []{ UCI::searcher.resizeTT(UCI::optionHash.value); });
ButtonOption UCI::optionClearHash    = ButtonOption("ClearHash", []{ UCI::searcher.clear(); });
StringOption UCI::optionSyzygyPath   = StringOption("SyzygyPath", "<empty>", []{ TBInit(UCI::optionSyzygyPath.value); });
StringOption UCI::optionNNUEPath     = StringOption("NNUEPath", "arcanum-net-v3.0.fnnue", []{ Evaluator::nnue.load(UCI::optionNNUEPath.value); });
SpinOption   UCI::optionMoveOverhead = SpinOption("MoveOverhead", 10, 0, 5000);

void UCI::listUCI()
{
    UCI_OUT(std::string("id name Arcanum ").append(TOSTRING(ARCANUM_VERSION)))
    UCI_OUT("id author Lars Murud Aurud")
    UCI::optionHash.list();
    UCI::optionClearHash.list();
    UCI::optionSyzygyPath.list();
    UCI::optionNNUEPath.list();
    UCI::optionMoveOverhead.list();
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
    if(UCI::optionClearHash.matches(name)) UCI::optionClearHash.set("");

    if(tokens[1] != "value") return;

    // Match non-button options
    if     (UCI::optionHash.matches(name)        ) UCI::optionHash.set(value);
    else if(UCI::optionMoveOverhead.matches(name)) UCI::optionMoveOverhead.set(value);
    else if(UCI::optionNNUEPath.matches(name)    ) UCI::optionNNUEPath.set(value);
    else if(UCI::optionSyzygyPath.matches(name)  ) UCI::optionSyzygyPath.set(value);
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
        if(token == "searchmoves")
        {
            while (is >> token)
            {
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
        Test::perft(UCI::board, perftDepth);
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
        UCI::searchThread = std::thread([&] {
            UCI::searcher.search(board, parameters);
            // Set isSearching to false when the search is done in the thread
            UCI::isSearching = false;
        });
    }
}

void UCI::position(std::istringstream& is)
{
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
        Move move = board.getMoveFromArithmetic(token);
        if(move == NULL_MOVE)
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
    UCI_OUT(evaluator.evaluate(board, 0))
}

void UCI::drawboard()
{
    UCI_OUT(FEN::toString(board))
    UCI_OUT("FEN: " << FEN::getFEN(board))
    UCI_OUT("Current Turn: " << ((board.getTurn() == Color::WHITE) ? "White" : "Black"))
}

void UCI::fengen(std::istringstream& is)
{
    std::string startPosPath = ""; // Start position epd file path
    std::string outputPath   = ""; // Output path
    uint32_t numFens         = 0;  // Number of fens to generate
    uint32_t numThreads      = 0;  // Number of threads
    uint32_t depth           = 0;  // Search depth

    std::string token;
    while(is >> token)
    {
        if     (token == "positions")  is >> startPosPath;
        else if(token == "output")     is >> outputPath;
        else if(token == "numfens")    is >> numFens;
        else if(token == "numthreads") is >> numThreads;
        else if(token == "depth")      is >> depth;
        else WARNING("Unknown token: " << token)
    }

    // Validate input
    if(numFens <= 0)    { ERROR("Number of fens cannot be 0 or less")    return; }
    if(numThreads <= 0) { ERROR("Number of threads cannot be 0 or less") return; }
    if(depth <= 0)      { ERROR("Search depth cannot be 0 or less")      return; }
    if(startPosPath == "") { ERROR("Path to list positions cannot be empty") return; }
    if(outputPath   == "") { ERROR("Output path cannot be empty")            return; }

    Tuning::fengen(startPosPath, outputPath, numFens, numThreads, depth);
}

void UCI::train(std::istringstream& is)
{
    std::string dataset    = "";    // Dataset created by fengen
    std::string outputPath = "";    // Output path (_epoch.fnnue will be added as a suffix)
    uint32_t batchSize     = 0;     // Batch size
    uint32_t startEpoch    = 0;     // Epoch to start at (used for naming output and ADAM time variable)
    uint32_t endEpoch      = 0;     // Epoch to end at
    bool randomize         = false; // Create a new randomized net

    std::string token;
    while(is >> token)
    {
        if     (token == "dataset")    is >> dataset;
        else if(token == "output")     is >> outputPath;
        else if(token == "batchsize")  is >> batchSize;
        else if(token == "startepoch") is >> startEpoch;
        else if(token == "endepoch")   is >> endEpoch;
        else if(token == "randomize")  randomize = true;
        else WARNING("Unknown token: " << token)
    }

    if(dataset == "") { ERROR("Path to the dataset cannot be empty") return; }
    if(outputPath == "") { ERROR("Output path cannot be empty") return; }
    if(batchSize <= 0) { ERROR("Batch size cannot be 0 or less") return; }
    if(startEpoch < 1) { ERROR("Start epoch must be at least 1") return; }
    if(endEpoch <= startEpoch) { ERROR("End epoch must be larger than the end epoch") return; }

    Evaluator::nnue.train(dataset, outputPath, batchSize, startEpoch, endEpoch, randomize);
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
    UCI_OUT("train                                 - Train NNUE net")
    UCI_OUT("\tdataset <path>                      - Path to the dataset")
    UCI_OUT("\toutput <path>                       - Relative path to the output net. <epoch>.fnnue will be added as postfix")
    UCI_OUT("\tbatchsize <batchsize>               - Number of poisitions to process in each batch")
    UCI_OUT("\tstartepoch <epoch>                  - Epoch to start training. Epoch is used as timestep in ADAM optimizer")
    UCI_OUT("\tendepoch <epoch>                    - Epoch to stop training")
    UCI_OUT("\t[randomize]                         - Create a new randomly initialized net. Currently loaded net is used otherwise")
    UCI_OUT("fengen                                - Generate FENs used to train the NNUE")
    UCI_OUT("\tpositions <path>                    - Path to a file containing a list of stating positions")
    UCI_OUT("\toutput <path>                       - Path to the output file")
    UCI_OUT("\tnumfens <numfens>                   - Number of FENs to generate")
    UCI_OUT("\tnumthreads <numthreads>             - Number of threads to use")
    UCI_OUT("\tdepth <depth>                       - Search depth")
    UCI_OUT("\nFor more details, check out https://www.wbec-ridderkerk.nl/html/UCIProtocol.html")
}

void UCI::loop()
{
    UCI::newgame();

    LOG("Entering UCI loop")
    std::string token, cmd;
    do
    {
        if(!getline(std::cin, cmd))
            cmd = "quit";

        // Join the search thread if the search has finished
        if(!isSearching && searchThread.joinable())
            searchThread.join();

        // Convert the command to lower case
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return std::tolower(c); });

        DEBUG("UCI command: " << cmd)

        std::istringstream is(cmd);
        is >> std::skipws >> token;

        if      (token == "uci"       ) UCI::listUCI();
        else if (token == "setoption" ) UCI::setoption(is);
        else if (token == "go"        ) UCI::go(is);
        else if (token == "position"  ) UCI::position(is);
        else if (token == "ucinewgame") UCI::newgame();
        else if (token == "isready"   ) UCI::isready();
        else if (token == "stop"      ) UCI::stop();
        else if (token == "eval"      ) UCI::eval();
        else if (token == "d"         ) UCI::drawboard();
        else if (token == "fengen"    ) UCI::fengen(is);
        else if (token == "train"     ) UCI::train(is);
        else if (token == "help"      ) UCI::help();
    } while (token != "quit");

    TBFree();
    LOG("Exiting UCI loop")
}

void UCI::sendInfo(const SearchInfo& info)
{
    std::stringstream ss;

    ss << "info";
    if(info.depth > 0)
        ss << " depth " << std::setfill(' ') << std::setw(2) << info.depth;
    if(info.seldepth > 0)
        ss << " seldepth " << std::setfill(' ') << std::setw(2) << info.seldepth;
    if(info.msTime > 0)
        ss << " time " << std::setfill(' ') << std::setw(5) << info.msTime;
    else
        ss << " time " << std::setfill(' ') << std::setw(5) << 0;
    if(info.nodes > 0)
        ss << " nodes " << std::setfill(' ') << std::setw(9) << info.nodes;
    if(info.mate)
        ss << " score mate " << info.mateDistance;
    else
        ss << " score cp " << std::setfill(' ') << std::setw(5) << info.score;
    if(info.hashfull > 0)
        ss << " hashfull " << std::setfill(' ') << std::setw(3) << info.hashfull;
    else
        ss << " hashfull " << std::setfill(' ') << std::setw(3) << 0;
    if(info.nodes > 0 && info.msTime > 0)
        ss << " nps " << std::setfill(' ') << std::setw(7) << ((1000 * info.nodes) / info.msTime);
    else if(info.nodes > 0 && info.nsTime > 0)
        ss << " nps " << std::setfill(' ') << std::setw(7) << ((1000000000 * info.nodes) / info.nsTime);
    if(info.pvTable)
        ss << " pv " << info.pvTable->getPvLine();

    UCI_OUT(ss.str())
}

void UCI::sendBestMove(const Move& move)
{
    if(move == NULL_MOVE)
        ERROR("Illegal Null-Move was reported as the best move")

    UCI_OUT("bestmove " << move)
}