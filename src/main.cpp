#include <uci/uci.hpp>
#include <argsparser.hpp>
#include <eval.hpp>
#include <bitboardlookups.hpp>
#include <zobrist.hpp>

using namespace Arcanum;

int main(int argc, char *argv[])
{
    BitboardLookups::generateBitboardLookups();
    Zobrist::init();
    Evaluator::nnue.load(Interface::UCI::optionNNUEPath.value);

    if(argc == 1)
    {
        Interface::UCI::loop();
        return EXIT_SUCCESS;
    }

    if(!ArgsParser::parseArgumentsAndRunCommand(argc, argv))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}