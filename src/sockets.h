#pragma once
#include <memory>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <poll.h>

namespace zlynx {
	class Sockets;

	// Socket is the base for all sockets to be held in a Sockets container.
	class Socket : public std::enable_shared_from_this<Socket> {
		public:
		Socket(int h);
		Socket(int h, const sockaddr_in6 &remote, time_t timeout = 0);
		virtual ~Socket();
		Socket(const Socket&) = delete;
		void operator=(const Socket&) = delete;

		int get_handle() const { return handle; }
		void set_nonblocking();

		protected:
		// Return from the on_* calls. This is whether Sockets should keep or
		// remove the socket.
		enum: int {
			KEEP,
			REMOVE
		};
		typedef int Action;

		virtual Action on_input();
		virtual Action on_output();
		virtual Action on_priority();
		virtual Action on_error();
		virtual Action on_hangup();
		virtual Action on_invalid();
		virtual Action on_timeout();

		// The system handle of the socket
		unsigned handle;
		// timeout is added to expiration on every event
		// timeout of zero means disabled
		time_t timeout = 0;
		// The absolute time to expire this socket. It will be closed.
		time_t timeout_expiration = 0;
		// Local and remote addresses
		sockaddr_in6 local_addr;
		sockaddr_in6 remote_addr;

		// Most sockets need a pointer back to their container.
		friend class Sockets;
		std::shared_ptr<Sockets> sockets;

		void zero_addr(sockaddr_in6 &addr);
	};

	// Sockets contains individual Socket objects.
	// It manages their event loop.
	class Sockets : public std::enable_shared_from_this<Sockets> {
		private:
		typedef std::shared_ptr<Socket> ptr;

		public:
		Sockets();

		enum PollEvents : short {
			Read = POLLIN|POLLPRI,
			Write = Read|POLLOUT
		};
		void add_socket(ptr p, PollEvents events = Read);
		void set_write_event();
		void clear_write_event();
		void start();

		sig_atomic_t running = false;

		private:
		std::vector<ptr> sockets;
		std::array<std::vector<pollfd>, 2> pollfds;
		unsigned pollfds_current_index = 0;
		std::vector<pollfd>::iterator poll_position = pollfds[0].end();

		std::vector<pollfd>& curr_pollfds() { return pollfds[pollfds_current_index]; }
		std::vector<pollfd>& next_pollfds() { return pollfds[pollfds_current_index ^ 1]; }
		void flip_pollfds() {  pollfds_current_index ^= 1; }

		void poll();

	};

	// A Socket that listens on a port and creates new Connections.
	// Create it, call start(), and add it to a Sockets collection.
	class Listener : public Socket {
		public:
		struct AcceptResult {
			bool ok = false;
			int handle = -1;
			sockaddr_in6 remote_addr;
		};

		Listener(uint16_t port);

		void start();

		Action on_input() override;

		protected:
		int backlog = 256;

		AcceptResult do_accept();
	};

	template<typename T>
	struct no_construct_alloc : public std::allocator<T> {
		using std::allocator<T>::construct;
		void construct(T*) {
		}
		template<typename U>
		struct rebind {
			using other = no_construct_alloc<U>;
		};
	};

	// Connection reads and writes data from a Socket.
	// It has input and output buffers.
	class Connection : public Socket {
		public:
		Connection(int h, const sockaddr_in6 &remote, time_t timeout = 0);

		// Add to the output buffer and set the poll flags.
		template<class Iterator>
		void write(Iterator begin, Iterator end) {
			if(static_cast<size_t>(end - begin) >= io_direct_write_size) {
				write_directly(begin, end);
			} else {
				this->output.insert(this->output.end(), begin, end);
			}
			if(sockets && !this->output.empty())
				sockets->set_write_event();
		}

		template<class Container>
		void write(const Container &c) {
			this->write(cbegin(c), cend(c));
		}

		void close_output();

		protected:
		Action on_input() override;
		Action on_output() override;

		void write_directly(const char* begin, const char* end);

		static constexpr size_t io_block_size = 8 * 1024;
		static constexpr size_t io_direct_write_size = 4 * 1024;
		std::vector<char, no_construct_alloc<char>> input;
		std::vector<char, no_construct_alloc<char>> output;
		// Set to true during a graceful close.
		bool closing = false;
	};
};
