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

# Handling cleanup errors in C++

## Introduction

C++ is an unusual language in that it provides high-level concepts such as
classes and exceptions while at the same time not providing garbage collection.
Programmers must deallocate resources explicitly, such as by freeing memory and
closing file descriptors. Consequently, C++ doesn't hide the problem of dealing
with errors that occur during cleanup. For example, what should a program do
when an attempt to close a file fails?

RAII doesn't solve this problem. RAII merely makes cleanup simpler by causing it
to happen automatically when a class instance goes out of scope, thus reducing
the likelihood of resource leaks. However, RAII doesn't help handle errors that
occur during that cleanup. Indeed, RAII gets in the way of dealing with cleanup
errors—as we'll see later.

In this lecture, I'm going to talk about the language rules for mixing
exceptions with destructors, and, by using a couple examples, I'll show why
error-handling during cleanup is a problem with no clear solution. Then I'll
opine as to some rules of thumb for dealing with the problem, as well as making
sure you're aware of the contrary viewpoints.

## Language rules for destructors that throw

The language rules for destructors that throw changed in C++11. The following
program, [terminate.cpp](terminate.cpp), shows the differences between C++03 and
C++11.

_Note:_ When I say <q>destructor that throws</q>, I mean a destructor that
throws an exception or causes an exception to be thrown and that doesn't catch
that exception. A destructor that throws and catches an exception is not a
<q>throwing destructor</q>.

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

In C++03, destructors may throw exceptions as other functions do. After a throw,
a function higher up the call stack may `catch` the exception, or else the
process exits via `std::terminate()`.

However, a problem arises that if two destructors are called one after the
other but prior to the `catch`—such as may happen during a stack unwind—and if
both destructors throw an exception, then the call stack will have two
exceptions in flight at the same time. C++ doesn't allow this: as soon as the
second destructor's exception leaves that destructor, the process terminates.
This is shown in the output of [terminate.cpp](terminate.cpp), compiled for
C++03:

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

Here's the output of [terminate.cpp](terminate.cpp), compiled for C++11 (`gcc`
version >= 4.7.3):

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
remainder of this article; the important thing to remember is that an exception
thrown from a destructor may prove fatal to the process. Therefore, from now on,
we're going to consider only C++11 programs.

## Example #1: `class mutex`

### Problem

Sometimes a resource can't be freed successfully. An example of such a resource
is the POSIX Threads mutex, `pthread_mutex_t`. Its cleanup function,
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
		// The mutex is locked and therefore in a non-destructible state.
		return 0;
	}

Here's the program's output:

	terminate called after throwing an instance of 'std::runtime\_error'
	  what():  error destroying mutex: Device or resource busy
	Aborted

### Solution

How should we solve the problem in [mutex-1.cpp](mutex-1.cpp) so that the
program doesn't terminate via `std::terminate()`? Here are some possibilities:

1. Ignore the error. Don't throw.

2. Add a `noexcept(false)` specifier to the destructor, throw as normal, and let
	 the caller be warned.

3. Rather than throwing an exception in the destructor, log or print an error.

4. Call `mutex::unlock()` before destructing the mutex.

The first three fixes are flawed.

1. In a more complex program, ignoring an error can lead to more errors later,
	 and the later errors may be hard to diagnose because there's no indication of
	 the underlying problem.

2. Adding `noexcept(false)` doesn't guard against the problem of a program
	 terminating because of two exceptions being in flight at the same time. Also,
	 there are more subtle problems with destructors that throw, which I'll
	 explain later.

3. Logging or printing an error ties a low-level module to the program's
	 overarching strategy for handling errors. A low-level module such as `class
	 mutex` shouldn't depend on whether errors are, say, logged to a file or
	 printed to the screen.

This leaves fix #4, which happens to be a good solution. The programmer has
control over the outcome of `pthread_mutex_destroy()`; that is, the function
fails only if the programmer makes the mistake of destructing the mutex while
it's in a non-destructible state. The simplest fix is for the programmer not
make that mistake.

[Here](mutex-2.cpp)'s the same program with fix #4 applied.

## Example #2: `class file`

### Problem

Not all cleanup errors are logic errors. Sometimes cleanup fails for external
reasons: these are environmental problems that the programmer has no control
over and thus must anticipate and deal with. One example of such an error is
when closing a file descriptor.

In addition to the usual logic errors that could lead to `EBADF` (invalid file
descriptor), the `close()` function may fail because of an I/O error (`EIO`).
From the `close(2)` man page:

> Not  checking  the  return value of `close()` is a common but nevertheless
> serious programming error.  It is quite possible that errors on a previous
> `write(2)` operation are first reported at the final `close()`.  Not checking
> the return value when closing the file may lead to silent loss of data.  This
> can especially be observed with NFS and with disk quota.

