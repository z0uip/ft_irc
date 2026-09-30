#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include "client.hpp"
#include "parsmessage.hpp"

// // Rend \r et \n visibles a l'affichage
// static std::string visible(const std::string &s)
// {
// 	std::string out;
// 	for (size_t i = 0; i < s.size(); ++i)
// 	{
// 		if (s[i] == '\r')
// 			out += "\\r";
// 		else if (s[i] == '\n')
// 			out += "\\n";
// 		else
// 			out += s[i];
// 	}
// 	return (out);
// }

// static void printParams(const std::vector<std::string> &params)
// {
// 	std::cout << params.size() << " element(s) :";
// 	for (size_t i = 0; i < params.size(); ++i)
// 		std::cout << " [" << params[i] << "]";
// 	std::cout << std::endl;
// }

// // Simule des morceaux recus par recv(), comme dans le test nc du sujet
// static void testInput()
// {
// 	client		clt(42, "127.0.0.1");
// 	std::string	line;
// 	const char	*chunks[] = {
// 		"com", "man", "d\r\n",
// 		"NICK bob\r\nJOIN #a\r\n",
// 		"privmsg #a :salut a tous\r\nJOI", "N #b\r\n",
// 		"TOPIC #a :\r\n",
// 		NULL
// 	};

// 	std::cout << "=== Buffer d'entree ===" << std::endl;
// 	for (size_t i = 0; chunks[i] != NULL; ++i)
// 	{
// 		std::cout << "recu : \"" << visible(chunks[i]) << "\"" << std::endl;
// 		clt.add_byte(chunks[i], std::strlen(chunks[i]));
// 		while (clt.extract_line(line))
// 		{
// 			std::cout << "  ligne -> ";
// 			printParams(parsmessage(line));
// 		}
// 	}
// }

// // Simule un send() qui n'envoie qu'une partie du buffer
// static void testOutput()
// {
// 	client clt(42, "127.0.0.1");

// 	std::cout << "=== Buffer de sortie ===" << std::endl;
// 	std::cout << "en attente au depart : " << clt.hasPendingOutput() << std::endl;
// 	clt.queueMessage(":ircserv 001 bob :Welcome");
// 	clt.queueMessage(":bob PRIVMSG #a :salut");
// 	std::cout << "buffer : \"" << visible(clt.getOutBuffer()) << "\"" << std::endl;
// 	clt.consumeOutPut(10);
// 	std::cout << "apres envoi de 10 octets : \"" << visible(clt.getOutBuffer()) << "\"" << std::endl;
// 	clt.consumeOutPut(-1);
// 	std::cout << "apres un send() a -1 : \"" << visible(clt.getOutBuffer()) << "\"" << std::endl;
// 	clt.consumeOutPut(clt.getOutBuffer().size());
// 	std::cout << "en attente a la fin : " << clt.hasPendingOutput() << std::endl;
// }

// // Tape tes propres lignes au clavier
// static void testInteractive()
// {
// 	std::string line;

// 	std::cout << "=== Parsing interactif (ctrl+D pour quitter) ===" << std::endl;
// 	while (std::getline(std::cin, line))
// 		printParams(parsmessage(line));
// }

// int main(int ac, char **av)
// {
// 	std::string mode = (ac > 1) ? av[1] : "";

// 	if (mode == "input")
// 		testInput();
// 	else if (mode == "output")
// 		testOutput();
// 	else if (mode == "parse")
// 		testInteractive();
// 	else
// 		std::cout << "usage : ./test [input | output | parse]" << std::endl;
// 	return (0);
// }


int main()
{
	client clt(42, "127.0.0.1");

	sendNumeric(clt, "433", "bob", "Nickname is already in use");
	clt.modifie_nickname("bob");
	sendNumeric(clt, "001", "", "Welcome to the IRC network bob!bob@127.0.0.1");
	sendNumeric(clt, "461", "JOIN", "Not enough parameters");
	sendNumeric(clt, "482", "#general", "You're not channel operator");

	const std::string &out = clt.getOutBuffer();
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] == '\r')
			std::cout << "\\r";
		else if (out[i] == '\n')
			std::cout << "\\n" << std::endl;
		else
			std::cout << out[i];
	}
	return (0);
}