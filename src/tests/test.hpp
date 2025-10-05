#pragma once

namespace Arcanum::Test
{
    bool parseArgumentsAndRunTests(int argc, char* argv[]);

    bool runBinpackTest();
    bool runSelfplayTest();
    bool runSeeTest();
    bool runEngineTest();
    bool runPerftTest();
    bool runZobristTest();
    bool runCaptureTest();
    bool runDrawTest();
}