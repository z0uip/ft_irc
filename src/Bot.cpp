#include "../headers/Bot.hpp"

#include "Bot.hpp"

Bot::Bot(const std::string &ip, int port, const std::string &password)
	: _socketFd(-1), _ip(ip), _port(port), _password(password), _nickname("ft_bot"), _channel("#42")
{
}

Bot::~Bot()
{
	if (_socketFd != -1)
	{
		close(_socketFd);
	}
}

void Bot::connectToServer()
{
	_socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_socketFd == -1)
		throw std::runtime_error("Error while creating ft_bot socket\n");
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;//la famille de notre socket donc ipv4
	addr.sin_port = htons(_port);//donne notre port et le converti au format reseau
	addr.sin_addr.s_addr = inet_addr(_ip.c_str());
	if (connect(_socketFd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		throw std::runtime_error("Error while ft_bot connection\n");
	authenticate();
}

void Bot::sendData(const std::string &msg)
{
	send(_socketFd, msg.c_str(), msg.length(), 0);
}

void Bot::authenticate()
{
	sendData("PASS " + _password + "\r\n");
	sendData("NICK " + _nickname + "\r\n");
	sendData("USER bot 0 * :I am a bot\r\n");
	sendData("JOIN " + _channel + "\r\n");
}

void Bot::run()
{
	char buffer[1024];
	while (true)
	{
		int bytes_read = recv(_socketFd, buffer, sizeof(buffer) - 1, 0);
		if (bytes_read <= 0)
		{
			std::cout << "disconnect from the server." << std::endl;
			break;
		}
		buffer[bytes_read] = '\0';
		std::string msg(buffer);
		handleMessage(msg);
	}
}

void Bot::handleMessage(const std::string &msg)
{
	std::cout << msg;
	if (msg.find(":" + _nickname + "!") != std::string::npos)//si on trv le pseudo du bot ds le msg on return
		return;
	if (msg.find("PRIVMSG " + _channel) != std::string::npos)
	{
		if (msg.find(":!ping") != std::string::npos)
		{
			sendData("PRIVMSG " + _channel + " :Pong !\r\n");
		}
		else if (msg.find(":!joke") != std::string::npos)
		{
			sendData("PRIVMSG " + _channel + " :There is no jokes.\r\n");
		}
		else if (msg.find(":!help") != std::string::npos)
		{
			sendData("PRIVMSG " + _channel + " :Available commands :  !ping (connexion test), !joke, !help (print this msg).\r\n");
		}
	}
}
