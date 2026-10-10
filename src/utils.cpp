/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/10 00:48:10 by fbenech           #+#    #+#             */
/*   Updated: 2026/10/10 00:51:55 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "commands.hpp"
#include "client.hpp"
#include "parsmessage.hpp"
#include <cstdlib>

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
