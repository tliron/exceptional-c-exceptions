#include "exceptional.h"
#include <pthread.h>
#include <stdlib.h>

static pthread_key_t exception_context_posix = 0;

typedef void (*pthread_key_destroy_fn)(void *);

void ExceptionScope_initialize_posix() {
	pthread_key_create(&exception_context_posix, (pthread_key_destroy_fn) ExceptionContext_destroy_and_free);
}

void ExceptionScope_shutdown_posix() {
	pthread_key_delete(exception_context_posix);
}

static ExceptionContext *ExceptionScope_posix_get(ExceptionScope_posix *scope) {
	return scope->context;
}

void ExceptionScope_posix_create(ExceptionScope_posix *self) {
	ExceptionScope_create(&self->super);

	// Thread-local context
	ExceptionContext *context = pthread_getspecific(exception_context_posix);
	if (!context) {
		context = malloc(sizeof(ExceptionContext));
		ExceptionContext_create(context);
		pthread_setspecific(exception_context_posix, context);
	}
	else
		// The context may have been previously used
		ExceptionContext_destroy(context);

	self->super.get = (ExceptionScope_get_fn) ExceptionScope_posix_get;
	self->context = context;
}

ExceptionScope_posix ExceptionScope_posix_new() {
	ExceptionScope_posix scope = {0};
	ExceptionScope_posix_create(&scope);
	return scope;
}
