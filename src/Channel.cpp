/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/03 23:33:21 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/05 23:16:01 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel(const std::string &name) : _name(name), _topic(""), _password(""),
_inviteOnly(false),
_topicRestricted(true),	_userLimit(0)
{
	_inviteOnly = false;
	_topicRestricted = false;
	_userLimit = false;
}

Channel::~Channel()
{
}

std::string Channel::get_name() const
{
	return _name;
}

std::string Channel::get_topic() const
{
	return _topic;
}

bool Channel::isClientInChannel(client *clt) const
{
	for (size_t i = 0; i < _clients.size(); i++)
	{
		if (_clients[i] == clt)
			return true;
	}
	return false;
}

void Channel::addClient(client *clt)
{
	if (isClientInChannel(clt) == true)
		return;
	_clients.push_back(clt);
}

void Channel::removeClient(client *clt)
{
	std::vector<client*>::iterator it;//on creer un iterateur psk erase() refuse les index
	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (*it == clt)//verif si le pointeur correspond au client en memoire
		{
			_clients.erase(it);
			return;
		}
	}
}

bool Channel::isOperator(client *clt) const
{
	for (size_t i = 0; i < _operators.size(); i++)
	{
		if (_operators[i] == clt)
			return true;
	}
	return false;
}

void Channel::addOperator(client *clt)
{
	if (isOperator(clt) == true)
		return;
	_operators.push_back(clt);
}

void Channel::removeOperator(client *clt)
{
	std::vector<client*>::iterator it;//on creer un iterateur psk erase() refuse les index
	for (it = _operators.begin(); it != _operators.end(); ++it)
	{
		if (*it == clt)//verif si le pointeur correspond au client en memoire
		{
			_operators.erase(it);
			return;
		}
	}
}

void Channel::broadcast(const std::string &msg, client *exclude)
{
	for (size_t i = 0; i < _clients.size(); i++)
	{
		if (_clients[i] != exclude)//on exclue le client qui a envoyer le msg
		{
			_clients[i]->queueMessage(msg);//on met le msg ds le buffer de sortie de chaque client
		}
	}
}
