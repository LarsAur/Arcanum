#include <tests/test.hpp>
#include <tuning/binpack.hpp>
#include <tuning/gamerunner.hpp>
#include <board.hpp>
#include <search.hpp>
#include <fen.hpp>
#include <random>

using namespace Arcanum;

constexpr char filename[] = "test_binpack.binpack";

static bool compareChunkAfterEncodeDecode(
    std::vector<std::string> fens,
    std::vector<std::vector<Move>> moves,
    std::vector<std::vector<eval_t>> scores,
    std::vector<GameResult> results
)
{
    BinpackEncoder encoder;
    BinpackParser parser;
    const uint32_t numGames = results.size();

    encoder.open(filename);
    for(uint32_t i = 0; i < numGames; i++)
    {
        Board board = Board(fens[i]);
        encoder.addGame(board, moves[i], scores[i], results[i]);
    }
    encoder.close();

    bool pass = true;
    parser.open(filename);

    for(uint32_t i = 0; i < numGames; i++)
    {
        Board encodedBoard = Board(fens[i]);
        const uint32_t numMoves = moves[i].size();
        for(uint32_t j = 0; j < numMoves; j++)
        {
            Board *parsedBoard = parser.getNextBoard();

            if(parsedBoard->fen() != encodedBoard.fen())
            {
                pass = false;
                FAIL("BinpackTest: FEN[" << i << "][" << j << "] " << parsedBoard->fen() << " != " << encodedBoard.fen())
                break;
            }

            if(parser.getResult() != results[i])
            {
                pass = false;
                FAIL("BinpackTest: Result[" << i << "]" << parser.getResult() << " != " << results[i])
                break;
            }

            if(parser.getScore() != scores[i][j])
            {
                pass = false;
                FAIL("BinpackTest: Score[" << i << "][" << j << "] " << parser.getScore() << " != " << scores[i][j])
                break;
            }

            encodedBoard.performMove(moves[i][j]);
        }

        if(!pass)
        {
            break;
        }
    }

    parser.close();
    std::remove(filename);

    return pass;
}

static bool compareAfterEncodeDecode(
    std::string fen,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
    GameResult result
)
{
    BinpackEncoder encoder;
    BinpackParser parser;
    Board encodedBoard = Board(fen);

    encoder.open(filename);
    encoder.addGame(encodedBoard, moves, scores, result);
    encoder.close();

    bool pass = true;
    parser.open(filename);
    for(uint32_t i = 0; i < moves.size(); i++)
    {
        Board *parsedBoard = parser.getNextBoard();

        if(parsedBoard->fen() != encodedBoard.fen())
        {
            pass = false;
            FAIL("BinpackTest: FEN[" << i << "]" << parsedBoard->fen() << " != " << encodedBoard.fen())
            break;
        }

        if(parser.getResult() != result)
        {
            pass = false;
            FAIL("BinpackTest: Result[" << i << "]" << parser.getResult() << " != " << result)
            break;
        }

        if(parser.getScore() != scores[i])
        {
            pass = false;
            FAIL("BinpackTest: Score[" << i << "]" << parser.getScore() << " != " << scores[i])
            break;
        }

        encodedBoard.performMove(moves[i]);
    }

    parser.close();
    std::remove(filename);

    return pass;
}

static void generateRandomGame(
    std::string& fen,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
    GameResult& result,
    std::mt19937& generator
)
{
    std::uniform_int_distribution<uint64_t> distributionU64(0, UINT64_MAX);
    std::uniform_int_distribution<int16_t>  distributionI16(-INT16_MAX, INT16_MAX);

    fen = FEN::startpos;
    Board board = Board(FEN::startpos);

    result = GameResult::DRAW;

    while(board.hasLegalMove())
    {
        Move* legalMoves = board.getLegalMoves();
        uint8_t numLegalMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();

        // Select random move and score
        Move move = legalMoves[distributionU64(generator) % numLegalMoves];
        eval_t score = distributionI16(generator);

        moves.push_back(move);
        scores.push_back(score);

        board.performMove(move);

        if(moves.size() >= 300)
        {
            break;
        }
    }

    if(!board.hasLegalMove() && board.isChecked())
    {
        if(board.getTurn() == Color::WHITE)
        {
            result = GameResult::BLACK_WIN;
        }
        else
        {
            result = GameResult::WHITE_WIN;
        }
    }
}

bool Test::runBinpackTest()
{
    bool passed = true;

    {
        std::string fen = FEN::startpos;
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;
        GameRunner runner;
        Searcher wSearcher(false);
        Searcher bSearcher(false);
        SearchParameters params;

        params.useDepth = true;
        params.depth = 10;
        runner.setSearchers(&wSearcher, &bSearcher);
        runner.setSearchParameters(params);

        // -- Play a game and try to encode and decode it.

        TESTINFO("Testing played game")
        runner.play(fen, &moves, &scores, &result);
        if(!compareAfterEncodeDecode(fen, moves, scores, result))
        {
            FAIL("Error encountered when encoding and decoding played game using binpack")
            passed = false;
        }
        else
        {
            SUCCESS("Encoded and decoded played game correctly using binpack")
        }
    }

    // Play a number of random games and try to encode and decode it.

    {
        constexpr uint32_t NumRandomGames = 100;
        std::string fen;
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;
        std::mt19937 generator(0);

        bool pass = true;
        TESTINFO("Testing " << NumRandomGames << " random games")
        for(uint32_t i = 0; i < NumRandomGames; i++)
        {
            generateRandomGame(fen, moves, scores, result, generator);
            if(!compareAfterEncodeDecode(fen, moves, scores, result))
            {
                pass = false;
                FAIL("Error encountered when encoding and decoding random game using binpack")
                passed = false;
                break;
            }

            moves.clear();
            scores.clear();
        }

        if(pass)
        {
            SUCCESS("Encoded and decoded " << NumRandomGames << " random games using binpack")
        }
    }

    // Play multiple games to create a binpack chunk

    {
        constexpr uint32_t NumChunkGames = 1000;
        std::vector<std::string> fens;
        std::vector<std::vector<Move>> moves;
        std::vector<std::vector<eval_t>> scores;
        std::vector<GameResult> results;
        std::mt19937 generator(0);

        fens.resize(NumChunkGames);
        moves.resize(NumChunkGames);
        scores.resize(NumChunkGames);
        results.resize(NumChunkGames);

        TESTINFO("Testing binpack chunks")
        for(uint32_t i = 0; i < NumChunkGames; i++)
        {
            generateRandomGame(fens[i], moves[i], scores[i], results[i], generator);
        }

        if(!compareChunkAfterEncodeDecode(fens, moves, scores, results))
        {
            FAIL("Error encountered when encoding and decoding chunks using binpack")
            passed = false;
        }
        else
        {
            SUCCESS("Encoded and decoded chunk of " << NumChunkGames << " games correctly")
        }
    }

    return passed;
}