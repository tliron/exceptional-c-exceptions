#ifndef EXCEPTIONAL_H_
#define EXCEPTIONAL_H_

// See README.md for a detailed guide

#include "simclist.h"
#include "bstrlib.h"
#include <setjmp.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/*
 * Set to a stream (for example: stdout, stderr) in order to dump debug information.
 */
extern FILE *exceptional_debug;

//
// Keywords
//

/*
 * Declares a code block with local variable scope.
 *
 * Can contain "try/finally" code blocks.
 *
 * Uncaught exceptions will not be accessible after the block finishes execution.
 *
 * Possible contexts: "local", "global", "posix", "sdl", "openmp"
 */
#define with_exceptions(CONTEXT) \
	/* Create scope. */ \
	ExceptionScope_##CONTEXT EXCEPTIONAL_LOCAL(scope) = ExceptionScope_##CONTEXT##_new(); \
	/* Execute the code block with "current_execution_scope", and then destroy it. */ \
	for (ExceptionScope *current_exception_scope = (ExceptionScope *) &EXCEPTIONAL_LOCAL(scope); !current_exception_scope->done; current_exception_scope->done = true, \
		ExceptionScope_destroy(current_exception_scope))

/*
 * Declares a code block with local variable scope.
 *
 * After execution relays all uncaught exceptions to the containing context.
 *
 * Can only be used inside a "with_exceptions" code block or a function decorated with "WITH_EXCEPTIONS".
 *
 * Possible contexts: "local", "global", "posix", "sdl", "openmp"
 */
#define with_exceptions_relay(CONTEXT) \
	/* Create scopes. */ \
	ExceptionScope_##CONTEXT EXCEPTIONAL_LOCAL(scope) = ExceptionScope_##CONTEXT##_new(); \
	ExceptionScope *EXCEPTIONAL_LOCAL(relay_scope) = current_exception_scope; \
	/* Create a jump point. */ \
	jmp_buf EXCEPTIONAL_LOCAL(jmp); \
	JumpReason EXCEPTIONAL_LOCAL(jump_reason) = setjmp(EXCEPTIONAL_LOCAL(jmp)); \
	/* Execute the code block, relay uncaught exceptions, and then jump to last jump point in the relay context. */ \
	ExceptionContext *EXCEPTIONAL_LOCAL(exception_context) = ((ExceptionScope *) &EXCEPTIONAL_LOCAL(scope))->get(&EXCEPTIONAL_LOCAL(scope)); \
	if (ExceptionScope_with_exceptions_relay((ExceptionScope *) &EXCEPTIONAL_LOCAL(scope), EXCEPTIONAL_LOCAL(relay_scope), &EXCEPTIONAL_LOCAL(jmp), EXCEPTIONAL_LOCAL(jump_reason), "with_exceptions_relay", __FILE__, __LINE__, __FUNCTION__)) \
		for (ExceptionScope *current_exception_scope = (ExceptionScope *) &EXCEPTIONAL_LOCAL(scope); !current_exception_scope->done; current_exception_scope->done = true, \
			ExceptionScope_with_exceptions_relay_done(current_exception_scope, EXCEPTIONAL_LOCAL(relay_scope)))

/*
 * Like "with_exceptions", but after execution relays all uncaught exceptions to a specified context.
 *
 * This is the same as, but more efficient than, using "with_exceptions (RELAYCONTEXT)" with a
 * "with_exceptions_relay (CONTEXT)" inside.
 *
 * Possible contexts:       "local", "global", "posix", "sdl", "openmp"
 * Possible relay contexts: "global", "posix", "sdl", "openmp"
 */
