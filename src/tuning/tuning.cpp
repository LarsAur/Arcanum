#include <tuning.hpp>
#include <eval.hpp>
#include <fstream>
#include <utils.hpp>
#include <cmath>

using namespace Tuning;

Tuner::Tuner()
{

}

void Tuner::m_loadWeights() //TODO: Path should be relative to exe
{
    std::ifstream is(m_inputFilePath, std::ios::in | std::ios::binary);

    if(!is.is_open())
    {
        WARNING("Unable to open file " << m_inputFilePath)
        LOG("Creating empty weights")
        m_weights.resize(Arcanum::Evaluator::NUM_WEIGHTS);
        memset(m_weights.data(), 0, Arcanum::Evaluator::NUM_WEIGHTS * sizeof(Arcanum::eval_t));
        return;
    }

    std::streampos bytesize = is.tellg();
    is.seekg(0, std::ios::end);
    bytesize = is.tellg() - bytesize;
    is.seekg(0);

    LOG("Reading " << (bytesize >> 1) << " weights from " << m_inputFilePath)

    m_weights.resize(Arcanum::Evaluator::NUM_WEIGHTS);
    is.read((char*) m_weights.data(), bytesize);
    // Set the remaining weights to zero
    memset(m_weights.data() + (bytesize >> 1), 0, Arcanum::Evaluator::NUM_WEIGHTS - (bytesize >> 1));
    is.close();

}

void Tuner::m_storeWeights() //TODO: Path should be relative to exe
{
    std::ofstream os(m_outputFilePath, std::ios::out | std::ios::trunc | std::ios::binary);

    if(!os.is_open())
    {
        ERROR("Unable to store file " << m_outputFilePath);
        return;
    }

    LOG("Storing weights in " << m_outputFilePath);
    os.write((char*) m_weights.data(), m_weights.size() << 1);
    os.close();
}

inline double Tuner::m_sigmoid(int16_t eval)
{
    return 1.0 / (1.0 + pow(10.0, -double(eval)/400));
}

double Tuner::m_squareError(float result, Arcanum::eval_t eval)
{
    return pow(result - m_sigmoid(eval), 2);
}

double Tuner::m_getError()
{
    std::ifstream is(m_trainingDataFilePath, std::ios::in);

    if(!is.is_open())
    {
        ERROR("Unable to open file " << m_trainingDataFilePath)
        exit(-1);
    }

    Arcanum::Evaluator evaluator = Arcanum::Evaluator();
    evaluator.loadWeights(m_weights.data());
    evaluator.setEnableNNUE(false);
    double totalError = 0;
    uint64_t totalGames = 0LL;

    std::string header;
    std::string strResult;
    std::string strGames;
    std::string fen;

    is.seekg(m_iterationStartPos);
    while (!is.eof() && totalGames < Tuner::BATCH_SIZE)
    {
        std::getline(is, header, ' ');
        std::getline(is, strResult, ' ');
        std::getline(is, strGames);

        float result = atof(strResult.c_str());
        uint8_t numGames = atoi(strGames.c_str());
        totalGames += numGames;

        for(uint8_t i = 0; i < numGames; i++)
        {
            std::getline(is, fen);
            Arcanum::Board board = Arcanum::Board(fen);
            evaluator.initializeAccumulatorStack(board);
            Arcanum::eval_t eval = evaluator.evaluate(board, 0);
            totalError += m_squareError(result, eval);
        }
    }
    is.close();

    return totalError / totalGames;
}

void Tuner::m_runIteration()
{
    constexpr int16_t rate = 1;
    double baselineError = m_getError();
    LOG("Baseline Error: " << baselineError)
    for(size_t i = 0; i < Arcanum::Evaluator::NUM_WEIGHTS; i++)
    {

        int16_t delta = rate;
        if(m_deltas[i])
            delta = m_deltas[i];

        m_weights[i] += delta;
        double error = m_getError();
        if(error < baselineError)
        {
            baselineError = error;
            m_deltas[i] = delta;
            continue;
        }

        m_weights[i] -= 2*delta;
        error = m_getError();

        if(error < baselineError)
        {
            baselineError = error;
            m_deltas[i] = -delta;
            continue;
        }

        // Reset the weight
        m_weights[i] += delta;
    }
}

void Tuner::m_calculateNextStartPos()
{
    std::ifstream is(m_trainingDataFilePath, std::ios::in);

    if(!is.is_open())
    {
        ERROR("Unable to open file " << m_trainingDataFilePath)
        exit(-1);
    }

    uint64_t totalGames = 0LL;
    std::string header;
    std::string strResult;
    std::string strGames;
    std::string fen;

    is.seekg(m_iterationStartPos);
    while (!is.eof() && totalGames < Tuner::BATCH_SIZE)
    {
        std::getline(is, header, ' ');
        std::getline(is, strResult, ' ');
        std::getline(is, strGames);

        uint8_t numGames = atoi(strGames.c_str());
        totalGames += numGames;

        for(uint8_t i = 0; i < numGames; i++)
        {
            std::getline(is, fen);
        }
    }

    if(is.eof())
        m_iterationStartPos = 0;
    else
        m_iterationStartPos = is.tellg();

    is.close();
}

void Tuner::setInputFile(std::string path)
{
    m_inputFilePath = path;
}

void Tuner::setOutputFile(std::string path)
{
    m_outputFilePath = path;
}

void Tuner::setTrainingDataFilePath(std::string path)
{
    m_trainingDataFilePath = path;
}

void Tuner::start()
{
    m_loadWeights();

    m_deltas.resize(m_weights.size());
    for(size_t i = 0; i < m_weights.size(); i++)
        m_deltas.push_back(0);

    m_iterationStartPos = 0;
    while(true)
    {
        m_runIteration();
        m_storeWeights();
        m_calculateNextStartPos();
    }
}