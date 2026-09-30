/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   answers.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/30 19:57:39 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/30 19:57:44 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "client.hpp"

void sendNumeric(class client &clt, const std::string &code, const std::string &params, const std::string &text)
{
	std::string message;
	std::string target = clt.get_nickname();
	if (target.empty())
		target = "*";
	message = ":ircserv " + code + " " + target;
	if (!params.empty())
		message.append(" " + params);
	message.append(" :" + text);
	clt.queueMessage(message);
}
