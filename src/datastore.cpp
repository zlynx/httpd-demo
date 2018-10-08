#include "datastore.h"

namespace zlynx {
	Entry Datastore::get(std::string_view key) const {
		auto r = store.find(std::string(key));
		if(r == store.end())
			return Entry();
		return Entry{
			r->second.content_type,
			r->second.body
		};
	}

	void Datastore::set(std::string_view key, Entry value) {
		store[std::string(key)] = EntryInternal(value);
	}

	void Datastore::del(std::string_view key) {
		store.erase(std::string(key));
	}
}
