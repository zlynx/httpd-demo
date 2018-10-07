#pragma once
#include <exception>
#include <cerrno>

namespace zlynx {
	extern std::ostream& logger;

	class
	posix_error : public std::runtime_error {
		private:
		int errno_;

		static std::string err_str(int err)
		{
			char buf[128] = {0};
#ifdef _GNU_SOURCE
			return strerror_r(err, buf, sizeof(buf));
#else
			strerror_r(err, buf, sizeof(buf));
#endif
			// safe because it is used to immediately construct a std::string
			return buf;
		}

		public:
		explicit
		posix_error(const std::string &what, int err)
		: std::runtime_error(what+": "+err_str(err)), errno_(err)
		{}

		int get_errno() const { return errno_; }
	};
};

#define STR(s) #s
#define XSTR(s) STR(s)
#define FILE_LINE __FILE__ ":" XSTR(__LINE__)

#define ERRSTR(x) std::string(FILE_LINE).append(": ").append(__PRETTY_FUNCTION__).append(": ").append(#x)

#define throw_win32_err_if(x) \
	do { if(x) { \
		std::string estr( ERRSTR(x) ); \
		throw win32_error( estr, GetLastError() ); \
	} } while(0)

#define throw_posix_errno_if(x) \
	do { if(x) { \
		std::string estr( ERRSTR(x) ); \
		throw posix_error( estr, errno ); \
	} } while(0)

#define throw_posix_reterr_if(x) \
	do { \
		int r__ = (x); \
		if(r__) { \
			std::string estr( ERRSTR(x) ); \
			throw posix_error( estr, r__ ); \
		} \
	} while(0)

#define throw_if(x) \
	do { if(x) { \
		std::string estr( ERRSTR(x) ); \
		throw std::runtime_error( estr ); \
	} } while(0)
