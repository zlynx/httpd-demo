#include <algorithm>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "sockets.h"
#include "errors.h"

namespace zlynx {
	std::ostream& operator<<(std::ostream &os, const sockaddr_in6 &x) {
		os << '[';
		for(size_t i = 0; i < sizeof x.sin6_addr.s6_addr; i += 2) {
			if(i>0)
				os << ':';
			short v = (x.sin6_addr.s6_addr[i]<<8) + (x.sin6_addr.s6_addr[i+1]);
			os << std::hex << std::setfill('0') << std::setw(4) << v;
		}
		os << "]:" << std::dec << ntohs(x.sin6_port);
		return os;
	}

	Socket::Socket(int h): handle(h) {
		if(h<0) {
			throw std::range_error("cannot accept a negative handle");
		}
		zero_addr(local_addr);
		zero_addr(remote_addr);
	}

	Socket::Socket(int h, const sockaddr_in6 &remote, time_t timeout) :
		handle(h),
		timeout(timeout),
		remote_addr(remote)
	{
		if(h<0) {
			throw std::range_error("cannot accept a negative handle");
		}
		zero_addr(local_addr);
		set_nonblocking();

		timespec now;
		throw_posix_errno_if( clock_gettime(CLOCK_MONOTONIC, &now) );
		timeout_expiration = now.tv_sec + timeout;
	}

	Socket::~Socket() {
		try {
			logger << "closing handle " << handle << std::endl;
			::shutdown(handle, SHUT_RDWR);
			throw_posix_errno_if( ::close(handle) );
		} catch( const std::exception &e ) {
			logger << e.what() << std::endl;
		} catch(...) {
			logger << "unknown error in socket close\n";
		}
	}

	void Socket::zero_addr(sockaddr_in6 &addr) {
		std::memset(&addr, 0, sizeof addr);
		addr.sin6_family = AF_INET6;
	}

	void Socket::set_nonblocking() {
		int flags = ::fcntl(handle, F_GETFL, 0);
		throw_posix_errno_if( ::fcntl(handle, F_SETFL, flags | O_NONBLOCK) );
	}

	Socket::Action Socket::on_input() {
		throw std::runtime_error("not implemented");
	}

	Socket::Action Socket::on_output() {
		throw std::runtime_error("not implemented");
	}

	Socket::Action Socket::on_priority() {
		throw std::runtime_error("not implemented");
	}

	Socket::Action Socket::on_error() {
		int err;
		socklen_t err_size = sizeof err;
		throw_posix_errno_if( getsockopt(handle, SOL_SOCKET, SO_ERROR, &err, &err_size) );
		logger << "error on handle " << handle << ": " << err << std::endl;
		return REMOVE;
	}

	Socket::Action Socket::on_hangup() {
		logger << "hangup on handle " << handle << std::endl;
		return REMOVE;
	}

	Socket::Action Socket::on_invalid() {
		logger << "invalid on handle " << handle << std::endl;
		return REMOVE;
	}

	Socket::Action Socket::on_timeout() {
		logger << "timeout on handle " << handle << std::endl;
		return REMOVE;
	}

	Sockets::Sockets() {
		sockets.reserve(32);
		for(auto &p: pollfds) {
			p.reserve(32);
		}
	}

	void Sockets::add_socket(ptr p, PollEvents events) {
		unsigned h = p->get_handle();
		if(sockets.size() <= h) {
			sockets.resize(h+1);
		}
		sockets.at(h) = p;
		next_pollfds().emplace_back( pollfd { p->get_handle(), events, 0 } );
		p->sockets = shared_from_this();
	}

	void Sockets::set_write_event() {
		if(poll_position != curr_pollfds().end())
			poll_position->events |= POLLOUT;
	}

	void Sockets::clear_write_event() {
		if(poll_position != curr_pollfds().end())
			poll_position->events &= ~POLLOUT;
	}

	static std::shared_ptr<Sockets> handler_target;
	static
	void handler(int) {
		if(handler_target)
			handler_target->running = false;
	}

	void Sockets::start() {
		if(next_pollfds().empty())
			return;

		sigset_t blockset;
		throw_posix_errno_if( ::sigemptyset(&blockset) );
		throw_posix_errno_if( ::sigaddset(&blockset, SIGPIPE) );
		throw_posix_errno_if( ::sigprocmask(SIG_BLOCK, &blockset, nullptr) );

		running = true;
		struct sigaction sigact;
		std::memset(&sigact, 0, sizeof sigact);
		sigact.sa_handler = handler;
		sigaction(SIGINT, &sigact, nullptr);
		handler_target = shared_from_this();
		while(!next_pollfds().empty()) {
			poll();
		}
	}

