// vim: set noet ts=2 sw=2:

#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

class file {
	FILE *f;
public:

	~file() {
		if (EOF == ::fclose(f)) {
			std::ostringstream ss;
			ss << "error closing file: " << strerror(errno);
			throw std::runtime_error(ss.str());
		}
	}

	file() = delete;

	file(std::string const &s) {
		f = ::fopen(s.c_str(), "w");
		if (!f) {
			std::ostringstream ss;
			ss << "error opening file: " << strerror(errno);
			throw std::runtime_error(ss.str());
		}
	}

	file(file const &) = delete;
	file(file &&) = default;
	file &operator=(file const &) = delete;
	file &operator=(file &&) = delete;

	void write(std::string const &s) const {
		size_t n = ::fwrite(s.c_str(), s.size(), 1, f);
		if (static_cast<size_t>(-1) == n) {
			std::ostringstream ss;
			ss << "error writing data: " << strerror(errno);
			throw std::runtime_error(ss.str());
		}
		if (1 != n) {
			throw std::runtime_error("incomplete write operation");
		}
	}
};

int main(int argc, char **argv) {
	file x(argv[1]);
	x.write("Hello, from C++.");
	std::string dummy;
	std::getline(std::cin, dummy);
	return 0;
}

