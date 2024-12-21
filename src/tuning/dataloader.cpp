#include <tuning/dataloader.hpp>
#include <tuning/legacyparser.hpp>
#include <tuning/binpack.hpp>

namespace Arcanum
{
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

    eval_t DataLoader::getScore()
    {
        return m_parser->getScore();
    }

    DataParser::Result DataLoader::getResult()
    {
        return m_parser->getResult();
    }
}