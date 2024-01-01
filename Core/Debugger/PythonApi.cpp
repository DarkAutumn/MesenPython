#include "pch.h"
#include "PythonScriptingContext.h"
#include "PythonApi.h"
#include "Utilities/FolderUtilities.h"
#include "Debugger/Debugger.h"
#include "Shared/Emulator.h"
#include "Shared/SaveStateManager.h"
#include "Shared/BaseControlManager.h"
#include "Shared/BaseControlDevice.h"

static PyObject* PythonEmuLog(PyObject* self, PyObject* args);
static PyObject* PythonRead8(PyObject* self, PyObject* args);
static PyObject* PythonRegisterFrameMemory(PyObject* self, PyObject* args);
static PyObject* PythonUnregisterFrameMemory(PyObject* self, PyObject* args);
static PyObject* PythonRegisterScreenMemory(PyObject* self, PyObject* args);
static PyObject* PythonUnregisterScreenMemory(PyObject* self, PyObject* args);
static PyObject* PythonAddEventCallback(PyObject* self, PyObject* args);
static PyObject* PythonRemoveEventCallback(PyObject* self, PyObject* args);
static PyObject* PythonLoadSaveState(PyObject* self, PyObject* args);
static PyObject* PythonSetInput(PyObject* self, PyObject* args);
static PyObject* PythonGetInput(PyObject* self, PyObject* args);

