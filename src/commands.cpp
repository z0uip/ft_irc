#include "client.hpp"
#include "parsmessage.hpp"
#include "commands.hpp"

std::vector<std::string> splitString(const std::string &str, char delimiter)
{
	std::vector<std::string> result;
	size_t start = 0;
	size_t end = str.find(delimiter);
	while (end != std::string::npos)
	{
		result.push_back(str.substr(start, end - start));
		start = end + 1;
		end = str.find(delimiter, start);
	}
	result.push_back(str.substr(start)); // Ajoute le dernier élément
	return result;
}

//void handlePass(Server &serv, client &clt, const std::vector<std::string> &params)
//{
//	if (params[0].empty())
//		sendNumeric(clt, "461", "", )
//}

bool checkChannelModes(Channel *chan, client &clt, const std::string &key)
{
	//faire Mode +k puis +l puis +i
}

void handleJoin(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2 || params[1].empty())
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}

	//logique split si le client veut rejoindre plusieurs salon en mm temps
	std::vector<std::string> channels = splitString(params[1], ',');
	std::vector<std::string> keys;//si y'a des mdp
	if (params.size() > 2)
		keys = splitString(params[2], ',');

	for (size_t i = 0; i < channels.size(); i++)
	{
		std::string chanName = channels[i];
		std::string key;
		if (i < keys.size())
			key = keys[i];
		else
			key = "";

		std::cout << "[DEBUG] Tentative de JOIN sur : " << chanName << " (Clé: " << key << ")" << std::endl;
		Channel *chan = serv.getChannel(chanName);

		if (chan == NULL)
		{
			chan = serv.createChannel(chanName);
			chan->addClient(&clt);
			chan->addOperator(&clt);//premier arrive est membre et operateur
		}
		else
		{
			if (!checkChannelModes(chan, clt, key))
				continue;
			chan->addClient(&clt);
		}

		//Format IRC : :<nickname>!<username>@<ip> JOIN <nom_du_salon>
		std::string joinMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" +clt.get_ip() + " JOIN " + chanName;
		chan->broadcast(joinMsg, NULL);
	}
}
