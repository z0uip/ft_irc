/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   commands.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/05 22:57:14 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/10 01:19:04 by fbenech          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <vector>
#include <string>
#include "Channel.hpp"

class Server;
class client;


//utils
bool is_valid_nick(const std::string &nick);
std::vector<std::string> splitString(const std::string &str, char delimiter);
bool checkChannelModes(Channel *chan, client &clt, const std::string &key);
void displayChannelModes(client &clt, Channel *chan, const std::string &chanName);
void applyChannelModes(Server &serv, client &clt, Channel *chan, const std::vector<std::string> &params);

//userCommands
void handleQuit(Server &serv, client &clt, const std::vector<std::string> &params);
void handleCap(client &clt, const std::vector<std::string> &params);
void handlePing(client &clt, const std::vector<std::string> &params);
void handleUser(client &clt, const std::vector<std::string> &params);
void handlePass(Server &serv, client &clt, const std::vector<std::string> &params);
void handleJoin(Server &serv, client &clt, const std::vector<std::string> &params);
void handleNick(Server &serv, client &clt, const std::vector<std::string> &params);
void handlePrivmsg(Server &serv, client &clt, const std::vector<std::string> &params);
void handlePart(Server &serv, client &clt, const std::vector<std::string> &params);

//operatorCommands
void handleInvite(Server &serv, client &clt, const std::vector<std::string> &params);
void handleMode(Server &serv, client &clt, const std::vector<std::string> &params);
void handleKick(Server &serv, client &clt, const std::vector<std::string> &params);
void handleTopic(Server &serv, client &clt, const std::vector<std::string> &params);