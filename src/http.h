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

		Action on_input() override;

		protected:
		// Called when the method, path and headers have been received.
		virtual void on_headers();

		// Called depending on which method was used.
		// If a Content-Length was provided, the body_view will be filled in.
		virtual void on_get();
		virtual void on_post();
		virtual void on_delete();

		void write(std::string_view str);
		void writeln(std::string_view line = std::string_view());
		void write_body(std::string_view body);
		void write_error(std::string_view err);

		std::string_view get_header(std::string_view header) const;

		size_t search_point = 0;
		size_t content_length = 0;

		container_index_view<decltype(input)> method_view;
		container_index_view<decltype(input)> path_view;
		container_index_view<decltype(input)> proto_view;
		container_index_view<decltype(input)> headers_view;
		container_index_view<decltype(input)> body_view;
	};

};
