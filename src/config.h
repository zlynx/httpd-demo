#include <cstdint>

namespace zlynx {
	struct Config  {
		std::uint16_t port;

		Config(int argc, char *argv[]);
	};
};


