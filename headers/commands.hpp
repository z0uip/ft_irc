/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   commands.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fbenech <fbenech@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/10/05 22:57:14 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/08 22:48:12 by fbenech          ###   ########.fr       */
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

//cmds
void handlePass(Server &serv, client &clt, const std::vector<std::string> &params);
void handleJoin(Server &serv, client &clt, const std::vector<std::string> &params);
void handleNick(Server &serv, client &clt, const std::vector<std::string> &params);
void handleTopic(Server &serv, client &clt, const std::vector<std::string> &params);
void handleInvite(Server &serv, client &clt, const std::vector<std::string> &params);
