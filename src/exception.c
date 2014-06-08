#include "exceptional.h"
#include <stdlib.h>
#include <stdarg.h>

Exception *Exception_new(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, char *message, bool own_message) {
	Exception *exception = malloc(sizeof(Exception));
	exception->type = type;
	exception->cause = cause;
	exception->location.file = file;
	exception->location.line = line;
	exception->location.fn = fn;
	exception->message = message;
	exception->own_message = own_message;
	return exception;
}

Exception *Exception_newc(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, const char *message) {
	return Exception_new(type, cause, file, line, fn, (char *) message, false);
}

Exception *Exception_newd(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, char *message) {
	return Exception_new(type, cause, file, line, fn, exceptional_strdup(message), true);
}

Exception *Exception_newf(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, const char *format, ...) {
	va_list args;
	va_start(args, format);
	return Exception_new(type, cause, file, line, fn, exceptional_sprintf(EXCEPTION_MAX_MESSAGE_SIZE, format, args), true);
}

void Exception_destroy(Exception *self) {
	if (self->own_message && self->message) {
		free(self->message);
		self->message = NULL;
	}
	while (true) {
		self = self->cause;
		if (self)
			Exception_destroy_and_free(self);
		else
			break;
	}
}

void Exception_destroy_and_free(Exception *self) {
	Exception_destroy(self);
	free(self);
}

static void Exception_dump_causes(Exception *self, FILE *file) {
	if (!self)
		return;
	fprintf(file, "  caused by ");
	Exception_dump(self, file, EXCEPTION_DUMP_LONG);
	Exception_dump_causes(self->cause, file);
}

void Exception_dump(Exception *self, FILE *file, ExceptionDumpDetail detail) {
	switch (detail) {
	case EXCEPTION_DUMP_SHORT:
		fprintf(file, "%s: %s\n", self->type->name, self->message);
		break;
	case EXCEPTION_DUMP_LONG:
		fprintf(file, "%s: %s at %s:%d %s()\n", self->type->name, self->message, self->location.file, self->location.line, self->location.fn);
		break;
	case EXCEPTION_DUMP_NESTED:
		fprintf(file, "%s: %s at %s:%d %s()\n", self->type->name, self->message, self->location.file, self->location.line, self->location.fn);
		Exception_dump_causes(self->cause, file);
		break;
	}
}
