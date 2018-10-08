#pragma once
#include "datastore.h"
#include "http.h"

namespace zlynx {
	class AppConnection : public HTTPConnection {
		public:
		AppConnection(
			int h,
			const sockaddr_in6 &remote,
			time_t timeout,
			std::shared_ptr<Datastore> store
		):
			HTTPConnection(h, remote, timeout),
			store(store)
		{
		}

		void on_get() override;
		void on_post() override;
		void on_delete() override;

		private:
		std::shared_ptr<Datastore> store;
	};

	class AppListener : public Listener {
		public:
		AppListener(
			uint16_t port,
			std::shared_ptr<Datastore> store,
			time_t connection_timeout = 5
		) :
			Listener(port),
			store(store),
			connection_timeout(connection_timeout)
		{
		}

		Action on_input() override {
			if(!sockets->running)
				return REMOVE;
			auto result = do_accept();
			if(result.ok) {
				auto conn = std::make_shared<AppConnection>(
					result.handle, result.remote_addr, connection_timeout,
					store
				);
				sockets->add_socket(conn);
			}
			return KEEP;
		}

		private:
		std::shared_ptr<Datastore> store;
		time_t connection_timeout = 0;
	};
}
