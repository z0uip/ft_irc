#include "client.hpp"
#include "parsmessage.hpp"

void parsmessage(std::string message)
{
	std::istringstream mess(message);
	std::vector<std::string> params;
	std::string command;
	std::string word;

	if (word[0] == ':')
		mess >> word;
	while (mess >> word)
	{
		if (word.find(":"))
		{
			params.push_back(word.substr(0, word.size()));
			while (mess >> word)
				params.back().append(word + " ");
			return ;
		}
		else
		{
			for (size_t i = 0; i < word.size(); ++i)
				word[i] = std::toupper(static_cast<unsigned char>(word[i]));
			
		}
	}
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

/*integret la class client au fonction et finir le dispatcher et le parsing message*/