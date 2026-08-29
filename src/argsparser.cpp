#include <argsparser.hpp>
#include <utils.hpp>
#include <tuning/fengen.hpp>
#include <tuning/nnuetrainer.hpp>
#include <tuning/datamerger.hpp>
#include <tuning/postprocessing.hpp>
#include <tests/test.hpp>
#include <tuning/analysis.hpp>

using namespace Arcanum;

bool ArgsParser::parseArgumentsAndRunCommand(int argc, char* argv[])
{
    if(argc < 2)
    {
        WARNING("No command provided to argument parser")
        return false;
    }

    std::string command = std::string(argv[1]);
    toLowerCase(command);

    if(command == "fengen")
    {
        return parseArgumentsAndRunFengen(argc, argv);
    }
    else if(command == "train")
    {
        return parseArgumentsAndRunNnueTrainer(argc, argv);
    }
    else if(command == "test")
    {
        return Test::parseArgumentsAndRunTests(argc, argv);
    }
    else if(command == "merge")
    {
        return parseArgumentsAndMergeData(argc, argv);
    }
    else if(command == "reeval")
    {
        return parseArgumentsAndReeval(argc, argv);
    }
    else if(command == "quiesce")
    {
        return parseArgumentsAndQuiesce(argc, argv);
    }
    else if(command == "filter")
    {
        return parseArgumentsAndFilter(argc, argv);
    }
    else if(command == "analyse")
    {
        return parseArgumentsAndAnalyse(argc, argv);
    }
    else if (command == "deduplicate")
    {
        return parseArgumentsAndDeduplicate(argc, argv);
    }

    INFO("Unknown command: " << command)

    return false;
}

