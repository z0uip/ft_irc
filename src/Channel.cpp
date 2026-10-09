/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/03 23:33:21 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 02:12:20 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Channel.hpp"

Channel::Channel(const std::string &name) : _name(name), _topic(""), _password(""),
_inviteOnly(false),
_topicRestricted(true), _isLimitActive(false), _maxUsers(0)
{
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

std::string Channel::getPassword() const
{
	return _password;
}

size_t Channel::getUserLimit() const
{
	return _maxUsers;
}

size_t Channel::getClientCount() const
{
	return _clients.size();
}

bool Channel::isInvited(const std::string &nickname) const
{
	for (size_t i = 0; i < _invitedUsers.size(); i++)
	{
		if (_invitedUsers[i] == nickname)
			return true;
	}
	return false;
}

//obtenir la liste des clients presents ds le channel et mettre un @ dvnt les admins
std::string Channel::getClientList() const
{
	std::string list = "";
	for (size_t i = 0; i < _clients.size(); i++)
	{
		if (isOperator(_clients[i]))
		{
			list += "@" + _clients[i]->get_nickname() + " ";
		}
		else
			list += _clients[i]->get_nickname() + " ";
	}
	return (list);
}

void Channel::set_topic(const std::string &new_topic)
{
	_topic = new_topic;
}

void Channel::addInvitedUser(const std::string &nickname)
{
	if (!isInvited(nickname))
		_invitedUsers.push_back(nickname);
}

void Channel::setInviteOnly(bool status)
{
	_inviteOnly = status;
}

void Channel::setTopicRestricted(bool status)
{
	_topicRestricted = status;
}

void Channel::setPassword(const std::string &password)
{
	_password = password;
}

void Channel::setUserLimit(size_t limit)
{
	_maxUsers = limit;
	if (limit > 0)
		_isLimitActive = true;
	else
		_isLimitActive = false;
}