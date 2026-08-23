#pragma once


void assertFuncProduction(
	const char *expression,
	const char *file_name,
	unsigned const line_number,
	const char *comment = "---");

void assertFuncInternal(
	const char *expression,
	const char *file_name,
	unsigned const line_number,
	const char *comment = "---");

// PRODUCTION_BUILD is defined by CMakeLists.txt as 0 or 1 for every target;
// this used to check an undefined DEVELOPLEMT_BUILD macro (typo of
// DEVELOPMENT_BUILD), which always evaluated to 0, so every configuration —
// including development builds — silently took the production assert path
// below and permaAssertDevelopement/permaAssertCommentDevelopement always
// compiled away to nothing.
#if PRODUCTION_BUILD == 0

#define permaAssert(expression) (void)(											\
					(!!(expression)) ||												\
					(assertFuncInternal(#expression, __FILE__, (unsigned)(__LINE__)), 0)	\
				)

#define permaAssertComment(expression, comment) (void)(								\
					(!!(expression)) ||														\
					(assertFuncInternal(#expression, __FILE__, (unsigned)(__LINE__), comment), 1)\
				)

#else

#define permaAssert(expression) (void)(											\
					(!!(expression)) ||												\
					(assertFuncProduction(#expression, __FILE__, (unsigned)(__LINE__)), 0)	\
				)

#define permaAssertComment(expression, comment) (void)(								\
					(!!(expression)) ||														\
					(assertFuncProduction(#expression, __FILE__, (unsigned)(__LINE__), comment), 1)	\
				)

#endif

#if PRODUCTION_BUILD == 0
#define permaAssertDevelopement permaAssert
#define permaAssertCommentDevelopement permaAssertComment

#else
#define permaAssertDevelopement
#define permaAssertCommentDevelopement
#endif