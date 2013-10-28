// vim: set noet ts=2 sw=2:

#include <iostream>
#include <stdexcept>

class alpha {
public:
	~alpha() {
		std::cerr << "begin alpha::~alpha()" << std::endl;
		std::cerr << "end alpha::~alpha()" << std::endl;
	}
	alpha() {
		std::cerr << "begin alpha::alpha()" << std::endl;
		std::cerr << "end alpha::alpha()" << std::endl;
	}
};

class bravo {
public:
	~bravo() {
		std::cerr << "begin bravo::~bravo()" << std::endl;
		alpha a;
		std::cerr << "throwing from bravo::~bravo()" << std::endl;
		throw std::runtime_error("bravo::~bravo()");
		std::cerr << "end bravo::~bravo()" << std::endl;
	}
	bravo() {
		std::cerr << "begin bravo::bravo()" << std::endl;
		std::cerr << "end bravo::bravo()" << std::endl;
	}
};

class charlie {
public:
	~charlie() {
		std::cerr << "begin charlie::~charlie()" << std::endl;
		bravo a;
		std::cerr << "throwing from charlie::~charlie()" << std::endl;
		throw std::runtime_error("charlie::~charlie()");
		std::cerr << "end charlie::~charlie()" << std::endl;
	}
	charlie() {
		std::cerr << "begin charlie::charlie()" << std::endl;
		std::cerr << "end charlie::charlie()" << std::endl;
	}
};

int main() {
	try {
		std::cerr << "begin main()" << std::endl;
		charlie a;
		std::cerr << "end main()" << std::endl;
	} catch (std::exception &e) {
		std::cerr << "caught exception: " << e.what() << std::endl;
	}
}

