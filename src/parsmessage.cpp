/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parsmessage.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/30 17:26:43 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/05 17:38:33 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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


void dispatcher(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (!params.empty())
	{
		if (params[0] == "PASS")
			/*envoyer vers fonction PASS*/;
		else if (params[0] == "NICK")
			/*traiter en fonction*/;
		else if (params[0] == "USER")
			/*traiter en fonction*/;
		else if (params[0] == "CAP")
			/*traiter en fonction*/;
		else if (params[0] == "PING")
			/*traiter en foncion*/;
		else if (params[0] == "QUIT")
			/*traiter en fonction*/;
		else if (!clt.is_saved())
			sendNumeric(clt, "451", "", "You have not registered");
		else if (params[0] == "PRIVMSG")
			/*traiter en fonction*/;
		else if (params[0] == "JOIN")
			/*traiter en fonction*/;
		else if (params[0] == "KICK")
			/*traiter en fonction*/;
		else if (params[0] == "INVITE")
			/*traiter en fonction*/;
		else if (params[0] == "TOPIC")
			/*traiter en fonction*/;
		else if (params[0] == "MODE")
			/*traiter en fonction*/;
		else
			sendNumeric(clt, "421", params[0], "Unkown comand");
	}
	return ;
}
