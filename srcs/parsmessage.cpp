#include "client.hpp"
#include "parsmessage.hpp"

std::vector<std::string> parsmessage(std::string message)
{
	std::vector<std::string> params;
	bool addlast = false;
	std::string word;
	std::string last;
	size_t pos = message.find(" :");

	if (message.empty())
		return (params);
	if (pos != std::string::npos)
	{
		last = message.substr(pos + 2);
		message = message.substr(0, pos);
		addlast = true;
	}
	std::istringstream mess(message);
	if (!(mess >> word))
		return (params);
	if (word[0] == ':')
	{
		if (!(mess >> word))
			return (params);
	}
	for (size_t i = 0; i < word.size(); ++i)
		word[i] = std::toupper(static_cast<unsigned char>(word[i]));
	params.push_back(word);
	while (mess >> word)
		params.push_back(word);
	if (addlast)
		params.push_back(last);
	return (params);
}


int dispatcher(std::string word)
{
	if (word == "PASS")
		/*envoyer vers fonction PASS*/;
	else if (word == "NICK")
		/*traiter en fonction*/;
	else if (word == "USER")
		/*traiter en fonction*/;
	else if (word == "JOIN")
		/*traiter en fonction*/;
	else if (word == "PRIVMSG")
		/*traiter en fonction*/;
	else if (word == "KICK")
		/*traiter en fonction*/;
	else if (word == "INVITE")
		/*traiter en fonction*/;
	else if (word == "TOPIC")
		/*traiter en fonction*/;
	else if (word == "MODE")
		/*traiter en fonction*/;
	else if (word == "QUIT")
		/*traiter en fonction*/;
	else
		return (0);
	return (1);
}