<!-- vim: set filetype=markdown noet ts=2 sw=2: -->

<meta charset="utf-8">
<style>
	body {
		width: 30em;
	}
	pre {
		background-color: lightgray;
		display: inline-block;
	}
</style>

throwing exceptions out of a destructor
http://stackoverflow.com/questions/130117/throwing-exceptions-out-of-a-destructor

Exception thrown from destructor
http://software.intel.com/sites/products/documentation/doclib/iss/2013/sa-ptr/sa-ptr_win_lin/GUID-D2983B74-74E9-4868-90E0-D65A80F8F69F.htm


# C++: error-handling during resource cleanup

## Introduction

C++ is an unusual language in that it provides high-level concepts such as
classes and exception-handling while at the same time not providing garbage
collection. This sometimes forces the programmer to explicitly deallocate
resources, such as by freeing memory and closing file descriptors. Consequently,
C++ doesn't hide away the problem of _dealing with errors that occur during
cleanup_. For example, what should a program do when an attempt to close a file
fails?

Note that RAII doesn't solve this problem. RAII merely makes cleanup simpler by
causing it to happen automatically when a resource goes out of scope, thus
reducing the likelihood of resource-leak bugs. However, RAII doesn't help handle
errors that occur during that cleanup. Indeed, RAII gets in the way of dealing
with cleanup errors.

In this lecture, I'm going to talk about the language rules for mixing
exceptions with destructors, and, by using the example of a file I/O error, I'll
show why error-handling during cleanup is a fundamentally hard problem. Then
I'll opine as to some rules of thumb for dealing with the problem, as well as
making sure you're aware of the contrary viewpoints.

## Language rules for destructors that throw

The language rules for destructors that throw changed in C++11. The following
sections summarize the rules, and the program [terminate.cpp](terminate.cpp),
below, shows the differences between the specs.

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

### C++03

In C++03, destructors throw exceptions just as other functions do, and the
program behaves as many would expect: a function higher up the call stack may
`catch` the exception and deal with it, or else the process exits via
`std::terminate()`.

However, a problem arises that if two destructors are called one after the other
but prior to the `catch`, and if both destructors throw an exception, then the
call stack will have two exceptions in flight at the same time. C++ doesn't
allow this: as soon as the second destructor's exception leaves the destructor,
the process terminates.

A prudent rule of thumb is that _any destructor that throws an exception may
kill your process_.

The output of [terminate.cpp](terminate.cpp):

	begin main()
	begin charlie::charlie()
	end charlie::charlie()
	end main()
	begin charlie::~charlie()
	begin bravo::bravo()
	end bravo::bravo()
	throwing from charlie::~charlie()
	begin bravo::~bravo()
	begin alpha::alpha()
	end alpha::alpha()
	throwing from bravo::~bravo()
	begin alpha::~alpha()
	end alpha::~alpha()
	terminate called after throwing an instance of 'std::runtime_error'
	  what():  bravo::~bravo()
	Aborted (core dumped)

As shown in the output above, both the `~charlie` and `~bravo` exceptions are
thrown, and the process terminates when the `~bravo` exception—the second
exception—escapes the destructor. Note that after the `~bravo` exception is
thrown but before the process terminates, the destructor's local variable is
destructed.

### C++11

In C++11, destructors implicitly gain, by default, a `noexcept` specifier, which
means that if the destructor throws an exception then the process will
terminate. There's no need to have two exceptions in flight at the same time:
one exception is enough to kill your process.

The output of [terminate.cpp](terminate.cpp):

	begin main()
	begin charlie::charlie()
	end charlie::charlie()
	end main()
	begin charlie::~charlie()
	begin bravo::bravo()
	end bravo::bravo()
	throwing from charlie::~charlie()
	terminate called after throwing an instance of 'std::runtime_error'
	  what():  charlie::~charlie()
	Aborted (core dumped)

As shown above, only the `~charlie` exception is thrown, _immediately_ after
which the process terminates. Not even the destructor's local variable is
destructed.

## Examples


### An internal error: `class mutex`

### An external error: `class file`
