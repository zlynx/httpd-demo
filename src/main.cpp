#include <iostream>
#include <memory>

#include "config.h"
#include "sockets.h"
#include "datastore.h"
#include "app.h"

namespace zlynx {
	std::ostream& logger(std::cout);
}

using namespace zlynx;

int main(int argc, char *argv[]) {
	Config config(argc, argv);

	logger << "Starting mersive-http server on port " << config.port << std::endl;
	auto sockets = std::make_shared<Sockets>();
	auto listener = std::make_unique<AppListener>(config.port, std::make_unique<Datastore>());
	listener->start();
	sockets->add_socket(std::move(listener));
	sockets->start();
	return 0;
}
