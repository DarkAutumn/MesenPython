def log(msg):
	"""Logs a message to the Mesen log file."""
	print(msg)

def read8(address) -> int:
	"""Reads an 8-bit value from the specified memory address."""
	raise NotImplementedError()

def registerFrameMemory(type, addresses) -> int:
	"""Registers memory addresses (and sizes) to be tracked and updated every frame.
		type: memory type (e.g. memoryType.nesMemory)
		addresses: list of addresses to track
		returns: handle to the registered memory (used to unregister the memory later), which is also the pointer to the first address in the registered memory block"""
	raise NotImplementedError()

def unregisterFrameMemory(handle):
	"""Unregisters frame memory updates.
		handle: handle returned by registerFrameMemory"""
	raise NotImplementedError()

def registerScreenMemory() -> int:
	"""Registers screen memory addresses (and sizes) to be tracked and updated every frame.
		returns: handle to the registered memory (used to unregister the memory later), which is also the pointer to the first address in the registered memory block"""
	raise NotImplementedError()

def unregisterScreenMemory(handle):
	"""Unregisters screen memory updates.
		handle: handle returned by registerScreenMemory"""
	raise NotImplementedError()

def addEventCallback(callback, eventType):
	"""Adds an event callback.  e.g. emu.addEventCallback(function, eventType.startFrame)
		callback: function to call when the event occurs
		eventType: type of event to listen for (e.g. eventType.startFrame)"""
	raise NotImplementedError()

def removeEventCallback(callback, eventType):
	"""Removes an event callback.
		callback: function to remove
		eventType: type of event to remove (e.g. eventType.startFrame)"""
	raise NotImplementedError()

def loadSaveState(filename):
	"""Loads a save state.
		filename: path to the save state file"""
	raise NotImplementedError()

def setInput(port, subport, input : []):
	"""Sets input for a controller.
		port: controller port (0-3)
		subport: controller subport (0-3)
		input: input data (a list of booleans, one for each button)"""
	raise NotImplementedError()

def getInput(port, subport) -> []:
	"""Gets input for a controller.
		port: controller port (0-3)
		subport: controller subport (0-3)
		returns: input data (a list of booleans, one for each button)"""
	raise NotImplementedError()

# overwrite the above definitions
from mesen_ import *

class LogStream(object):
	def __init__(self):
		import sys
		sys.stdout = self
		sys.stderr = self
		
	def write(self, msg):
		log(msg)
	def flush(self):
		pass

LogStream()

class eventType:
	def __init__(self):
		self.reset = 0
		self.nmi = 1
		self.irq = 2
		self.startFrame = 3
		self.endFrame = 4
		self.codeBreak = 5
		self.stateLoaded = 6
		self.stateSaved = 7
		self.inputPolled = 8
		self.spriteZeroHit = 9
		self.scriptEnded = 10

eventType = eventType()

class memoryType:
	def __init__(self):
		self.nesMemory = 7
		self.ppuMemory = 8
		self.nesPrgRom = 36
		self.nesInternalRam = 37
		self.nesWorkRam = 38
		self.nesSaveRam = 39
		self.nesNametableRam = 40
		self.nesSpriteRam = 41
		self.nesSecondarySpriteRam = 42
		self.nesPaletteRam = 43
		self.nesChrRam = 44
		self.nesChrRom = 45

memoryType = memoryType()

class eventType:
	def __init__(self):
		self.nmi = 0
		self.irq = 1
		self.startFrame = 2
		self.endFrame = 3
		self.reset = 4
		self.scriptEnded = 5
		self.inputPolled = 6
		self.stateLoaded = 7
		self.stateSaved = 8
		self.codeBreak = 9
		
eventType = eventType()