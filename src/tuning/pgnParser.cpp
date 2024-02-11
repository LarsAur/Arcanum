#include <tuning/pgnParser.hpp>
#include <sstream>
#include <utils.hpp>

using namespace Tuning;

PGNParser::PGNParser(std::string path)
{
    m_fstream = new std::ifstream(path);

    if(!m_fstream->is_open())
    {
        ERROR("Unable to open " << path)
        exit(-1);
    }

    m_parseTags();
    m_parseMoves();
    m_convertMovesToInternalFormat();
}

PGNParser::~PGNParser()
{
    m_fstream->close();
    delete m_fstream;
}

bool PGNParser::m_isWhitespace(char c)
{
    return c == ' ' || c == '\n' || c == '\r';
}

bool PGNParser::m_consumeNextChar()
{
    char _;
    while (true)
    {
        if(m_fstream->eof())
            return false;
        m_fstream->read(&_, 1);
        if(!m_isWhitespace(_))
            return true;
    }
}

bool PGNParser::m_peekNextChar(char* c)
{
    while (true)
    {
        if(m_fstream->eof())
            return false;
        *c = m_fstream->peek();
        if(m_isWhitespace(*c))
            m_fstream->read(c, 1);
        else
            return true;
    }
}

std::string PGNParser::m_getNextToken()
{
    std::stringstream token;
    char c;

    // Remove potential preceeding whitespace
    while(m_isWhitespace(m_fstream->peek()))
        m_fstream->read(&c, 1);

    // Read chars until EOF or whitespace
    while (true)
    {
        if(m_isWhitespace(m_fstream->peek())) break;
        m_fstream->read(&c, 1);
        if(m_fstream->eof()) break;
        token << c;
        if(c == '.') break; // In case the move number is not separated by whitespace
    }

    return token.str();
}

void PGNParser::m_parseTags()
{
    char c;

    while (true)
    {
        std::stringstream tagName;
        std::stringstream tagValue;

        if(!m_peekNextChar(&c)) return;
        if(c != '[') return;
        m_consumeNextChar(); //  Consume [
        while(true)
        {
            if(!m_peekNextChar(&c)) return;
            if(c == '\"') break;
            tagName << c;
            m_consumeNextChar();
        }

        if(!m_peekNextChar(&c))
        if(c != '\"') return;
        m_consumeNextChar(); // Consume "

        while(true)
        {
            m_fstream->read(&c, 1);
            if(c == '\"') break;;
            tagValue << c;
        }

        if(!m_peekNextChar(&c)) return;
        if(c != ']') return;
        m_consumeNextChar(); //  Consume ]

        PGNTag tag = {.name = tagName.str(), .value = tagValue.str()};
        m_tags.push_back(tag);
    }
}

void PGNParser::m_parseMoves()
{
    bool skip = false;

    while (!m_fstream->eof())
    {
        std::string token = m_getNextToken();

        // Skip potential comments
        if(skip)
        {
            if(token[token.length() - 1] == '}')
            {
                skip = false;
                continue;
            }
        }
        else if(token[0] == '{')
        {
            skip = true;
        }

        if(skip)
            continue;

        // Skip half move counters (e.g 3. or 3...)
        if(token[token.length() - 1] == '.')
            continue;

        if(token == "*" || token == "1/2-1/2")
        {
            m_result = 0.5f;
            break;
        }else if(token == "1-0")
        {
            m_result = 1.0f;
            break;
        }
        else if(token == "0-1")
        {
            m_result = 0.0f;
            break;
        }

        m_pgnMoves.push_back(token);
    }
}

Arcanum::square_t PGNParser::m_getSquareIdx(std::string arithmetic)
{
    char f = arithmetic[0];
    char r = arithmetic[1];

    uint8_t file = f - 'a';
    uint8_t rank = r - '1';

    return (rank << 3) | file;
}