	void Sockets::poll() {
		// Ensure enough reserve that we never reallocate the vectors while
		// inside the loop.
		sockets.reserve(sockets.size() + 32);

		flip_pollfds();

		int poll_result = ::poll(curr_pollfds().data(), curr_pollfds().size(), 1 * 1000);
		if(poll_result < 0) {
			if(errno == EINTR) {
				// Run the loop anyway.
				// There are checks for !running
			} else {
				throw_posix_errno_if(poll_result < 0);
			}
		}

		// Empty out the next pollfds
		next_pollfds().clear();

		// Run the result loop even if poll_result was 0 in order to handle
		// the timeouts.

		timespec now;
		throw_posix_errno_if( clock_gettime(CLOCK_MONOTONIC, &now) );

		// Handle the poll results and build the next pollfds
		for(
			poll_position = begin(curr_pollfds());
			poll_position != end(curr_pollfds());
			++poll_position
		) {
			Socket::Action act = Socket::KEEP;
			try {
				// Using a shared_ptr here will also ensure the Socket
				// will not be destroyed until this loop iteration ends.
				std::shared_ptr<Socket> s = sockets.at(poll_position->fd);

				if(s->timeout) {
					if(poll_position->revents) {
						s->timeout_expiration = now.tv_sec + s->timeout;
					} else {
						if(s->timeout_expiration < now.tv_sec) {
							act |= s->on_timeout();
						}
					}
				}
				// if not running punch all the sockets on_input to poke the listeners.
				if( (poll_position->revents & POLLIN) || !running ) {
					act |= s->on_input();
				}
				if(poll_position->revents & POLLPRI) {
					s->on_priority();
				}
				if(poll_position->revents & POLLOUT) {
					s->on_output();
				}
				if(poll_position->revents & POLLERR) {
					s->on_error();
				}
				if(poll_position->revents & POLLHUP) {
					s->on_hangup();
				}
				if(poll_position->revents & POLLNVAL) {
					s->on_invalid();
				}
			} catch( const std::exception &e ) {
				logger
					<< "exception while processing handle " << poll_position->fd
					<< ": " << e.what()
					<< std::endl;
			}
			if(act == Socket::KEEP) {
				next_pollfds().emplace_back( pollfd{poll_position->fd, poll_position->events, 0} );
			} else {
				sockets.at(poll_position->fd).reset();
			}
		}
	}

	Listener::Listener(uint16_t port):
		Socket(0)
	{
		// Set up the local address.
		// Default to all addresses for now.
		local_addr.sin6_port = htons(port);
		local_addr.sin6_addr = in6addr_any;
	}

	void Listener::start() {
		// Bind the local address.
		int sock = socket(AF_INET6, SOCK_STREAM, 0);
		throw_posix_errno_if(sock<0);
		handle = sock;
		set_nonblocking();
		int val = 1;
		throw_posix_errno_if( ::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val) );
		throw_posix_errno_if( ::bind(handle, (sockaddr*)&local_addr, sizeof local_addr) );
		// Start listening.
		throw_posix_errno_if( ::listen(handle, backlog) );
	}

	Socket::Action Listener::on_input() {
		if(!sockets->running)
			return REMOVE;
		auto result = do_accept();
		if(result.ok) {
			auto conn = std::make_shared<Connection>(result.handle, result.remote_addr);
			sockets->add_socket(conn);
		}
		return KEEP;
	}

	Listener::AcceptResult  Listener::do_accept() {
		AcceptResult result;
		socklen_t remote_addr_len = sizeof result.remote_addr;

		int new_handle = ::accept(handle, (sockaddr*)&result.remote_addr, &remote_addr_len);
		if(new_handle < 0 && errno == EAGAIN)
			return result;
		throw_posix_errno_if(new_handle < 0);
		result.handle = new_handle;
		result.ok = true;
		return result;
	}

	Connection::Connection(int h, const sockaddr_in6 &remote, time_t timeout):
		Socket(h, remote, timeout)
	{
		logger
			<< "making a new connection, handle: " << h
			<< " from " << remote;
		if(timeout) logger
			<< " with timeout " << timeout;
		logger << std::endl;

		input.reserve(io_block_size);
		output.reserve(io_block_size);
	}

	void Connection::close_output() {
		closing = true;
	}

	Socket::Action Connection::on_input() {
		size_t input_point = input.size();
		input.resize(input_point + io_block_size);
		ssize_t bytes = ::read(handle, input.data()+input_point, io_block_size);
		if(bytes < 0 && errno == EAGAIN)
			return KEEP;
		if(bytes < 0 && errno == ECONNRESET)
			return REMOVE;
		if(bytes < 0 && errno == ETIMEDOUT)
			return REMOVE;
		throw_posix_errno_if( bytes < 0 );
		input.resize(input_point + bytes);
		if(bytes == 0) {
			logger << "end of file on handle " << handle << std::endl;
			return REMOVE;
		}
		return KEEP;
	}

	Socket::Action Connection::on_output() {
		ssize_t bytes = ::write(handle, output.data(), output.size());
		if(bytes < 0) {
			if(errno == EAGAIN)
				return KEEP;
			if(errno == EPIPE) {
				logger << "output closed on handle " << handle << std::endl;
				return REMOVE;
			}
			if(errno == ECONNRESET)
				return REMOVE;
		}
		throw_posix_errno_if( bytes < 0 );
		output.erase(output.begin(), output.begin()+bytes);
		if(sockets) {
			if(output.empty()) {
				sockets->clear_write_event();
				if(closing)
					::shutdown(handle, SHUT_WR);
			} else {
				sockets->set_write_event();
			}
		}
		return KEEP;
	}

};
