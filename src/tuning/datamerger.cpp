#include <tuning/datamerger.hpp>
#include <tuning/dataloader.hpp>

using namespace Arcanum;

void DataMerger::addInputPath(const std::string& path)
{
    m_inputPaths.push_back(path);
}

void DataMerger::setOutputPath(const std::string& path)
{
    m_outputPath = path;
}

bool DataMerger::mergeData()
{
    DataStorer storer;
    DataLoader loader;

    if(!storer.open(m_outputPath))
    {
        ERROR("Unable to open output path: " << m_outputPath)
        return false;
    }

    // Check that all input paths can be opened
    for(const std::string& path : m_inputPaths)
    {
        DEBUG("Checking input path: " << path)
        if(!loader.open(path))
        {
            ERROR("Unable to open input path: " << path)
            return false;
        }
        loader.close();
    }

    Board initialBoard;
    std::vector<Move> moves;
    std::vector<eval_t> scores;

    uint64_t positionCount = 0;
    uint64_t prevPositionCount = 0;

    // Merge all input paths
    for(const std::string& path : m_inputPaths)
    {
        DEBUG("Merging input path: " << path)
        if(!loader.open(path))
        {
            ERROR("Unable to open input path: " << path)
            return false;
        }

        while(!loader.eof())
        {
            Board* board = loader.getNextBoard();
            Move move = loader.getMove();
            eval_t score = loader.getScore();

            if(moves.empty())
            {
                initialBoard = *board;
            }

            moves.push_back(move);
            scores.push_back(score);

            if(loader.isEndOfGame())
            {
                GameResult result = loader.getResult();
                storer.addGame(initialBoard, moves, scores, result);
                positionCount += moves.size();
                moves.clear();
                scores.clear();
            }

            if(positionCount - prevPositionCount >= 1000000)
            {
                INFO(positionCount << " Positions merged...")
                prevPositionCount = positionCount;
            }
        }

        loader.close();
    }

    storer.close();

    INFO("Merging completed")
    INFO("Total positions merged: " << positionCount)

    return true;
}