static PyMethodDef MyMethods[] = {
	{"log", PythonEmuLog, METH_VARARGS, "Logging function"},
	{"read8", PythonRead8, METH_VARARGS, "Read 8-bit value from memory"},
	{"registerFrameMemory", PythonRegisterFrameMemory, METH_VARARGS, "Register memory addresses (and sizes) to be tracked and updated every frame."},
	{"unregisterFrameMemory", PythonUnregisterFrameMemory, METH_VARARGS, "Unregister frame memory updates."},
	{"registerScreenMemory", PythonRegisterScreenMemory, METH_VARARGS, "Register screen memory addresses (and sizes) to be tracked and updated every frame."},
	{"unregisterScreenMemory", PythonUnregisterScreenMemory, METH_VARARGS, "Unregister screen memory updates."},
	{"addEventCallback", PythonAddEventCallback, METH_VARARGS, "Adds an event callback.  e.g. emu.addEventCallback(function, eventType.startFrame)"},
	{"removeEventCallback", PythonRemoveEventCallback, METH_VARARGS, "Removes an event callback."},
	{"loadSaveState", PythonLoadSaveState, METH_VARARGS, "Loads a save state."},
	{"setInput", PythonSetInput, METH_VARARGS, "Sets input for a controller."},
	{"getInput", PythonGetInput, METH_VARARGS, "Gets input for a controller."},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef emumodule = {
	 PyModuleDef_HEAD_INIT,
	 "emu",
	 NULL,
	 -1,
	 MyMethods
};


PyMODINIT_FUNC PyInit_mesen(void)
{
	return PyModule_Create(&emumodule);
}

static PyObject* PythonEmuLog(PyObject* self, PyObject* args)
{
	PythonScriptingContext *context = GetScriptingContextFromThreadState();
	if(!context)
	{
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	// Extract the only argument as an object, convert to a string
	PyObject* pyMessage;
	if(!PyArg_ParseTuple(args, "O", &pyMessage))
	{
		PyErr_SetString(PyExc_TypeError, "Failed to parse arguments");
		return nullptr;
	}

	PyObject* pyStr = PyObject_Str(pyMessage);
	if(!pyStr)
	{
		PyErr_SetString(PyExc_TypeError, "Failed to convert argument to string");
		return nullptr;
	}

	string message = PyUnicode_AsUTF8(pyStr);
	context->Log(message);

	Py_RETURN_NONE;
}

static PyObject* PythonLoadSaveState(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context)
	{
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	PyObject* pyStr;
	if(!PyArg_ParseTuple(args, "O", &pyStr)) {
		PyErr_SetString(PyExc_TypeError, "Failed to parse arguments");
		return nullptr;
	}

	string path = PyUnicode_AsUTF8(pyStr);
	context->GetDebugger()->GetEmulator()->GetSaveStateManager()->LoadState(path);

	Py_RETURN_NONE;
}

static PyObject* PythonRead8(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	int address, kind;
	PyObject* py_signed;

	// Unpack the Python arguments. Expected: two integers and a boolean
	if(!PyArg_ParseTuple(args, "iiO", &address, &kind, &py_signed))
		return nullptr;

	// Check if py_signed is a boolean
	if(!PyBool_Check(py_signed)) {
		PyErr_SetString(PyExc_TypeError, "Third argument must be a boolean");
		return nullptr;
	}

	// Convert the PyObject to a C++ boolean
	bool sgn = PyObject_IsTrue(py_signed);

	uint8_t result = 0;
	bool success = context->ReadMemory(address, static_cast<MemoryType>(kind), sgn, result);

	// Convert the result to a Python object based on is_signed
	PyObject* pyResult = nullptr;
	if(sgn)
		pyResult = PyLong_FromLong(static_cast<int8_t>(result));
	else
		pyResult = PyLong_FromUnsignedLong(result);

	return pyResult;
}



static PyObject* PythonUnregisterScreenMemory(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	void* ptr;
	if(!PyArg_ParseTuple(args, "p", &ptr))
		return nullptr;

	bool result = context->UnregisterScreenMemory(ptr);
	if(result)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

static PyObject* PythonRegisterScreenMemory(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	void* ptr = context->RegisterScreenMemory();
	if(ptr == nullptr)
		Py_RETURN_NONE;

	return PyLong_FromVoidPtr(ptr);
}

static PyObject* PythonRegisterFrameMemory(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	std::vector<int> result;

	// Extract args as a (MemoryType enum, list of addresses)
	int memType = 0;
	PyObject* pyAddresses = nullptr;
	if(!PyArg_ParseTuple(args, "iO", &memType, &pyAddresses))
		return nullptr;

	// extract the list of addresses
	PyObject* pyIter = PyObject_GetIter(pyAddresses);
	if(!pyIter) {
		PyErr_SetString(PyExc_TypeError, "Second argument must be iterable");
		return nullptr;
	}

	PyObject* pyItem;
	while((pyItem = PyIter_Next(pyIter))) {
		if(!PyLong_Check(pyItem)) {
			PyErr_SetString(PyExc_TypeError, "Second argument must be a list of integers");
			return nullptr;
		}

		int addr = PyLong_AsLong(pyItem);
		result.push_back(addr);
	}

	void* ptr = context->RegisterFrameMemory(static_cast<MemoryType>(memType), result);
	if(ptr == nullptr)
		Py_RETURN_NONE;

	return PyLong_FromVoidPtr(ptr);
}

static PyObject* PythonGetInput(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	// extract args as (port, list_of_buttons)
	int port = 0, subport = 0;
	if(!PyArg_ParseTuple(args, "ii", &port, &subport))
		return nullptr;

	PyObject *result = PyList_New(8);

	// get the controller
	shared_ptr<BaseControlDevice> controller = context->GetDebugger()->GetEmulator()->GetConsoleUnsafe()->GetControlManager()->GetControlDevice(port, subport);
	if(!controller) {
		PyErr_SetString(PyExc_TypeError, "Invalid port");
		return nullptr;
	}
	vector<DeviceButtonName> buttons = controller->GetKeyNameAssociations();
	for(DeviceButtonName& btn : buttons) {
		if(!btn.IsNumeric) {
			bool btnState = controller->IsPressed(btn.ButtonId);
			PyObject* pyItem = PyBool_FromLong(btnState);
			PyList_SetItem(result, btn.ButtonId, pyItem);
		}
	}

	return result;
}

static PyObject* PythonSetInput(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	// extract args as (port, list_of_buttons)
	PyObject* pySequence = nullptr;
	int port = 0, subport = 0;
	if(!PyArg_ParseTuple(args, "iiO", &port, &subport, &pySequence))
		return nullptr;

	// extract the list of buttons
	PyObject* pyIter = PyObject_GetIter(pySequence);
	if(!pyIter) {
		PyErr_SetString(PyExc_TypeError, "Second argument must be iterable");
		return nullptr;
	}

	// get the controller
	shared_ptr<BaseControlDevice> controller = context->GetDebugger()->GetEmulator()->GetConsoleUnsafe()->GetControlManager()->GetControlDevice(port, subport);
	if(!controller) {
		PyErr_SetString(PyExc_TypeError, "Invalid port");
		return nullptr;
	}

	// get the list of buttons
	vector<DeviceButtonName> buttons = controller->GetKeyNameAssociations();
	for(DeviceButtonName& btn : buttons) {
		if(!btn.IsNumeric) {
			// get the button state
			PyObject* pyItem = PyIter_Next(pyIter);
			if(!pyItem) {
				PyErr_SetString(PyExc_TypeError, "Second argument must be a list of booleans");
				return nullptr;
			}

			// check if is none
			if(pyItem == Py_None) {
				continue;
			}

			if(!PyBool_Check(pyItem)) {
				PyErr_SetString(PyExc_TypeError, "Second argument must be a list of booleans");
				return nullptr;
			}

			bool btnState = PyObject_IsTrue(pyItem);
			controller->SetBitValue(btn.ButtonId, btnState);
		}
	}

	Py_RETURN_NONE;
}

static PyObject* PythonUnregisterFrameMemory(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	void* ptr;
	if(!PyArg_ParseTuple(args, "p", &ptr))
		return nullptr;

	bool result = context->UnregisterFrameMemory(ptr);
	if(result)
		Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

static PyObject* PythonAddEventCallback(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}

	// Extract args as a (PyObject function, EventType enum)
	PyObject* pyFunc;
	int eventType;
	if(!PyArg_ParseTuple(args, "Oi", &pyFunc, &eventType))
		return nullptr;

	// Check if pyFunc is a function
	if(!PyCallable_Check(pyFunc)) {
		PyErr_SetString(PyExc_TypeError, "First argument must be callable");
		return nullptr;
	}

	// Check if eventType is a valid EventType
	if(eventType < 0 || eventType >(int)EventType::LastValue) {
		PyErr_SetString(PyExc_TypeError, "Second argument must be a valid EventType");
		return nullptr;
	}

	Py_INCREF(pyFunc);

	// Add the callback to the list of callbacks
	context->RegisterEventCallback(static_cast<EventType>(eventType), pyFunc);

	Py_RETURN_NONE;
}

static PyObject* PythonRemoveEventCallback(PyObject* self, PyObject* args)
{
	PythonScriptingContext* context = GetScriptingContextFromThreadState();
	if(!context) {
		PyErr_SetString(PyExc_TypeError, "No registered python context.");
		return nullptr;
	}


	// Extract args as a (PyObject function, EventType enum)
	PyObject* pyFunc;
	int eventType;
	if(!PyArg_ParseTuple(args, "Oi", &pyFunc, &eventType))
		return nullptr;

	// Check if pyFunc is a function
	if(!PyCallable_Check(pyFunc)) {
		PyErr_SetString(PyExc_TypeError, "First argument must be callable");
		return nullptr;
	}

	Py_DECREF(pyFunc);

	// Check if eventType is a valid EventType
	if(eventType < 0 || eventType >(int)EventType::LastValue) {
		PyErr_SetString(PyExc_TypeError, "Second argument must be a valid EventType");
		return nullptr;
	}

	// Remove the callback from the list of callbacks
	context->UnregisterEventCallback(static_cast<EventType>(eventType), pyFunc);

	Py_RETURN_NONE;
}


static string ReadFileContents(const string& path)
{
	FILE* f = nullptr;
	auto err = fopen_s(&f, path.c_str(), "r");

	if(err != 0 || f == nullptr) {
		std::cerr << "Failed to open file." << std::endl;
		return ""; // Or handle the error as needed
	}

	std::stringstream buffer;
	char ch;

	while((ch = fgetc(f)) != EOF) {
		buffer << ch;
	}

	fclose(f);
	return buffer.str();
}

std::unordered_map<PyThreadState*, PythonScriptingContext*> s_pythonContexts;



PyThreadState *InitializePython(PythonScriptingContext *context, const string & path)
{
	if(!Py_IsInitialized())
	{
		PyImport_AppendInittab("mesen_", PyInit_mesen);
		Py_Initialize();
	}

#ifdef USE_SUBINTERPRETERS
	PyThreadState* curr = Py_NewInterpreter();
	PyThreadState* old = PyThreadState_Swap(curr);
#else
	PyThreadState* curr = nullptr;
#endif
	s_pythonContexts[curr] = context;

	// add script directory to sys.path
	PyObject* sysPath = PySys_GetObject("path"); // Borrowed reference
	if(sysPath) {
		PyObject* pPath = PyUnicode_FromString(path.c_str());
		if(pPath) {
			PyList_Insert(sysPath, 0, pPath);
			Py_DECREF(pPath);
		}
	}

#ifdef USE_SUBINTERPRETERS
	PyThreadState_Swap(old);
#endif

	return curr;
}

void ReportEndScriptingContext(PythonScriptingContext* ctx)
{
	for(auto it = s_pythonContexts.begin(); it != s_pythonContexts.end(); ++it)
	{
		if(it->second == ctx)
		{
			s_pythonContexts.erase(it);
			break;
		}
	}
}

PythonScriptingContext* GetScriptingContextFromThreadState()
{
#if USE_SUBINTERPRETERS
	PyThreadState* state = PyThreadState_Get();
#else
	PyThreadState* state = nullptr;
#endif

	auto it = s_pythonContexts.find(state);
	if(it != s_pythonContexts.end())
		return it->second;

	return nullptr;
}