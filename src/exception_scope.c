#include "exceptional.h"
#include <stddef.h>

void ExceptionScope_create(ExceptionScope *self) {
	#ifdef _OPENMP
	omp_init_lock(&self->lock);
	#endif
	list_init(&self->captured_exceptions);
}

void ExceptionScope_destroy(ExceptionScope *self) {
	#ifdef _OPENMP
	omp_destroy_lock(&self->lock);
	#endif
	if (exceptional_list_destroy_with_elements(&self->captured_exceptions, (exceptional_list_destroy_element_fn) Exception_destroy))
		self->captured_exceptions = (list_t) {0};
}

void ExceptionScope_move_exceptions_to_other_context(ExceptionScope *self, ExceptionScope *relay) {
	ExceptionContext *context = self->get(self);
	ExceptionContext *relay_context = relay->get(relay);
	exceptional_list_move(&context->exceptions, &relay_context->exceptions);
}

void ExceptionScope_move_exceptions_from_context(ExceptionScope *self) {
	ExceptionContext *context = self->get(self);
	if (!list_empty(&context->exceptions)) {
		#ifdef _OPENMP
		omp_set_lock(&self->lock);
		#endif
		exceptional_list_for_each (&context->exceptions, Exception, exception)
			list_append(&self->captured_exceptions, exception);
		#ifdef _OPENMP
		omp_unset_lock(&self->lock);
		#endif
		list_clear(&context->exceptions);
	}
}

void ExceptionScope_move_exceptions_to_context(ExceptionScope *self) {
	// Move all captured exceptions from scope to context
	ExceptionContext *context = self->get(self);
	exceptional_list_move(&self->captured_exceptions, &context->exceptions);
}

void ExceptionScope_dump_captured_exceptions(ExceptionScope *self, FILE *file) {
	exceptional_list_for_each (&self->captured_exceptions, Exception, exception) {
		fprintf(file, ANSI_COLOR_BRIGHT_RED "  ! ");
		Exception_dump(exception, file, EXCEPTION_DUMP_LONG);
		fprintf(file, ANSI_COLOR_RESET);
	}
}

// Helpers

bool ExceptionScope_with_exceptions_relay(ExceptionScope *self, ExceptionScope *relay, jmp_buf *jmp, JumpReason reason, const char *keyword, const char *file, int line, const char *fn) {
	ExceptionContext *context = self->get(self);

	if (reason) {
		// We've jumped here due to an uncaught exception

		ExceptionContext *relay_context = relay->get(relay);
		ExceptionScope_move_exceptions_to_other_context(self, relay);

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "end", NULL);
			ExceptionContext_dump_exceptions(relay_context, exceptional_debug);
			ExceptionContext_dump_frames(relay_context, exceptional_debug);
		}

		ExceptionScope_destroy(self);
		ExceptionScope_destroy(relay);
		ExceptionContext_jump_because(relay_context, reason); // important: jumping to the *relay* stack
		return false; // not supposed to get here
	}
	else {
		// All we did was set the jump point

		ExceptionContext_push_frame(context, jmp, JUMP_REASON_DONT, false, false, "with_exceptions_relay", file, line, fn);

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "begin", NULL);
			ExceptionContext_dump_exceptions(context, exceptional_debug);
			ExceptionContext_dump_frames(context, exceptional_debug);
		}

		return true;
	}
}

void ExceptionScope_with_exceptions_relay_done(ExceptionScope *self, ExceptionScope *relay) {
	ExceptionContext *context = self->get(self);
	ExceptionContext *relay_context = relay->get(relay);

	// Uncapture locally
	ExceptionScope_move_exceptions_to_context(self);

	if (exceptional_debug) {
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
		ExceptionContext_dump_exceptions(context, exceptional_debug);
		ExceptionContext_dump_frames(context, exceptional_debug);
	}

	// Relay
	ExceptionScope_move_exceptions_to_other_context(self, relay);

	ExceptionContext_pop_frame(context);
	ExceptionScope_destroy(self);
	ExceptionScope_destroy(relay);

	if (ExceptionContext_has_exceptions(relay_context))
		// We have relay exceptions, so throw them
		ExceptionContext_jump_because(relay_context, JUMP_REASON_THROW);
}

bool ExceptionScope_capture_exceptions(ExceptionScope *self, jmp_buf *jmp, JumpReason reason, const char *file, int line, const char *fn) {
	ExceptionContext *context = self->get(self);

	if (reason) {
		// We've jumped here due to an uncaught exception

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "end", NULL);
			ExceptionContext_dump_exceptions(context, exceptional_debug);
			ExceptionContext_dump_frames(context, exceptional_debug);
		}

		ExceptionScope_move_exceptions_from_context(self);

		return false;
	}
	else {
		// All we did was set the jump point

		ExceptionContext_push_frame(context, jmp, JUMP_REASON_DONT, false, false, "capture_exceptions", file, line, fn);

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "begin", NULL);
			ExceptionContext_dump_exceptions(context, exceptional_debug);
			ExceptionContext_dump_frames(context, exceptional_debug);
		}

		return true;
	}
}

void ExceptionScope_uncapture_exceptions(ExceptionScope *self) {
	ExceptionScope_move_exceptions_to_context(self);

	if (exceptional_debug) {
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
		ExceptionContext *context = self->get(self);
		ExceptionContext_dump_exceptions(context, exceptional_debug);
		ExceptionContext_dump_frames(context, exceptional_debug);
	}
}

void ExceptionScope_throw_captured(ExceptionScope *self) {
	ExceptionScope_move_exceptions_to_context(self);

	ExceptionContext *context = self->get(self);

	if (ExceptionContext_has_exceptions(context)) {
		// We have exceptions, so throw

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "throwing", NULL);
			ExceptionContext_dump_exceptions(context, exceptional_debug);
			ExceptionContext_dump_frames(context, exceptional_debug);
		}

		ExceptionFrame *frame = ExceptionContext_get_current_frame(context);
		if (frame && frame->rethrowing)
			ExceptionContext_jump_because(context, JUMP_REASON_RETHROW);
		else
			ExceptionContext_jump_because(context, JUMP_REASON_THROW);
	}
	else if (exceptional_debug) {
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, "not throwing", NULL);
		ExceptionContext_dump_exceptions(context, exceptional_debug);
		ExceptionContext_dump_frames(context, exceptional_debug);
	}
}
