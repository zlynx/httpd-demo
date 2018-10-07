#include <iostream>
#include <cstdlib>
#include <string_view>
#include <functional>
#include <charconv>
#include "config.h"

namespace zlynx {
	namespace {
		void display_help(Config&, const std::string_view);

		struct config_key {
			std::string_view env_key;
			std::string_view long_option;
			char             short_option;
			int              argument;
			std::function<void(Config&, const std::string_view)> f;
		};

		const std::array<config_key, 3> keys = {
			config_key{"SERVER_PORT", "port", 'p', 1, [](Config& c, const std::string_view v) {
				 std::from_chars(v.begin(), v.end(), c.port);
			}},
			config_key{"", "help", 'h', 0, display_help},
			config_key{"", "test",   0, 0, display_help},
		};

		void display_help(Config&, const std::string_view) {
			std::clog << "Usage:\n";
			for(auto& k: keys) {
				std::clog << "  ";
				if(k.short_option)
					std::clog << '-' << k.short_option;
				if(!k.long_option.empty()) {
					if(k.short_option)
						std::clog << ", ";
					else
						std::clog << "    ";
					std::clog << "--" << k.long_option;
				}
				if(!k.env_key.empty())
					std::clog << " [" << k.env_key << "]";
				std::clog << '\n';
			}
			std::exit(0);
		}
	};

	Config::Config(int argc, char *argv[]):
		port(8080)
	{
		// Environment variables
		for(auto& k: keys) {
			const char* r = getenv(k.env_key.data());
			if(r) {
				k.f(*this, r);
			}
		}
		int arg_bump = 1;
		for(int i = 0; i < argc; i += arg_bump) {
			for(auto& k: keys) {
				std::string_view arg;
				if(k.argument && i+1<argc) {
					arg_bump = 2;
					arg = argv[i+1];
				} else {
					arg_bump = 1;
					arg = "";
				}
				// Short options
				if(argv[i][0] == '-' && argv[i][1] == k.short_option) {
					k.f(*this, arg);
					break;
				}
				// Long options
				if(argv[i][0] == '-' && argv[i][1] == '-' && k.long_option == argv[i]+2) {
					k.f(*this, arg);
					break;
				}
			}
		}
	}
};
