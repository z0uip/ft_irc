/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:28:36 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 04:00:34 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../headers/Server.hpp"
#include "../headers/client.hpp"
#include "../headers/parsmessage.hpp"
#include <sys/socket.h>
#include <stdexcept>
#include <netinet/in.h>//sockadrr_in/htons
#include <fcntl.h>//fcntl() file control
#include <csignal>

bool Server::Signal = false;

void signalHandler(int signum)
{
	(void)signum;
	std::cout << "\nSignal received, server shut down..." << "\n";
	Server::Signal = true;
}

Server::Server(long port, const std::string &pwd) : _pwd(pwd), _port(port) {}

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

void Server::acceptNewClient()
{
	struct sockaddr_in clientAddr;
	socklen_t clientAddrSize = sizeof(clientAddr);
	int clientFd = accept(_servFd, (struct sockaddr *)&clientAddr, &clientAddrSize);//sert a recup une connexion entrante et creer un nv socket pr la communication avec
	if (clientFd == -1)
	{
		std::cerr << "Connexion error from accept()" << "\n";//pas de throw, on veut pas que le serveur entier plante si 1 prsn narrive pas a se co
		return;
	}
	std::string clientIP = inet_ntoa(clientAddr.sin_addr);
	_clients.insert(std::make_pair(clientFd, client(clientFd, clientIP)));//insert cette pair ds std::map
	fcntl(clientFd, F_SETFL, O_NONBLOCK);
	struct pollfd clientpollfd;
	clientpollfd.fd = clientFd;
	clientpollfd.events = POLLIN;
	clientpollfd.revents = 0;
	_pollFds.push_back(clientpollfd);
	std::cout << "New connexion : FD = " << clientFd << " from " << clientIP << "\n";
}

void Server::handleClientData(size_t &i)//&i parce qu'on veut modifier le i de la boucle for dans run()
{
	char buf[1024];
	ssize_t res = recv(_pollFds[i].fd, buf, sizeof(buf), 0);//lis les donnees dispo sur le socket _pollFds et les mets dans buf sans depasser sa limite

	if (res > 0)
	{
		std::map<int, client>::iterator it = _clients.find(_pollFds[i].fd);//it pointe vers le client correspondant au FD qui vient de recevoir des données avc recv
		if (it != _clients.end())//si find() a trouver le client
		{
			it->second.add_byte(buf, res);//ajoute les données reçues au buffer d'entrée du client correspondant pour reconstituer une cmd complete
			std::string line;
			while (it->second.extract_line(line))//extrait jusqu'au \n
			{
				std::vector<std::string> params	= parsmessage(line);
				dispatcher(*this, it->second, params);//*this = l'objet Server
			}
		}
		std::cout << "Client " << _pollFds[i].fd << " a envoyé " << res << " octets.\n";
	}

	else if (res == 0)//si le client a fermer sa connexion
	{
		std::cout << "Client " << _pollFds[i].fd << " déconnecté.\n";
		close(_pollFds[i].fd);//ferme le socket associe au fd
		_clients.erase(_pollFds[i].fd);
		_pollFds.erase(_pollFds.begin() + i);//supp le fd du vecteur
		i--;//on recule pr ne pas rater le client qui a etait decaler
	}

	else if (res == -1)
		std::cerr << "Recv error" << "\n";
}

void Server::sendClientData(size_t &i)
{
	std::map<int, client>::iterator it = _clients.find(_pollFds[i].fd);
	if (it != _clients.end())
	{
		std::string msg = it->second.getOutBuffer();//recup le texte a envoyer (bufferout)
		ssize_t bytes_sent = send(_pollFds[i].fd, msg.c_str(), msg.size(), 0);//serveur envoie des donnees au client
		if (bytes_sent > 0)
			it->second.consumeOutPut(bytes_sent);//on nettoie les octets lus
		else if (bytes_sent == -1)
			std::cerr << "Send error\n";
	}
}

void Server::run()
{	//signal(signal a gerer, fonction a appeler)
	signal(SIGINT, signalHandler);//SIGINT = signal envoyer au programme pr qu'il stop
	signal(SIGQUIT, signalHandler);
	
	while (Server::Signal == false)
	{
		std::cout << "Waiting for connection" << "\n";//msg tmporaire pr debug
		for (size_t i = 1; i < _pollFds.size(); i++)//i = 1 psk 0 = servfd
		{
			std::map<int, client>::iterator it = _clients.find(_pollFds[i].fd);
			if (it != _clients.end())
			{
				if ((it->second.hasPendingOutput()) == true)///verifie si le serveur a des donnees à envoyer à ce client
					_pollFds[i].events = POLLIN | POLLOUT;// pollout = socket pret a accepter une ecriture
				else
					_pollFds[i].events = POLLIN;
			}
		}

		if (poll(&_pollFds[0], _pollFds.size(), -1) == -1)//1 = adresse de debut du vecteur, 2 la taille, 3le temps d'attente -1 = infini
		{
			if (Server::Signal == true)//verif de la cause de l'erreur de poll()
				break;
			throw std::runtime_error("Poll error\n");
		}

		for (size_t i = 0; i < _pollFds.size(); i++)//size_t psk size() renv un size_t
		{
			if (_pollFds[i].revents & POLLIN)//si on detecte des donnes a lire, & psk on peut avoir plusieurs events en mm temps
			{
				if (_pollFds[i].fd == _servFd)
					acceptNewClient();
				else
					handleClientData(i);
			}
			if (_pollFds[i].revents & POLLOUT)
			{
				sendClientData(i);
			}
		}
	}
	for (size_t i = 0; i < _pollFds.size(); i++)
	{
		close(_pollFds[i].fd);
	}
}

const std::string &Server::getPassword() const
{
	return _pwd;
}

client *Server::getClientByNick(const std::string &nick)
{
	std::map<int, client>::iterator it;

	for (it = _clients.begin(); it != _clients.end(); ++it)
	{
		if (it->second.get_nickname() == nick)
			return (&it->second);
	}
	return (NULL);
}

//Logique des channels
Channel* Server::getChannel(const std::string &name)
{
	std::map<std::string, Channel>::iterator it = _channels.find(name);
	if (it != _channels.end())
	{
		return &(it->second);//on return l'adresse memoire de l'objet Channel
	}
	return NULL;
}

Channel* Server::createChannel(const std::string &name)
{
	Channel tmp_channel(name);
	_channels.insert(std::make_pair(name, tmp_channel));
	return getChannel(name);
}
