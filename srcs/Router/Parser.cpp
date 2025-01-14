#include <Parser.hpp>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <map>
// #include <cstdint>
#include <algorithm>
#include <climits>
#include <string.h>
#include <stdexcept>
#include <string>
#include <Directive.hpp>
#include "Type.hpp"

// void Parser::freeBlocks()
// {
//     BlockServer *tmp1 = _blocks;
//     BlockServer *tmp2 = _blocks;
//     while (tmp1)
//     {
//         std::vector<BlockLocation *> const &childs = tmp1->getChilds();
//         for (std::vector<BlockLocation *>::const_iterator it = childs.begin(); it != childs.end(); it++)
//         {
//             delete (*it);
//         }
//         tmp2 = tmp1;
//         tmp1 = tmp1->getSibling();
//         delete (tmp2);
//         tmp2 = NULL;
//     }
// }

Parser::~Parser()
{
    // freeBlocks();
}

Parser::Parser(TokenList const &tokens) : _tokens(tokens), _blockServers()
{
    std::string dn[12] = {LISTEN, SERVERNAMES, ALLOWEDMETHODS, MAXBODYSIZE, REDIRECT, ROOT, INDEX, AUTOINDEX, CGISETUP, ERRORFILE, UPLOADS};
    for (uint i = 0; i != 11; i++)
    {
        directiveNames.push_back(dn[i]);
    }
    parse(tokens, 0);
    // deletePortDupServers(&_blocks); //function that should delete the ServerBlocks having the same port
}

bool notSpace(t_token token)
{
    return (token.first != TOK_SP && token.first != TOK_RL);
}

int findNextNonSpTok(TokenList const &tokens, uint i)
{
    TokenList::const_iterator tokNotSp = find_if(tokens.begin() + i, tokens.end(), notSpace);
    // Throw error if there is none
    return (tokNotSp - tokens.begin());
}

void printVector(TokenList tokens)
{
    uint i = 0;
    for (TokenList::iterator it = tokens.begin(); it != tokens.end(); it++)
    {
        std::cout << i << " " << it->first << " " << it->second << std::endl;
        i++;
    }
}

uint closingIndexBracket(TokenList const &tokens, uint j)
{
    uint i = j;
    uint br = 1;
    for (TokenList::const_iterator it = tokens.begin() + j; it != tokens.end(); it++)
    {
        if (it->first == TOK_BR_OP)
        {
            br++;
        }
        else if (it->first == TOK_BR_CL)
        {
            br--;
            if (br == 0)
            {
                return (i);
            }
        }
        i++;
    }
    return (0);
}

bool isDirective(t_token token, std::vector<std::string> const &directiveNames)
{
    return (token.first == TOK_WORD && std::find(directiveNames.begin(), directiveNames.end(), token.second) != directiveNames.end());
}

bool tokensAreNotWords(TokenList const &tokens)
{
    size_t i = 0;
    while (i < tokens.size() && tokens[i].first != TOK_WORD)
    {
        i++;
    }
    return (i == tokens.size());
}

TokenList subVectorFrom(TokenList originalTokens, uint index)
{
    TokenList toReturn(originalTokens.begin() + index, originalTokens.end());
    return (toReturn);
}


Directive parseDirective(TokenList const &tokens, uint &i)
{
    std::string directiveName = tokens[i].second;
    Directive directive(directiveName);
    uint j = 1;
    while (i < tokens.size() && tokens[i].first != TOK_SC)
    {
        uint firstNonSpTokIndex = findNextNonSpTok(tokens, i + 1);
		if (firstNonSpTokIndex >= tokens.size())
			throw std::runtime_error(directiveName + " directive never end\n");
        t_token directiveValueTok = tokens[firstNonSpTokIndex];
        if (directiveValueTok.first == TOK_WORD)
        {
            if (j > 1 && directiveName != SERVERNAMES && directiveName != CGISETUP && directiveName != ERRORFILE && directiveName != ALLOWEDMETHODS)
                throw std::runtime_error(directiveName + " is given more than 1 argument!\n");
            if (j > 2 && directiveName != SERVERNAMES && directiveName != ALLOWEDMETHODS)
                throw std::runtime_error(directiveName + " is given more than 2 arguments!\n");
            if (j > 3 && directiveName != SERVERNAMES)
                throw std::runtime_error(directiveName + " is given more than 3 arguments!\n");
            directive.addDirectiveValue(directiveValueTok.second);
            j++;
        }
        i = firstNonSpTokIndex;
    }
    i++;
    return directive;
}