#define with_exceptions_relay_to(CONTEXT, RELAYCONTEXT) \
	/* Create scopes. */ \
	ExceptionScope_##CONTEXT EXCEPTIONAL_LOCAL(scope) = ExceptionScope_##CONTEXT##_new(); \
	ExceptionScope_##RELAYCONTEXT EXCEPTIONAL_LOCAL(relay_scope) = ExceptionScope_##RELAYCONTEXT##_new(); \
	/* Create a jump point. */ \
	jmp_buf EXCEPTIONAL_LOCAL(jmp); \
	JumpReason EXCEPTIONAL_LOCAL(jump_reason) = setjmp(EXCEPTIONAL_LOCAL(jmp)); \
	/* Execute the code block, relay uncaught exceptions, and then jump to last jump point in the relay context. */ \
	ExceptionContext *EXCEPTIONAL_LOCAL(exception_context) = ((ExceptionScope *) &EXCEPTIONAL_LOCAL(scope))->get(&EXCEPTIONAL_LOCAL(scope)); \
	if (ExceptionScope_with_exceptions_relay((ExceptionScope *) &EXCEPTIONAL_LOCAL(scope), (ExceptionScope *) &EXCEPTIONAL_LOCAL(relay_scope), &EXCEPTIONAL_LOCAL(jmp), EXCEPTIONAL_LOCAL(jump_reason), "with_exceptions_relay_to", __FILE__, __LINE__, __FUNCTION__)) \
		for (ExceptionScope *current_exception_scope = (ExceptionScope *) &EXCEPTIONAL_LOCAL(scope); !current_exception_scope->done; current_exception_scope->done = true, \
			ExceptionScope_with_exceptions_relay_done(current_exception_scope, (ExceptionScope *) &EXCEPTIONAL_LOCAL(relay_scope)))

/*
 * Declares a code block with local variable scope.
 *
 * Exceptions can be handled here: either via a local "throw", called functions, or relayed from another context.
 * The code block will exit prematurely if an exception is thrown.
 * A matching "finally" code block must follow it, and will *always* be executed after it.
 *
 * Can only be used inside a "with_exceptions" code block or a function decorated with "WITH_EXCEPTIONS".
 */
#define try \
	/* Create a jump point. */ \
	jmp_buf EXCEPTIONAL_LOCAL(jmp); \
	JumpReason EXCEPTIONAL_LOCAL(jump_reason) = setjmp(EXCEPTIONAL_LOCAL(jmp)); \
	/* Execute the code block. */ \
	/* If an exception is thrown, we will switch to unwinding mode. */ \
	/* If the exception is caught by a "catch", unwinding mode will be disabled. */ \
	ExceptionContext *EXCEPTIONAL_LOCAL(exception_context) = get_current_exception_context(); \
	for (ExceptionContext_try(EXCEPTIONAL_LOCAL(exception_context), &EXCEPTIONAL_LOCAL(jmp), EXCEPTIONAL_LOCAL(jump_reason), __FILE__, __LINE__, __FUNCTION__); ExceptionContext_is_trying(EXCEPTIONAL_LOCAL(exception_context)); ExceptionContext_stop_trying(EXCEPTIONAL_LOCAL(exception_context)))

/*
 * Declares a code block with local variable scope.
 *
 * It is always executed immediately after a "try" code block, and must follow it.
 *
 * Can contain one or more "catch" code blocks.
 */
#define finally \
	/* Execute the code block, and then jump to last jump point in the context, maintaining the mode. */ \
	for (bool done_ = false; !done_; done_ = true, \
		ExceptionContext_finally_done(get_current_exception_context()))

/*
 * Executes a code block with local variable scope only if an exception of a certain type or
 * its sub-types was thrown in the preceding "try" code block.
 *
 * The variable (an Exception pointer) is defined in the code block's scope, and the value will
 * be destroyed when the block ends.
 *
 * Can only be used inside a "finally" code block.
 */