Demonstrating a `close()` error is hard to do in practice, but the
line-buffering behavior of a C runtime library `FILE` object makes `fclose()`
errors easy to bring about, so we're going to use `fclose()` in our example.

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

Fix #4, above, works only if the programmer explicitly calls the `file::close()`
function. However, this subverts RAII. The purpose of RAII is to eliminate the
burden of explicitly freeing resources, so forcing a `file::close()` call on the
programmer is flawed.

(You may be wondering why fix #4 can't be combined with a scope guard whose
destructor calls `file::close()`. The reason is that the scope guard destructor
might throw and would thus suffer the same problems that the `class file`
destructor suffers.)

This leaves us with four flawed possibilities:

1. Ignore an error.

2. Allow the program to terminate.

3. Tie error-reporting to error-handling.

4. Give up RAII.

Any fix we implement will suffer from at least one of the unpleasant side
effects listed above. This is why cleanup errors are fundamentally hard to deal
with.

In my opinion, fixes #1, #2, and #3 should be avoided in all but trivial
non-production programs. Fix #1 subverts error-handling, and fix #3 scales badly
as the program increases in size. As for fix #2, the bad side effects are more
nuanced.

At best, fix #2 kicks the can down the road: rather than causing immediate
program termination (as in C++11), the I/O error will cause program termination
only in conjunction with a second exception being thrown—i.e., another cleanup
error occurring. But C++11 has good reason to add the implicit `noexcept`
specifier to destructors—explained later—and undoing this spec change is a bad
idea in most cases. So fix #2 is out.

That leaves only fix #4, again. But is it reasonable to expect code higher up
the call stack to explicitly call `file::close()`? No. It's hard to write
non-trivial, correct C++ code without using RAII. However, it's reasonable to
_allow_ code higher up the stack to explicitly do cleanup. Code that does
explicit cleanup will handle exceptions as normal, with no possibility of
program termination, while code that relies on automatic cleanup via the
destructor suffers possible program termination.

[Here](io-2.cpp)'s the same program with fix #4 applied.

Note that a `FILE` object, as with a POSIX file descriptor, can't be used after
it's closed—regardless whether the close operation succeeded. This simplifies
explicit cleanup because the destructor can guarantee both that (1) it won't
throw after explicit cleanup is done and (2) it won't leak resources because the
underlying file object is (presumably) closed by the operating system.

### Crash-only design

Providing the means for explicit cleanup—but not mandating it—leaves a program
open to the possibility of termination. Is that so bad? Perhaps not.

[Crash-only design](http://en.wikipedia.org/wiki/Crash-only_software) takes
advantage of program termination as a means for handling errors. The rationale
is that since programs must be able to recover from crashes anyway, it's better
to abort on non-trivial errors, such as cleanup errors, and use the program's
existing crash-recovery mechanism, than to attempt to recover from errors within
the same process space.

The partial solution of fix #4, in the previous section, fits in nicely with
crash-only design. While it's a good idea to allow code higher up the call stack
to do explicit cleanup by providing an explicit <q>close</q> function, the best
design for many programs is to let the program fall back to RAII and destructors
that throw and to let any cleanup error cause a crash. This strategy of
<q>failing fast</q> tends to lead to simpler software that's easier to test,
owing to more straightforward control flows.

### Opposing viewpoints

However, be aware that crash-only design is _not_ the norm in most industries
using C++. Many C++ shops treat abnormal program termination as one of the worst
possible scenarios, and consequently they endeavor to make their programs limp
on in a degraded state rather than restart. Proponents of these opposing
viewpoints sometimes state that a destructor must never throw an exception.

Here are some links expressing that view:

* [stackoverflow.com: throwing exceptions out of a
	destructor](http://stackoverflow.com/questions/130117/throwing-exceptions-out-of-a-destructor)

* [Scott Meyers: More Effective
	C++](http://bin-login.name/ftp/pub/docs/programming_languages/cpp/cffective_cpp/MEC/MI11_FR.HTM)

* [Herb Sutter: Exception-Safe Generic
	Containers](http://bin-login.name/ftp/pub/docs/programming_languages/cpp/cffective_cpp/MAGAZINE/SU_FRAME.HTM#destruct)

* [Intel: Static Analysis Problem Type
	Reference](http://software.intel.com/sites/products/documentation/doclib/iss/2013/sa-ptr/sa-ptr_win_lin/GUID-D2983B74-74E9-4868-90E0-D65A80F8F69F.htm)

The Sutter article explains subtle reasons why it's usually a bad idea for an
exception to escape a destructor. For example, a throwing destructor as part of
a call to `delete[]` results in undefined behavior. This is partly why the C++11
implicit `noexcept` specifier is beneficial: it prevents undefined behavior
owing to destructors that throw. Nevertheless, in the standard C++ library,
destructors are forbidden from throwing exceptions, though they may call
`std::terminate()` directly.

