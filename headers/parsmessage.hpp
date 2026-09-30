#ifndef PARSMESSAGE_HPP
#define PARSMESSAGE_HPP

#include "client.hpp"


std::vector<std::string> parsmessage(std::string message);
int dispatcher(std::string word);
void sendNumeric(class client &clt, const std::string &code, const std::string &params, const std::string &text);

#endif