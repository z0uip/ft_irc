/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:01:06 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 03:53:57 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <string>
#include <cstdlib>
#include <vector>
#include <poll.h>//poll function/POLLIN events
#include <map>
#include <csignal>
#include "client.hpp"
#include "Server.hpp"
#include "Channel.hpp"

class Server
{
	private:
		std::string _pwd;
		long _port;
		int _servFd;
		std::vector<struct pollfd> _pollFds;//pollfd plus ou moins une fiche pr un fd contenant le fd a surveiller l'events(POLLIN) et la reponse a levents
		std::map<int, client> _clients;
		std::map<std::string, Channel> _channels;//cle = nom du salon (ex : #general)
	public:
		static bool Signal;//static pr avoir une seule variable commune
		Server(long port, const std::string &pwd);
		~Server();

		void start();//initialisation reseau
		void acceptNewClient();
		void handleClientData(size_t &i);
		void sendClientData(size_t &i);
		void run();//methode contenant boucle infini du serveur/multiplexeur
		
		//getters
		const std::string &getPassword() const;
		client *getClientByNick(const std::string &nick);
		std::map<std::string, Channel> getChannelMap();

		//gestion des channels
		Channel* getChannel(const std::string &name);
		Channel* createChannel(const std::string &name);//pr creer un channel qd getChannel renvoie NULL
		
		//delete channel
		void removeChannel(const std::string &name);
};
