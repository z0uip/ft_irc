/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 21:01:06 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/21 22:47:55 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <string>
#include <cstdlib>

class Server
{
	private:
		std::string _pwd;
		long _port;
		int _servFd;
	public:
		Server(long port, const std::string &pwd);
		~Server();
		void start();
};