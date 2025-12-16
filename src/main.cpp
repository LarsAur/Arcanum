#include <uci/uci.hpp>
#include <tests/test.hpp>
#include <tuning/fengen.hpp>
#include <eval.hpp>
#include <bitboardlookups.hpp>

using namespace Arcanum;

int main(int argc, char *argv[])
{
    BitboardLookups::generateBitboardLookups();

    Evaluator::nnue.load(Interface::UCI::optionNNUEPath.value);

    if(argc == 1)
    {
        Interface::UCI::loop();
        return EXIT_SUCCESS;
    }

    if(Fengen::parseArgumentsAndRunFengen(argc, argv))
    {
        return EXIT_SUCCESS;
    }

    if(!Test::parseArgumentsAndRunTests(argc, argv))
    {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}