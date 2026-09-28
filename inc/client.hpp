/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 17:36:52 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/29 00:27:20 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <exception>
#include <cstring>
#include <string>
#include <stdexcept>
#include <cerrno>
#include <sstream>
#include <vector>

class client
{
	private :
		int _fd;
		bool _prfl_saved;
		bool _prfl_valid;
		std::string _bufferin;
		std::string _bufferout;
		std::string _password;
		std::string _name;
	public :
		std::string get_password();
		std::string get_name();
		void modifie_password(const std::string password);
		void modifie_name(const std::string name);
		void add_byte(const char *data, size_t len);
		std::string extract_line();
		bool is_valid();
		bool is_saved();
		client(int fd, std::string name, std::string password);
		void queueMessage(const std::string &msg);
		bool hasPendingOutput();
		std::string getOutBuffer();
		void consumeOutPut(int n);
};

#endif