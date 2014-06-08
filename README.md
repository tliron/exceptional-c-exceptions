Exceptional C Exceptions
========================

Yet another try/throw/catch/finally library for C! (for C99, actually)

_Currently in beta and not recommended for production use (as of June 8, 2014)._

What makes this library "exceptional" is its flexible support for multi-threaded
environments, specifically POSIX threads (pthreads), [SDL](http://www.libsdl.org/)
threads, and OpenMP. Especially powerful is its ability to allow for exceptions
to be thrown in OpenMP-optimized parallel code and caught in the containing context.
(Even C++ can't do that properly.)

Its architecture does introduce some usage quirks. If these are unacceptable to
you, please take a look at the alternative libraries mentioned below. We won't be
offended: pick the right tool for the job, always!

Exceptional C Exceptions also has the following standard features:

* Exceptions contain call location information: specifically the filename and line
where they were thrown.
* Create a custom exception type hierarchy, whereby a `catch` would also match any
descendant types.
* Exception message formatting a-la `printf`.
* Nested exceptions: an exception can have a "cause", another exception, which can
also have a "cause", and so on.

Building and Running the Example
--------------------------------

We use [Waf](https://code.google.com/p/waf/) to build. You need Python installed for
Waf to work, and of course a C compiler to build (tested with gcc). Build and run:

		./waf configure
		./waf build
		build/example

The example showcases both POSIX threads and OpenMP. To keep things simple, it
doesn't use SDL. If you want to build Exceptional C Exceptions with SDL support,
just make sure to include the file "exception_scope_sdl.c" in your build.

If you run "example" with any argument, it will also dump some nice and colorful
Exceptional C Exceptions debug information to stderr.

The git repository also includes an Eclipse CDT project, so you can just import
from the main directory.

Usage
-----

Before you make any calls that can throw exceptions, you must have a `with_exceptions`
code block:

		#include "exceptional.h"

		with_exceptions (global) {
			try {
				throw(Exception, "oops");
			}
			finally {
				catch (Exception, e) {
					Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
				}
				printf("finally\n");
			}
		}

You'll immediately notice that `catch` is inside `finally`. This syntax is slightly
different from C++/Java/JavaScript/C#, but the semantics are otherwise the same: just
make sure that the code to be run finally is anywhere in `finally` but outside your
`catch` code blocks. Exceptional C Exceptions will make sure it is executed during
unwinding.

Don't worry too much about forgetting the `with_exceptions`: if you don't put it in,
you will get a compiler error when trying to use `try`/`throw`/`catch`/`finally`.

By the way, anywhere our keywords are followed by a code block, you don't have to use
"{ ... }" if you only have a single line of code, similarly to the way standard C
keywords work (like `if`, `for`, etc.). So, this is a fine alternative for the above
code:

			try
				throw(Exception, "oops");
			finally {
				catch (Exception, e)
					Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
				printf("finally\n");
			}

You can even write things like this if you don't have any finally-executing code:

			try
				throw(Exception, "oops");
			finally catch (Exception, e)
				Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);

And this:

			try throw(Exception, "oops");
			finally printf("we're done here\n");

That said, note that some keywords are _not_ themselves single lines of code: `try`,
`with_exceptions`, `with_exceptions_relay`, `with_exceptions_relay_to`, and
`capture_exceptions` should all themselves be inside "{ ... }" if the are to be
considered as a whole.

In above example, we've used the `Exception_dump` utility API to print out the
exception information: it supports `EXCEPTION_DUMP_SHORT`,
`EXCEPTION_DUMP_LONG`, and `EXCEPTION_DUMP_NESTED`. You can also access this
information directly:

		printf("%s: %s\n%s\nAt %s:%d %s()\n",
			e->type->name,
			e->type->description,
			e->message,
			e->location.file,
			e->location.line,
			e->location.fn);

#### Throwing

There are two additional variants of `throw`:

* Use `throwd` when you want to let Exceptional C Exceptions free the message string
automatically. Internally, it creates a duplicate (like `strdup`) of your message.
* Use `throwf` for printf-style formatted messages, e.g.:
`throwf(Value, "wrong number: %d, for: %s", number, name)`. The default maximum
message size is 2048, but you can change it by defining `EXCEPTION_MAX_MESSAGE_SIZE`.
Exceptional C Exceptions will free the generated string automatically.

Inside a `catch` code block, you can use `rethrow` variants instead of `throw`,
specifying the caught exception as a "cause":

		catch (Authorization, e)
			rethrowf(e, IO, "could not access file: %s", filename);

You can access the cause of any exception using `e->cause` recursively. It will be
null if there is no cause.

#### Catching

You need to be careful about operations that would exit the `catch` code block
prematurely, because then the exception instance would be left unmanaged, which is
a memory leak.

It is safe to use `rethrow` to exit a `catch`, because then Exceptional C Exceptions
will manage the "cause" together with the newly thrown exception (recursively).

Another alternative, if you _do_ need to exit the `catch` code block without a
`rethrow` (for example: via a  C `return` or a `break` to exit the `catch` block)
is to release the exception first by calling `release_exception`:

		catch (IO, e) {
			release_exception(e);
			return false;
		}

Generally, if you are doing anything that might throw an exception in a `catch`, you
must be very careful that the original exception instance doesn't remain unmanaged.
One way to ensure this is to use another `try`: 

		catch (Authorization, e) {
			try {
				// some code that might throw an exception
			}
			finally catch (Exception, ee) {
				release_exception(e);
				rethrowf(ee, IO, "could not access file: %s", filename);
			}
		}

In the above example, the `e` exception is explicitly released, while `ee` is
managed by `rethrow`. So we're good, no memory leaks.		

#### Contexts

The argument to `with_exceptions` specifies the context used to store the exceptions
and the call stack. In a multi-threaded environment, you would need a separate context
per thread. You can use the contexts `posix`, `openmp` and `sdl`, which all make use of
thread-local storage specific to that technology.

`global` can be used in a single-threaded environment: the context is stored in a global
variable used by all `with_exceptions (global)` code blocks. For an even more restricted
scope, you can use `local` instead, for which the context will exist only in the current
local variable scope. `local` is useful for one-off runs of code in which you need
localized exception handling that will not effect anything else. Note that you can still
relay  to `local` contexts from called functions (see below).

Here's an SDL example:

		int background_thread(void *data) {
			with_exceptions (sdl) {
				try
					throw(Exception, "oops");
				finally catch (Exception, e)
					Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
			}
			return 0;
		}
 
		int main(int argc, char *argv[]) {
			initialize_exceptions(sdl);
		
			int r = 0;
			SDL_Thread *thread1 = SDL_CreateThread(background_thread, "Background1", NULL);
			SDL_Thread *thread2 = SDL_CreateThread(background_thread, "Background2", NULL);
			SDL_WaitThread(thread1, &r);
			SDL_WaitThread(thread2, &r);
		
			return r;
		}

In the above, each thread will use its own independent exception context, thus an
exception thrown in one thread will have no effect on the other. Sometimes, though,
you actually _do_ want threads to affect each other: see "capturing", below.

For some contexts, you must also call initialization and/or shutdown commands before
and/or after using them in `with_exceptions`:

* global: `shutdown_exceptions(global)`
* posix: `initialize_exceptions(posix)`, `shutdown_exceptions(posix)`
* openmp: `initialize_exceptions(openmp)`, `shutdown_exceptions(openmp)`
* sdl: `initialize_exceptions(sdl)`

If you're using OpenMP together with another context (see "relaying", below), then
you must make sure to call  `initialize_exceptions(openmp)` and
`shutdown_exceptions(openmp)` from _within the containing context_. For example,
if you're using POSIX threads, initialize/shutdown OpenMP in the POSIX thread
function. The reason for this is that implementations of OpenMP may create separate
thread teams per thread from which they are called.

#### Functions

Let's say you're writing a function that has a `throw`. If you know in advance
which context you'll be supporting, you just need to use a `with_exceptions`:

		int divide(int x, int y) {
			with_exceptions (global)
				if (y == 0)
					throw(DivideByZero, "oops");
			return x / y;
		}

(Note that a `local` context for such a function doesn't normally make sense,
because then exceptions will never be thrown to the caller, though it might
be useful if you specifically want to swallow all exceptions.)

The problem is that you might want your function to be usable for any arbitrary
context. The only way to pass such information from the caller to the function is
via a function argument. And this is where Exceptional C Exceptions gets a little
quirky: we've created function decorators that insert the context scope as the
first argument of the function:

		int divide WITH_EXCEPTIONS (int x, int y) {
			if (y == 0)
				throw(DivideByZero, "oops");
			return x / y;
		}

That's not too bad, is it? The requirement for decorating the function is similar
to that for checked exceptions in other languages. And it even takes fewer lines
than when specifying a `with_exceptions` code block.

Well, unfortunately we also need to call such functions with a special decorator:

		with_exceptions (posix) {
			try
				printf("10 / 0 = %d", divide CALL_WITH_EXCEPTIONS (10, 0));
			finally catch (Exception, e)
				Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
		}

Note that if your function has no arguments, you must use `WITH_EXCEPTIONS_VOID`
and `CALL_WITH_EXCEPTIONS_VOID` variants. This is due to limitations of C macros.

We don't think this function decorator stuff is too awful, but it definitely seem
a bit strange. The good thing about this requirement is that the compiler will
notify you if you're calling such functions without `CALL_WITH_EXCEPTIONS`. You will
likewise get an error if you try to use `throw` in a function that doesn't have
`WITH_EXCEPTIONS` (or not in a `with_exceptions` section). This ensures that the
semantics are always adhered to, and that you can look at code and clearly see which
functions are capable of throwing exceptions.

Once again, if you find this syntax too cumbersome, we happily refer you to the
alternative libraries mentioned below!

Note that this syntax works with function pointers, too:

		typedef int (*Listener) WITH_EXCEPTIONS (void *data);
		Listener my_listener = ...
		int r = (*my_listener) CALL_WITH_EXCEPTIONS (&my_data);

#### Custom Exception Types

In your header files, declare new exception types like so:

		DECLARE_EXCEPTION_TYPE(OpenGL)
		DECLARE_EXCEPTION_TYPE(TextureTooBig)

Define them in any ".c" file like so:

		DEFINE_EXCEPTION_TYPE(OpenGL, Exception, "An OpenGL error occurred.")
		DEFINE_EXCEPTION_TYPE(TextureTooBig, OpenGL, "The requested texture size is not supported by this OpenGL implementation.")

The second argument to this macro is the parent type. A `catch` will match any
descendant types. For example, `catch (Exception, e)` would match our `OpenGL`
exception type. If your type is supposed to be a root type, then specify itself
as the parent: `DEFINE_EXCEPTION_TYPE(OpenGL, OpenGL, ...)`.

#### Capturing

Usage with OpenMP parallel code requires special consideration: because the semantics
are fork/join, you will be joined back to your current thread when parallelization is
done. You often will want to postpone exception handling to the "join" phase: after all,
some threads might fail, while others succeed. For this, we introduce the powerful
`capture_exceptions` and `throw_captured` keywords:

		with_exceptions (openmp) {
			try {
				#pragma omp parallel for
				for (int i = 0; i < 10; i++) {
					capture_exceptions
						if (i % 3 == 0)
							throwf(Value, "bad number in loop %d", i);
				}
				throw_captured();
			}
			finally catch (Exception, e)
				Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
		}

Uncaught exceptions thrown in the `capture_exceptions` code block are stored in a
waiting area within the current scope. Then, when we call `throw_captured`, the first
waiting exception is thrown, as if by calling `rethrow` on it. The rest of the
exceptions are discarded. If there are no waiting exceptions, `throw_captured` will do
nothing.

We must discard the rest of the exceptions in order to adhere to the try/catch
semantics, which only allow a single exception to unwind at once. However, it can
sometimes be useful to know about _all_ the exceptions thrown in the parallel
code. You can manually traverse these exceptions before calling `throw_captured` by
calling `uncapture_exceptions`:

		with_exceptions (openmp) {
			try {
				#pragma omp parallel for
				for (int i = 0; i < 10; i++) {
					capture_exceptions
						if (i % 3 == 0)
							throwf(Value, "bad number in loop %d", i);
				}
				uncapture_exceptions();
				printf("All these were thrown:\n");
				for (int i = 0, l = exception_count(); i < l, i++) {
					Exception *e = get_exception(i);
					Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
				}
				throw_captured();
			}
			finally catch (Exception, e)
				Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
		}

#### Relaying

You can also use one context inside another, via `with_exceptions_relay`: 

		int background_thread(void *data) {
			with_exceptions (sdl) {
				try {
					with_exceptions_relay (openmp) {
						#pragma omp parallel for
						for (int i = 0; i < 10; i++) {
							capture_exceptions {
								if (i % 3 == 0)
									throwf(Value, "bad number in loop %d", i);
							}
						}
					}
				}
				finally catch (Exception, e)
					Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
			}
			return 0;
		}

At the end of the `with_exceptions_relay` code block, any exceptions uncaught there
will be automatically thrown into the containing context (`sdl`, in this example),
so you don't have to call `throw_captured`.

If you just need a code block to relay from one context to another, you can use the
`with_exceptions_relay_to` shortcut (it's also a bit more efficient):

		int background_thread(void *data) {
			with_exceptions_relay_to (openmp, sdl) {
				#pragma omp parallel for
				for (int i = 0; i < 10; i++)
					capture_exceptions
						if (i % 3 == 0)
							throwf(Value, "bad number in loop %d", i);
			}
			return 0;
		}

This is Exceptional C Exceptions' killer feature! Yeah!

#### Debugging

Programming is hard and life is short. To turn on color-coded debug messages, which
include dumps of the exception unwinding frame stack and queue:

		exceptional_debug = stderr; 

License and Cost
----------------

This library is provided for free of charge (and without warranty), with a permissive
MIT-style distribution license. All I ask in return (but do not require) is that you
credit me somewhere in your final product, at the very least with something like this:

"This product makes use of code written by Tal Liron."

You may also provide a link to this site, and of course provide more details about my
contribution to your product. 

If you are feeling especially grateful, I am graciously accepting donations: :)

[![Donate](https://www.paypalobjects.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=HUB5PM74N234W)

I am also available for hire (via [Three Crickets](http://threecrickets.com/)) to help
you integrate this library into your product. 

Portability
-----------

Eh. At the time of this writing, Exceptional C Exceptions relies on the C99 standard,
and has only been tested with gcc. So, you'll need to compile with `gcc -std=c99`.
However, it has been tested with gcc on Linux, with MinGW cross-compiling for Windows,
and with Android NDK.

Patches and forks porting this library to other C standards and compilers will be
appreciated.

Exceptional C Exceptions depends on [SimCList](http://mij.oltrelinux.com/devel/simclist/)
and the [Better String Library](http://bstring.sourceforge.net/), which are both very
portable. These libraries are redistributed with Exceptional C Exceptions for your
convenience. Better String Library is used to ensure safe string operations: the last
thing you want it your exception library introducing more bugs! We strongly recommend you
use it for your code, too.

Alternatives
------------

[exceptions4c](https://code.google.com/p/exceptions4c/)
is a richer and more portable alternative to Exceptional C Exceptions.
It supports catching signals (SIGHUP, SIGSEGV, etc.) as exceptions and various, powerful
customizations. Though it can work in a threaded environment, it only supports the POSIX
API (pthreads). On Windows, you can get the the POSIX API using
[pthreads-win32](https://www.sourceware.org/pthreads-win32/).
A lightweight version exists with fewer features.
It is distributed under the LGPL.

[OSSP ex](http://www.ossp.org/pkg/lib/ex/) is a simple ISO C library that only supports
single threads. It is distributed under an MIT-type license.

[libexception](https://github.com/hollow/libexception/) is a simple library that only
supports single threads. It is distributed under a BSD-type license.
