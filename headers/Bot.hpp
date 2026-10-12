#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>

class Bot
{
	private:
		int _socketFd;
		std::string _ip;
		int _port;
		std::string _password;
		std::string _nickname;
		std::string _channel;
		void sendData(const std::string &msg);
		void handleMessage(const std::string &msg);
		void authenticate();
	public:
		Bot(const std::string &ip, int port, const std::string &password);
		~Bot();
		void connectToServer();
		void run();
};
