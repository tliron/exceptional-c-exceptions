#include "exceptional.h"

static ExceptionContext exception_context_global = {0};

void ExceptionScope_shutdown_global() {
	ExceptionContext_destroy(&exception_context_global);
}

static ExceptionContext *ExceptionScope_global_get(ExceptionScope_global *scope) {
	return &exception_context_global;
}

void ExceptionScope_global_create(ExceptionScope_global *self) {
	ExceptionScope_create(&self->super);
	if (!exception_context_global.valid)
		ExceptionContext_create(&exception_context_global);
	self->super.get = (ExceptionScope_get_fn) ExceptionScope_global_get;
}

ExceptionScope_global ExceptionScope_global_new() {
	ExceptionScope_global scope = {0};
	ExceptionScope_global_create(&scope);
	return scope;
}
