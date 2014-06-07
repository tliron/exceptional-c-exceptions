#ifdef _OPENMP

#include "exceptional.h"
#include <stdlib.h>

static ExceptionContext exception_context_openmp = {0};
#pragma omp threadprivate(exception_context_openmp)

static list_t exception_contexts = {0};
static omp_lock_t lock;

void ExceptionScope_initialize_openmp() {
	list_init(&exception_contexts);
	omp_init_lock(&lock);
}

#include <stdio.h>

void ExceptionScope_shutdown_openmp() {
	if (exceptional_list_initialized(&exception_contexts)) {
		//exceptional_list_for_each (&exception_contexts, ExceptionContext, context)
			//ExceptionContext_destroy(context); // TODO segfaults sometimes :(
		list_destroy(&exception_contexts);
		exception_contexts = (list_t) {0};
	}
	omp_destroy_lock(&lock);
}

static ExceptionContext *ExceptionScope_openmp_get(ExceptionScope_openmp *scope) {
	if (!exception_context_openmp.valid) {
		// This context has not been used yet, so create it
		ExceptionContext_create(&exception_context_openmp);
		omp_set_lock(&lock);
		// Keep track of created contexts so that we can properly destroy them later
		list_append(&exception_contexts, &exception_context_openmp);
		omp_unset_lock(&lock);
	}

	return &exception_context_openmp;
}

void ExceptionScope_openmp_create(ExceptionScope_openmp *self) {
	ExceptionScope_create(&self->super);
	self->super.get = (ExceptionScope_get_fn) ExceptionScope_openmp_get;
}

ExceptionScope_openmp ExceptionScope_openmp_new() {
	ExceptionScope_openmp scope = {0};
	ExceptionScope_openmp_create(&scope);
	return scope;
}

#endif
