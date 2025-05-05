#include <test/binpackTest.hpp>
#include <tuning/binpack.hpp>
#include <board.hpp>
#include <search.hpp>
#include <fen.hpp>
#include <array>
#include <random>

using namespace Arcanum::Benchmark;
using namespace Arcanum;

constexpr char filename[] = "test_binpack.binpack";

bool compareChunkAfterEncodeDecode(
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
        encoder.addGame(fens[i], moves[i], scores[i], results[i]);
    }
    encoder.close();

    bool pass = true;
    parser.open(filename);

    for(uint32_t i = 0; i < numGames; i++)
    {
        Board encodedBoard = Board(fens[i]);
        const uint32_t numMoves = moves.size();
        for(uint32_t j = 0; j < numMoves; j++)
        {
            Board *parsedBoard = parser.getNextBoard();

            if(parsedBoard->fen() != encodedBoard.fen())
            {
                pass = false;
                FAIL("BinpackTest: FEN[" << j << "]" << parsedBoard->fen() << " != " << encodedBoard.fen())
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
                FAIL("BinpackTest: Score[" << j << "]" << parser.getScore() << " != " << scores[i][j])
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

    if(pass) {
        // Delete the binpack file
        std::remove(filename);
    }

    return pass;
}

bool compareAfterEncodeDecode(
    std::string fen,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
    GameResult result
)
{
    BinpackEncoder encoder;
    BinpackParser parser;

    encoder.open(filename);
    encoder.addGame(fen, moves, scores, result);
    encoder.close();

    bool pass = true;
    parser.open(filename);
    Board encodedBoard = Board(fen);
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

    if(pass) {
        // Delete the binpack file
        std::remove(filename);
    }

    return pass;
}

void generateRandomGame(
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

void generatePlayedGame(
    std::string& fen,
    std::vector<Move>& moves,
    std::vector<eval_t>& scores,
    GameResult& result
)
{
    fen = FEN::startpos;

    Searcher searcher = Searcher(false);
    Board board = Board(FEN::startpos);

    result = GameResult::DRAW;

    while(board.hasLegalMove() && (searcher.getHistory().find(board.getHash()) == searcher.getHistory().end()))
    {
        searcher.addBoardToHistory(board);

        SearchResult searchResult;
        Move move = searcher.getBestMove(board, 10, &searchResult);

        moves.push_back(move);
        scores.push_back(searchResult.eval);

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

void BinpackTest::runBinpackTest()
{
    LOG("Running binpack test")

    {
        std::string fen;
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;

        // -- Play a game and try to encode and decode it.

        LOG("Testing played game")
        generatePlayedGame(fen, moves, scores, result);
        if(!compareAfterEncodeDecode(fen, moves, scores, result))
        {
            FAIL("Error encountered when encoding and decoding played game using binpack")
            //TODO: Log fen, result and moves.
        }
        else
        {
            SUCCESS("Encoded and decoded played game correctly using binpack")
        }
    }

    // Play a number of random games and try to encode and decode it.

    {
        constexpr uint32_t NumRandomGames = 10000;
        std::string fen;
        std::vector<Move> moves;
        std::vector<eval_t> scores;
        GameResult result;
        std::mt19937 generator(0);

        bool pass = true;
        LOG("Testing " << NumRandomGames << " random games")
        for(uint32_t i = 0; i < NumRandomGames; i++)
        {
            generateRandomGame(fen, moves, scores, result, generator);
            if(!compareAfterEncodeDecode(fen, moves, scores, result))
            {
                pass = false;
                FAIL("Error encountered when encoding and decoding random game using binpack")
                //TODO: Log fen, result and moves.
                break;
            }

            moves.clear();
            scores.clear();

            if(i % (NumRandomGames / 10) == 0)
            {
                LOG(i << " / " << NumRandomGames)
            }
        }

        if(pass)
        {
            SUCCESS("Encoded and decoded " << NumRandomGames << " random games using binpack")
        }
    }

    // Play multiple games to create a binpack chunk

    {
        constexpr uint32_t NumChunkGames = 10;
        std::vector<std::string> fens;
        std::vector<std::vector<Move>> moves;
        std::vector<std::vector<eval_t>> scores;
        std::vector<GameResult> results;
        std::mt19937 generator(0);

        LOG("Testing binpack chunks")
        for(uint32_t i = 0; i < NumChunkGames; i++)
        {
            generateRandomGame(fens[i], moves[i], scores[i], results[i], generator);
        }

        if(!compareChunkAfterEncodeDecode(fens, moves, scores, results))
        {
            FAIL("Error encountered when encoding and decoding chunks using binpack")
            //TODO: Log fen, result and moves.
        }
        else
        {
            SUCCESS("Encoded and decoded chunk correctly")
        }
    }

    LOG("Completed binpack test")
}