bool PGNParser::m_isMatchingMove(std::string pgnMove, Arcanum::Move move)
{
    // Remove check or checkmate
    if(pgnMove[pgnMove.length() - 1] == '+' || pgnMove[pgnMove.length() - 1] == '#')
    {
        pgnMove = pgnMove.substr(0, pgnMove.length() - 1);
    }

    if(pgnMove == "O-O")
        return move.moveInfo & (MOVE_INFO_CASTLE_WHITE_KING | MOVE_INFO_CASTLE_BLACK_KING);
    if(pgnMove == "O-O-O")
        return move.moveInfo & (MOVE_INFO_CASTLE_WHITE_QUEEN | MOVE_INFO_CASTLE_BLACK_QUEEN);

    // Promition
    if(pgnMove[pgnMove.length() - 2] == '=')
    {
        switch (pgnMove[pgnMove.length() - 1])
        {
            case 'R':
            if(!(move.moveInfo & MOVE_INFO_PROMOTE_ROOK)) return false;
            break;
            case 'N':
            if(!(move.moveInfo & MOVE_INFO_PROMOTE_KNIGHT)) return false;
            break;
            case 'B':
            if(!(move.moveInfo & MOVE_INFO_PROMOTE_BISHOP)) return false;
            break;
            case 'Q':
            if(!(move.moveInfo & MOVE_INFO_PROMOTE_QUEEN)) return false;
            break;
        }

        // Remove the promotion
        pgnMove = pgnMove.substr(0, pgnMove.length() - 2);
    }


    // Find potential capture and remove the x
    if(pgnMove.find('x') != std::string::npos)
    {
        if(!(move.moveInfo & MOVE_INFO_CAPTURE_MASK)) return false;
        size_t pos = pgnMove.find('x');
        std::string pre = pgnMove.substr(0, pos);
        std::string post = pgnMove.substr(pos + 1);
        pgnMove = pre.append(post);
    }

    // Moving piece type
    switch (pgnMove[0])
    {
    case 'R':
        if(!(move.moveInfo & MOVE_INFO_ROOK_MOVE)) return false;
        pgnMove = pgnMove.substr(1);
        break;
    case 'N':
        if(!(move.moveInfo & MOVE_INFO_KNIGHT_MOVE)) return false;
        pgnMove = pgnMove.substr(1);
        break;
    case 'B':
        if(!(move.moveInfo & MOVE_INFO_BISHOP_MOVE)) return false;
        pgnMove = pgnMove.substr(1);
        break;
    case 'Q':
        if(!(move.moveInfo & MOVE_INFO_QUEEN_MOVE)) return false;
        pgnMove = pgnMove.substr(1);
        break;
    case 'K':
        if(!(move.moveInfo & MOVE_INFO_KING_MOVE)) return false;
        pgnMove = pgnMove.substr(1);
        break;
    default: // Pawn
        if(!(move.moveInfo & MOVE_INFO_PAWN_MOVE)) return false;
        break;
    }

    if(pgnMove.length() == 2)
    {
        Arcanum::square_t to = m_getSquareIdx(pgnMove);
        return move.to == to;
    }

    // Rank or file for from square
    if(pgnMove.length() == 3)
    {
        if(pgnMove[0] >= 'a' && pgnMove[0] <= 'h')
        {
            uint8_t file = pgnMove[0] - 'a';
            if((move.from & 0b111) != file) return false;
        }
        else if(pgnMove[0] >= '1' && pgnMove[0] <= '8')
        {
            uint8_t rank = pgnMove[0] - '1';
            if((move.from >> 3) != rank) return false;
        }

        Arcanum::square_t to = m_getSquareIdx(pgnMove.substr(1));
        return move.to == to;
    }

    // Rank and file for from square
    // Not sure if this can occur
    if(pgnMove.length() == 4)
    {
        Arcanum::square_t from = m_getSquareIdx(pgnMove.substr(0, 2));
        Arcanum::square_t to = m_getSquareIdx(pgnMove.substr(2, 2));

        return move.to == to && move.from == from;
    }

    return false;
}

void PGNParser::m_convertMovesToInternalFormat()
{
    Arcanum::Board board = Arcanum::Board(Arcanum::startFEN);

    for(auto pgnMove : m_pgnMoves)
    {
        Arcanum::Move* legalMoves = board.getLegalMoves();
        board.generateCaptureInfo();
        uint8_t numMoves = board.getNumLegalMoves();
        for(uint8_t i = 0; i < numMoves; i++)
        {
            if(m_isMatchingMove(pgnMove, legalMoves[i]))
            {
                m_moves.push_back(legalMoves[i]);
                break;
            }

            if(i == numMoves - 1)
            {
                ERROR("No matching move found for " << pgnMove)
                ERROR(board.getBoardString())
                ERROR(board.getTurn())
                ERROR(board.getFEN())
                exit(-1);
            }
        }

        board.performMove(m_moves[m_moves.size() - 1]);
    }
}

std::vector<Arcanum::Move>& PGNParser::getMoves()
{
    return m_moves;
}

float PGNParser::getResult()
{
    return m_result;
}