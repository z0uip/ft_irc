/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/21 17:36:52 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/05 19:41:25 by abensaid         ###   ########.fr       */
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
		bool _pass_ok;
		std::string _ip;
		bool _prfl_saved;
		std::string _nickname;
		std::string _username;
		std::string _bufferin;
		std::string _bufferout;
		bool _has_leaved;
	public :
		bool is_saved() const;
		bool is_pass_ok() const;
		bool hasPendingOutput() const;
		void consumeOutPut(int n);
		std::string get_username() const;
		std::string get_nickname() const;
		const std::string &getOutBuffer() const;
		bool extract_line(std::string &line);
		client(int fd, const std::string &ip);
		void queueMessage(const std::string &msg);
		void modifie_username(const std::string name);
		void modifie_nickname(const std::string name);
		void add_byte(const char *data, size_t len);
		void set_pass_ok(bool info);
		void set_saved(bool info);
		std::string get_ip();
		void set_has_leaved(bool b);
};

#endif
