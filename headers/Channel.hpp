/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Channel.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/03 23:33:19 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 02:10:46 by abensaid         ###   ########.fr       */
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
		bool _isLimitActive;// Mode + l

		std::vector<client*> _clients;//liste des membres
		std::vector<client*> _operators;//liste des admins du salon
		
		size_t _maxUsers;
		std::vector<std::string> _invitedUsers;
	public:
		Channel(const std::string &name);
		~Channel();
		std::string get_name() const;
		std::string get_topic() const;
		std::string getPassword() const;
		size_t getUserLimit() const;
		size_t getClientCount() const;

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

		bool isInvited(const std::string &nickname) const;
		bool isInviteOnly() const { return _inviteOnly; }
		bool isTopicRestricted() const { return _topicRestricted; }
		bool hasUserLimit() const { return _isLimitActive; }

		std::string getClientList() const;
		//setter
		void set_topic(const std::string &new_topic);
		void addInvitedUser(const std::string &nickname);
		void setInviteOnly(bool status);
		void setTopicRestricted(bool status);
		void setPassword(const std::string &password);
		void setUserLimit(size_t limit);
};