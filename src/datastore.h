#pragma once
#include <string>
#include <string_view>
#include <unordered_map>

namespace zlynx {
	struct Entry {
		std::string_view content_type;
		std::string_view body;
	};

	class Datastore {
		public:
		Entry get(std::string_view key) const;
		void set(std::string_view, Entry value);
		void del(std::string_view key);

		private:
		struct EntryInternal {
			std::string content_type;
			std::string body;

			EntryInternal() {}
			EntryInternal(const Entry& e):
				content_type(e.content_type),
				body(e.body)
			{
			}
		};
		std::unordered_map<std::string, EntryInternal> store;
	};
}
