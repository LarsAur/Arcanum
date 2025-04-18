#include <test/seeTest.hpp>
#include <board.hpp>
#include <fen.hpp>

using namespace Arcanum::Benchmark;

// From: https://github.com/lithander/Leorik/blob/master/Leorik.Test/see.epd
std::string seeEDPs[] = {
    "6k1/1pp4p/p1pb4/6q1/3P1pRr/2P4P/PP1Br1P1/5RKN w - -; pm Rfxf4; ce -100;",
    "5rk1/1pp2q1p/p1pb4/8/3P1NP1/2P5/1P1BQ1P1/5RK1 b - -; pm Bxf4; ce 0;",
    "4R3/2r3p1/5bk1/1p1r3p/p2PR1P1/P1BK1P2/1P6/8 b - -; pm hxg4; ce 0;",
    "4R3/2r3p1/5bk1/1p1r1p1p/p2PR1P1/P1BK1P2/1P6/8 b - -; pm hxg4; ce 0;",
    "4r1k1/5pp1/nbp4p/1p2p2q/1P2P1b1/1BP2N1P/1B2QPPK/3R4 b - -; pm Bxf3; ce 0;",
    "2r1r1k1/pp1bppbp/3p1np1/q3P3/2P2P2/1P2B3/P1N1B1PP/2RQ1RK1 b - -; pm dxe5; ce 100;",
    "7r/5qpk/p1Qp1b1p/3r3n/BB3p2/5p2/P1P2P2/4RK1R w - -; pm Re8; ce 0;",
    "6rr/6pk/p1Qp1b1p/2n5/1B3p2/5p2/P1P2P2/4RK1R w - -; pm Re8; ce -500;",
    "7r/5qpk/2Qp1b1p/1N1r3n/BB3p2/5p2/P1P2P2/4RK1R w - -; pm Re8; ce -500;",
    "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -; pm f8=Q; ce 200;",
    "6RR/4bP2/8/8/5r2/3K4/5p2/4k3 w - -; pm f8=N; ce 200;",
    "7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -; pm f8=Q; ce 800;",
    "7R/5P2/8/8/6r1/3K4/5p2/4k3 w - -; pm f8=B; ce 200;",
    "7R/4bP2/8/8/1q6/3K4/5p2/4k3 w - -; pm f8=R; ce -100;",
    "8/4kp2/2npp3/1Nn5/1p2PQP1/7q/1PP1B3/4KR1r b - -; pm Rxf1+; ce 0;",
    "8/4kp2/2npp3/1Nn5/1p2P1P1/7q/1PP1B3/4KR1r b - -; pm Rxf1+; ce 0;",
    "2r2r1k/6bp/p7/2q2p1Q/3PpP2/1B6/P5PP/2RR3K b - -; pm Qxc1; ce 100;",
    "r2qk1nr/pp2ppbp/2b3p1/2p1p3/8/2N2N2/PPPP1PPP/R1BQR1K1 w kq -; pm Nxe5; ce 100;",
    "6r1/4kq2/b2p1p2/p1pPb3/p1P2B1Q/2P4P/2B1R1P1/6K1 w - -; pm Bxe5; ce 0;",
    "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R4B/PQ3P1P/3R2K1 w - h6; pm gxh6; ce 0;",
    "3q2nk/pb1r1p2/np6/3P2Pp/2p1P3/2R1B2B/PQ3P1P/3R2K1 w - h6; pm gxh6; ce 100;",
    "2r4r/1P4pk/p2p1b1p/7n/BB3p2/2R2p2/P1P2P2/4RK2 w - -; pm Rxc8; ce 500;",
    "2r5/1P4pk/p2p1b1p/5b1n/BB3p2/2R2p2/P1P2P2/4RK2 w - -; pm Rxc8; ce 500;",
    "2r4k/2r4p/p7/2b2p1b/4pP2/1BR5/P1R3PP/2Q4K w - -; pm Rxc5; ce 300;",
    "8/pp6/2pkp3/4bp2/2R3b1/2P5/PP4B1/1K6 w - -; pm Bxc6; ce -200;",
    "4q3/1p1pr1k1/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -; pm Rxe4; ce -400;",
    "4q3/1p1pr1kb/1B2rp2/6p1/p3PP2/P3R1P1/1P2R1K1/4Q3 b - -; pm Bxe4; ce 100;",
    "3r3k/3r4/2n1n3/8/3p4/2PR4/1B1Q4/3R3K w - -; pm Rxd4; ce -100;",
    "1k1r4/1ppn3p/p4b2/4n3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -; pm Nxe5; ce 100;",
    "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -; pm Nxe5; ce -200;",
    "rnb2b1r/ppp2kpp/5n2/4P3/q2P3B/5R2/PPP2PPP/RN1QKB2 w Q -; pm Bxf6; ce 100;",
    "r2q1rk1/2p1bppp/p2p1n2/1p2P3/4P1b1/1nP1BN2/PP3PPP/RN1QR1K1 b - -; pm Bxf3; ce 0;",
    "r1bqkb1r/2pp1ppp/p1n5/1p2p3/3Pn3/1B3N2/PPP2PPP/RNBQ1RK1 b kq -; pm Nxd4; ce 0;",
    "r1bq1r2/pp1ppkbp/4N1p1/n3P1B1/8/2N5/PPP2PPP/R2QK2R w KQ -; pm Nxg7; ce 0;",
    "r1bq1r2/pp1ppkbp/4N1pB/n3P3/8/2N5/PPP2PPP/R2QK2R w KQ -; pm Nxg7; ce 300;",
    "rnq1k2r/1b3ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -; pm Bxh2; ce -200;",
    "rn2k2r/1bq2ppp/p2bpn2/1p1p4/3N4/1BN1P3/PPP2PPP/R1BQR1K1 b kq -; pm Bxh2; ce 100;",
    "r2qkbn1/ppp1pp1p/3p1rp1/3Pn3/4P1b1/2N2N2/PPP2PPP/R1BQKB1R b KQq -; pm Bxf3; ce 100;",
    "rnbq1rk1/pppp1ppp/4pn2/8/1bPP4/P1N5/1PQ1PPPP/R1B1KBNR b KQ -; pm Bxc3; ce 0;",
    "r4rk1/3nppbp/bq1p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -; pm Qxb2; ce -800;",
    "r4rk1/1q1nppbp/b2p1np1/2pP4/8/2N2NPP/PP2PPB1/R1BQR1K1 b - -; pm Nxd5; ce -200;",
    "1r3r2/5p2/4p2p/2k1n1P1/2PN1nP1/1P3P2/8/2KR1B1R b - -; pm Rxb3; ce -400;",
    "1r3r2/5p2/4p2p/4n1P1/kPPN1nP1/5P2/8/2KR1B1R b - -; pm Rxb4; ce 100;",
    "2r2rk1/5pp1/pp5p/q2p4/P3n3/1Q3NP1/1P2PP1P/2RR2K1 b - -; pm Rxc1; ce 0;",
    "5rk1/5pp1/2r4p/5b2/2R5/6Q1/R1P1qPP1/5NK1 b - -; pm Bxc2; ce -100;",
    "1r3r1k/p4pp1/2p1p2p/qpQP3P/2P5/3R4/PP3PP1/1K1R4 b - -; pm Qxa2; ce -800;",
    "1r5k/p4pp1/2p1p2p/qpQP3P/2P2P2/1P1R4/P4rP1/1K1R4 b - -; pm Qxa2; ce 100;",
    "r2q1rk1/1b2bppp/p2p1n2/1ppNp3/3nP3/P2P1N1P/BPP2PP1/R1BQR1K1 w - -; pm Nxe7; ce 0;",
    "rnbqrbn1/pp3ppp/3p4/2p2k2/4p3/3B1K2/PPP2PPP/RNB1Q1NR w - -; pm Bxe4; ce 100;",
    "rnb1k2r/p3p1pp/1p3p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -; pm Nd6; ce -200;",
    "r1b1k2r/p4npp/1pp2p1b/7n/1N2N3/3P1PB1/PPP1P1PP/R2QKB1R w KQkq -; pm Nd6; ce 0;",
    "2r1k2r/pb4pp/5p1b/2KB3n/4N3/2NP1PB1/PPP1P1PP/R2Q3R w k -; pm Bc6; ce -300;",
    "2r1k2r/pb4pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w k -; pm Bc6; ce 0;",
    "2r1k3/pbr3pp/5p1b/2KB3n/1N2N3/3P1PB1/PPP1P1PP/R2Q3R w - -; pm Bc6; ce -300;",
    "5k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -; pm d8=Q; ce 800;",
    "r4k2/p2P2pp/8/1pb5/1Nn1P1n1/6Q1/PPP4P/R3K1NR w KQ -; pm d8=Q; ce -100;",
    "5k2/p2P2pp/1b6/1p6/1Nn1P1n1/8/PPP4P/R2QK1NR w KQ -; pm d8=Q; ce 200;",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PP1PPPPP/RNBQKBNR w KQk -; pm c8=Q; ce -100;",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -; pm c8=Q; ce 200;",
    "4kbnr/p1P1pppp/b7/4q3/7n/8/PPQPPPPP/RNB1KBNR w KQk -; pm c8=Q; ce 200;",
    "4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6; pm gxf6; ce 0;",
    "4kbnr/p1P4p/b1q5/5pP1/4n3/5Q2/PP1PPP1P/RNB1KBNR w KQk f6; pm gxf6;	ce 0;",
    "4kbnr/p1P4p/b1q5/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk f6; pm gxf6; ce 0;",
    "1n2kb1r/p1P4p/2qb4/5pP1/4n2Q/8/PP1PPP1P/RNB1KBNR w KQk -; pm cxb8=Q; ce 200;",
    "rnbqk2r/pp3ppp/2p1pn2/3p4/3P4/N1P1BN2/PPB1PPPb/R2Q1RK1 w kq -; pm Kxh2; ce 300;",
    "3N4/2K5/2n5/1k6/8/8/8/8 b - -; pm Nxd8; ce 0;",
    "3N4/2P5/2n5/1k6/8/8/8/4K3 b - -; pm Nxd8; ce -800;",
    "3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -; pm Qxd8; ce 300;",
    "3n3r/2P5/8/1k6/8/8/3Q4/4K3 w - -; pm cxd8=Q; ce 700;",
    "r2n3r/2P1P3/4N3/1k6/8/8/8/4K3 w - -; pm Nxd8; ce 300;",
    "8/8/8/1k6/6b1/4N3/2p3K1/3n4 w - -; pm Nxd1; ce 0;",
    "8/8/1k6/8/8/2N1N3/2p1p1K1/3n4 w - -; pm Nxd1; ce -800;",
    "8/8/1k6/8/8/2N1N3/4p1K1/3n4 w - -; pm Ncxd1; ce 100;",
    "r1bqk1nr/pppp1ppp/2n5/1B2p3/1b2P3/5N2/PPPP1PPP/RNBQK2R w KQkq -; pm O-O; ce 0;",
};

bool SeeTest::m_testPosition(std::string fen, Move move, bool expected)
{
    Board board = Board(fen, false);
    board.getLegalMoves();
    bool seeScore = board.see(move);

    if(seeScore != expected)
    {
        FAIL("SEE test for " << move << " in " << fen << " got: " << seeScore << " expected: " << expected)
    }
    else
    {
        SUCCESS("SEE test for " << move << " in " << fen << " got: " << seeScore)
    }

    return seeScore == expected;
}

void SeeTest::runSeeTest()
{
    int32_t successes = 0;
    constexpr uint32_t numTests = sizeof(seeEDPs) / sizeof(seeEDPs[0]);

    for(uint32_t i = 0; i < numTests; i++)
    {
        EDP edp = FEN::parseEDP(seeEDPs[i]);
        successes += m_testPosition(edp.fen, edp.pm, edp.ce >= 0);
    }

    if(numTests != successes)
        FAIL("Completed SEE test. Successes: " << successes << " / " << numTests)
    else
        SUCCESS("Completed SEE test. Successes: " << successes << " / " << numTests)
}
