#include "exceptional.h"
#include <stdlib.h>
#include <string.h>

FILE *exceptional_debug = NULL;

void ExceptionFrame_dump(ExceptionFrame *self, FILE *file) {
	fprintf(file, "%s at %s:%d %s()\n", self->keyword, self->location.file, self->location.line, self->location.fn);
}

void ExceptionContext_create(ExceptionContext *self) {
	list_init(&self->frames);
	list_init(&self->exceptions);
	self->valid = true;
}

void ExceptionContext_destroy(ExceptionContext *self) {
	self->valid = false;
	if (exceptional_list_destroy_with_elements(&self->frames, NULL))
		self->frames = (list_t) {0};
	if (exceptional_list_destroy_with_elements(&self->exceptions, (exceptional_list_destroy_element_fn) Exception_destroy))
		self->exceptions = (list_t) {0};
}

void ExceptionContext_destroy_and_free(ExceptionContext *context) {
	ExceptionContext_destroy(context);
	free(context);
}

// Frames

void ExceptionContext_push_frame(ExceptionContext *self, jmp_buf *jmp, JumpReason finally_jump_reason, bool trying, bool rethrowing, const char *keyword, const char *file, int line, const char *fn) {
	ExceptionFrame *frame = malloc(sizeof(ExceptionFrame));
	memcpy(frame->jmp, jmp, sizeof(jmp_buf));
	frame->finally_jump_reason = finally_jump_reason;
	frame->trying = trying;
	frame->rethrowing = rethrowing;
	frame->keyword = keyword;
	frame->location.file = file;
	frame->location.line = line;
	frame->location.fn = fn;
	list_prepend(&self->frames, frame);
}

void ExceptionContext_pop_frame(ExceptionContext *self) {
	ExceptionFrame *frame = list_fetch(&self->frames);
	if (frame) {
		JumpReason finally_jump_reason = frame->finally_jump_reason;
		free(frame);
		// Maintain the reason on the last frame
		frame = list_get_at(&self->frames, 0);
		if (frame)
			frame->finally_jump_reason = finally_jump_reason;
	}
}

ExceptionFrame *ExceptionContext_get_current_frame(ExceptionContext *self) {
	return list_get_at(&self->frames, 0);
}

void ExceptionContext_jump(ExceptionContext *self) {
	ExceptionFrame *frame = list_fetch(&self->frames);

	if (frame) {
		JumpReason finally_jump_reason = frame->finally_jump_reason;
		if (finally_jump_reason) {
			jmp_buf jmp;
			memcpy(jmp, frame->jmp, sizeof(jmp_buf));
			free(frame);
			longjmp(jmp, finally_jump_reason);
		}
		else
			free(frame);
	}
}

void ExceptionContext_jump_because(ExceptionContext *self, JumpReason reason) {
	ExceptionFrame *frame = list_fetch(&self->frames);

	if (frame) {
		if (reason) {
			jmp_buf jmp;
			memcpy(jmp, frame->jmp, sizeof(jmp_buf));
			free(frame);
			longjmp(jmp, reason);
		}
		else
			free(frame);
	}
}

bool ExceptionContext_is_trying(ExceptionContext *self) {
	ExceptionFrame *frame = list_get_at(&self->frames, 0);
	if (frame)
		return frame->trying;
	return false;
}

void ExceptionContext_stop_trying(ExceptionContext *self) {
	ExceptionFrame *frame = list_get_at(&self->frames, 0);
	if (frame)
		frame->trying = false;
}

void ExceptionContext_dump_frames(ExceptionContext *self, FILE *file) {
	exceptional_list_for_each (&self->frames, ExceptionFrame, frame) {
		fprintf(file, ANSI_COLOR_BRIGHT_BLUE "  > ");
		ExceptionFrame_dump(frame, file);
		fprintf(file, ANSI_COLOR_RESET);
	}
}

// Exceptions

void ExceptionContext_add_exception(ExceptionContext *self, Exception *exception) {
	list_append(&self->exceptions, exception);
}

Exception *ExceptionContext_fetch_exception(ExceptionContext *self, const ExceptionType *type) {
	Exception *found_exception = NULL;

	// Find a matching exception
	exceptional_list_for_each (&self->exceptions, Exception, exception)
		if (ExceptionType_is_a(exception->type, type)) {
			found_exception = exception;
			break;
		}

	if (found_exception)
		list_delete(&self->exceptions, found_exception);

	return found_exception;
}

bool ExceptionContext_has_exceptions(ExceptionContext *self) {
	return !list_empty(&self->exceptions);
}

void ExceptionContext_clear_exceptions(ExceptionContext *self, bool except_first) {
	if (list_empty(&self->exceptions))
		return;

	Exception *first = NULL;
	if (except_first) {
		if (list_size(&self->exceptions) == 1)
			return;
		first = list_fetch(&self->exceptions);
	}

	exceptional_list_for_each (&self->exceptions, Exception, exception)
		Exception_destroy_and_free(exception);
	list_clear(&self->exceptions);

	if (first)
		list_append(&self->exceptions, first);
}

