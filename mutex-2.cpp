// vim: set noet ts=2 sw=2:

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
	x.lock();
	x.unlock();
	// The mutex is unlocked and therefore in a destructible state.
	return 0;
}

