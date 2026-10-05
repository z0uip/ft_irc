#include "client.hpp"

std::string client::get_username() const
{
	return (_username);
}

std::string client::get_nickname() const
{
	return (_nickname);
}

bool client::is_saved() const
{
	return (_prfl_saved);
}

bool client::is_pass_ok() const
{
	return (_pass_ok);
}

void client::modifie_nickname(std::string name)
{
	_nickname = name;
}
void client::modifie_username(std::string name)
{
	_username = name;
}

void client::add_byte(const char *data, size_t len)
{
	_bufferin.append(data, len);
}

bool client::extract_line(std::string &line)
{
	size_t pos = _bufferin.find("\r\n");

	if (pos == std::string::npos)//npos = pas trouver de \r ou de \n
		return (false);
	line = _bufferin.substr(0, pos);
	_bufferin.erase(0, pos + 2);
	return (true);
}

client::client(int fd, const std::string &ip)
{
	_fd = fd;
	_pass_ok = false;
	_ip = ip;
	_prfl_saved = false;
}

void client::queueMessage(const std::string &msg)
{
	_bufferout.append(msg);
	_bufferout.append("\r\n");
}

bool client::hasPendingOutput() const
{
	if (_bufferout.empty())
		return (false);
	return (true);
}

const std::string &client::getOutBuffer() const
{
	return (_bufferout);
}

void client::consumeOutPut(int n)
{
	if (n <= 0)
		return ;
	_bufferout.erase(0, n);
}