void ExceptionContext_dump_exceptions(ExceptionContext *self, FILE *file) {
	exceptional_list_for_each (&self->exceptions, Exception, exception) {
		fprintf(file, ANSI_COLOR_BRIGHT_RED "  ! ");
		Exception_dump(exception, file, EXCEPTION_DUMP_LONG);
		fprintf(file, ANSI_COLOR_RESET);
	}
}

// Helpers

void ExceptionContext_try(ExceptionContext *self, jmp_buf *jmp, JumpReason reason, const char *file, int line, const char *fn) {
	if (reason) {
		// We've jumped here!

		if (reason == JUMP_REASON_THROW)
			// Re-insert the "try" jump point: an exception might be thrown again in "finally"
			ExceptionContext_push_frame(self, jmp, JUMP_REASON_THROW, false, false, "try/throw", file, line, fn);
		else if (reason == JUMP_REASON_RETHROW)
			// Re-insert the "try" jump point: an exception might be thrown again in "finally"
			ExceptionContext_push_frame(self, jmp, JUMP_REASON_THROW, false, true, "try/rethrow", file, line, fn);

		// Make sure we have no more than one exception
		ExceptionContext_clear_exceptions(self, true);

		if (exceptional_debug) {
			if (reason == JUMP_REASON_THROW)
				exceptional_dump_fn(exceptional_debug, __FUNCTION__, "thrown", NULL);
			else if (reason == JUMP_REASON_RETHROW)
				exceptional_dump_fn(exceptional_debug, __FUNCTION__, "rethrown", NULL);
			else
				exceptional_dump_fn(exceptional_debug, __FUNCTION__, "end", NULL);
			ExceptionContext_dump_exceptions(self, exceptional_debug);
			ExceptionContext_dump_frames(self, exceptional_debug);
		}
	}
	else {
		// All we did was set the jump point

		ExceptionContext_push_frame(self, jmp, JUMP_REASON_DONT, true, false, "try", file, line, fn);

		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "begin", NULL);
			ExceptionContext_dump_exceptions(self, exceptional_debug);
			ExceptionContext_dump_frames(self, exceptional_debug);
		}
	}
}

void ExceptionContext_throw(ExceptionContext *self, Exception *exception) {
	ExceptionContext_add_exception(self, exception);

	if (exceptional_debug) {
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, exception->type->name);
		ExceptionContext_dump_exceptions(self, exceptional_debug);
		ExceptionContext_dump_frames(self, exceptional_debug);
	}

	ExceptionFrame *frame = ExceptionContext_get_current_frame(self);
	if (frame && frame->rethrowing)
		ExceptionContext_jump_because(self, JUMP_REASON_RETHROW);
	else
		ExceptionContext_jump_because(self, JUMP_REASON_THROW);
}

Exception *ExceptionContext_catch(ExceptionContext *self, const ExceptionType *type) {
	ExceptionFrame *frame = ExceptionContext_get_current_frame(self);
	if (frame && frame->rethrowing)
		// Don't catch when an exception is rethrowm
		return NULL;

	Exception *exception = ExceptionContext_fetch_exception(self, type);
	if (exception) {
		// The exception was caught
		if (frame) {
			frame->finally_jump_reason = JUMP_REASON_DONT;
			frame->rethrowing = true;
		}
	}

	if (exceptional_debug) {
		if (exception)
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "hit", type->name);
		else
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "miss", type->name);
		ExceptionContext_dump_exceptions(self, exceptional_debug);
		ExceptionContext_dump_frames(self, exceptional_debug);
	}

	return exception;
}

void ExceptionContext_catch_done(ExceptionContext *self, Exception *exception) {
	if (exceptional_debug) {
		if (exception)
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "hit", exception->type->name);
		else
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "miss", NULL);
		ExceptionContext_dump_exceptions(self, exceptional_debug);
		ExceptionContext_dump_frames(self, exceptional_debug);
	}

	if (exception)
		Exception_destroy_and_free(exception);
}

void ExceptionContext_finally_done(ExceptionContext *self) {
	ExceptionFrame *frame = ExceptionContext_get_current_frame(self);
	if (frame && (frame->finally_jump_reason != JUMP_REASON_DONT)) {
		// Unwinding, so we need to pop the current "try"
		if (exceptional_debug) {
			exceptional_dump_fn(exceptional_debug, __FUNCTION__, "unwind", NULL);
			ExceptionContext_dump_exceptions(self, exceptional_debug);
			ExceptionContext_dump_frames(self, exceptional_debug);
		}
		ExceptionContext_pop_frame(self);
	}
	else if (exceptional_debug) {
		exceptional_dump_fn(exceptional_debug, __FUNCTION__, NULL, NULL);
		ExceptionContext_dump_exceptions(self, exceptional_debug);
		ExceptionContext_dump_frames(self, exceptional_debug);
	}

	// Might unwind (depending on current reason)
	ExceptionContext_jump(self);
}

int ExceptionContext_count_exceptions(ExceptionContext *self) {
	return list_size(&self->exceptions);
}

Exception *ExceptionContext_get_exception(ExceptionContext *self, int index) {
	return list_get_at(&self->exceptions, index);
}
