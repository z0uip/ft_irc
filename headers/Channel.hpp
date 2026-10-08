/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/03 23:33:19 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/05 23:16:57 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "client.hpp"

class Channel
{
	private:
		std::string _name;//nom du salon (ex #general)
		std::string _topic;//sujet du salon
		std::string _password;//Mode +k dmd un mdp avant de rentrer ds le salon

		bool _inviteOnly;//Mode +i sur invitation
		bool _topicRestricted;//Mode +t seul l'admin du salon change le topic
		bool _userLimit;//Mode +l nb max de user

		std::vector<client*> _clients;//liste des membres
		std::vector<client*> _operators;//liste des admins du salon
	public:
		Channel(const std::string &name);
		~Channel();
		std::string get_name() const;
		std::string get_topic() const;

		//Gestion des clients
		void addClient(client *clt);
		void removeClient(client *clt);
		bool isClientInChannel(client *clt) const;

		//Gestion des operateurs
		void addOperator(client *clt);
		void removeOperator(client *clt);
		bool isOperator(client *clt) const;

		//diffuse un msg aux autres clients presents ds le salon
		void broadcast(const std::string &msg, client *exclude);

		bool isInviteOnly() const { return _inviteOnly; }
		bool isTopicRestricted() const { return _topicRestricted; }
		bool hasUserLimit() const { return _userLimit; }
};