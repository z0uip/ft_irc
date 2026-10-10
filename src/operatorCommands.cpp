/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   operatorCommands.cpp                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/10 00:59:56 by fbenech           #+#    #+#             */
/*   Updated: 2026/10/10 01:18:59 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "client.hpp"
#include "parsmessage.hpp"
#include "commands.hpp"
#include <cstdlib>

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