bool portExists(BlockServer block)
{
    std::vector<Directive>::const_iterator it;
    for (it = block.getDirectives().begin(); it != block.getDirectives().end(); it++)
    {
        if (it->getDirectiveName() == "listen")
            return true;
    }
    return false;
}

void Parser::parse(TokenList const &tokens, uint serverNumber)
{
    if (tokens.size() || !tokensAreNotWords(tokens))
    {
        uint firstNonSpTokIndex = findNextNonSpTok(tokens, 0);                       // the first non space token in the tokens given
        uint secondNonSpTokIndex = findNextNonSpTok(tokens, firstNonSpTokIndex + 1); // the second non space token in the tokens given
        if (firstNonSpTokIndex < tokens.size() && tokens[firstNonSpTokIndex].second == "server" && tokens[secondNonSpTokIndex].first == TOK_BR_OP)
        {
            std::ostringstream oss;
            oss << serverNumber;
            BlockServer block("server_" + oss.str());
            uint i = secondNonSpTokIndex + 1;
            uint clServerBrIndex = closingIndexBracket(tokens, i);
            while (i < clServerBrIndex)
            {
                if (isDirective(tokens[i], directiveNames))
                    block.addDirective(parseDirective(tokens, i));
                else if (tokens[i].second == "location")
                {
                    firstNonSpTokIndex = findNextNonSpTok(tokens, i + 1);
                    secondNonSpTokIndex = findNextNonSpTok(tokens, firstNonSpTokIndex + 1);
                    if (tokens[firstNonSpTokIndex].first == TOK_WORD && tokens[secondNonSpTokIndex].first == TOK_BR_OP)
                    {
                        oss << block.getSizeLocations();
                        std::string locationName = "Location_" + oss.str();
                        std::string locationValue = tokens[firstNonSpTokIndex].second;
                        BlockLocation   locationBlock(locationName, locationValue);
                        Directive locationDirective("location");
                        locationDirective.addDirectiveValue(locationValue);
                        locationBlock.addDirective(locationDirective);
                        i = secondNonSpTokIndex + 1;
                        uint clLocationBrIndex = closingIndexBracket(tokens, i);
                        while (i < clLocationBrIndex)
                        {
                            if (isDirective(tokens[i], directiveNames))
                                locationBlock.addDirective(parseDirective(tokens, i));
                            else if (tokens[i].first == TOK_SP || tokens[i].first == TOK_RL)
                                i++;
                            else
                            {
                                throw std::runtime_error("\"" + tokens[i].second + "\" is not a valid directive name!\n");
                            }
                        }
                        block.addLocation(locationBlock);
                        i = clLocationBrIndex + 1;
                    }
                }
                else if (tokens[i].first == TOK_RL || tokens[i].first == TOK_SP)
                    i++;
                else
                {
                    throw std::runtime_error("\"" + tokens[i].second + "\" is not a valid directive name!\n");
                }
            }
            if (!portExists(block))
            {
                throw std::runtime_error(block.getName() + " is not listening on any port!\n");
            }
            _blockServers.push_back(block);
            TokenList subToken(tokens.begin() + clServerBrIndex + 1, tokens.end());
            parse(subToken, serverNumber + 1);
            return;
        }
        else if (firstNonSpTokIndex < tokens.size())
            throw std::runtime_error("Error  while parsing the config file!\n");
    }
}

void Parser::printBlocks() const
{
    for (std::vector<BlockServer>::const_iterator   it = _blockServers.begin(); it != _blockServers.end(); it++)
    {
     it->printBlock();
    }
}

std::vector<BlockServer> const &Parser::getBlockServers() const
{
    return (_blockServers);
}
