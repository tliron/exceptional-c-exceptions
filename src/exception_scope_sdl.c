#include "exceptional.h"
#include "SDL.h"

static SDL_TLSID exception_context_sdl = 0;

typedef void (*sdl_tls_destroy_fn)(void *);

void ExceptionScope_initialize_sdl() {
	if (exceptional_debug)
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
	exception_context_sdl = SDL_TLSCreate();
}

static ExceptionContext *ExceptionScope_sdl_get(ExceptionScope_sdl *scope) {
	return scope->context;
}

void ExceptionScope_sdl_create(ExceptionScope_sdl *self) {
	ExceptionScope_create(&self->super);

	// Thread-local context
	ExceptionContext *context = SDL_TLSGet(exception_context_sdl);
	if (!context) {
		context = malloc(sizeof(ExceptionContext));
		ExceptionContext_create(context);
		SDL_TLSSet(exception_context_sdl, context, (sdl_tls_destroy_fn) ExceptionContext_destroy_and_free);
	}
	else
		// The context may have been previously used
		ExceptionContext_destroy(context);

	self->super.get = (ExceptionScope_get_fn) ExceptionScope_sdl_get;
	self->context = context;
}

ExceptionScope_sdl ExceptionScope_sdl_new() {
	ExceptionScope_sdl scope = {0};
	ExceptionScope_sdl_create(&scope);
	return scope;
}
