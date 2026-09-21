/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:28:36 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/21 23:13:08 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include <sys/socket.h>
#include <stdexcept>

Server::Server(long port, const std::string &pwd) : _port(port), _pwd(pwd) {}

Server::~Server() {}

void Server::start()
{
	_servFd = socket(AF_INET, SOCK_STREAM, 0);//af_inet = communication pour le reseau des adresses ipv4
	if (_servFd == -1)
		throw std::runtime_error("Error while creating socket");//pr eviter de revenir ds le main
	bind()
}