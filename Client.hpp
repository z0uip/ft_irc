/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 17:36:52 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/21 21:28:07 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <string>

class Client
{
	private:
		int _fd;
		std::string _ipA;
	public:
		Client();
		~Client();
		const std::string &getIp() const {return _ipA;}
		int getFd() const {return _fd;}
		
};