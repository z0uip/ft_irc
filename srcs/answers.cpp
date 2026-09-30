#include "client.hpp"

void sendNumeric(class client &clt, const std::string &code, const std::string &params, const std::string &text)
{
	std::string message;
	std::string target = clt.get_nickname();
	if (target.empty())
		target = "*";
	message = ":ircserv " + code + " " + target;
	if (!params.empty())
		message.append(" " + params);
	message.append(" :" + text);
	clt.queueMessage(message);
}
