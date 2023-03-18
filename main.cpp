
#include <boost/asio/signal_set.hpp>
#include "network/server.hpp"


int main(int argc, char* argv[]) {

	std::string filename = "file.log";
	asio::ip::port_type port = 1883;


	for(int i = 1; i < argc; i += 2) {
		if(std::string(argv[i]) == "-p") {
			if (i + 1 < argc && argv[i + 1][0] != '-') {
				port = std::atoi(argv[i + 1]);
			}
			else
				return -1;
		}
		if(std::string(argv[i]) == "-f") {
			if (i + 1 < argc)
				filename = argv[i + 1];
			else
				return -1;
		}
	}

	std::cout << "  __  __   ____  _______  _______         ____    __    __ \n" 
			  << " |  \\/  | / __ \\|__   __||__   __|       |___ \\  /_ |  /_ | \n"
			  << " | \\  / || |  | |  | |      | |    __   __ __) |  | |   | |\n"
			  << " | |\\/| || |  | |  | |      | |    \\ \\ / /|__ <   | |   | |\n"
			  << " | |  | || |__| |  | |      | |     \\ V / ___) |_ | | _ | |\n"
			  << " |_|  |_| \\___\\_\\  |_|      |_|      \\_/ |____/(_)|_|(_)|_|\n";

	std::cout << '\n' << "Filename: " << filename << " port: " << port << '\n';

	asio::io_context io;
	tcp::acceptor ac{ io, {tcp::v4(), port} };
	asio::signal_set signals(io, SIGINT, SIGTERM);

	try {

		asio::co_spawn(io, network::server.Listen(std::move(ac), filename), asio::detached);

		signals.async_wait([&](auto, auto) {io.stop(); });

		io.run();
	}
	catch (std::exception&) {
		Log(filename, info, 0, "Error in the main file");
	}
}