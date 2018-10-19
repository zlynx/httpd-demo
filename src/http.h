#pragma once
#include "container_index_view.h"
#include "sockets.h"

namespace zlynx {
	using namespace std::literals;

	class HTTPConnection : public Connection {
		public:
		HTTPConnection(int h, const sockaddr_in6 &remote, time_t timeout = 0):
			Connection(h, remote, timeout)
		{
		}

		protected:
		Action on_input() override;

		// Called when the method, path and headers have been received.
		virtual void on_headers();

		// Called depending on which method was used.
		// If a Content-Length was provided, the body_view will be filled in.
		virtual void on_get();
		virtual void on_put();
		virtual void on_post();
		virtual void on_delete();

		void write(std::string_view str);
		void writeln(std::string_view line = std::string_view());
		void write_body(std::string_view body = std::string_view());
		void write_error(std::string_view err);

		std::string_view get_header(std::string_view header) const;

		size_t search_point = 0;
		size_t content_length = 0;
		bool keep_alive = true;

		container_index_view<decltype(input)> method_view;
		container_index_view<decltype(input)> path_view;
		container_index_view<decltype(input)> proto_view;
		container_index_view<decltype(input)> headers_view;
		container_index_view<decltype(input)> body_view;

		private:
		// Look for and process one request out of the input.
		// Return true if a request was processed.
		bool do_request();
		void reset();
	};

};
