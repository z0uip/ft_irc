#include "../headers/Bot.hpp"

int	main(int ac, char **av)
{
	if (ac != 3)
	{
		std::cerr << "Usage: ./ircbot <port> <pwd>" << "\n";
		return 1;
	}
	int port = std::atoi(av[1]);//convertit le port en nbr
	std::string password = av[2];//stocke le mdp
	try
	{
		Bot ircBot("127.0.0.1", port, password);//objet bot avc ip locale port et mdp
		ircBot.connectToServer();//on co le bot au serv
		ircBot.run();//lancement de la boucle infinie
	}
	catch(const std::exception& e)
	{
		std::cerr << "Bot error : " << e.what() << "\n";
		return 1;
	}
	return 0;
}
