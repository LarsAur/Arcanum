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

template<uint32_t NumChunkGames>
bool compareChunkAfterEncodeDecode(
    std::array<std::string, NumChunkGames> fens,
    std::array<std::array<Move, Arcanum::BinpackEncoder::MaxGameLength>, NumChunkGames> moves,
    std::array<std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength>, NumChunkGames> scores,
    std::array<uint32_t, NumChunkGames> numMoves,
    std::array<GameResult, NumChunkGames> result
)
{
    BinpackEncoder encoder;
    BinpackParser parser;

    encoder.open(filename);
    for(uint32_t i = 0; i < NumChunkGames; i++)
    {
        encoder.addGame(fens[i], moves[i], scores[i], numMoves[i], result[i]);
    }
    encoder.close();

    bool pass = true;
    parser.open(filename);

    for(uint32_t i = 0; i < NumChunkGames; i++)
    {
        Board encodedBoard = Board(fens[i]);
        for(uint32_t j = 0; j < numMoves[i]; j++)
        {
            Board *parsedBoard = parser.getNextBoard();

            if(parsedBoard->fen() != encodedBoard.fen())
            {
                pass = false;
                FAIL("BinpackTest: FEN[" << j << "]" << parsedBoard->fen() << " != " << encodedBoard.fen())
                break;
            }

            if(parser.getResult() != result[i])
            {
                pass = false;
                FAIL("BinpackTest: Result[" << i << "]" << parser.getResult() << " != " << result[i])
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
    std::array<Move, Arcanum::BinpackEncoder::MaxGameLength>& moves,
    std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength>& scores,
    uint32_t numMoves,
    GameResult result
)
{
    BinpackEncoder encoder;
    BinpackParser parser;

    encoder.open(filename);
    encoder.addGame(fen, moves, scores, numMoves, result);
    encoder.close();

    bool pass = true;
    parser.open(filename);
    Board encodedBoard = Board(fen);
    for(uint32_t i = 0; i < numMoves; i++)
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
    std::array<Move, Arcanum::BinpackEncoder::MaxGameLength>& moves,
    std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength>& scores,
    uint32_t& numMoves,
    GameResult& result,
    std::mt19937& generator
)
{
    std::uniform_int_distribution<uint64_t> distributionU64(0, UINT64_MAX);
    std::uniform_int_distribution<int16_t>  distributionI16(-INT16_MAX, INT16_MAX);

    fen = FEN::startpos;
    Board board = Board(FEN::startpos);

    result = GameResult::DRAW;
    numMoves = 0;

    while(board.hasLegalMove())
    {
        Move* legalMoves = board.getLegalMoves();
        uint8_t numLegalMoves = board.getNumLegalMoves();
        board.generateCaptureInfo();

        // Select random move and score
        Move move = legalMoves[distributionU64(generator) % numLegalMoves];
        eval_t score = distributionI16(generator);

        moves[numMoves] = move;
        scores[numMoves] = score;

        board.performMove(move);

        numMoves++;

        if(numMoves >= Arcanum::BinpackEncoder::MaxGameLength)
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
    std::array<Move, Arcanum::BinpackEncoder::MaxGameLength>& moves,
    std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength>& scores,
    uint32_t& numMoves,
    GameResult& result
)
{
    fen = FEN::startpos;

    Searcher searcher = Searcher(false);
    Board board = Board(FEN::startpos);

    result = GameResult::DRAW;
    numMoves = 0;

    while(board.hasLegalMove() && (searcher.getHistory().find(board.getHash()) == searcher.getHistory().end()))
    {
        searcher.addBoardToHistory(board);

        SearchResult searchResult;
        Move move = searcher.getBestMove(board, 10, &searchResult);

        moves[numMoves] = move;
        scores[numMoves] = searchResult.eval;

        board.performMove(move);

        numMoves++;

        if(numMoves >= Arcanum::BinpackEncoder::MaxGameLength)
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
        std::array<Move, Arcanum::BinpackEncoder::MaxGameLength> moves;
        std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength> scores;
        uint32_t numMoves;
        GameResult result;

        // -- Play a game and try to encode and decode it.

        LOG("Testing played game")
        generatePlayedGame(fen, moves, scores, numMoves, result);
        if(!compareAfterEncodeDecode(fen, moves, scores, numMoves, result))
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
        std::array<Move, Arcanum::BinpackEncoder::MaxGameLength> moves;
        std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength> scores;
        uint32_t numMoves;
        GameResult result;
        std::mt19937 generator(0);

        bool pass = true;
        LOG("Testing " << NumRandomGames << " random games")
        for(uint32_t i = 0; i < NumRandomGames; i++)
        {
            generateRandomGame(fen, moves, scores, numMoves, result, generator);
            if(!compareAfterEncodeDecode(fen, moves, scores, numMoves, result))
            {
                pass = false;
                FAIL("Error encountered when encoding and decoding random game using binpack")
                //TODO: Log fen, result and moves.
                break;
            }

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
        std::array<std::string, NumChunkGames> fens;
        std::array<std::array<Move, Arcanum::BinpackEncoder::MaxGameLength>, NumChunkGames> moves;
        std::array<std::array<eval_t, Arcanum::BinpackEncoder::MaxGameLength>, NumChunkGames> scores;
        std::array<uint32_t, NumChunkGames> numMoves;
        std::array<GameResult, NumChunkGames> result;
        std::mt19937 generator(0);

        LOG("Testing binpack chunks")
        for(uint32_t i = 0; i < NumChunkGames; i++)
        {
            generateRandomGame(fens[i], moves[i], scores[i], numMoves[i], result[i], generator);
        }

        if(!compareChunkAfterEncodeDecode<NumChunkGames>(fens, moves, scores, numMoves, result))
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