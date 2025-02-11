#include "Parser.hpp"
#include "WebServ.hpp"
#include "Outils.hpp"
#include <iostream>
#include <cstdlib>
#include <signal.h>

// Глобальный флаг для корректного завершения работы
volatile sig_atomic_t stop_flag = 0;

// Обработчик сигнала SIGINT (Ctrl+C)
void handle_sigint(int signum) {
    (void)signum;  // подавление предупреждения о неиспользуемом параметре
    stop_flag = 1;
}

int main(int argc, char *argv[])
{
    // Установка обработчика сигнала SIGINT для корректного завершения работы
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    Parser p;
	Outils outils;
    std::string configFile = "config/config.conf";

    if (argc > 1)
        configFile = argv[1];

    try
    {
        p.parseConfig(configFile);
        WebServ ws(p.getServers());
		outils.printConf(p.getServers());
        ws.start();
    }
    catch (std::exception &e)
    {
        std::cerr << "Error parsing config: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}