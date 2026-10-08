/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parsmessage.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/30 17:26:40 by abensaid          #+#    #+#             */
/*   Updated: 2026/10/05 19:25:48 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PARSMESSAGE_HPP
#define PARSMESSAGE_HPP

#include "client.hpp"
#include "Server.hpp"


std::vector<std::string> parsmessage(std::string message);
void dispatcher(Server &serv, client &clt, const std::vector<std::string> &params);
void sendNumeric(class client &clt, const std::string &code, const std::string &params, const std::string &text);

#endif
