/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   commands.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/08 00:26:33 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/08 02:49:33 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

void handlePass(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", "PASS", "Not enough parameters.");
		return ;
	}
	else if (clt.is_saved())
	{
		sendNumeric(clt, "462", "", "You may not be register");
		return ;
	}
	else if (serv.getPassword() != params[1])
	{
		sendNumeric(clt, "464", "", "Wrong password.");
		return ;
	}
	else
		clt.set_pass_ok(true);
}

bool checkChannelModes(Channel *chan, client &clt, const std::string &key)
{
	//Mode +k
	if (chan->getPassword() != "")
	{
		if (chan->getPassword() != key)
		{
			sendNumeric(clt, "475", chan->get_name(), "Cannot join channel (+k)");
			return false;
		}
	}

	//Mode +l
	if (chan->hasUserLimit() == true)
	{
		if (chan->getClientCount() >= chan->getUserLimit())
		{
			sendNumeric(clt, "471", chan->get_name(), "Cannot join channel (+l)");
			return false;
		}
	}

	//Mode +i
	if (chan->isInviteOnly() == true)
	{
		if (!chan->isInvited(clt.get_nickname()))
		{
			sendNumeric(clt, "473", chan->get_name(), "Cannot join channel (+i)");
			return false;
		}

	}
	return true;
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
				continue;//si le client est refuser d'un channel on passe au suivant
			chan->addClient(&clt);
		}

		//Format IRC : :<nickname>!<username>@<ip> JOIN <nom_du_salon>
		std::string joinMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" +clt.get_ip() + " JOIN " + chanName;
		chan->broadcast(joinMsg, NULL);
		if (chan->get_topic().empty())
		{
			sendNumeric(clt, "331", chanName, "No topic is set");
		}
		else
			sendNumeric(clt, "332", chanName, chan->get_topic());
		sendNumeric(clt, "353", "= " + chanName, chan->getClientList());
		sendNumeric(clt, "366", chanName, "End of /NAMES list");
	}
}

void handleNick(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "431", "NICK", "Not enough paramters.");
		return ;
	}
	if (!is_valid_nick(params[1]))
	{
		sendNumeric(clt, "432", params[1], "Unvalid nickname.");
		return ;
	}
	client *other = serv.getClientByNick(params[1]);
	if (other != NULL && other != &clt)
	{
		sendNumeric(clt, "433", params[1], "Nickname is already in use");
		return ;
	}
	clt.modifie_nickname(params[1]);
	tryRegister(clt);
}

bool is_valid_nick(const std::string &nick)
{
	const std::string allowed =
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"[]\\`_^{|}-";
	if (nick.empty())
		return (false);
	else if (nick.size() > 9)
		return (false);
	else if (nick.find_first_not_of(allowed) != std::string::npos)
		return (false);
	else if (std::isdigit(nick[0]) || nick[0] == '-')
		return (false);
	return (true);
}

void handleUser(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 5)
		sendNumeric(clt, "461", "USER", "Not enough parameters.");
	else if (clt.is_saved())
		sendNumeric(clt, "462", "", "You may not register.");
	else
	{
		clt.modifie_username(params[1]);
		tryRegister(clt);
	}
}

/*fonction qui sert a la negociation des capacitees en mode est ce que y'a des options suplementaire sur le serveur*/
void handleCap(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		sendNumeric(clt, "461", "CAP", "Not enough parameters");
	else if (params[1] == "LS")
		clt.queueMessage(":ircserv CAP * LS :");
}

void handlePing(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", "PING", "Not enough parameters");
		return ;
	}
	clt.queueMessage(":ircserv PONG ircserv :" + params[1]);
}

void handleQuit(client &clt)
{
	clt.set_has_leaved(true);
}

void handlePrivmsg(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 3)
		sendNumeric(clt, "461", "PRIVMSG", "Not enough parameters.");
	else if (params[1].empty() || serv.getClientByNick(params[1]) == NULL)
		sendNumeric(clt, "411", "", "No recipient given(PRIVMSG).");
	else if (params[2].empty())
		sendNumeric(clt, "412", "", "No text to send.");
	else
	{
		
	}
}