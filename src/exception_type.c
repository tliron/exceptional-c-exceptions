#include "exceptional.h"

DEFINE_EXCEPTION_TYPE(Exception, Exception, "An exception was detected");

DEFINE_EXCEPTION_TYPE(Value, Exception, "An unsupported value was encountered");
DEFINE_EXCEPTION_TYPE(Type, Value, "An encountered value was of the wrong type");

DEFINE_EXCEPTION_TYPE(Authorization, Exception, "Could not allow access");
DEFINE_EXCEPTION_TYPE(Credentials, Authorization, "Credentials could not be authorized");
DEFINE_EXCEPTION_TYPE(Password, Credentials, "A password was wrong");

DEFINE_EXCEPTION_TYPE(Thread, Exception, "A thread-related exception was detected");
DEFINE_EXCEPTION_TYPE(NotEnoughTreads, Thread, "Threads were required but not enough were available");
DEFINE_EXCEPTION_TYPE(Synchronization, Thread, "Multi-threaded access was not properly synchronized");
DEFINE_EXCEPTION_TYPE(LockNotAcquired, Synchronization, "A required lock was not acquired");
DEFINE_EXCEPTION_TYPE(DeadLock, Synchronization, "A thread dead-lock situation was detected");

DEFINE_EXCEPTION_TYPE(Memory, Exception, "A memory-related exception was detected");
DEFINE_EXCEPTION_TYPE(NotEnoughMemory, Memory, "More memory was required than was available");

DEFINE_EXCEPTION_TYPE(IO, Exception, "An input or output exception was detected");
DEFINE_EXCEPTION_TYPE(File, IO, "A filesystem exception was detected");
DEFINE_EXCEPTION_TYPE(FileNotFound, File, "A file could not be accessed");
DEFINE_EXCEPTION_TYPE(FileReadOnly, File, "A file could be written to");

DEFINE_EXCEPTION_TYPE(Signal, Exception, "A signal has been raised");
DEFINE_EXCEPTION_TYPE(AbnormalTermination, Signal, "A SIGABRT has been raised");
DEFINE_EXCEPTION_TYPE(FloatingPointException, Signal, "A SIGFPE has been raised");
DEFINE_EXCEPTION_TYPE(InvalidInstruction, Signal, "A SIGILL has been raised");
DEFINE_EXCEPTION_TYPE(InteractiveAttentionRequest, Signal, "A SIGINT has been raised");
DEFINE_EXCEPTION_TYPE(InvalidMemoryAccess, Signal, "A SIGSEGV has been raised");
DEFINE_EXCEPTION_TYPE(TerminationRequest, Signal, "A SIGTERM has been raised");

bool ExceptionType_is_a(const ExceptionType *self, const ExceptionType *type) {
	while (true) {
		if (self == type)
			return true;
		else if (!self->super || (self == self->super))
			break;
		else
			self = self->super;
	}
	return false;
}
