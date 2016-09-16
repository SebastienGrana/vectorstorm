/*
 *  VS_Backtrace.cpp
 *  VectorStorm
 *
 *  Created by Trevor Powell on 15/09/2016
 *  Copyright 2016 Trevor Powell.  All rights reserved.
 *
 */

#include "VS_Backtrace.h"

#ifdef BACKTRACE_SUPPORTED
#ifdef _WIN32

#include "exchndl.h"

void vsBacktrace()
{
}

void vsInstallBacktraceHandler()
{
	ExcHndlInit();
}

#else

#include <execinfo.h>
#include <signal.h>
#include <unistd.h>

void handler(int sig) {
	switch(sig)
	{
		case SIGABRT:
			fputs("Caught SIGABRT: usually caused by abort() or assert()\n", stderr);
			break;
		case SIGFPE:
			fputs("Caught SIGFPE: arithmetic exception, such as divide by zero.\n", stderr);
			break;
		case SIGILL:
			fputs("Caugh SIGILL: illegal instruction\n", stderr);
			break;
		case SIGSEGV:
			fputs("Caught SIGSEGV: segfault\n", stderr);
			break;
		case SIGTERM:
			fputs("Caught SIGTERM: a termination request was sent to the program\n", stderr);
			break;
	}
	// print out all the frames to stderr

	vsBacktrace();
	exit(1);
}



void vsInstallBacktraceHandler()
{
	signal(SIGABRT, handler);
	signal(SIGFPE, handler);
	signal(SIGILL, handler);
	signal(SIGINT, handler);
	signal(SIGSEGV, handler);
	signal(SIGTERM, handler);
}

void vsBacktrace()
{
	void *array[10];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, 10);

	backtrace_symbols_fd(array, size, STDERR_FILENO);
}

#endif
#else

void vsInstallBacktraceHandler()
{
}

void vsBacktrace()
{
}

#endif