bool ArgsParser::parseArgumentsAndRunFengen(int argc, char* argv[])
{
    FengenParameters params;

    int index = 2; // Skip the executable name and command

    while(index < argc)
    {
        if(matchAndParseArg("--positions",      params.startposPath,   argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",         params.outputPath,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--syzygypath",     params.syzygyPath,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--numrandommoves", params.numRandomMoves, argc, argv, index)) { continue; }
        if(matchAndParseArg("--numfens",        params.numFens,        argc, argv, index)) { continue; }
        if(matchAndParseArg("--numthreads",     params.numThreads,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--depth",          params.depth,          argc, argv, index)) { continue; }
        if(matchAndParseArg("--movetime",       params.movetime,       argc, argv, index)) { continue; }
        if(matchAndParseArg("--nodes",          params.nodes,          argc, argv, index)) { continue; }
        if(matchAndParseArg("--offset",         params.offset,         argc, argv, index)) { continue; }
        if(matchAndParseArg("--scorelimit",     params.scoreLimit,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--resigneval",     params.resignEval,     argc, argv, index)) { params.resignEnabled = true; continue; }
        if(matchAndParseArg("--resignrepeats",  params.resignRepeats,  argc, argv, index)) { params.resignEnabled = true; continue; }
        if(matchAndParseArg("--resignmoves",    params.resignMoves,    argc, argv, index)) { params.resignEnabled = true; continue; }
        if(matchAndParseArg("--draweval",       params.drawEval,       argc, argv, index)) { params.drawEnabled = true; continue; }
        if(matchAndParseArg("--drawrepeats",    params.drawRepeats,    argc, argv, index)) { params.drawEnabled = true; continue; }
        if(matchAndParseArg("--drawmoves",      params.drawMoves,      argc, argv, index)) { params.drawEnabled = true; continue; }
        if(matchAndParseArg("--ttsize",         params.ttSize,         argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    // Validate input
    bool valid = true;

    if(params.numFens <= 0)
    { valid = false; INFO("Number of fens cannot be 0 or less") }

    if(params.numThreads <= 0)
    { valid = false; INFO("Number of threads cannot be 0 or less") }

    if(params.startposPath == "" && params.numRandomMoves == 0)
    { valid = false; INFO("numrandommoves cannot be 0 when there is no path to edp file with starting positions") }

    if(params.outputPath == "")
    { valid = false; INFO("Output path cannot be empty")            }

    if(params.depth == 0 && params.movetime == 0 && params.nodes == 0)
    { valid = false; INFO("Search depth, movetime and nodes cannot be 0 at the same time") }

    if(valid)
    {
        INFO("Starting fengen with parameters:")
        INFO("Startpos path:     " << params.startposPath)
        INFO("Output path:       " << params.outputPath)
        INFO("Syzygy path:       " << params.syzygyPath)
        INFO("Num random moves:  " << params.numRandomMoves)
        INFO("Randomize limit:   " << params.scoreLimit)
        INFO("Offset:            " << params.offset)
        INFO("Num fens:          " << params.numFens)
        INFO("Num threads:       " << params.numThreads)
        INFO("Depth:             " << params.depth)
        INFO("Movetime (ms):     " << params.movetime)
        INFO("Nodes:             " << params.nodes)
        INFO("TT size (MB):      " << params.ttSize)
        INFO("Resign enabled:    " << (params.resignEnabled ? "true" : "false"))
        if(params.resignEnabled)
        {
            INFO("Resign eval:       " << params.resignEval)
            INFO("Resign repeats:    " << params.resignRepeats)
            INFO("Resign moves:      " << params.resignMoves)
        }
        INFO("Draw enabled:      " << (params.drawEnabled ? "true" : "false"))
        if(params.drawEnabled)
        {
            INFO("Draw eval:         " << params.drawEval)
            INFO("Draw repeats:      " << params.drawRepeats)
            INFO("Draw moves:        " << params.drawMoves)
        }

        Fengen::start(params);
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndRunNnueTrainer(int argc, char* argv[])
{
    TrainingParameters params;

    // Set default values for training parameters
    params.dataset        = "";          // Path to the dataset
    params.output         = "";          // Path to the output net. "<epoch>.fnnue" is appended to the name
    params.initialNet     = "";          // Path to the initial net. Randomized if not set
    params.batchSize      = 20000;       // Batch size
    params.startEpoch     = 0;           // Epoch to start at (used for naming output LR scaling)
    params.endEpoch       = INT32_MAX;   // Epoch to end at. Runs for INT32_MAX epochs if not set.
    params.epochSize      = 0;           // Number of positions in each epoch
    params.useFullDataset = true;        // Whether to use the full dataset or only epochSize positions per epoch
    params.validationSize = 0;           // Size of the validation set
    params.alpha          = 0.001f;      // Learning rate
    params.lambda         = 1.0f;        // Weighting between wdlTarget and cpTarget in loss function 1.0 = 100% cpTarget 0.0 = 100% wdlTarget
    params.gamma          = 1.0f;        // Scaling of learning rate over epochs
    params.gammaSteps     = 1;           // How often to apply gamma scaling
    params.filter         = false;       // If true, checked positions, positions with captures as best move, or positions with very high evals are filtered out.
    params.numThreads     = 1;           // Number of threads to use for training. Each thread will have its own copy of gradients and traces. The batch size is divided between the threads.

    int index = 2; // Skip the executable name and command

    while(index < argc)
    {
        if(matchAndParseArg("--dataset",        params.dataset,         argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",         params.output,          argc, argv, index)) { continue; }
        if(matchAndParseArg("--initialnet",     params.initialNet,      argc, argv, index)) { continue; }
        if(matchAndParseArg("--batchsize",      params.batchSize,       argc, argv, index)) { continue; }
        if(matchAndParseArg("--startepoch",     params.startEpoch,      argc, argv, index)) { continue; }
        if(matchAndParseArg("--endepoch",       params.endEpoch,        argc, argv, index)) { continue; }
        if(matchAndParseArg("--epochsize",      params.epochSize,       argc, argv, index)) { continue; }
        if(matchAndParseArg("--validationsize", params.validationSize,  argc, argv, index)) { continue; }
        if(matchAndParseArg("--alpha",          params.alpha,           argc, argv, index)) { continue; }
        if(matchAndParseArg("--lambda",         params.lambda,          argc, argv, index)) { continue; }
        if(matchAndParseArg("--gamma",          params.gamma,           argc, argv, index)) { continue; }
        if(matchAndParseArg("--gammasteps",     params.gammaSteps,      argc, argv, index)) { continue; }
        if(matchAndParseArg("--filter",         params.filter,          argc, argv, index)) { continue; }
        if(matchAndParseArg("--numthreads",     params.numThreads,      argc, argv, index)) { continue; }

        ERROR("Unknown argument: " << argv[index])
        return false;
    }

    params.useFullDataset = (params.epochSize == 0);

    // Validate input
    bool valid = true;

    if(params.dataset == "")
    { valid = false; INFO("Path to the dataset cannot be empty") }

    if(params.output == "")
    { valid = false; INFO("Output path cannot be empty") }

    if(params.batchSize <= 0)
    { valid = false; INFO("Batch size cannot be 0 or less") }

    if(params.endEpoch <= params.startEpoch)
    { valid = false; INFO("End epoch must be larger than the start epoch") }

    if(params.gammaSteps <= 0)
    { valid = false; INFO("GammaSteps has to be larger than 1. Use Gamma=1 to disable gamma scaling") }

    if((params.lambda < 0) || (params.lambda > 1))
    { valid = false; INFO("Lambda has to be between 0 and 1 (inclusive)") }

    if(params.numThreads <= 0)
    { valid = false; INFO("Number of threads cannot be 0 or less") }

    if(valid)
    {
        INFO("Starting NNUE trainer with parameters:")
        INFO("Dataset:           " << params.dataset)
        INFO("Output:            " << params.output)
        INFO("Initial net:       " << params.initialNet)
        INFO("Batch size:        " << params.batchSize)
        INFO("Start epoch:       " << params.startEpoch)
        INFO("End epoch:         " << params.endEpoch)
        INFO("Epoch size:        " << (params.useFullDataset ? "Full dataset" : std::to_string(params.epochSize)))
        INFO("Validation size:   " << params.validationSize)
        INFO("Alpha:             " << params.alpha)
        INFO("Lambda:            " << params.lambda)
        INFO("Gamma:             " << params.gamma)
        INFO("Gamma steps:       " << params.gammaSteps)
        INFO("Filter positions:  " << (params.filter ? "true" : "false"))
        INFO("Num threads:       " << params.numThreads)

        NNUETrainer trainer;
        trainer.train(params);
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndMergeData(int argc, char* argv[])
{
    DataMerger merger;
    std::string path;

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input",  path, argc, argv, index))  { merger.addInputPath(path);   continue; }
        if(matchAndParseArg("--output", path,  argc, argv, index)) { merger.setOutputPath(path);  continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    return merger.mergeData();
}

bool ArgsParser::parseArgumentsAndReeval(int argc, char* argv[])
{
    PostProcessing::ReEvalParameters params = PostProcessing::ReEvalParameters();

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input",      params.inputPath,  argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",     params.outputPath, argc, argv, index)) { continue; }
        if(matchAndParseArg("--numthreads", params.numThreads, argc, argv, index)) { continue; }
        if(matchAndParseArg("--depth",      params.depth,      argc, argv, index)) { continue; }
        if(matchAndParseArg("--nodes",      params.nodes,      argc, argv, index)) { continue; }
        if(matchAndParseArg("--movetime",   params.movetime,   argc, argv, index)) { continue; }
        if(matchAndParseArg("--offset",     params.offset,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--ttsize",     params.ttSize,     argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    bool valid = true;

    if(params.inputPath == "")
    { valid = false; INFO("Input path cannot be empty") }

    if(params.outputPath == "")
    { valid = false; INFO("Output path cannot be empty") }

    if(params.numThreads <= 0)
    { valid = false; INFO("Number of threads cannot be 0 or less") }

    if(params.depth == 0 && params.movetime == 0 && params.nodes == 0)
    { valid = false; INFO("Search depth, movetime and nodes cannot be 0 at the same time") }

    if(valid)
    {
        INFO("Starting re-evaluation with parameters:")
        INFO("Input path:        " << params.inputPath)
        INFO("Output path:       " << params.outputPath)
        INFO("Num threads:       " << params.numThreads)
        INFO("Depth:             " << params.depth)
        INFO("Movetime (ms):     " << params.movetime)
        INFO("Nodes:             " << params.nodes)
        INFO("Offset:            " << params.offset)
        INFO("TT size:           " << params.ttSize)

        PostProcessing::reeval(params);
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndQuiesce(int argc, char* argv[])
{
    PostProcessing::QuiesceParameters params = PostProcessing::QuiesceParameters();

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input",      params.inputPath,  argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",     params.outputPath, argc, argv, index)) { continue; }
        if(matchAndParseArg("--numthreads", params.numThreads, argc, argv, index)) { continue; }
        if(matchAndParseArg("--offset",     params.offset,     argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    bool valid = true;

    if(params.inputPath == "")
    { valid = false; INFO("Input path cannot be empty") }

    if(params.outputPath == "")
    { valid = false; INFO("Output path cannot be empty") }

    if(params.numThreads <= 0)
    { valid = false; INFO("Number of threads cannot be 0 or less") }

    if(valid)
    {
        INFO("Starting quiescing with parameters:")
        INFO("Input path:        " << params.inputPath)
        INFO("Output path:       " << params.outputPath)
        INFO("Num threads:       " << params.numThreads)
        INFO("Offset:            " << params.offset)

        PostProcessing::quiesce(params);
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndAnalyse(int argc, char* argv[])
{
    Analyser analyser;

    std::string inputPath;

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input", inputPath, argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    bool valid = true;

    if(inputPath == "")
    { valid = false; INFO("Input path cannot be empty") }

    if(valid)
    {
        INFO("Starting analysis with parameters:")
        INFO("Input path:        " << inputPath)

        analyser.analyseDataset(inputPath);
        analyser.printResults();
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndDeduplicate(int argc, char* argv[])
{
    PostProcessing::DeduplicateParameters params = PostProcessing::DeduplicateParameters();

    params.buckets = 0; // Default value, must be set by user
    params.startBucket = 0; // Default value, can be set by user

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input",   params.inputPath,  argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",  params.outputPath, argc, argv, index)) { continue; }
        if(matchAndParseArg("--buckets", params.buckets,    argc, argv, index)) { continue; }
        if(matchAndParseArg("--start",   params.startBucket, argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    bool valid = true;

    if(params.inputPath == "")
    { valid = false; INFO("Input path cannot be empty") }

    if(params.outputPath == "")
    { valid = false; INFO("Output path cannot be empty") }

    if(params.buckets <= 0)
    { valid = false; INFO("Number of buckets cannot be 0 or less") }

    if(params.startBucket >= params.buckets)
    { valid = false; INFO("Start bucket must be between 0 and buckets-1") }

    if(valid)
    {
        INFO("Starting deduplication with parameters:")
        INFO("Input path:        " << params.inputPath)
        INFO("Output path:       " << params.outputPath)
        INFO("Buckets:           " << params.buckets)
        INFO("Start bucket:      " << params.startBucket)

        PostProcessing::deduplicate(params);
    }

    return valid;
}

bool ArgsParser::parseArgumentsAndFilter(int argc, char* argv[])
{
    PostProcessing::FilterParameters params = PostProcessing::FilterParameters();

    int index = 2; // Skip the executable name and command
    while(index < argc)
    {
        if(matchAndParseArg("--input",        params.inputPath,        argc, argv, index)) { continue; }
        if(matchAndParseArg("--output",       params.outputPath,       argc, argv, index)) { continue; }
        if(matchAndParseArg("--offset",       params.offset,           argc, argv, index)) { continue; }
        if(matchAndParseArg("--staticmargin", params.staticMargin,     argc, argv, index)) { params.filterStaticMargin = true; continue; }
        if(matchAndParseArg("--maxhalfmoves", params.maxHalfMoves,     argc, argv, index)) { params.filterMaxHalfMoves = true; continue; }
        if(matchAndParseArg("--maxeval",      params.maxEval,          argc, argv, index)) { params.filterMaxEval = true;      continue; }
        if(matchAndParseArg("--minpieces",    params.minPieces,        argc, argv, index)) { params.filterMinPieces = true;    continue; }
        if(matchAndParseArg("--captures",     params.filterCaptures,   argc, argv, index)) { continue; }
        if(matchAndParseArg("--checks",       params.filterChecks,     argc, argv, index)) { continue; }
        if(matchAndParseArg("--singlemove",   params.filterSingleMove, argc, argv, index)) { continue; }

        INFO("Unknown argument: " << argv[index])
        return false;
    }

    bool valid = true;

    if(params.inputPath == "")
    { valid = false; INFO("Input path cannot be empty") }

    if(params.outputPath == "")
    { valid = false; INFO("Output path cannot be empty") }

    if(valid)
    {
        INFO("Starting filtering with parameters:")
        INFO("Input path:        " << params.inputPath)
        INFO("Output path:       " << params.outputPath)
        INFO("Static margin:     " << params.staticMargin << " " << (params.filterStaticMargin ? "enabled" : "disabled"))
        INFO("Max half moves:    " << params.maxHalfMoves << " " << (params.filterMaxHalfMoves ? "enabled" : "disabled"))
        INFO("Max eval:          " << params.maxEval      << " " << (params.filterMaxEval ?      "enabled" : "disabled"))
        INFO("Min pieces:        " << params.minPieces    << " " << (params.filterMinPieces ?    "enabled" : "disabled"))
        INFO("Filter captures:   " << (params.filterCaptures ? "true" : "false"))
        INFO("Filter checks:     " << (params.filterChecks ? "true" : "false"))
        INFO("Filter single move:" << (params.filterSingleMove ? "true" : "false"))
        INFO("Offset:            " << params.offset)

        PostProcessing::filter(params);
    }

    return valid;
}