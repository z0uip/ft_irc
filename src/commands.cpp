/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   commands.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/08 00:26:33 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 04:13:53 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "client.hpp"
#include "parsmessage.hpp"
#include "commands.hpp"
#include <cstdlib>

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

		// AJOUT : nom vide ou sans '#' -> channel invalide
		if (chanName.empty() || chanName[0] != '#')
		{
			sendNumeric(clt, "403", chanName, "No such channel");
			continue;
		}

		Channel *chan = serv.getChannel(chanName);
		if (chan == NULL)
		{
			chan = serv.createChannel(chanName);
			chan->addClient(&clt);
			chan->addOperator(&clt);//premier arrive est membre et operateur
		}
		else
		{
			// AJOUT : deja membre -> on ne refait rien
			if (chan->isClientInChannel(&clt))
				continue;
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
		sendNumeric(clt, "431", "", "Not enough paramters.");
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

void handleQuit(Server &serv, client &clt, const std::vector<std::string> &params)
{
	std::string reason;
	if (params.size() > 1)
		reason = params[1];
	else
		reason = "Client Quit";
	std::string msg = ":" + clt.get_nickname() + "!" + clt.get_username()
		+ "@" + clt.get_ip() + " QUIT :" + reason;
	std::map<std::string, Channel> map = serv.getChannelMap();
	std::map<std::string, Channel>::iterator it;
	std::vector<std::string> toDelete;
	for (it = map.begin(); it != map.end(); ++it)
	{
		Channel *chan = &it->second;
		if (chan->isClientInChannel(&clt))
		{
			chan->removeClient(&clt);
			chan->removeOperator(&clt);
			chan->broadcast(msg, NULL);
			if (chan->getClientCount() == 0)
				toDelete.push_back(it->first);
		}
	}
	for (size_t i = 0; i < toDelete.size(); i++)
		serv.removeChannel(toDelete[i]);
	clt.set_has_leaved(true);
}

void handlePrivmsg(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2 || params[1].empty())
		sendNumeric(clt, "411", "", "No recipient given(PRIVMSG).");
	else if (params.size() < 3 || params[2].empty())
		sendNumeric(clt, "412", "", "No text to send.");
	else
	{
		std::string msg = ":" + clt.get_nickname() + "!" + clt.get_username()
			+ "@" +clt.get_ip() + " PRIVMSG " + params[1] + " :" + params[2];
		if (params[1][0] == '#')
		{
			Channel *chan = serv.getChannel(params[1]);
			if (!chan)
			{
				sendNumeric(clt, "403", params[1], "No such channel.");
				return ;
			}
			if (chan->isClientInChannel(&clt))
				chan->broadcast(msg, &clt);
			else
				sendNumeric(clt, "404", params[1], "Cannot send to channel.");
			return ;
		}
		else
		{
			client *target = serv.getClientByNick(params[1]);
			if (!target)
				sendNumeric(clt, "401", params[1], "No such nick.");
			else
				target->queueMessage(msg);
		}
	}
}

void handleTopic(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	std::string chanName = params[1];
	Channel *chan = serv.getChannel(chanName);
	if (chan == NULL)
	{
		sendNumeric(clt, "403", chanName, "No such channel");
		return;
	}
	if (chan->isClientInChannel(&clt) == false)//si le client est co au serv mais n'a pas fait join sur le salon
	{
		sendNumeric(clt, "442", chanName, "You're not on that channel");
		return;
	}
	if (params.size() == 2)
	{
		if (chan->get_topic().empty())
		{
			sendNumeric(clt, "331", chanName, "No topic is set");
		}
		else
			sendNumeric(clt, "332", chanName, chan->get_topic());
		return;
	}
	if (chan->isTopicRestricted() && !chan->isOperator(&clt))
	{
		sendNumeric(clt, "482", chanName, "You're not channel operator");
		return;
	}

	chan->set_topic(params[2]);
	std::string topicMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" + clt.get_ip() + " TOPIC " + chanName + " :" + params[2];
	chan->broadcast(topicMsg, NULL);
}

void handleInvite(Server &serv, client &clt, const std::vector<std::string> &params)
{
	//params[1] = cible, params 2 = salon
	if (params.size() < 3)
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	client *target = serv.getClientByNick(params[1]);
	if (target == NULL)
	{
		sendNumeric(clt, "401", params[1], "No such nick/channel");
		return;
	}
	Channel *chan = serv.getChannel(params[2]);
	if (chan == NULL)
	{
		sendNumeric(clt, "403", params[2], "No such channel");
		return;
	}
	if (!chan->isClientInChannel(&clt))
	{
		sendNumeric(clt, "442", params[2], "You're not on that channel");
		return;
	}
	if (chan->isClientInChannel(target))
	{
		sendNumeric(clt, "443", params[1] + " " + params[2], "is already on channel");
		return;
	}
	if (chan->isInviteOnly() && !chan->isOperator(&clt))
	{
		sendNumeric(clt, "482", params[2], "You're not channel operator");
		return;
	}

	chan->addInvitedUser(params[1]);
	sendNumeric(clt, "341", params[1] + " " + params[2], "");
	target->queueMessage(":" + clt.get_nickname() + "!" + clt.get_username() + "@" + clt.get_ip() + " INVITE " + target->get_nickname() + " :" + params[2]);
}

