#include <argsparser.hpp>
#include <utils.hpp>
#include <tuning/fengen.hpp>
#include <tuning/nnuetrainer.hpp>
#include <tuning/datamerger.hpp>
#include <tests/test.hpp>

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
        INFO("Offset:            " << params.offset)
        INFO("Num fens:          " << params.numFens)
        INFO("Num threads:       " << params.numThreads)
        INFO("Depth:             " << params.depth)
        INFO("Movetime (ms):     " << params.movetime)
        INFO("Nodes:             " << params.nodes)

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
    params.epochSize      = 100'000'000; // Number of positions in each epoch
    params.validationSize = 0;           // Size of the validation set
    params.alpha          = 0.001f;      // Learning rate
    params.lambda         = 1.0f;        // Weighting between wdlTarget and cpTarget in loss function 1.0 = 100% cpTarget 0.0 = 100% wdlTarget
    params.gamma          = 1.0f;        // Scaling of learning rate over epochs
    params.gammaSteps     = 1;           // How often to apply gamma scaling

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

        ERROR("Unknown argument: " << argv[index])
        return false;
    }

    // Validate input
    bool valid = true;

    if(params.dataset == "")
    { valid = false; ERROR("Path to the dataset cannot be empty") }

    if(params.output == "")
    { valid = false; ERROR("Output path cannot be empty") }

    if(params.batchSize <= 0)
    { valid = false; ERROR("Batch size cannot be 0 or less") }

    if(params.endEpoch <= params.startEpoch)
    { valid = false; ERROR("End epoch must be larger than the end epoch") }

    if(params.epochSize <= 0)
    { valid = false; ERROR("Epoch size has to be larger than 0") }

    if(params.gammaSteps <= 0)
    { valid = false; ERROR("GammaSteps has to be larger than 1. Use Gamma=1 to disable gamma scaling") }

    if((params.lambda < 0) || (params.lambda > 1))
    { valid = false; ERROR("Lambda has to be between 0 and 1 (inclusive)") }

    if(valid)
    {
        INFO("Starting NNUE trainer with parameters:")
        INFO("Dataset:           " << params.dataset)
        INFO("Output:            " << params.output)
        INFO("Initial net:       " << params.initialNet)
        INFO("Batch size:        " << params.batchSize)
        INFO("Start epoch:       " << params.startEpoch)
        INFO("End epoch:         " << params.endEpoch)
        INFO("Epoch size:        " << params.epochSize)
        INFO("Validation size:   " << params.validationSize)
        INFO("Alpha:             " << params.alpha)
        INFO("Lambda:            " << params.lambda)
        INFO("Gamma:             " << params.gamma)
        INFO("Gamma steps:       " << params.gammaSteps)

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