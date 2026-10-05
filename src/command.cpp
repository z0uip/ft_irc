#include "client.hpp"
#include "parsmessage.hpp"


void handlePass(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params[0].empty())
		sendNumeric(clt, "461", "", )
}