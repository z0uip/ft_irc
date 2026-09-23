/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:28:36 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/23 23:01:44 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <sys/socket.h>
#include <stdexcept>
#include <netinet/in.h>//sockadrr_in/htons
#include <fcntl.h>//fcntl() file control

Server::Server(long port, const std::string &pwd) : _port(port), _pwd(pwd) {}

Server::~Server() {}

void Server::start()
{			  //socket() = fonction qui demande a l'OS de creer un point d'acces reseau en memoire et recup son id (_servFd)
	_servFd = socket(AF_INET, SOCK_STREAM, 0);//af_inet = communication pour le reseau des adresses ipv4
	if (_servFd == -1)
		throw std::runtime_error("Error while creating socket");//pr eviter de revenir ds le main

	struct sockaddr_in addr;//struct contenant adresse ip et port pour ipv4
	addr.sin_family = AF_INET;//la famille de notre socket donc ipv4
	addr.sin_port = htons(_port);//donne notre port et le converti au format reseau
	addr.sin_addr.s_addr = htonl(INADDR_ANY); //ip sur laquelle ecouter les connexions INADDR_ANY dit a l'OS d'ecouter sur ttes les IP de la machine
	
	if (bind(_servFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)//Associe notre socket a notre port et IP pr que l'OS sache que les connexions entrant sur le port sont pr nous
		throw std::runtime_error("Error of bind functions");//signale l'erreur au reste du programme
		
	if (listen(_servFd, SOMAXCONN) == -1)//indique a l'OS d'accepter les connexions entrantes et de les mettre ds une file d'attente (1er var = num de socket 2e var taille de la file)
		throw std::runtime_error("Error while listening a socket");
	
	//F_SETFL = cmd pr definir les flags de servFd/ O_NONBLOCK rend les sockets non bloquants
	if (fcntl(_servFd, F_SETFL, O_NONBLOCK) == -1)//ajt un flag aux sockets pr les rendre non bloquant pr pas que ca bloque sur le recv d'un client qd yen a plusieurs en simultaner (empeche le serv de se figer sur la fonction accept())
		throw std::runtime_error("Error fcntl");
}
