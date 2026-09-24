/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:28:36 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/24 22:13:15 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../inc/Server.hpp"
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
		throw std::runtime_error("Error while creating socket\n");//pr eviter de revenir ds le main

	struct sockaddr_in addr;//struct contenant adresse ip et port pour ipv4
	addr.sin_family = AF_INET;//la famille de notre socket donc ipv4
	addr.sin_port = htons(_port);//donne notre port et le converti au format reseau
	addr.sin_addr.s_addr = htonl(INADDR_ANY); //ip sur laquelle ecouter les connexions INADDR_ANY dit a l'OS d'ecouter sur ttes les IP de la machine
	
	if (bind(_servFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)//Associe notre socket a notre port et IP pr que l'OS sache que les connexions entrant sur le port sont pr nous
		throw std::runtime_error("Error of bind functions\n");//signale l'erreur au reste du programme
		
	if (listen(_servFd, SOMAXCONN) == -1)//indique a l'OS d'accepter les connexions entrantes et de les mettre ds une file d'attente (1er var = num de socket 2e var taille de la file)
		throw std::runtime_error("Error while listening a socket\n");
	
	//F_SETFL = cmd pr definir les flags de servFd/ O_NONBLOCK rend les sockets non bloquants
	if (fcntl(_servFd, F_SETFL, O_NONBLOCK) == -1)//ajt un flag aux sockets pr les rendre non bloquant pr pas que ca bloque sur le recv d'un client qd yen a plusieurs en simultaner (empeche le serv de se figer sur la fonction accept())
		throw std::runtime_error("Error fcntl\n");
	struct pollfd servpollfd;//var tmp pr le serveur
	servpollfd.fd = _servFd;//on ajt le socket serveur aux fd surveillés par poll()
	servpollfd.events = POLLIN;//POLLIN = dmd a poll() de prevenir qd le socket de _servFd est pret a etre lu (nvl connexion)
	servpollfd.revents = 0;//sera rempli par poll() avc les evenements qui se sont produits
	_pollFds.push_back(servpollfd);//on l'envoie au fond de notre tableau de fd
}

void Server::run()
{
	while (true)
	{
		std::cout << "Waiting for connection" << "\n";//msg tmporaire pr debug
		if (poll(&_pollFds[0], _pollFds.size(), -1) == -1)//1 = adresse de debut du vecteur, 2 la taille, 3le temps d'attente -1 = infini
			throw std::runtime_error("Poll error\n");
		for (size_t i = 0; i < _pollFds.size(); i++)//size_t psk size() renv un size_t
		{
			if (_pollFds[i].revents & POLLIN)//si on detecte des donnes a lire, & psk on peut avoir plusieurs events en mm temps
			{
				if (_pollFds[i].fd == _servFd)
				{
					std::cout << "New connexion !" << "\n";
					//rajouter accept() j'espere que tu auras reussi a cop Dieudonne et la nvl vitrine
				}
				else
				{
					std::cout << "Msg from a client !" << "\n";

				}
			}
		}
	}
}
