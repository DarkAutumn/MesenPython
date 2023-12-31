#pragma once
#include "pch.h"

#ifdef _WIN32
/* struct timeval */
#include <winsock2.h>
#endif

#ifdef _DEBUG
#undef _DEBUG
#define DEBUG_WAS_DEFINED
#endif

#include "Python.h"

#ifdef DEBUG_WAS_DEFINED
#define _DEBUG
#undef DEBUG_WAS_DEFINED
#endif

class PythonScriptingContext;

PythonScriptingContext* GetScriptingContextFromThreadState(PyThreadState *state);
PyThreadState *InitializePython(PythonScriptingContext *ctx, const string &directory);
void ReportEndScriptingContext(PythonScriptingContext *ctx);