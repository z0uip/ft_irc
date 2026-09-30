/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:01:06 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/30 17:56:00 by abensaid         ###   ########.fr       */
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

class Server
{
	private:
		std::string _pwd;
		long _port;
		int _servFd;
		std::vector<struct pollfd> _pollFds;//pollfd plus ou moins une fiche pr un fd contenant le fd a surveiller l'events(POLLIN) et la reponse a levents
		std::map<int, client> _clients;
	public:
		static bool Signal;//static pr avoir une seule variable commune
		Server(long port, const std::string &pwd);
		~Server();
		void start();//initialisation reseau
		void acceptNewClient();
		void handleClientData(size_t &i);
		void run();//methode contenant boucle infini du serveur/multiplexeur
		
		
};