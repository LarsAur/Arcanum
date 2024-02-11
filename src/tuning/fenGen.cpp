#include <tuning/fenGen.hpp>
#include <tuning/pgnParser.hpp>
#include <vector>
#include <fstream>
#include <filesystem>

using namespace Tuning;

bool FenGen::m_isQuiet(Arcanum::Board& board)
{
    if(board.isChecked(board.getTurn()))
        return false;

    Arcanum::Move* moves = board.getLegalCaptureMoves();
    uint8_t numMoves = board.getNumLegalMoves();
    for(uint8_t i = 0; i < numMoves; i++)
    {
        if(board.see(moves[i]))
            return false;
    }

    return true;
}

void FenGen::m_runIteration(std::string pgnFile)
{
    std::vector<std::string> fens;
    Arcanum::Board board = Arcanum::Board(Arcanum::startFEN);

    PGNParser parser = PGNParser(pgnFile);
    std::vector<Arcanum::Move>& moves = parser.getMoves();

    // Dont include the 10 last moves, it is assumed that checkmate or draw is found
    for(uint32_t i = 0; i < moves.size() - std::min(moves.size(), (size_t)10); i++)
    {
        board.performMove(moves[i]);
        if(m_isQuiet(board))
            fens.push_back(board.getFEN());
    }

    if(fens.size() == 0)
        return;

    std::ofstream os(m_outputFilePath, std::ios::out | std::ios::app);

    if(!os.is_open())
    {
        ERROR("Unable to store file " << m_outputFilePath);
        return;
    }

    std::stringstream ss;
    ss << "GameData: " << parser.getResult() << " " << fens.size() << "\n";
    for(size_t i = 0; i < fens.size(); i++)
    {
        ss << fens[i] << "\n";
    }
    m_fenCount += fens.size();
    os.write(ss.str().c_str(), ss.str().size());
    os.close();
}

void FenGen::start(uint8_t threadCount, std::string directory)
{
    for(const auto & entry : std::filesystem::directory_iterator(directory))
    {
        m_runIteration(entry.path().string());
        LOG("FEN count:" << m_fenCount);
    }

    m_fenCount = 0;
}

void FenGen::setOutputFile(std::string path)
{
    m_outputFilePath = path;
}