#define catch(TYPE, VAR) \
	/* If an exception is caught, execute the code block and then free the exception. */ \
	for (Exception *VAR = ExceptionContext_catch(get_current_exception_context(), &ExceptionType##TYPE); VAR; \
		ExceptionContext_catch_done(get_current_exception_context(), VAR), \
		VAR = NULL)

/*
 * Executes a code block with local variable scope.
 *
 * All uncaught exceptions are "captured" locally, and can later be thrown by via "throw_captured".
 *
 * Can only be used inside a "with_exceptions" code block or a function decorated with "WITH_EXCEPTIONS".
 */
#define capture_exceptions \
	/* Create a jump point. */ \
	jmp_buf EXCEPTIONAL_LOCAL(jmp); \
	JumpReason EXCEPTIONAL_LOCAL(jump_reason) = setjmp(EXCEPTIONAL_LOCAL(jmp)); \
	/*  Executes the code block and then moves all uncaught exceptions to the scope. */ \
	if (ExceptionScope_capture_exceptions(current_exception_scope, &EXCEPTIONAL_LOCAL(jmp), EXCEPTIONAL_LOCAL(jump_reason), __FILE__, __LINE__, __FUNCTION__)) \
		for (bool done_ = false; !done_; done_ = true, \
			ExceptionContext_pop_frame(get_current_exception_context()))

//
// API
//

/*
 * Exits the current code block ("try", "with_exceptions_relay" or "capture_exceptions").
 *
 * The exception might be caught by a "catch" in the innermost "try". If not caught,
 * execution will continue to unwind until it is, or else it will be left in uncaught
 * status. You can iterate uncaught exceptions using "exception_count" and "get_exception".
 *
 * Note: "message" should be a literal string or otherwise have its memory managed by you.
 * It must be accessible at least until the "with_exceptions" code block finishes execution.
 * If you can't guarantee that, use "throwd" instead.
 */
#define throw(TYPE, MESSAGE) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newc(&ExceptionType##TYPE, NULL, __FILE__, __LINE__, __FUNCTION__, MESSAGE))

/*
 * Like "throw", except that "message" will be duplicated, and eventually freed (either
 * at the end of the "throw" code block or at the end of the "with_exceptions" code block).
 */
#define throwd(TYPE, MESSAGE) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newd(&ExceptionType##TYPE, NULL, __FILE__, __LINE__, __FUNCTION__, MESSAGE))

/*
 * Like "throwd", with printf-style formatting.
 *
 * The resulting message is limited in size to EXCEPTION_MAX_MESSAGE_SIZE.
 */
#define throwf(TYPE, FORMAT, ...) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newf(&ExceptionType##TYPE, NULL, __FILE__, __LINE__, __FUNCTION__, FORMAT, __VA_ARGS__))

/*
 * Like "throw", with a cause (can be null).
 */
#define rethrow(CAUSE, TYPE, MESSAGE) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newc(&ExceptionType##TYPE, CAUSE, __FILE__, __LINE__, __FUNCTION__, MESSAGE))

/*
 * Like "throwd", with a cause (can be null).
 */
#define rethrowd(CAUSE, TYPE, MESSAGE) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newd(&ExceptionType##TYPE, CAUSE, __FILE__, __LINE__, __FUNCTION__, MESSAGE))

/*
 * Like "throwf", with a cause (can be null).
 */
#define rethrowf(CAUSE, TYPE, FORMAT, ...) \
	/* Add an exception and then jump to the previous jump point on the stack. */ \
	ExceptionContext_throw(get_current_exception_context(), \
		Exception_newf(&ExceptionType##TYPE, CAUSE, __FILE__, __LINE__, __FUNCTION__, FORMAT, __VA_ARGS__))

/*
 * Release an exception.
 *
 * Can only be used inside a "catch" code block.
 */
#define release_exception(EXCEPTION) \
	Exception_destroy_and_free(EXCEPTION)

/*
 * Moves all captured exceptions back to the context, so that they can be accessed
 * via get_exception().
 */
#define uncapture_exceptions() \
		ExceptionScope_uncapture_exceptions(current_exception_scope)

/*
 * Throws exceptions captured in a previous "capture_exceptions" code block.
 *
 * Can only be used inside a "with_exceptions" code block.
 */
#define throw_captured() \
	ExceptionScope_throw_captured(current_exception_scope)

/*
 * Should be called only once.
 *
 * Possible contexts: "posix", "sdl", "openmp"
 */
#define initialize_exceptions(CONTEXT) \
	ExceptionScope_initialize_##CONTEXT()

/*
 * Should be called only once.
 *
 * Possible contexts: "global", "posix", "openmp"
 */
#define shutdown_exceptions(CONTEXT) \
	ExceptionScope_shutdown_##CONTEXT()

/*
 * The number of uncaught exceptions.
 *
 * Can only be used inside a "with_exceptions" code block.
 */
#define exception_count() \
	ExceptionContext_count_exceptions(get_current_exception_context())

/*
 * Access an uncaught exception.
 *
 * Can only be used inside a "with_exceptions" code block.
 */
#define get_exception(INDEX) \
	ExceptionContext_get_exception(get_current_exception_context(), INDEX)

/*
 * The current ExceptionContext.
 *
 * Can only be used inside a "with_exceptions" code block.
 */
#define get_current_exception_context() \
	(current_exception_scope->get(current_exception_scope))

//
// Function decorators
//

/*
 * Equivalent to a "with_exceptions" code block for a whole function, for
 * which the context type is received from the caller.
 *
 * Insert before the argument list, e.g.:
 *
 * void my_function WITH_EXCEPTIONS (int count, int cut) { ... }
 *
 * If there are no arguments, you must use the special "WITH_EXCEPTIONS_VOID"
 * decorator.
 */
#define WITH_EXCEPTIONS(...) \
	(ExceptionScope *current_exception_scope, __VA_ARGS__)

#define WITH_EXCEPTIONS_VOID() \
	(ExceptionScope *current_exception_scope)

/*
 * Used to call a "WITH_EXCEPTIONS" function.
 *
 * Insert before the argument list, e.g.:
 *
 * my_function CALL_WITH_EXCEPTIONS (1, 2);
 *
 * If there are no arguments, you must use the special "CALL_WITH_EXCEPTIONS_VOID"
 * decorator.
 */
#define CALL_WITH_EXCEPTIONS(...) \
	(current_exception_scope, __VA_ARGS__)

#define CALL_WITH_EXCEPTIONS_VOID() \
	(current_exception_scope)

//
// Macros
//

#define DECLARE_EXCEPTION_TYPE(TYPE) \
	extern const ExceptionType ExceptionType##TYPE

#define DEFINE_EXCEPTION_TYPE(TYPE, SUPERTYPE, DESCRIPTION) \
	const ExceptionType ExceptionType##TYPE = { \
		.name = #TYPE, \
		.description = DESCRIPTION, \
		.super = &ExceptionType##SUPERTYPE \
	}

//
// ExceptionType
//

typedef struct struct_ExceptionType {
	const char *name, *description;
	const struct struct_ExceptionType *super;
} ExceptionType;

bool ExceptionType_is_a(const ExceptionType *self, const ExceptionType *type);

DECLARE_EXCEPTION_TYPE(Exception);

DECLARE_EXCEPTION_TYPE(Value); // Exception
DECLARE_EXCEPTION_TYPE(Type); // Value

DECLARE_EXCEPTION_TYPE(Authorization); // Exception
DECLARE_EXCEPTION_TYPE(Credentials); // Authorization
DECLARE_EXCEPTION_TYPE(Password); // Credentials

DECLARE_EXCEPTION_TYPE(Thread); // Exception
DECLARE_EXCEPTION_TYPE(NotEnoughThreads); // Thread
DECLARE_EXCEPTION_TYPE(Synchronization); // Thread
DECLARE_EXCEPTION_TYPE(LockNotAcquired); // Synchronization
DECLARE_EXCEPTION_TYPE(DeadLocked); // Synchronization

DECLARE_EXCEPTION_TYPE(Memory); // Exception
DECLARE_EXCEPTION_TYPE(NotEnoughMemory); // Memory
DECLARE_EXCEPTION_TYPE(PoolEmpty); // Memory
DECLARE_EXCEPTION_TYPE(PoolFull); // Memory

DECLARE_EXCEPTION_TYPE(IO); // Exception
DECLARE_EXCEPTION_TYPE(File); // IO
DECLARE_EXCEPTION_TYPE(FileNotFound); // File
DECLARE_EXCEPTION_TYPE(FileReadOnly); // File

DECLARE_EXCEPTION_TYPE(Signal); // Exception
DECLARE_EXCEPTION_TYPE(AbnormalTermination); // Signal
DECLARE_EXCEPTION_TYPE(FloatingPointException); // Signal
DECLARE_EXCEPTION_TYPE(InvalidInstruction); // Signal
DECLARE_EXCEPTION_TYPE(InteractiveAttentionRequest); // Signal
DECLARE_EXCEPTION_TYPE(InvalidMemoryAccess); // Signal
DECLARE_EXCEPTION_TYPE(TerminationRequest); // Signal

//
// ExceptionProgramLocation
//

typedef struct struct_ExceptionProgramLocation {
	const char *file, *fn;
	int line;
} ExceptionProgramLocation;

//
// Exception
//

#ifndef EXCEPTION_MAX_MESSAGE_SIZE
#define EXCEPTION_MAX_MESSAGE_SIZE 2048
#endif

#define EXCEPTION_DUMP_SHORT  ((ExceptionDumpDetail) 0)
#define EXCEPTION_DUMP_LONG   ((ExceptionDumpDetail) 1)
#define EXCEPTION_DUMP_NESTED ((ExceptionDumpDetail) 2)

typedef unsigned char ExceptionDumpDetail;

typedef struct struct_Exception {
	const ExceptionType *type;
	char *message;
	bool own_message;
	ExceptionProgramLocation location;
	struct struct_Exception *cause;
} Exception;

Exception *Exception_new(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, char *message, bool own_message);
Exception *Exception_newc(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, const char *message);
Exception *Exception_newd(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, char *message);
Exception *Exception_newf(const ExceptionType *type, Exception *cause, const char *file, int line, const char *fn, const char *format, ...);
void Exception_destroy(Exception *self);
void Exception_destroy_and_free(Exception *self);
void Exception_dump(Exception *self, FILE *file, ExceptionDumpDetail detail);

//
// ExceptionFrame
//

#define JUMP_REASON_DONT    ((JumpReason) 0)
#define JUMP_REASON_THROW   ((JumpReason) 1)
#define JUMP_REASON_RETHROW ((JumpReason) 2)

typedef int JumpReason;

typedef struct struct_ExceptionFrame {
	jmp_buf jmp;
	const char *keyword;
	ExceptionProgramLocation location;
	bool trying, rethrowing;
	JumpReason finally_jump_reason;
} ExceptionFrame;

void ExceptionFrame_dump(ExceptionFrame *self, FILE *file);

//
// ExceptionContext
//

typedef struct struct_ExceptionContext {
	bool valid;
	list_t frames, exceptions;
} ExceptionContext;

void ExceptionContext_create(ExceptionContext *self);
void ExceptionContext_destroy(ExceptionContext *self);
void ExceptionContext_destroy_and_free(ExceptionContext *self);

// Frames
void ExceptionContext_push_frame(ExceptionContext *self, jmp_buf *jmp, JumpReason finally_jump_reason, bool trying, bool rethrowing, const char *keyword, const char *file, int line, const char *fn);
void ExceptionContext_pop_frame(ExceptionContext *self);
ExceptionFrame *ExceptionContext_get_current_frame(ExceptionContext *self);
void ExceptionContext_jump(ExceptionContext *self);
void ExceptionContext_jump_because(ExceptionContext *self, JumpReason reason);
bool ExceptionContext_is_trying(ExceptionContext *self);
void ExceptionContext_stop_trying(ExceptionContext *self);
void ExceptionContext_dump_frames(ExceptionContext *self, FILE *file);

// Exceptions
void ExceptionContext_add_exception(ExceptionContext *self, Exception *exception);
Exception *ExceptionContext_fetch_exception(ExceptionContext *self, const ExceptionType *type);
bool ExceptionContext_has_exceptions(ExceptionContext *self);
void ExceptionContext_clear_exceptions(ExceptionContext *self, bool except_first);
void ExceptionContext_dump_exceptions(ExceptionContext *self, FILE *file);

// Helpers
void ExceptionContext_try(ExceptionContext *self, jmp_buf *jmp, JumpReason reason, const char *file, int line, const char *fn);
void ExceptionContext_throw(ExceptionContext *self, Exception *exception);
Exception *ExceptionContext_catch(ExceptionContext *self, const ExceptionType *type);
void ExceptionContext_catch_done(ExceptionContext *self, Exception *exception);
void ExceptionContext_finally_done(ExceptionContext *self);
int ExceptionContext_count_exceptions(ExceptionContext *self);
Exception *ExceptionContext_get_exception(ExceptionContext *self, int index);

//
// ExceptionScope
//

typedef ExceptionContext *(*ExceptionScope_get_fn)(void *reference);

typedef struct struct_ExceptionScope {
	ExceptionScope_get_fn get;
	list_t captured_exceptions;
	bool done;
	#ifdef _OPENMP
	omp_lock_t lock;
	#endif
} ExceptionScope;

void ExceptionScope_create(ExceptionScope *self);
void ExceptionScope_destroy(ExceptionScope *self);
void ExceptionScope_move_exceptions_to_other_context(ExceptionScope *self, ExceptionScope *relay);
void ExceptionScope_move_exceptions_from_context(ExceptionScope *self);
void ExceptionScope_move_exceptions_to_context(ExceptionScope *self);
void ExceptionScope_dump_captured_exceptions(ExceptionScope *self, FILE *file);

// Helpers
bool ExceptionScope_with_exceptions_relay(ExceptionScope *self, ExceptionScope *relay, jmp_buf *jmp, JumpReason reason, const char *keyword, const char *file, int line, const char *fn);
void ExceptionScope_with_exceptions_relay_done(ExceptionScope *self, ExceptionScope *relay);
bool ExceptionScope_capture_exceptions(ExceptionScope *self, jmp_buf *jmp, JumpReason reason, const char *file, int line, const char *fn);
void ExceptionScope_uncapture_exceptions(ExceptionScope *self);
void ExceptionScope_throw_captured(ExceptionScope *self);

typedef struct struct_ExceptionScope_generic {
	ExceptionScope super;
} ExceptionScope_generic;

typedef struct struct_ExceptionScope_local {
	ExceptionScope super;
	ExceptionContext local_context;
} ExceptionScope_local;

typedef struct struct_ExceptionScope_global {
	ExceptionScope super;
} ExceptionScope_global;

typedef struct struct_ExceptionScope_posix {
	ExceptionScope super;
	ExceptionContext *context;
} ExceptionScope_posix;

typedef struct struct_ExceptionScope_sdl {
	ExceptionScope super;
	ExceptionContext *context;
} ExceptionScope_sdl;

typedef struct struct_ExceptionScope_openmp {
	ExceptionScope super;
} ExceptionScope_openmp;

// Local
void ExceptionScope_local_create(ExceptionScope_local *self);
ExceptionScope_local ExceptionScope_local_new();

// Global
void ExceptionScope_shutdown_global();
void ExceptionScope_global_create(ExceptionScope_global *self);
ExceptionScope_global ExceptionScope_global_new();

// POSIX
void ExceptionScope_initialize_posix();
void ExceptionScope_shutdown_posix();
void ExceptionScope_posix_create(ExceptionScope_posix *self);
ExceptionScope_posix ExceptionScope_posix_new();

// SDL
void ExceptionScope_initialize_sdl();
void ExceptionScope_sdl_create(ExceptionScope_sdl *self);
ExceptionScope_sdl ExceptionScope_sdl_new();

// OpenMP
#ifdef _OPENMP
void ExceptionScope_initialize_openmp();
void ExceptionScope_shutdown_openmp();
void ExceptionScope_openmp_create(ExceptionScope_openmp *self);
ExceptionScope_openmp ExceptionScope_openmp_new();
#endif

//
// Utilities
//

#define EXCEPTIONAL_LOCAL(PREFIX)          EXCEPTIONAL_LOCAL1(PREFIX, __LINE__)
// We need these two layers of macros because C is weird
#define EXCEPTIONAL_LOCAL1(PREFIX, SUFFIX) EXCEPTIONAL_LOCAL2(PREFIX, SUFFIX)
#define EXCEPTIONAL_LOCAL2(PREFIX, SUFFIX) PREFIX##_##SUFFIX##_

// ANSI terminal colors

#define ANSI_COLOR_RED            "\x1b[31m"
#define ANSI_COLOR_GREEN          "\x1b[32m"
#define ANSI_COLOR_YELLOW         "\x1b[33m"
#define ANSI_COLOR_BLUE           "\x1b[34m"
#define ANSI_COLOR_MAGENTA        "\x1b[35m"
#define ANSI_COLOR_CYAN           "\x1b[36m"
#define ANSI_COLOR_BRIGHT_RED     "\x1b[01;31m"
#define ANSI_COLOR_BRIGHT_GREEN   "\x1b[01;32m"
#define ANSI_COLOR_BRIGHT_YELLOW  "\x1b[01;33m"
#define ANSI_COLOR_BRIGHT_BLUE    "\x1b[01;34m"
#define ANSI_COLOR_BRIGHT_MAGENTA "\x1b[01;35m"
#define ANSI_COLOR_BRIGHT_CYAN    "\x1b[01;36m"
#define ANSI_COLOR_RESET          "\x1b[0m"

void exceptional_dump_fn(FILE *file, const char *fn, const char *tag, const char *extra);

// Better String Library

char *exceptional_bstring_to_string(bstring string_b);
char *exceptional_strdup(const char *string);
char *exceptional_sprintf(size_t max_size, const char *format, va_list args);

// SimCList

#define exceptional_list_for_each(LIST, TYPE, VAR) \
	list_t *EXCEPTIONAL_LOCAL(list) = LIST; \
	if (!list_empty(EXCEPTIONAL_LOCAL(list)) && list_iterator_start(EXCEPTIONAL_LOCAL(list))) \
		for (bool next_ = list_iterator_hasnext(EXCEPTIONAL_LOCAL(list)); next_; next_ = false, list_iterator_stop(EXCEPTIONAL_LOCAL(list))) \
			for (TYPE *VAR = list_iterator_next(EXCEPTIONAL_LOCAL(list)); next_; next_ = list_iterator_hasnext(EXCEPTIONAL_LOCAL(list)), VAR = next_ ? list_iterator_next(EXCEPTIONAL_LOCAL(list)) : NULL)

typedef void (*exceptional_list_destroy_element_fn)(void *element);

bool exceptional_list_initialized(list_t *list);
void exceptional_list_move(list_t *source, list_t *destination);
bool exceptional_list_destroy_with_elements(list_t *list, exceptional_list_destroy_element_fn destroy_element);

#endif
