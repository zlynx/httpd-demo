#include "errors.h"
#include "app.h"

namespace zlynx {
	void AppConnection::on_get() {
		logger << "GET " << path_view << "\n";

		auto entry = store->get(path_view);

		write(proto_view);
		if(entry.body.empty()) {
			writeln(" 404 Not Found");
		} else {
			writeln(" 200 OK");
		}
		write("Content-Type: ");
		writeln(entry.content_type);
		write_body(entry.body);
	}

	void AppConnection::on_post() {
		auto content_type_view = get_header("\r\nContent-Type: ");
		//logger << "POST " << path_view << ' ' << content_type_view << '\n' << body_view << '\n';
		logger << "POST " << path_view << ' ' << content_type_view << '\n';

		store->set(path_view, Entry{content_type_view,  body_view});

		write(proto_view);
		writeln(" 201 Created");
		write("Location: ");
		writeln(path_view);
		writeln("Content-Length: 0");
		writeln();
	}

	void AppConnection::on_delete() {
		logger << "DELETE " << path_view << "\n";

		store->del(path_view);

		write(proto_view);
		writeln(" 200 OK");
		write_body("");
	}
}
