#include <Parser.hpp>
#include <iostream>
#include <algorithm>

Parser::Parser(std::vector<t_token> const &tokens) : _tokens(tokens), _blocks(NULL)
{
    std::string dn[7] = {"listen", "server_name", "client_max_body_size", "root", "allowed_methods", "autoindex", "cgi_setup"};
    for (uint i = 0; i != 7; i++) {
        directiveNames.push_back(dn[i]);
    }
    // t_block_type type = BL_SERVER;
    parse(tokens);
}

bool notSpace(t_token token)
{
    return (token.first != TOK_SP && token.first != TOK_RL);
}

int funNextNonSpTok(std::vector<t_token> const &tokens)
{
    std::vector<t_token>::const_iterator tokNotSp = find_if(tokens.begin(), tokens.end(), notSpace);
    // Throw error if there is none
    return (tokNotSp - tokens.begin());
}

uint closingIndexBracket(std::vector<t_token> &tokens)
{
    uint i = 0;
    uint br = 1;
    for (std::vector<t_token>::iterator it = tokens.begin(); it != tokens.end(); it++)
    {
        i++;
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
    }
        // Throw an error if you don't find
    return(0);
}

bool isDirective(t_token token, std::vector<std::string> const & directiveNames)
{
    return (token.first == TOK_WORD && std::find(directiveNames.begin(), directiveNames.end(), token.second) != directiveNames.end());
}

void    printVector(std::vector<t_token> tokens){
    for (std::vector<t_token>::iterator  it = tokens.begin(); it != tokens.end(); it++) {
        std::cout << it->first << " " << it->second << std::endl;
    }
}

// void Parser::parse(std::pair<uint, uint> limits, t_block_type type, std::vector<t_token> const &tokens)
void Parser::parse(std::vector<t_token>  const & tokens)
{
    std::cout << "-----------------TOKENS------------------" << std::endl;
    printVector(tokens);
    uint    st3 = 0;
    std::vector<t_token>    stt3;
    if (tokens.size())
    {
        uint nextNonSpTok = funNextNonSpTok(tokens);
        std::vector<t_token>    subToken1(tokens.begin() +nextNonSpTok + 1, tokens.end());
        std::cout << "-----------------SUBTOKEN1------------------" << std::endl;
        printVector(subToken1);
        uint nextNextNonSpTok = funNextNonSpTok(subToken1);
        // std::cout << tokens[nextNonSpTok].second << std::endl;
        // std::cout << subToken1[nextNextNonSpTok].first << std::endl;
        // std::cout << TOK_BR_OP << std::endl;
        // exit(1);
        if (tokens[nextNonSpTok].second == "server" && subToken1[nextNextNonSpTok].first == TOK_BR_OP)
        {
            std::cout << "I entered here" << std::endl;
            Block   *currentBlock = new Block(BL_SERVER);
            if (!_blocks)
            {
                _blocks = currentBlock;
            }
            else
            {
                _blocks->addSibling(currentBlock);
            }
            std::vector<t_token>    subToken2(subToken1.begin() +nextNextNonSpTok + 1, subToken1.end());
            std::cout << "-----------------SUBTOKEN2------------------" << std::endl;
            printVector(subToken2);
            uint    i = 0;
            while (i < closingIndexBracket(subToken2)) {
                if (isDirective(subToken2[i], directiveNames)){
                    std::vector<t_token>    subToken3(subToken2.begin() + i + 1, subToken2.end());
                    st3 = funNextNonSpTok(subToken3);
                    std::cout << "-----------------SUBTOKEN3------------------" << std::endl;
                    printVector(subToken3);
                    currentBlock->addDirective(std::make_pair(subToken2[i].second, subToken3[st3].second));
                    stt3 = subToken3;
                    i += st3 + 1;
                    }
                else {
                    i++;
                }
            }
       }
       std::cout << st3 + 1 << std::endl;
    //    parse(st3 + 1, stt3);
    }
}

void    Parser::printBlocks() const{
   Block    *tmp = _blocks;
   while (tmp){
    tmp->printBlock();
    tmp = tmp->getSibling();
   }
}
