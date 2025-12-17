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
    std::vector<Board> initialPositions,
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
        encoder.addGame(initialPositions[i], moves[i], scores[i], results[i]);
    }
    encoder.close();

    bool pass = true;
    parser.open(filename);

    for(uint32_t i = 0; i < numGames; i++)
    {
        Board encodedBoard = initialPositions[i];
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
    const Board& initialBoard,
    const std::vector<Move>& moves,
    const std::vector<eval_t>& scores,
    GameResult result
)
{
    BinpackEncoder encoder;
    BinpackParser parser;
    Board encodedBoard = Board(initialBoard);

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

bool Test::runBinpackTest()
{
    bool passed = true;

    {
        GameRunner runner;
        SearchParameters params;

        params.useDepth = true;
        params.depth = 10;
        runner.setSearchParameters(params);
        runner.setMoveLimit(300);

        // -- Play a game and try to encode and decode it.

        INFO("Testing played game")
        runner.play();
        if(!compareAfterEncodeDecode(runner.getInitialPosition(), runner.getMoves(), runner.getEvals(), runner.getResult()))
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

        GameRunner runner;
        SearchParameters params;

        params.useDepth = true;
        params.depth = 1;
        runner.setSearchParameters(params);
        runner.setMoveLimit(300);

        bool pass = true;
        INFO("Testing " << NumRandomGames << " random games")
        for(uint32_t i = 0; i < NumRandomGames; i++)
        {
            runner.randomizeInitialPosition((i % 9) + 1); // Randomize between 1 and 9 moves
            runner.play();

            if(!compareAfterEncodeDecode(runner.getInitialPosition(), runner.getMoves(), runner.getEvals(), runner.getResult()))
            {
                pass = false;
                FAIL("Error encountered when encoding and decoding random game using binpack")
                passed = false;
                break;
            }
        }

        if(pass)
        {
            SUCCESS("Encoded and decoded " << NumRandomGames << " random games using binpack")
        }
    }

    // Play multiple games to create a binpack chunk

    {
        constexpr uint32_t NumChunkGames = 1000;
        GameRunner runner;
        SearchParameters params;
        std::vector<Board> initialPositions;
        std::vector<std::vector<Move>> moves;
        std::vector<std::vector<eval_t>> scores;
        std::vector<GameResult> results;

        initialPositions.resize(NumChunkGames);
        moves.resize(NumChunkGames);
        scores.resize(NumChunkGames);
        results.resize(NumChunkGames);


        params.useDepth = true;
        params.depth = 1;
        runner.setSearchParameters(params);
        runner.setMoveLimit(300);

        INFO("Testing binpack chunks")
        for(uint32_t i = 0; i < NumChunkGames; i++)
        {
            runner.randomizeInitialPosition((i % 9) + 1); // Randomize between 1 and 9 moves
            runner.play();

            // Copy the initial position, moves, scores and result
            initialPositions[i] = runner.getInitialPosition();
            results[i] = runner.getResult();

            for(uint32_t j = 0; j < runner.getMoves().size(); j++)
            {
                moves[i].push_back(runner.getMoves()[j]);
                scores[i].push_back(runner.getEvals()[j]);
            }
        }

        if(!compareChunkAfterEncodeDecode(initialPositions, moves, scores, results))
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