/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   parsmessage.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: abensaid <abensaid@student.42lehavre.fr>   +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/09/30 17:26:40 by abensaid          #+#    #+#             */
/*   Updated: 2026/09/30 17:26:42 by abensaid         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PARSMESSAGE_HPP
#define PARSMESSAGE_HPP

#include "client.hpp"


std::vector<std::string> parsmessage(std::string message);
int dispatcher(std::string word);
void sendNumeric(class client &clt, const std::string &code, const std::string &params, const std::string &text);

#endif