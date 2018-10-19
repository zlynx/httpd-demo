#include <string_view>
#include <algorithm>
#include <array>
#include <vector>
#include <cstring>
#include <iostream>
#include <iterator>
#include <charconv>
#include <sstream>
#include <cctype>
#include "http.h"
#include "errors.h"
#include "container_index_view.h"

namespace zlynx {
	using namespace std::literals;

	constexpr auto header_divider = "\r\n\r\n"sv;

	template<class Iterator>
	const char* find_string(
		Iterator hay_begin, Iterator hay_end,
		const std::string_view &needle
	) {
		const char* begin = reinterpret_cast<const char*>(&*hay_begin);
		const char* end   = reinterpret_cast<const char*>(&*hay_end);
		while(begin != end) {
			size_t n = end - begin;
			const char *p = static_cast<const char*>(std::memchr(begin, needle[0], n));
			if(!p)
				return p;
			if(p + needle.size() > end)
				return nullptr;
			if(0 == std::memcmp(p, needle.data(), needle.size()))
				return p;
			begin = p + 1;
		}
		return nullptr;
	}

	template<class SrcT, class DstT>
	size_t split_string_into(const SrcT &src, const char c, DstT &dst) {
		size_t pos = 0;
		size_t end;
		size_t count = 0;

		while( (end = src.find_first_of(c, pos)) != SrcT::npos ) {
			dst[count++] = src.substr(pos,end-pos);
			pos = end + 1;
			if(count == dst.size())
				return count;
		}
		dst[count++] = src.substr(pos, src.size()-pos);
		return count;
	}

	Socket::Action HTTPConnection::on_input() {
		Action act = Connection::on_input();
		while(do_request())
			/* empty */;
		return act;
	}

	bool HTTPConnection::do_request() {
		// Have we received all of the headers yet?
		if(headers_view.empty()) {
			const char *header_end = find_string(
				input.cbegin()+search_point, input.cend(),
				header_divider
			);
			if(header_end) {
				const char *line_end = find_string(
					input.cbegin(), input.cend(),
					"\r\n"sv
				);
				if(!line_end)
					throw std::logic_error("no line ending should be impossible");

				auto begin = reinterpret_cast<const char*>(input.data());
				size_t n = line_end - begin;
				auto first_line = std::string_view(begin, n);
				n = header_end - begin;
				headers_view = container_index_view(input, begin, n);

				std::array<std::string_view, 3> first_line_words;
				if(split_string_into(first_line, ' ', first_line_words) != 3) {
					// write a HTTP error
					// and get out of here
					write_error("400 Bad Request");
				}
				method_view = container_index_view(input, first_line_words[0]);
				path_view   = container_index_view(input, first_line_words[1]);
				proto_view  = container_index_view(input, first_line_words[2]);

				on_headers();
			} else {
				// Looking for four bytes, so always back up in case we
				// only had part of it last time.
				if(input.size() > header_divider.size())
					search_point = input.size() - header_divider.size();
			}
		}

		if(body_view.size() < content_length) {
			const char *begin =
				input.data()
				+ headers_view.size()
				+ header_divider.size();
			const char *end =
				input.data()
				+ input.size();
			if(static_cast<size_t>(end-begin) >= content_length)
				body_view = container_index_view(input, begin, content_length);
		}

		if(!headers_view.empty() && body_view.size() == content_length) {
			// We have headers and body (if any), now call the on_method
			if(method_view == "GET"sv) {
				on_get();
			} else if(method_view == "PUT"sv) {
				on_put();
			} else if(method_view == "POST"sv) {
				on_post();
			} else if(method_view == "DELETE"sv) {
				on_delete();
			}
			reset();
			return true;
		}
		return false;
	}

	void HTTPConnection::reset() {
		size_t input_end =
			headers_view.size() +
			header_divider.size() +
			body_view.size();

		// Reset the HTTP data.
		search_point = 0;
		content_length = 0;
		method_view.reset();
		path_view.reset();
		proto_view.reset();
		headers_view.reset();
		body_view.reset();

		// Clear the input buffer.
		input.erase(input.begin(), input.begin() + input_end);

		header_map.clear();
	}

