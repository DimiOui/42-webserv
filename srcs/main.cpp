#include "Type.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <Server.hpp>

int main(int ac, char *av[])
{
	(void)ac;
	(void)av;
	Logger::setLevel(DEBUG);
	// if (ac == 2)
	// {
	// 	std::ifstream file(av[1]);
	// 	std::stringstream buffer;
	// 	buffer << file.rdbuf();
	// 	std::string big_buffer = buffer.str();

		// Lexer	Lex(big_buffer);
		// Lex.fillTokens();
		// Lex.printTokens();
	// Logger::debug("test" << "truc");

	Server my_server("");
	try {
		my_server.routine();
	}
	catch(const std::exception& e) {
		Logger::error << e.what();
	}
}
