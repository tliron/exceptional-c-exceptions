#include "exceptional.h"

static ExceptionContext *ExceptionScope_local_get(ExceptionScope_local *scope) {
	return &scope->local_context;
}

void ExceptionScope_local_create(ExceptionScope_local *self) {
	ExceptionScope_create(&self->super);
	self->super.get = (ExceptionScope_get_fn) ExceptionScope_local_get;
	ExceptionContext_create(&self->local_context);
}

ExceptionScope_local ExceptionScope_local_new() {
	ExceptionScope_local scope = {0};
	ExceptionScope_local_create(&scope);
	return scope;
}
