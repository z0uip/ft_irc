#include "client.hpp"
#include "parsmessage.hpp"
#include "commands.hpp"


void handlePass(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", "PASS", "Not enough parameters.");
		return ;
	}
	else if (clt.is_saved())
	{
		sendNumeric(clt, "462", "", "You may not be register");
		return ;
	}
	else if (serv.getPassword() != params[1])
	{
		sendNumeric(clt, "464", "", "Wrong password.");
		return ;
	}
	else
		clt.set_pass_ok(true);
}

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

void handleNick(Server &serv, client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "431", "NICK", "Not enough paramters.");
		return ;
	}
	if (!is_valid_nick(params[1]))
	{
		sendNumeric(clt, "432", params[1], "Unvalid nickname.");
		return ;
	}
	client *other = serv.getClientByNick(params[1]);
	if (other != NULL && other != &clt)
	{
		sendNumeric(clt, "433", params[1], "Nickname is already in use");
		return ;
	}
	clt.modifie_nickname(params[1]);
	tryRegister(clt);
}

bool is_valid_nick(const std::string &nick)
{
	const std::string allowed =
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"0123456789"
		"[]\\`_^{|}-";
	if (nick.empty())
		return (false);
	else if (nick.size() > 9)
		return (false);
	else if (nick.find_first_not_of(allowed) != std::string::npos)
		return (false);
	else if (std::isdigit(nick[0]) || nick[0] == '-')
		return (false);
	return (true);
}

void handleUser(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 5)
		sendNumeric(clt, "461", "USER", "Not enough parameters.");
	else if (clt.is_saved())
		sendNumeric(clt, "462", "", "You may not register.");
	else
	{
		clt.modifie_username(params[1]);
		tryRegister(clt);
	}
}

/*fonction qui sert a la negociation des capacitees en mode est ce que y'a des options suplementaire sur le serveur*/
void handleCap(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
		sendNumeric(clt, "461", "CAP", "Not enough parameters");
	else if (params[1] == "LS")
		clt.queueMessage(":ircserv CAP * LS :");
}

void handlePing(client &clt, const std::vector<std::string> &params)
{
	if (params.size() < 2)
	{
		sendNumeric(clt, "461", "PING", "Not enough parameters");
		return ;
	}
	clt.queueMessage(":ircserv PONG ircserv :" + params[1]);
}

void handleQuit(client &clt)
{
	clt.set_has_leaved(true);
}

// void handlePrivmsg(Server &serv, client &clt, const std::vector<std::string> &params)
// {

// }