#include <cstring>
#include <pthread.h>
#include <sstream>
#include <stdexcept>

class mutex {
	::pthread_mutex_t m;
public:

	~mutex() {
		int n = ::pthread_mutex_destroy(&m);
		if (n) {
			std::ostringstream ss;
			ss << "error destroying mutex: " << strerror(n);
			throw std::runtime_error(ss.str());
		}
	}

	mutex() {
		int n = ::pthread_mutex_init(&m, nullptr);
		if (n) {
			std::ostringstream ss;
			ss << "error creating mutex: " << strerror(n);
			throw std::runtime_error(ss.str());
		}
	}

	mutex(mutex const &) = delete;
	mutex(mutex &&) = default;
	mutex &operator=(mutex const &) = delete;
	mutex &operator=(mutex &&) = default;

	void lock() {
		int n = ::pthread_mutex_lock(&m);
		if (n) {
			std::ostringstream ss;
			ss << "error locking mutex: " << strerror(n);
			throw std::runtime_error(ss.str());
		}
	}

	void unlock() {
		int n = ::pthread_mutex_unlock(&m);
		if (n) {
			std::ostringstream ss;
			ss << "error unlocking mutex: " << strerror(n);
			throw std::runtime_error(ss.str());
		}
	}

};

int main() {
	mutex x;
	return 0;
}

