/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   commands.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/05 22:57:14 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/09 02:50:07 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <vector>
#include <string>

class Server;
class client;

void handleQuit(client &clt);
bool is_valid_nick(const std::string &nick);
void handleCap(client &clt, const std::vector<std::string> &params);
void handlePing(client &clt, const std::vector<std::string> &params);
void handleUser(client &clt, const std::vector<std::string> &params);
//utils
std::vector<std::string> splitString(const std::string &str, char delimiter);
bool checkChannelModes(Channel *chan, client &clt, const std::string &key);
void displayChannelModes(client &clt, Channel *chan, const std::string &chanName);
void applyChannelModes(Server &serv, client &clt, Channel *chan, const std::vector<std::string> &params);

//cmds
void handlePass(Server &serv, client &clt, const std::vector<std::string> &params);
void handleJoin(Server &serv, client &clt, const std::vector<std::string> &params);
void handleNick(Server &serv, client &clt, const std::vector<std::string> &params);
void handleTopic(Server &serv, client &clt, const std::vector<std::string> &params);
void handleInvite(Server &serv, client &clt, const std::vector<std::string> &params);
void handleKick(Server &serv, client &clt, const std::vector<std::string> &params);
void handleMode(Server &serv, client &clt, const std::vector<std::string> &params);
void handlePrivmsg(Server &serv, client &clt, const std::vector<std::string> &params);
void handlePart(Server &serv, client &clt, const std::vector<std::string> &params);
