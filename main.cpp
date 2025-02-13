#include "Parser.hpp"
#include "WebServ.hpp"
#include "Outils.hpp"
#include <iostream>
#include <cstdlib>
#include <signal.h>

volatile sig_atomic_t stop_flag = 0;

void handle_sigint(int signum) {
    (void)signum;  
    stop_flag = 1;
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    Parser p;
	//Outils outils;
    std::string configFile = "config/config.conf";

    if (argc > 1)
        configFile = argv[1];

    try
    {
        p.parseConfig(configFile);
        WebServ ws(p.getServers());
		//outils.printConf(p.getServers());
        ws.start();
    }
    catch (std::exception &e)
    {
        std::cerr << "Error parsing config: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}