/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/07/16 09:21:48 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/21 21:40:53 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "Server.hpp"
#include <iostream>
#include <cstdlib>

int	main(int ac, char **av)
{
	if (ac != 3)//3 pr program name, port, mdp
	{
		std::cerr << "Wrong number of arguments brother" << std::endl;
		return (1);
	}
	char *endptr;
	long port = std::strtol(av[1], &endptr, 10);
	if (port < 1024 || port > 65535 || *endptr != '\0')
	{
		std::cerr << "port non valide" << std::endl;
		return (1);
	}
	std::string pwd = av[2];
	Server serv(port, pwd);
	serv.start();
}