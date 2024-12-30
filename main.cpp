#include "Parser.hpp"
#include <iostream>
#include <cstdlib>
#include "outils.cpp"
#include "WebServ.hpp"

int main(int argc, char *argv[])
{
	Parser p;

	std::string configFile = "config/config.conf";

	if (argc > 1)
		configFile = argv[1];

	try
	{
		p.parseConfig(configFile);
		WebServ ws(p.getServers());
		ws.getListenSockets();
		ws.start();
		// printConf(p.getServers());
	}
	catch (std::exception &e)
	{
		std::cerr << "Error parsing config: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
