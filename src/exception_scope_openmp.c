#ifdef _OPENMP

#include "exceptional.h"
#include <stdlib.h>

static ExceptionContext exception_context_openmp;
#pragma omp threadprivate(exception_context_openmp)

void ExceptionScope_initialize_openmp() {
	#pragma omp parallel
	{
		if (exceptional_debug)
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
		exception_context_openmp = (ExceptionContext) {0};
		ExceptionContext_create(&exception_context_openmp);
	}
}

void ExceptionScope_shutdown_openmp() {
	#pragma omp parallel
	{
		if (exceptional_debug)
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
		ExceptionContext_destroy(&exception_context_openmp);
	}
}

static ExceptionContext *ExceptionScope_openmp_get(ExceptionScope_openmp *scope) {
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
