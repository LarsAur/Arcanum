#include <tuning/dataloader.hpp>
#include <tuning/legacy.hpp>
#include <tuning/binpack.hpp>

namespace Arcanum
{

    // -- Data Loader

    DataLoader::DataLoader() : m_parser(nullptr)
    {

    }

    bool DataLoader::open(std::string path)
    {
        if(path.find(".binpack") != std::string::npos)
        {
            m_parser = std::make_unique<BinpackParser>();
            LOG("Loading binpack file: " << path)
        }
        else if(path.find(".txt") != std::string::npos)
        {
            m_parser = std::make_unique<LegacyParser>();
            LOG("Loading legacy file: " << path)
        }
        else{
            ERROR("Unsupported file format: " << path)
            return false;
        }

        return m_parser->open(path);
    }

    void DataLoader::close()
    {
        m_parser->close();
    }

    bool DataLoader::eof()
    {
        return m_parser->eof();
    }

    Board* DataLoader::getNextBoard()
    {
        return m_parser->getNextBoard();
    }

    Move DataLoader::getMove()
    {
        return m_parser->getMove();
    }

    eval_t DataLoader::getScore()
    {
        return m_parser->getScore();
    }

    GameResult DataLoader::getResult()
    {
        return m_parser->getResult();
    }

    // -- Data Storer

    DataStorer::DataStorer() : m_encoder(nullptr)
    {

    }

    bool DataStorer::open(std::string path)
    {
        if(path.find(".binpack") != std::string::npos)
        {
            m_encoder = std::make_unique<BinpackEncoder>();
            LOG("Loading binpack file: " << path)
        }
        else if(path.find(".txt") != std::string::npos)
        {
            m_encoder = std::make_unique<LegacyEncoder>();
            LOG("Loading legacy file: " << path)
        }
        else{
            ERROR("Unsupported file format: " << path)
            return false;
        }

        return m_encoder->open(path);
    }

    void DataStorer::close()
    {
        m_encoder->close();
    }

    // Encode a position and write it to file
    // The scores are from the current turns perspective
    void DataStorer::addPosition(
        const Board& board,
        const Move& move,
        eval_t score,
        GameResult result
    )
    {
        m_encoder->addPosition(board, move, score, result);
    }

    // Encode a game and write it to file
    // The scores are from the current turns perspective
    void DataStorer::addGame(
        const Board& startBoard,
        const std::vector<Move>& moves,
        const std::vector<eval_t>& scores,
        GameResult result
    )
    {
        m_encoder->addGame(startBoard, moves, scores, result);
    }
}