void handleKick(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 3)
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	Channel *chan = serv.getChannel(params[1]);
	if (chan == NULL)
	{
		sendNumeric(clt, "403", params[1], "No such channel");
		return;
	}
	if (!chan->isClientInChannel(&clt))
	{
		sendNumeric(clt, "442", params[1], "You're not on that channel");
		return;
	}
	if (!chan->isOperator(&clt))
	{
		sendNumeric(clt, "482", params[1], "You're not channel operator");
		return;
	}
	client *target = serv.getClientByNick(params[2]);
	if (target == NULL || !chan->isClientInChannel(target))
	{
		sendNumeric(clt, "441", params[2] + " " + params[1], "They aren't on that channel");
		return;
	}
	std::string reason;
	if (params.size() >= 4)
		reason = params[3];
	else
		reason = clt.get_nickname();
	std::string kickMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" + clt.get_ip() + " KICK " + params[1] + " " + target->get_nickname() + " :" + reason;
	chan->broadcast(kickMsg, NULL);
	chan->removeOperator(target);
	chan->removeClient(target);
}

//qd un utilisateur fait MODE #nom_du_salon il doit voir les modes actifs sur celui-ci
void displayChannelModes(client &clt, Channel *chan, const std::string &chanName)
{
	std::string modes = "+";//contient les modes actifs
	std::string modeParams = "";//contient les params des modes si besoin
	if (chan->isInviteOnly())
	{
		modes += "i";
	}
	if (chan->isTopicRestricted())
	{
		modes += "t";
	}
	if (chan->hasUserLimit())
	{
		modes += "l";
		std::ostringstream ss;//pr convertir le nombre en str
		ss << chan->getUserLimit();
		modeParams += " " + ss.str();
	}
	if (!chan->getPassword().empty())
	{
		modes += "k";
		modeParams += " " + chan->getPassword();
	}
	if (modes == "+")
	{
		modes = "";//supp le signe + si ya aucun mode
	}
	sendNumeric(clt, "324", chanName, modes + modeParams);
}

void handleMode(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	Channel *chan = serv.getChannel(params[1]);
	if (chan == NULL)
	{
		sendNumeric(clt, "403", params[1], "No such Channel");
		return;
	}
	if (params.size() == 2)
	{
		displayChannelModes(clt, chan, params[1]);
		return;
	}
	if (!chan->isOperator(&clt))
	{
		sendNumeric(clt, "482", params[1], "You're not operator on that channel");
		return;
	}
	applyChannelModes(serv, clt, chan, params);
}

void applyChannelModes(Server &serv, client &clt, Channel *chan, const std::vector<std::string> &params)
{
	std::string modeString = params[2];//contient les lettres et signes a parcourir
	char sign = '+';//memorise si on active ou desactive les modes
	size_t argIdx = 3;

	std::string appliedModes = ""; 
	std::string appliedParams = "";

	for (size_t i = 0; i < modeString.size(); i++)
	{
		char c = modeString[i];

		if (c == '+' || c == '-')
		{
			sign = c;
			appliedModes += c;
			continue;
		}

		if (c == 'i')
		{
			chan->setInviteOnly(sign == '+');
			appliedModes += c;
		}
		else if (c == 't')
		{
			chan->setTopicRestricted(sign == '+');
			appliedModes += c;
		}
		else if (c == 'k')
		{
			if (sign == '+')
			{
				if (argIdx < params.size())
				{
					chan->setPassword(params[argIdx]);
					appliedModes += c;
					appliedParams += " " + params[argIdx];
					argIdx++;
				}
			}
			else if (sign == '-')
			{
				chan->setPassword("");
				appliedModes += c;
			}
		}
		else if (c == 'l')
		{
			if (sign == '+')
			{
				if (argIdx < params.size())
				{
					int limit = std::atoi(params[argIdx].c_str());
					if (limit > 0)
					{
						chan->setUserLimit(limit);
						appliedModes += c;
						appliedParams += " " + params[argIdx];
					}
					argIdx++;
				}
			}
			else if (sign == '-')
			{
				chan->setUserLimit(0);
				appliedModes += c;
			}
		}
		else if (c == 'o')
		{
			if (argIdx < params.size())
			{
				client *target = serv.getClientByNick(params[argIdx]);
				if (target != NULL && chan->isClientInChannel(target))
				{
					if (sign == '+')
						chan->addOperator(target);
					else
						chan->removeOperator(target);
					appliedModes += c;
					appliedParams += " " + params[argIdx];
				}
				argIdx++;
			}
		}
	}

	if (appliedModes.empty() || appliedModes == "+" || appliedModes == "-")
		return;

	std::string modeMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" + clt.get_ip() + " MODE " + params[1] + " " + appliedModes + appliedParams;
	chan->broadcast(modeMsg, NULL);
}

void handlePart(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	std::vector<std::string> channels = splitString(params[1], ',');
	std::string reason = clt.get_nickname();
	if (params.size() >= 3)
		reason = params[2];

	for (size_t i = 0; i < channels.size(); i++)
	{
		std::string chanName = channels[i];
		Channel *chan = serv.getChannel(chanName);

		if (chan == NULL)
		{
			sendNumeric(clt, "403", chanName, "No such channel");
			continue;//on passe au salon suivant
		}
		if (!chan->isClientInChannel(&clt))
		{
			sendNumeric(clt, "442", chanName, "You're not on that channel");
			continue;
		}
		std::string partMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" + clt.get_ip() + " PART " + chanName + " :" + reason;
		chan->broadcast(partMsg, NULL);
		chan->removeOperator(&clt);
		chan->removeClient(&clt);
	}
}
