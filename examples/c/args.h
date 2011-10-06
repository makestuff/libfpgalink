#ifndef ARGS_H
#define ARGS_H

#define FAIL(failCode) \
	returnCode = failCode; \
	goto cleanup

#define GET_ARG(argName, var, failCode)					\
	argv++; \
	argc--; \
	if ( !argc ) { requires(prog, argName); FAIL(failCode); }	\
	var = *argv

void suggest(const char *prog);
void requires(const char *prog, const char *arg);
void missing(const char *prog, const char *arg);
void invalid(const char *prog, char arg);
void unexpected(const char *prog, const char *arg);
void usage(const char *prog);

#endif
