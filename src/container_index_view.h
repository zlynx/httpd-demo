#pragma once
#include <string_view>
#include <ostream>

namespace zlynx {
	// This class holds a pointer to a container and indexes into the contents.
	// Unlike string_view it will not become invalid after container reallocation.
	// Currently it only implements what is needed for this application.
	template<class Container>
	class container_index_view {
		using sv = std::string_view;

		public:
		container_index_view()
		{
		}

		container_index_view(const Container& c, typename Container::const_pointer p, size_t n):
			container(&c),
			begin_i(p - c.data()),
			n(n)
		{
		}

		container_index_view(const Container& c, const sv& sv):
			container(&c),
			begin_i(sv.data() - c.data()),
			n(sv.size())
		{
		}

		operator sv() const {
			if(container)
				return sv(data(), n);
			return sv("", 0);
		}

		typename Container::const_pointer data() const {
			return container->data() + begin_i;
		}

		sv::iterator begin() const {
			return data();
		}

		sv::iterator end() const {
			return data() + n;
		}

		size_t size() const {
			return n;
		}

		bool empty() const { return n == 0; }

		void reset() {
			*this = container_index_view();
		}

		private:
		const Container* container = nullptr;
		size_t begin_i = 0;
		size_t n = 0;
	};

	template<class T>
	std::ostream& operator<<(std::ostream& os, const container_index_view<T>& v) {
		return os << static_cast<std::string_view>(v);
	}
}
