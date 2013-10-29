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
collection. This forces the programmer to deallocate resources explicitly, such
as by freeing memory and closing file descriptors. Consequently, C++ doesn't
hide the problem of _dealing with errors that occur during cleanup_. For
example, what should a program do when an attempt to close a file fails?

RAII doesn't solve this problem. RAII merely makes cleanup simpler by causing it
to happen automatically when a class instance goes out of scope, thus reducing
the likelihood of resource-leak bugs. However, RAII doesn't help handle errors
that occur during that cleanup. Indeed, RAII gets in the way of dealing with
cleanup errors—as we'll see later.

In this lecture, I'm going to talk about the language rules for mixing
exceptions with destructors, and, by using a couple examples, I'll show why
error-handling during cleanup is a problem with no clear solution. Then I'll
opine as to some rules of thumb for dealing with the problem, as well as making
sure you're aware of the contrary viewpoints.

## Language rules for destructors that throw

The language rules for destructors that throw changed in C++11. The following
program, [terminate.cpp](terminate.cpp), behaves differently depending on which
spec it's compiled for.

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

The behavior differences are explained below.

### C++03

In C++03, destructors throw exceptions as other functions do, and the program
behaves as expected: a function higher up the call stack may `catch` the
exception, or else the process exits via `std::terminate()`.

However, a problem arises that if two destructors are called one after the other
but prior to the `catch`, and if both destructors throw an exception, then the
call stack will have two exceptions in flight at the same time. C++ doesn't
allow this: as soon as the second destructor's exception leaves that destructor,
the process terminates. This is shown in the output of
[terminate.cpp](terminate.cpp), compiled for C++03:

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

Both the `~charlie` and `~bravo` exceptions are thrown, and the process
terminates when the `~bravo` exception—the second exception—escapes the
destructor. Note that after the `~bravo` exception is thrown but before the
process terminates, the destructor's local variable is destructed.

In C++03, a prudent rule of thumb is that _any destructor that throws an
exception may kill your process_.

### C++11

In C++11, destructors implicitly gain, by default, a `noexcept` specifier. This
means that if the destructor throws an exception then the process will
immediately terminate. There's no need to have two exceptions in flight at the
same time: one exception is enough to kill your process.

Here's the output of [terminate.cpp](terminate.cpp), compiled for C++11:

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

The distinctions between the C++03 and C++11 specs don't matter for the
remainder of this article; the important thing to remember is an exception
thrown from a destructor may prove fatal to the process. Therefore, from now on,
we're going to consider C++11 programs.

## Example: `class mutex`

### Problem

Sometimes a resource can't be freed successfully. An example of such a resource
is the POSIX Threads mutex: `pthread_mutex_t`. Its cleanup function,
`pthread_mutex_destroy()`, returns a nonzero error code (`EBUSY`) if the mutex
is currently locked. The following program, [mutex-1.cpp](mutex-1.cpp), shows
the error.

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
		return 0;
	}

Here's the program's output:

  terminate called after throwing an instance of 'std::runtime\_error'
	  what():  error destroying mutex: Device or resource busy
	Aborted

### Solution

How should we solve the problem in [mutex-1.cpp](mutex-1.cpp)? Here are some
possibilities:

1. Ignore the error. Don't throw.

2. Add a `noexcept(false)` specifier to the destructor, throw as normal, and let
	 the caller be warned.

3. Rather than throwing an exception in the destructor, log or print an error.

4. Call `mutex::unlock()` before destructing the mutex.

I favor #4. My rationale is that the programmer has control over the outcome of
`pthread_mutex_destroy()`; that is, the function fails only if the programmer
makes the mistake of trying to destruct the mutex while it's in a
non-destructible state. The simplest fix is to not make that mistake.

[Here](mutex-2.cpp)'s the same program with fix #4 applied.

## Example: `class file`

### Problem

Not all cleanup errors are logic errors. Sometimes cleanup fails for external
reasons: these are environmental problems that the programmer has no control
over and thus must anticipate and deal with. One example of such an error is
when closing a file descriptor.

In addition to the usual logic errors that could lead to `EBADF` (invalid file
descriptor), the `close()` function may fail because of `EIO` (I/O error). From
the `close(2)` man page:

> Not  checking  the  return value of close() is a common but nevertheless
> serious programming error.  It is quite possible that errors on a previous
> write(2) operation are first reported at the final close().  Not checking the
> return value when closing the file may lead to silent loss of data.  This can
> especially be observed with NFS and with disk quota.

Demonstrating a `close()` error is hard to do in practice, but the
line-buffering behavior of a `FILE` object makes `fclose()` errors easy to bring
about. Therefore, we're going to use `fclose()` for our example.

The way it works is as follows:

1. Open a file on a network-backed file system.

2. Write an incomplete line to the file. The line is buffered in memory.

3. Wait for keyboard input from the user. During this time, the user forces a
	 network disconnection, such as by unplugging the network cable.

4. Close the file.

The last step causes the file's buffer to flush, but because the file system is
inaccessible, the operating system propagates an I/O error to the program.
Here's the program, [io-1.cpp](io-1.cpp):

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

Here's the program's output:

	terminate called after throwing an instance of 'std::runtime_error'
	  what():  error closing file: Input/output error
	Aborted

### No clear solution

How should we solve the problem in [io-1.cpp](io-1.cpp)? We have four
possibilities, analogous to mutex example.

1. Ignore the error. Don't throw.

2. Add a `noexcept(false)` specifier to the destructor, throw as normal, and let
	 the caller be warned.

3. Rather than throwing an exception in the destructor, log or print an error.

4. Implement `file::close()` and call `file::close()` before destructing the
	 file.

But unlike with the mutex example, where the root cause of the cleanup error was
a logic error that could be brought under control by the programmer, this I/O
cleanup error has no such easy out. The I/O error must be dealt with.

Fix #4, above, works so long as the programmer remembers to explicitly call the
`file::close()` function. However, this subverts RAII. The purpose of RAII is to
eliminate the burden of explicitly freeing resources, so forcing a
`file::close()` call on the programmer is flawed.

(You may be wondering why fix #4 can't be combined with a scope guard object
whose destructor calls `file::close()`. The reason is that the scope guard
destructor would suffer the same need-to-throw problem that the `class file`
destructor suffers.)

This leaves three flawed alternatives:

1. Ignoring the error is bad because it means there'll be no indication the file
	 failed to be written to disk. This could lead to severe problems and no clear
	 way to diagnose them.

2. Despite adding a `noexcept(false)` specifier to the destructor, the process
	 will nevertheless terminate if a second exception goes into flight. A
	 terminated process is innately bad.

3. Logging or printing an error has the problem of tying `class file` to the
	 program's overarching strategy of handling errors. But the program may wish
	 to handle errors in a different way. For example, `class file` may log the
	 error to a file, but the program might wish to log errors to `stderr`—or vice
	 versa. Fix #3 makes `class file` less general-purpose than it ought to be.
 
Are there other possibilities? If there are, I haven't found them. It turns out
cleanup errors are fundamentally hard to deal with.

In my opinion, fixes #1 and #3 should be avoided: #1 subverts error-handling and
\#3 doesn't scale to non-trivial-sized projects. Furthermore, fix #4 should be
avoided. While it's a good idea to implement a `file::close()` function and let
the programmer explicitly destruct the file if they choose, forcing them to give
up RAII is too expensive a cost.

This leaves us with fix #3 and its unpleasant side effect of a terminated
process. But is termination so bad?

### Crash-only design


(begin notes)

Some opinions:
- It's a bug to pretend the file descriptor closed when it didn't. A destructor
	that fails should throw.
- However, it's a mistake to force the class's caller to deal with possible
	termination. Thus, add an explicit `close()` function to let the caller handle
	errors outside of the destructor.
- But so long as the close operation fails, then the destructor probably will
	too. This forces the caller to keep the file instance in scope until the close
	operation succeeds. Annoying, but correct.
- Or, of course, the caller may allow things to fail.

Mention "crash-only design". If you must spend time and effort making your
program crash-recoverable, then maybe crashing because `close()` fails is the
best thing to do--better than all the extra complexity of handling the error.


Show [file_io-1.cpp](file_io-1.cpp) as an example of how the close error is not
handled correctly in Python3.

TODO: Show that RAII gets in the way of dealing with cleanup errors.

(end notes)


