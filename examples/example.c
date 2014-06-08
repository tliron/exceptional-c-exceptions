#include "exceptional.h"
#include <stdio.h>
#include <pthread.h>

static void test WITH_EXCEPTIONS (const char *text) {
	throwf(Exception, "our text is \"%s\"", text);
}

static void *mythread1(void *data) {
	with_exceptions (posix) {
		try
			throwf(Exception, "oops 7 in thread %lu", pthread_self());
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
	}
	return NULL;
}

static void *mythread2(void *data) {
	with_exceptions (posix) {
		try {
			with_exceptions_relay (openmp) {
				#pragma omp parallel for
				for (int i = 0; i < 10; i++) {
					capture_exceptions {
						if (i % 2 == 0)
							throwf(Value, "oops 9 in loop %d, thread %lu", i, pthread_self());
						printf("loop %d was OK in thread %lu\n", i, pthread_self());
					}
				}
			}
		}
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	if (argc > 1)
		exceptional_debug = stderr;

	initialize_exceptions(posix);
	initialize_exceptions(openmp);

	printf("\n");
	printf(ANSI_COLOR_BRIGHT_GREEN "Exceptional C Exceptions\n" ANSI_COLOR_RESET);
	printf(ANSI_COLOR_BRIGHT_GREEN "========================\n" ANSI_COLOR_RESET);

	with_exceptions (local) {
		printf("\n");
		printf(ANSI_COLOR_BRIGHT_GREEN "Using a local exception context...\n" ANSI_COLOR_RESET);
		printf("\n");

		printf(ANSI_COLOR_BRIGHT_GREEN "A simple throw/catch:\n" ANSI_COLOR_RESET);
		try
			throw(Exception, "oops 1");
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);

		printf(ANSI_COLOR_BRIGHT_GREEN "Calling a decorated function:\n" ANSI_COLOR_RESET);
		try
			test CALL_WITH_EXCEPTIONS ("oops 2");
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);

		printf(ANSI_COLOR_BRIGHT_GREEN "Nesting:\n" ANSI_COLOR_RESET);
		try {
			try
				test CALL_WITH_EXCEPTIONS ("oops 4");
			finally catch (Exception, e)
				rethrow(e, Exception, "oops 3");
		}
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);

		printf(ANSI_COLOR_BRIGHT_GREEN "Proper unwinding:\n" ANSI_COLOR_RESET);
		try {
			try
				test CALL_WITH_EXCEPTIONS ("oops 6");
			finally {
				catch (Exception, e)
					rethrow(e, Exception, "oops 5");
				printf("This line is executed even when an exception is thrown\n");
			}
			printf("This line won't be executed because an exception was thrown!\n");
		}
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
	}

	printf("\n");
	printf(ANSI_COLOR_BRIGHT_GREEN "Using POSIX exception contexts...\n" ANSI_COLOR_RESET);
	printf("\n");

	printf(ANSI_COLOR_BRIGHT_GREEN "Three threads:\n" ANSI_COLOR_RESET);
	pthread_t thread1, thread2, thread3;
	pthread_create(&thread1, NULL, mythread1, NULL);
	pthread_create(&thread2, NULL, mythread1, NULL);
	pthread_create(&thread3, NULL, mythread1, NULL);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);

	with_exceptions (openmp) {
		printf("\n");
		printf(ANSI_COLOR_BRIGHT_GREEN "Using an OpenMP exception context...\n" ANSI_COLOR_RESET);
		printf("\n");

		printf(ANSI_COLOR_BRIGHT_GREEN "Capturing exceptions:\n" ANSI_COLOR_RESET);
		try {
			#pragma omp parallel for
			for (int i = 0; i < 10; i++) {
				capture_exceptions {
					if (i % 2 == 0)
						throwf(Value, "oops 8 in loop %d", i);
					printf("loop %d was OK\n", i);
				}
			}
			uncapture_exceptions();
			printf("All exceptions:\n");
			for (int i = 0, l = exception_count(); i < l; i++) {
				printf("  ");
				Exception_dump(get_exception(i), stdout, EXCEPTION_DUMP_NESTED);
			}
			throw_captured();
		}
		finally catch (Exception, e)
			Exception_dump(e, stdout, EXCEPTION_DUMP_NESTED);
	}

	printf("\n");
	printf(ANSI_COLOR_BRIGHT_GREEN "Using POSIX and OpenMP together...\n" ANSI_COLOR_RESET);
	printf("\n");

	printf(ANSI_COLOR_BRIGHT_GREEN "Three POSIX threads, each relaying from OpenMP:\n" ANSI_COLOR_RESET);
	pthread_create(&thread1, NULL, mythread2, NULL);
	pthread_create(&thread2, NULL, mythread2, NULL);
	pthread_create(&thread3, NULL, mythread2, NULL);
	pthread_join(thread1, NULL);
	pthread_join(thread2, NULL);
	pthread_join(thread3, NULL);

	shutdown_exceptions(global);
	shutdown_exceptions(posix);
	shutdown_exceptions(openmp);

	printf("\n");
	return 0;
}
