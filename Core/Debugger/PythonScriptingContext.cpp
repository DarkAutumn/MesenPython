#include "pch.h"
#include <algorithm>
#include <filesystem>
#include "Debugger/PythonScriptingContext.h"
#include "Debugger/DebugTypes.h"
#include "Debugger/Debugger.h"
#include "Debugger/ScriptManager.h"
#include "Shared/Emulator.h"
#include "Shared/EmuSettings.h"
#include "Shared/SaveStateManager.h"
#include "Utilities/magic_enum.hpp"
#include "Shared/EventType.h"
#include "Debugger/MemoryDumper.h"
#include "PythonApi.h"
#include "Shared/Video/BaseVideoFilter.h"

void *PythonScriptingContext::RegisterScreenMemory()
{
	_frameBufferRegisterCount++;
	return _frameBuffer;
}


bool PythonScriptingContext::UnregisterScreenMemory(void* ptr)
{
	if(ptr == _frameBuffer && _frameBufferRegisterCount > 0) {
		_frameBufferRegisterCount--;
		return true;
	}

	return false;
}

void* PythonScriptingContext::RegisterFrameMemory(MemoryType type, const std::vector<int>& addresses)
{
	if(addresses.empty())
		return nullptr;

	uint8_t* result = new uint8_t[addresses.size()];
	memset(result, 0, addresses.size() * sizeof(uint8_t));

	MemoryRegistry reg = {};
	reg.Type = type;
	reg.BaseAddress = result;
	reg.Addresses = addresses;

	_frameMemory.push_back(reg);
	FillOneFrameMemory(reg);

	return result;
}

bool PythonScriptingContext::UnregisterFrameMemory(void* ptr)
{
	size_t count = _frameMemory.size();

	_frameMemory.erase(std::remove_if(_frameMemory.begin(), _frameMemory.end(), [ptr](const MemoryRegistry& entry) { return ptr == entry.BaseAddress; }), _frameMemory.end());

	if(count == _frameMemory.size())
		return false;

	delete[] ptr;
	return true;
}

void PythonScriptingContext::FillOneFrameMemory(const MemoryRegistry& reg)
{
	auto ptr = reg.BaseAddress;
	for(auto addr : reg.Addresses) {
		uint8_t value = _memoryDumper->GetMemoryValue(reg.Type, addr, true);
		*ptr++ = value;
	}
}

void PythonScriptingContext::UpdateFrameMemory()
{
	for(auto& reg : _frameMemory)
		FillOneFrameMemory(reg);
}

void PythonScriptingContext::UpdateScreenMemory()
{
	if(_frameBufferRegisterCount > 0) {
		Emulator* _emu = _debugger->GetEmulator();
		PpuFrameInfo frame = _emu->GetPpuFrame();
		FrameInfo frameSize = { 0 };
		frameSize.Height = frame.Height;
		frameSize.Width = frame.Width;

		unique_ptr<BaseVideoFilter> filter(_emu->GetVideoFilter());
		filter->SetBaseFrameInfo(frameSize);
		frameSize = filter->SendFrame((uint16_t*)frame.FrameBuffer, _emu->GetFrameCount(), _emu->GetFrameCount() & 0x01, nullptr, false);
		uint32_t* rgbBuffer = filter->GetOutputBuffer();
		memcpy_s(_frameBuffer, sizeof(_frameBuffer), rgbBuffer, frameSize.Height * frameSize.Width * sizeof(uint32_t));
	}
}

bool PythonScriptingContext::ReadMemory(uint32_t addr, MemoryType memType, bool sgned, uint8_t& result)
{
	uint8_t value = _memoryDumper->GetMemoryValue(memType, addr, true);
	result = value;
	return true;
}

PythonScriptingContext::PythonScriptingContext(Debugger* debugger)
{
	_debugger = debugger;
	_memoryDumper = debugger->GetMemoryDumper();
	_settings = debugger->GetEmulator()->GetSettings();
	_defaultCpuType = debugger->GetEmulator()->GetCpuTypes()[0];
	_defaultMemType = DebugUtilities::GetCpuMemoryType(_defaultCpuType);
}

PythonScriptingContext::~PythonScriptingContext()
{
	ReportEndScriptingContext(this);
}

