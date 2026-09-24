#include "../headers/client.hpp"


std::string client::get_name()
{
	return (_name);
}

std::string client::get_password()
{
	return (_password);
}

bool client::is_saved()
{
	return (_prfl_saved);
}
bool client::is_valid()
{
	return (_prfl_valid);
}

void client::modifie_name(std::string name)
{
	_name = name;
}

void client::modifie_password(std::string password)
{
	_password = password;
}

void client::add_byte(const char *data, size_t len)
{
	_bufferin.append(data, len);
}

std::string client::extract_line()
{
	std::string line;
	for (size_t n = 1; n > 0; n = _bufferin.find("\r\n"))
	{
		line.assign(_bufferin, n);
		_bufferin.erase(0, n + 2);
	}
	/*traiter la ligne*/
	return (line);
}

client::client(int fd, std::string name, std::string password)
{
	_name = name;
	_password = password;
	_fd = fd;
	if (name.empty() || password.empty())
		_prfl_valid = false;
	else
		_prfl_valid = true;
	_prfl_saved = true;
}

void client::queueMessage(const std::string &msg)
{
	_bufferout.append(msg);
	_bufferout.append("\r\n");
}

bool client::hasPendingOutput()
{
	if (_bufferout.empty())
		return (true);
}