#pragma once
#include <string_view>
#include "sockets.h"

namespace zlynx {
	using namespace std::literals;

	class HTTPConnection : public Connection {
		public:
		HTTPConnection(int h, const sockaddr_in6 &remote, time_t timeout = 0):
			Connection(h, remote, timeout)
		{
		}

		Action on_input() override;

		protected:
		virtual void on_headers();
		virtual void on_body();

		virtual void on_get();
		virtual void on_post();
		virtual void on_delete();

		size_t search_point = 0;
		std::string_view method_view;
		std::string_view path_view;
		std::string_view headers_view;
		std::string_view body_view;
	};

	class HTTPListener : public Listener {
		public:
			HTTPListener(uint16_t port, time_t connection_timeout = 5) :
				Listener(port),
				connection_timeout(connection_timeout)
			{
			}

			Action on_input() override {
				auto result = do_accept();
				if(result.ok) {
					auto conn = std::make_shared<HTTPConnection>(
						result.handle, result.remote_addr, connection_timeout
					);
					sockets->add_socket(conn);
				}
				return KEEP;
			}
		private:
		time_t connection_timeout = 0;
	};
};
