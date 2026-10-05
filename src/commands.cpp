#include "client.hpp"
#include "parsmessage.hpp"
#include "commands.hpp"


//void handlePass(Server &serv, client &clt, const std::vector<std::string> &params)
//{
//	if (params[0].empty())
//		sendNumeric(clt, "461", "", )
//}

void handleJoin(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2 || params[1].empty())
	{
		sendNumeric(clt, "461", params[0], "Not enough parameters");
		return;
	}
	std::string chanName = params[1];
	std::cout << "[DEBUG] Tentative de JOIN sur : " << chanName << std::endl;
	Channel *chan = serv.getChannel(chanName);

	if (chan == NULL)
	{
		chan = serv.createChannel(chanName);
		chan->addClient(&clt);
		chan->addOperator(&clt);//premier arrive est membre et operateur
	}
	else
		chan->addClient(&clt);

	//Format IRC : :<nickname>!<username>@<ip> JOIN <nom_du_salon>
	std::string joinMsg = ":" + clt.get_nickname() + "!" + clt.get_username() + "@" +clt.get_ip() + " JOIN " + chanName;
	chan->broadcast(joinMsg, NULL);
}