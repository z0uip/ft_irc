#include "../headers/socket.hpp"

void socket()
{
	int fd;
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		/*throw une erreur*/;
	try
	{
		int opt = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, 1);
	}
	catch (const std::exception)
	{
		close(fd);
		/*throw une erreur "setsockopt failed"*/;
	}
}