	static
	std::string to_lower_string(std::string_view s) {
		std::string result;
		result.reserve(s.size());
		std::transform(
			begin(s), end(s),
			std::back_inserter(result),
			::tolower
		);
		return result;
	}

	void HTTPConnection::build_header_map() {
		constexpr auto line_end_sv = "\r\n"sv;
		constexpr auto key_end_sv = ":"sv;
		const char *line_end = find_string(
			headers_view.begin(), headers_view.end(),
			line_end_sv
		);
		if(!line_end)
			throw std::runtime_error("corrupt headers");
		while(line_end != headers_view.end()) {
			// Advance to next line.
			const char *line_start = line_end + line_end_sv.size();
			const char *key_end = find_string(
				line_start, headers_view.end(),
				key_end_sv
			);
			line_end = find_string(
				line_start, headers_view.end(),
				line_end_sv
			);
			if(!line_end)
				line_end = headers_view.end();
			if(!key_end || key_end > line_end)
				throw std::runtime_error("corrupt header line");
			const char *value_start = key_end + 2;
			header_map.emplace(
				to_lower_string(std::string_view(line_start, key_end-line_start)),
				container_index_view(input, std::string_view(value_start, line_end - value_start))
			);
		}
	}

	void HTTPConnection::on_headers() {
		// Convert everything to a hash map with lower-cased keys.
		build_header_map();
		// Look for Content-Length
		static const auto content_length_s = "content-length"s;
		static const auto expect_s = "expect"s;
		static const auto connection_s = "connection"s;
		auto content_length_view = get_header(content_length_s);
		if(!content_length_view.empty()) {
			auto result = std::from_chars(
				content_length_view.begin(), content_length_view.end(),
				content_length
			);
			if(result.ec != std::errc()) {
				write_error("400 Bad Request");
			}
		}
		auto expect_view = get_header(expect_s);
		if(expect_view == "100-continue"sv) {
			// Immediatly send a 100-continue
			write(proto_view);
			writeln(" 100 Continue");
			writeln();
		}
		if(proto_view == "HTTP/1.0"sv) {
			auto connection_view = get_header(connection_s);
			if(connection_view == "Keep-Alive") {
				keep_alive = true;
			} else {
				keep_alive = false;
			}
		}
	}

	void HTTPConnection::on_get() {
		logger << "GET " << path_view << "\n";
	}

	void HTTPConnection::on_put() {
		logger << "PUT " << path_view << "\n";
	}

	void HTTPConnection::on_post() {
		logger << "POST " << path_view << '\n' << body_view << '\n';
	}

	void HTTPConnection::on_delete() {
		logger << "DELETE " << path_view << "\n";
	}

	void HTTPConnection::write(std::string_view str) {
		Connection::write(str);
	}

	void HTTPConnection::writeln(std::string_view line) {
		write(line);
		write("\r\n"sv);
	}

	void HTTPConnection::write_body(std::string_view body) {
		// Create a Content-Length header
		std::array<char, 32> buf;
		auto result = std::to_chars(buf.begin(), buf.end(), body.size());
		if(result.ec != std::errc())
			throw std::runtime_error("error formatting body.size for content length");
		write("Content-Length: ");
		Connection::write(buf.data(), result.ptr);
		writeln();
		if(keep_alive) {
			if(proto_view == "HTTP/1.0"sv) {
				writeln("Connection: Keep-Alive");
			}
		} else {
			writeln("Connection: close");
		}
		writeln();
		write(body);
		if(!keep_alive)
			close_output();
	}

	void HTTPConnection::write_error(std::string_view err) {
		write(proto_view);
		write(" ");
		writeln(err);
		writeln("Connection: close");
		writeln("");
		close_output();
	}

	// this expects a header search string that looks like:
	// "\r\nContent-Length: "
	// Yes a little ugly but makes things easier to search.
	// Could use a macro to stringify these.
	std::string_view HTTPConnection::get_header(const std::string& header) const {
		auto i = header_map.find(header);
		if(i != header_map.end())
			return i->second;
		return std::string_view();
	}
};
