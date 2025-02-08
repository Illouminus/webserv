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
		//printConf(p.getServers());
		ws.start();
	}
	catch (std::exception &e)
	{
		std::cerr << "Error parsing config: " << e.what() << "\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

/// opt/homebrew/bin/php-cgi MAC OS    cgi_pass /usr/bin/php-cgi; Linux