void PythonScriptingContext::LogError()
{
	PyObject* pExcType, * pExcValue, * pExcTraceback;
	PyErr_Fetch(&pExcType, &pExcValue, &pExcTraceback);
	if(pExcType != NULL) {
		PyObject* pRepr = PyObject_Repr(pExcType);
		Log(PyUnicode_AsUTF8(pRepr));
		Py_DecRef(pRepr);
		Py_DecRef(pExcType);
	}

	if(pExcValue != NULL) {
		PyObject* pRepr = PyObject_Repr(pExcValue);
		Log(PyUnicode_AsUTF8(pRepr));
		Py_DecRef(pRepr);
		Py_DecRef(pExcValue);
	}

	if(pExcTraceback != NULL) {
		PyObject* pRepr = PyObject_Repr(pExcTraceback);
		Log(PyUnicode_AsUTF8(pRepr));
		Py_DecRef(pRepr);
		Py_DecRef(pExcTraceback);
	}
}

static std::string GetDirectoryFromPath(const std::string& filepath)
{
	std::filesystem::path pathObj(filepath);
	return pathObj.parent_path().string();
}

bool PythonScriptingContext::LoadScript(string scriptName, string path, string scriptContent, Debugger*)
{
	PyThreadState* state = InitializePython(this, GetDirectoryFromPath(path));
	_python = PythonInterpreterHandler(state);
	_scriptName = scriptName;

	auto lock = _python.AcquireSafe();

	int error = PyRun_SimpleString(scriptContent.c_str());
	if(error) {
		LogError();
		return false;
	}

	return true;
}

void PythonScriptingContext::Log(const string& message)
{
	auto lock = _logLock.AcquireSafe();
	_logRows.push_back(message);
	if(_logRows.size() > 500) {
		_logRows.pop_front();
	}
}

string PythonScriptingContext::GetLog()
{
	auto lock = _logLock.AcquireSafe();
	stringstream ss;
	for(string& msg : _logRows) {
		ss << msg;
	}
	return ss.str();
}

Debugger* PythonScriptingContext::GetDebugger()
{
	return _debugger;
}

string PythonScriptingContext::GetScriptName()
{
	return _scriptName;
}

void PythonScriptingContext::CallMemoryCallback(AddressInfo relAddr, uint8_t& value, CallbackType type, CpuType cpuType)
{}

void PythonScriptingContext::CallMemoryCallback(AddressInfo relAddr, uint16_t& value, CallbackType type, CpuType cpuType)
{}

void PythonScriptingContext::CallMemoryCallback(AddressInfo relAddr, uint32_t& value, CallbackType type, CpuType cpuType)
{}

static PyObject *s_args = nullptr;

int PythonScriptingContext::CallEventCallback(EventType type, CpuType cpuType)
{

	if(type == EventType::ScriptEnded) {
		_python.Detach();
		return 0;
	}

	auto lock = _python.AcquireSafe();

	if(type == EventType::StartFrame) {
		UpdateFrameMemory();
		UpdateScreenMemory();
	}

	int count = 0;
	for(PyObject* callback : _eventCallbacks[(int)type]) {
		Py_ssize_t count = Py_REFCNT(callback);

		if(s_args == nullptr) {
			s_args = PyTuple_New(1);
			PyObject* pValue = PyLong_FromLong((int)cpuType);
			if(!pValue) {
				LogError();
				continue;
			}
			PyTuple_SET_ITEM(s_args, 0, pValue);  // pValue reference is stolen here.
		}

		PyObject* pResult = PyObject_CallObject(callback, s_args);
		if(pResult != NULL) {
			Py_DECREF(pResult);
		} else {
			LogError();
			PyErr_Print();  // Optional: print Python error to stderr.
		}

		count++;
	}

	return count;
}


bool PythonScriptingContext::CheckInitDone()
{
	return Py_IsInitialized();
}

bool PythonScriptingContext::IsSaveStateAllowed()
{
	return !_python.IsExecuting();
}


void PythonScriptingContext::RefreshMemoryCallbackFlags()
{}

void PythonScriptingContext::RegisterMemoryCallback(CallbackType type, int startAddr, int endAddr, MemoryType memType, CpuType cpuType, PyObject* obj)
{

}
void PythonScriptingContext::UnregisterMemoryCallback(CallbackType type, int startAddr, int endAddr, MemoryType memType, CpuType cpuType, PyObject* obj)
{

}
void PythonScriptingContext::RegisterEventCallback(EventType type, PyObject* obj)
{
	_eventCallbacks[(int)type].push_back(obj);
}

void PythonScriptingContext::UnregisterEventCallback(EventType type, PyObject* obj)
{
	_eventCallbacks[(int)type].erase(std::remove(_eventCallbacks[(int)type].begin(), _eventCallbacks[(int)type].end(), obj), _eventCallbacks[(int)type].end());
}