#include <string_view>
#include <array>
#include <vector>
#include <cstring>
#include <iostream>
#include "http.h"
#include "errors.h"

namespace zlynx {
	using namespace std::literals;

	const char* find_string(
		std::vector<uint8_t>::const_iterator hay_begin,
		std::vector<uint8_t>::const_iterator hay_end,
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
		// Have we received all of the headers yet?
		if(headers_view.empty()) {
			const char *header_end = find_string(
				input.begin()+search_point, input.end(),
				"\r\n\r\n"sv
			);
			if(header_end) {
				const char *line_end = find_string(
					input.begin(), input.end(),
					"\r\n"sv
				);
				if(!line_end)
					throw std::logic_error("no line ending should be impossible");

				auto begin = reinterpret_cast<const char*>(input.data());
				size_t n = line_end - begin;
				auto first_line = std::string_view(begin, n);
				begin = line_end + 2;
				n = header_end - begin;
				headers_view = std::string_view(begin, n);

				std::array<std::string_view,3> first_line_words;
				if(split_string_into(first_line, ' ', first_line_words) != 3) {
					// write a HTTP error
					// and get out of here
				}
				method_view = first_line_words[0];
				path_view = first_line_words[1];

				on_headers();
			} else {
				// Looking for four bytes, so always back up in case we
				// only had part of it last time.
				if(input.size() > 4)
					search_point = input.size() - 4;
			}
		} else if(body_view.empty()) {
		}
		return act;
	}

	void HTTPConnection::on_headers() {
		logger << "HTTP request " << method_view << " " << path_view << '\n';
		logger << "Headers:\n" << headers_view << '\n';
	}

	void HTTPConnection::on_body() {
	}
};
