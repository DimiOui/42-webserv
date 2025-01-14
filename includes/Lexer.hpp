#ifndef LEXER_HPP
# define LEXER_HPP

#include <vector>
#include <string>
#include <Type.hpp>

class Lexer
{
	t_chr_class				get_chr_class[255];
	t_s_tok					get_tok_type[CHR_MAX];
	int 					tok_rules[TOK_S_MAX][CHR_MAX];
	std::string				_confile;
	TokenList	_tokens;
	// Lexer(){};

public:
	Lexer(const std::string);
	// Lexer(const Lexer&);
	// void	operator=(const Lexer&);
	~Lexer();

	TokenList const & getTokens() const;

	void					fillTokens();
	void					printTokens();
};

std::ostream&	operator<<(std::ostream &out, const t_token &c);

#endif
