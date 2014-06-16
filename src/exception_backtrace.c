#ifdef EXCEPTIONAL_BACKTRACE

#define _POSIX_C_SOURCE 2 // for popen
#define _XOPEN_SOURCE 500 // for readlink

#include "exceptional.h"
#include <execinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

static bool addr2line(void *line, FILE *file);
static bool get_exe_name(__pid_t pid, char *exe_name, size_t exe_name_size);

void ExceptionBacktrace_create(ExceptionBacktrace *self) {
	self->size = backtrace(self->frames, EXCEPTION_MAX_BACKTRACE_SIZE);
	self->skip = 1;
}

void ExceptionBacktrace_dump(ExceptionBacktrace *self, FILE *file) {
	char **strings = backtrace_symbols(self->frames, self->size);
	if (strings) {
		fprintf(file, "Backtrace:\n");
#ifdef __OPTIMIZE__
    	fprintf(file, "  (Due to compiler optimizations, locations may not exactly match the source code)\n");
#endif
		for (int i = self->skip, size = self->size; i < size; i++) {
			fprintf(file, "  %s\n", strings[i]);
			addr2line(self->frames[i], file);
		}
		free(strings);
	}
}

#define MAX_EXE_NAME_SIZE 1024
#define MAX_COMMAND_SIZE 1024
#define MAX_LINE_SIZE 1024

static bool addr2line(void *line, FILE *file) {
    // See: http://stackoverflow.com/a/4611112/849021
	char exe_name[MAX_EXE_NAME_SIZE];
	if (!get_exe_name(getpid(), exe_name, sizeof(exe_name)))
		return false;

	char command[MAX_COMMAND_SIZE];
	char *escaped_exe_name = exceptional_escape_spaces(exe_name);
	sprintf(command, "/usr/bin/addr2line %p -p -f -i -e %s", line, escaped_exe_name);
	free(escaped_exe_name);

	FILE *f = popen(command, "r");
	if (!f)
		return false;

	char data[MAX_LINE_SIZE];
	while (fgets(data, sizeof(data), f))
		fprintf(file, "  > %s", data);
	return pclose(f) != -1;
}

#define MAX_LINK_SIZE 1024

static bool get_exe_name(__pid_t pid, char *exe_name, size_t exe_name_size) {
	char link[MAX_LINK_SIZE];
	if (snprintf(link, sizeof(link), "/proc/%d/exe", pid) < 0)
		return false;
	size_t r = readlink(link, exe_name, exe_name_size - 1);
	if (r == -1)
		return false;
	exe_name[r] = 0; // readlink doesn't add terminating null
	return true;
}

#endif
