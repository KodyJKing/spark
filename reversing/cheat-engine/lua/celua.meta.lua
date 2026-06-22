-- Definitions for Celua, the Cheat Engine Lua API.
-- EmmyLua annotations for lua-language-server (lua-ls)

-- ============================================================
-- GLOBAL VARIABLES
-- ============================================================

---@type string Path of the trainer that launched cheat engine (only set when launched as a trainer)
TrainerOrigin = ""

---@type string The main module name of the currently opened process
process = ""

---@type Form The main CE GUI
MainForm = nil

---@type AddressList The address list of the main CE GUI
AddressList = nil

-- ============================================================
-- DEBUG REGISTER GLOBALS (populated inside debugger_onBreakpoint)
-- ============================================================

---@type integer
EFLAGS = 0
---@type integer
EAX = 0
---@type integer
EBX = 0
---@type integer
ECX = 0
---@type integer
EDX = 0
---@type integer
EDI = 0
---@type integer
ESI = 0
---@type integer
EBP = 0
---@type integer
ESP = 0
---@type integer
EIP = 0
---@type integer
RAX = 0
---@type integer
RBX = 0
---@type integer
RCX = 0
---@type integer
RDX = 0
---@type integer
RDI = 0
---@type integer
RSI = 0
---@type integer
RBP = 0
---@type integer
RSP = 0
---@type integer
RIP = 0
---@type integer
R8 = 0
---@type integer
R9 = 0
---@type integer
R10 = 0
---@type integer
R11 = 0
---@type integer
R12 = 0
---@type integer
R13 = 0
---@type integer
R14 = 0
---@type integer
R15 = 0

-- ============================================================
-- ENUM CONSTANTS
-- The docs reference these by name but do not list integer values;
-- declared as integer globals so the linter accepts them.
-- ============================================================

-- TAlign
---@type integer alNone=0 alTop=1 alBottom=2 alLeft=3 alRight=4 alClient=5 alCustom=6
alNone = 0
---@type integer
alTop = 1
---@type integer
alBottom = 2
---@type integer
alLeft = 3
---@type integer
alRight = 4
---@type integer
alClient = 5
---@type integer
alCustom = 6

-- Breakpoint trigger types (debug_setBreakpoint trigger param)
---@type integer
bptExecute = 0
---@type integer
bptAccess = 1
---@type integer
bptWrite = 2

-- debug_continueFromBreakpoint continueMethod values
---@type integer
co_run = 0
---@type integer
co_stepinto = 1
---@type integer
co_stepover = 2

-- messageDialog type param
---@type integer
mtInformation = 0
---@type integer
mtWarning = 1
---@type integer
mtError = 2
---@type integer
mtConfirmation = 3

-- messageDialog button params
---@type integer
mbOK = 0
---@type integer
mbYes = 1
---@type integer
mbNo = 2
---@type integer
mbCancel = 3
---@type integer
mbAbort = 4
---@type integer
mbRetry = 5
---@type integer
mbIgnore = 6
---@type integer
mbAll = 7
---@type integer
mbHelp = 8

-- Scrollbar style (Memo/SynEdit Scrollbars property)
---@type integer
ssNone = 0
---@type integer
ssHorizontal = 1
---@type integer
ssVertical = 2
---@type integer
ssBoth = 3
---@type integer
ssAutoHorizontal = 4
---@type integer
ssAutoVertical = 5
---@type integer
ssAutoBoth = 6

-- ============================================================
-- VERSION / PLATFORM
-- ============================================================

---@return number version Floating-point CE version
function getCEVersion() end

---@return integer raw Raw integer version
---@return {major: integer, minor: integer, release: integer, build: integer} version
function getCheatEngineFileVersion() end

---@return integer 0=Windows 1=Mac
function getOperatingSystem() end

---@return boolean True if running in Windows Dark Mode
function darkMode() end

-- ============================================================
-- PROTECTION / PERMISSIONS
-- ============================================================

--- Prevents basic memory scanners from opening the CE process
function activateProtection() end

---@param altitude integer?
---@param secondaryprocessid integer?
function enableDRM(altitude, secondaryprocessid) end

---@param address integer|string
---@param size integer
function fullAccess(address, size) end

---@param address integer|string
---@param size integer
---@param protection {r: boolean, w: boolean, x: boolean}
function setMemoryProtection(address, size, protection) end

---@param address integer|string
---@return {r: boolean, w: boolean, x: boolean}
function getMemoryProtection(address) end

-- ============================================================
-- TABLE LOAD / SAVE
-- ============================================================

---@param filename string
---@param merge boolean?
function loadTable(filename, merge) end

---@param filename string
---@param protect boolean?
---@param dontDeactivateDesignerForms boolean?
function saveTable(filename, protect, dontDeactivateDesignerForms) end

---@param filename string
function signTable(filename) end

-- ============================================================
-- MEMORY COPY / COMPARE
-- ============================================================

---@param sourceAddress integer|string
---@param size integer
---@param destinationAddress integer? nil = CE allocates
---@param method integer? 0=target→target 1=target→CE 2=CE→target 3=CE→CE
---@return integer? address Address of copy on success, nil on failure
function copyMemory(sourceAddress, size, destinationAddress, method) end

---@param address1 integer|string
---@param address2 integer|string
---@param size integer
---@param method integer 0=target→target 1=addr1=target addr2=CE 2=addr1=CE addr2=CE
---@return boolean same True if identical
---@return integer? index Index of first difference if not equal
function compareMemory(address1, address2, size, method) end

-- ============================================================
-- MEMORY READ/WRITE — TARGET PROCESS
-- ============================================================

---@param address integer|string
---@param bytecount integer
---@param returnAsTable boolean?
---@return integer[]
function readBytes(address, bytecount, returnAsTable) end

---@param address integer|string
---@param ... integer|integer[] Bytes or a single table
function writeBytes(address, ...) end

---@param address integer|string
---@param signed boolean?
---@return integer
function readShortInteger(address, signed) end

---@param address integer|string
---@param signed boolean?
---@return integer
function readByte(address, signed) end

---@param address integer|string
---@param signed boolean?
---@return integer
function readSmallInteger(address, signed) end

---@param address integer|string
---@param signed boolean?
---@return integer
function readInteger(address, signed) end

---@param address integer|string
---@return integer
function readQword(address) end

---@param address integer|string
---@return integer
function readPointer(address) end

---@param address integer|string
---@return number
function readFloat(address) end

---@param address integer|string
---@return number
function readDouble(address) end

---@param address integer|string
---@param maxlength integer
---@param widechar boolean?
---@return string
function readString(address, maxlength, widechar) end

---@param address integer|string
---@param value integer
---@return boolean
function writeShortInteger(address, value) end

---@param address integer|string
---@param value integer
---@return boolean
function writeByte(address, value) end

---@param address integer|string
---@param value integer
---@return boolean
function writeSmallInteger(address, value) end

---@param address integer|string
---@param value integer
---@return boolean
function writeInteger(address, value) end

---@param address integer|string
---@param value integer
---@return boolean
function writeQword(address, value) end

---@param address integer|string
---@param value integer
function writePointer(address, value) end

---@param address integer|string
---@param value number
---@return boolean
function writeFloat(address, value) end

---@param address integer|string
---@param value number
---@return boolean
function writeDouble(address, value) end

---@param address integer|string
---@param text string
---@param widechar boolean?
---@return boolean
function writeString(address, text, widechar) end

-- ============================================================
-- MEMORY READ/WRITE — CE LOCAL PROCESS
-- ============================================================

---@param address integer
---@param bytecount integer
---@param returnAsTable boolean?
---@return integer[]
function readBytesLocal(address, bytecount, returnAsTable) end

---@param address integer
---@param signed boolean?
---@return integer
function readSmallIntegerLocal(address, signed) end

---@param address integer
---@param signed boolean?
---@return integer
function readIntegerLocal(address, signed) end

---@param address integer
---@return integer
function readQwordLocal(address) end

---@param address integer
---@return integer
function readPointerLocal(address) end

---@param address integer
---@return number
function readFloatLocal(address) end

---@param address integer
---@return number
function readDoubleLocal(address) end

---@param address integer
---@param maxlength integer
---@param widechar boolean?
---@return string
function readStringLocal(address, maxlength, widechar) end

---@param address integer
---@param value integer
---@return boolean
function writeSmallIntegerLocal(address, value) end

---@param address integer
---@param value integer
---@return boolean
function writeIntegerLocal(address, value) end

---@param address integer
---@param value integer
---@return boolean
function writeQwordLocal(address, value) end

---@param address integer
---@param value integer
function writePointerLocal(address, value) end

---@param address integer
---@param value number
---@return boolean
function writeFloatLocal(address, value) end

---@param address integer
---@param value number
---@return boolean
function writeDoubleLocal(address, value) end

---@param address integer
---@param text string
---@param widechar boolean?
function writeStringLocal(address, text, widechar) end

---@param address integer
---@param ... integer|integer[]
function writeBytesLocal(address, ...) end

-- ============================================================
-- SIGN EXTENSION
-- ============================================================

---@param value integer
---@param mostSignificantBit integer
---@return integer
function signExtend(value, mostSignificantBit) end

-- ============================================================
-- BYTE TABLE CONVERSIONS
-- ============================================================

---@param number integer
---@return integer[]
function wordToByteTable(number) end

---@param number integer
---@return integer[]
function dwordToByteTable(number) end

---@param number integer
---@return integer[]
function qwordToByteTable(number) end

---@param number number
---@return integer[]
function floatToByteTable(number) end

---@param number number
---@return integer[]
function doubleToByteTable(number) end

---@param number number
---@return integer[]
function extendedToByteTable(number) end

---@param s string
---@return integer[]
function stringToByteTable(s) end

---@param s string
---@return integer[]
function wideStringToByteTable(s) end

---@param t integer[]
---@param signed boolean?
---@param tableindex integer?
---@return integer
function byteTableToWord(t, signed, tableindex) end

---@param t integer[]
---@param signed boolean?
---@param tableindex integer?
---@return integer
function byteTableToDword(t, signed, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return integer
function byteTableToQword(t, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return number
function byteTableToFloat(t, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return number
function byteTableToDouble(t, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return number
function byteTableToExtended(t, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return string
function byteTableToString(t, tableindex) end

---@param t integer[]
---@param tableindex integer?
---@return string
function byteTableToWideString(t, tableindex) end

-- ============================================================
-- BINARY OPERATIONS
-- ============================================================

---@param int1 integer
---@param int2 integer
---@return integer
function bOr(int1, int2) end

---@param int1 integer
---@param int2 integer
---@return integer
function bXor(int1, int2) end

---@param int1 integer
---@param int2 integer
---@return integer
function bAnd(int1, int2) end

---@param int integer
---@param int2 integer
---@return integer
function bShl(int, int2) end

---@param int integer
---@param int2 integer
---@return integer
function bShr(int, int2) end

---@param int integer
---@return integer
function bNot(int) end

-- ============================================================
-- MEMORY REGIONS
-- ============================================================

---@class MemoryRegion
---@field BaseAddress integer
---@field AllocationBase integer
---@field AllocationProtect integer
---@field RegionSize integer
---@field State integer
---@field Protect integer
---@field Type integer

---@return MemoryRegion[]
function enumMemoryRegions() end

---@param filename string
---@param sourceaddress integer|string
---@param size integer
---@return integer bytesWritten
function writeRegionToFile(filename, sourceaddress, size) end

---@param filename string
---@param destinationaddress integer|string
function readRegionFromFile(filename, destinationaddress) end

-- ============================================================
-- LUA STATE / REFERENCES
-- ============================================================

--- Creates a new lua state (does not destroy the old one — memory leak)
function resetLuaState() end

---@param name string
---@return any
function getGlobalVariable(name) end

---@param name string
---@param value any
function setGlobalVariable(name, value) end

---@param ... any
---@return integer reference
function createRef(...) end

---@param reference integer
---@return any
function getRef(reference) end

---@param reference integer
function destroyRef(reference) end

-- ============================================================
-- FUNCTION ENCODING
-- ============================================================

---@param f function
---@return string
function encodeFunction(f) end

---@param s string
---@return function
function decodeFunction(s) end

---@param script string
---@param pathtodll string?
function encodeFunctionEx(script, pathtodll) end

---@param state boolean
function encodeFunctionCompatibilityMode(state) end

-- ============================================================
-- TRANSLATION
-- ============================================================

---@return string path Empty if no translation active
function getTranslationFolder() end

---@param path string
function loadPOFile(path) end

---@param s string
---@return string
function translate(s) end

---@param translationid string
---@param originalstring string?
---@return string
function translateID(translationid, originalstring) end

-- ============================================================
-- STRING / ENCODING CONVERSION
-- ============================================================

---@param s string|integer[]
---@param originalcodepagenumber integer
---@return string
function convertToUTF8(s, originalcodepagenumber) end

---@param utf8string string
---@param targetcodepage integer
---@return string
function convertFromUTF8(utf8string, targetcodepage) end

---@param s string
---@return string
function ansiToUTF8(s) end

---@param s string
---@return string
function UTF8ToAnsi(s) end

-- ============================================================
-- MODULE ENUMERATION
-- ============================================================

---@class ModuleInfo
---@field Name string
---@field Address integer Base address
---@field Size integer
---@field Is64Bit boolean
---@field PathToFile string

---@param processid integer?
---@return ModuleInfo[]
function enumModules(processid) end

-- ============================================================
-- HASHING / FILE UTILITIES
-- ============================================================

---@param address integer|string
---@param size integer
---@return string md5
function md5memory(address, size) end

---@param pathtofile string
---@return string md5
function md5file(pathtofile) end

---@param pathtofile string
---@return integer raw
---@return {major: integer, minor: integer, release: integer, build: integer} version
function getFileVersion(pathtofile) end

---@param path string
---@param searchMask string?
---@param searchSubDirs boolean?
---@param dirAttrib integer?
---@return string[]
function getFileList(path, searchMask, searchSubDirs, dirAttrib) end

---@param path string
---@param searchSubDirs boolean?
---@return string[]
function getDirectoryList(path, searchSubDirs) end

---@param filepath string
---@return string
function extractFileName(filepath) end

---@param filepath string
---@return string
function extractFileExt(filepath) end

---@param filepath string
---@return string
function extractFileNameWithoutExt(filepath) end

---@param filepath string
---@return string
function extractFilePath(filepath) end

---@return string path
function getTempFolder() end

---@param pathtofile string
---@return boolean
function fileExists(pathtofile) end

---@param pathtofile string
---@return boolean
function deleteFile(pathtofile) end

-- ============================================================
-- SYMBOL HANDLING
-- ============================================================

function enableWindowsSymbols() end

---@param symbolname string
---@param local_ boolean?
---@return integer address
function getAddress(symbolname, local_) end

function enableKernelSymbols() end

---@param symbolname string
---@param local_ boolean?
---@param shallow boolean?
---@return integer? nil if not found
function getAddressSafe(symbolname, local_, shallow) end

---@class SymbolInfo
---@field modulename string
---@field searchkey string
---@field address integer
---@field size integer

---@param symbolname string
---@return SymbolInfo
function getSymbolInfo(symbolname) end

---@param modulename string
---@return integer size
function getModuleSize(modulename) end

---@param address integer|string
---@return string? nil if unknown
function getRTTIClassName(address) end

---@param name string
---@return {offset: integer, name: string, vartype: string}[]
function getStructureElementsFromName(name) end

function loadNewSymbols() end

---@param waittilldone boolean?
function reinitializeSymbolhandler(waittilldone) end

---@param modulename string?
function reinitializeDotNetSymbolhandler(modulename) end

---@param waittilldone boolean?
function reinitializeSelfSymbolhandler(waittilldone) end

function waitForSections() end
function waitForExports() end
function waitForDotNet() end
function waitForPDB() end

---@return boolean
function symbolsDoneLoading() end

---@param state boolean
function searchPDBWhileLoading(state) end

---@param pathtofile string
---@param baseaddress integer
function symbolHandlerAddModule(pathtofile, baseaddress) end

---@param state boolean
---@return boolean previousState
function errorOnLookupFailure(state) end

---@param state boolean
function waitforsymbols(state) end

-- ============================================================
-- ASSEMBLY / DISASSEMBLY
-- ============================================================

---@param address string
---@param addresstojumpto string
---@param addresstogetnewcalladdress string?
---@param ext string?
---@param targetself boolean?
---@return string script
function generateAPIHookScript(address, addresstojumpto, addresstogetnewcalladdress, ext, targetself) end

---@param line string
---@param address integer?
---@param assemblePreference integer? 0=none 1=short 2=long 3=far
---@param skipRangeCheck boolean?
---@return integer[]
function assemble(line, address, assemblePreference, skipRangeCheck) end

---@param text string
---@param targetself boolean?
---@param disableInfo table?
---@return boolean success
---@return table? disableInfo
---@return string[]? warnings
function autoAssemble(text, targetself, disableInfo) end

---@param text string
---@param enable boolean
---@param targetself boolean?
---@return boolean success
---@return string? errorMessage
function autoAssembleCheck(text, enable, targetself) end

---@param text string|table
---@param address integer?
---@param targetself boolean?
---@param kernelmode boolean?
---@param nodebug boolean?
---@return table? symbols nil on failure
---@return string? errorMessage
function compile(text, address, targetself, kernelmode, nodebug) end

---@param filelist table
---@param address integer?
---@param targetself boolean?
---@return table? symbols
---@return string? errorMessage
function compileFiles(filelist, address, targetself) end

function compileTCCLib() end

---@param path string
function addCIncludePath(path) end

---@param path string
function removeCIncludePath(path) end

---@param params string
function setCustomTCCParameters(params) end

---@param text string
---@param references table
---@param coreAssembly string?
---@return string? filename
function compileCS(text, references, coreAssembly) end

---@param pathtodll string
---@param namespace string
---@param classname string
---@param methodname string
---@param parameters string
---@return integer
function dotNetExecuteClassMethod(pathtodll, namespace, classname, methodname, parameters) end

-- ============================================================
-- DISASSEMBLY QUERIES
-- ============================================================

---@param address integer|string
---@return string "address - bytes - opcode : extra"
function disassemble(address) end

---@param disassembledstring string
---@return string address
---@return string bytes
---@return string opcode
---@return string extra
function splitDisassembledString(disassembledstring) end

---@param address integer|string
---@return integer size
function getInstructionSize(address) end

---@param address integer|string
---@return integer address
function getPreviousOpcode(address) end

---@param bytesOrHex string|integer[]
---@param address integer?
---@return string
function disassembleBytes(bytesOrHex, address) end

-- ============================================================
-- REGISTRATION CALLBACKS
-- ============================================================

---@param featureName string
---@param callback fun(): {PathToFile: string, RelativePath: string}[]
---@return integer id
function registerEXETrainerFeature(featureName, callback) end

---@param id integer
function unregisterEXETrainerFeature(id) end

---@param command string
---@param callback fun(parameters: string, syntaxcheckonly: boolean): string?
function registerAutoAssemblerCommand(command, callback) end

---@param command string
function unregisterAutoAssemblerCommand(command) end

---@param functionname string
function registerLuaFunctionHighlight(functionname) end

---@param functionname string
function unregisterLuaFunctionHighlight(functionname) end

---@param callback fun(symbol: string): integer?
---@param location string slStart|slNotInt|slNotModule|slNotUserdefinedSymbol|slNotSymbol|slFailure
---@return integer id
function registerSymbolLookupCallback(callback, location) end

---@param id integer
function unregisterSymbolLookupCallback(id) end

---@param callback fun(address: integer): string?
---@param first boolean
---@return integer id
function registerAddressLookupCallback(callback, first) end

---@param id integer
function unregisterAddressLookupCallback(id) end

---@param callback fun(sender: any)
---@return integer id
function registerGlobalStructureListUpdateNotification(callback) end

---@param id integer
function unregisterGlobalStructureListUpdateNotification(id) end

---@param structureListCallback fun(): table
---@param elementListCallback fun(id1: integer, id2: integer): table
---@return integer id
function registerStructureAndElementListCallback(structureListCallback, elementListCallback) end

---@param id integer
function unregisterStructureAndElementListCallback(id) end

---@param callback fun(structure: any, baseaddress: integer): boolean?
---@return integer id
function registerStructureDissectOverride(callback) end

---@param id integer
function unregisterStructureDissectOverride(id) end

---@param callback fun(address: integer): string?, integer?
---@param first boolean?
---@return integer id
function registerStructureNameLookup(callback, first) end

---@param id integer
function unregisterStructureNameLookup(id) end

---@param callback fun(address: integer, instruction: string): integer[]?
---@return integer id
function registerAssembler(callback) end

---@param id integer
function unregisterAssembler(id) end

---@param callback fun(script: any, syntaxcheck: boolean)
---@param postAOB boolean?
---@return integer id
function registerAutoAssemblerPrologue(callback, postAOB) end

---@param id integer
function unregisterAutoAssemblerPrologue(id) end

---@param name string
---@param callback fun(script: any, sender: any)
---@param shortcut string?
---@return integer id
function registerAutoAssemblerTemplate(name, callback, shortcut) end

---@param id integer
function unregisterAutoAssemblerTemplate(id) end

---@param callback fun(): table?
function registerProcessListCallback(callback) end

---@param callback fun(): table?
function registerModuleListCallback(callback) end

---@param callback fun(address: integer, ceguess: string): string
function onAutoGuess(callback) end

-- ============================================================
-- CODE INJECTION SCRIPT HELPERS
-- ============================================================

---@param script any TStrings
---@param address string
---@param farjmp boolean
function generateCodeInjectionScript(script, address, farjmp) end

---@param script any TStrings
---@param symbolname string
---@param address string
---@param commentradius integer?
---@param farjmp boolean?
function generateAOBInjectionScript(script, symbolname, address, commentradius, farjmp) end

---@param script any TStrings
---@param address string
---@param commentradius integer?
---@param farjmp boolean?
function generateFullInjectionScript(script, address, commentradius, farjmp) end

---@param script any TStrings
---@return integer
function getNextAllocNumber(script) end

---@param script any TStrings
---@param address integer
---@param radius integer?
function addSnapshotAsComment(script, address, radius) end

---@param address integer
---@return string aobString
---@return integer offset
function getUniqueAOB(address) end

-- ============================================================
-- UI / DIALOGS
-- ============================================================

---@param text string
function showMessage(text) end

---@param caption string
---@param prompt string
---@param initialstring string
---@return string? nil on cancel
function inputQuery(caption, prompt, initialstring) end

---@param title string
---@param caption string
---@param stringlist any
---@param allowCustomInput boolean?
---@param formname string?
---@return integer lineNumber -1 if custom input
---@return string selectedString
function showSelectionList(title, caption, stringlist, allowCustomInput, formname) end

---@param text string
---@param ... any type and button args
function messageDialog(text, ...) end

---@param milliseconds integer
function sleep(milliseconds) end

---@return {pid: integer, name: string}[]
function getProcesslist() end

---@return table
function getWindowlist() end

---@param filter integer? 0=all 1=target handles only 2=handles to target 3=handles to CE
---@return {ProcessID: integer, ObjectTypeIndex: integer, HandleAttributes: integer, HandleValue: integer, Object: integer, GrantedAccess: integer}[]
function getHandleList(filter) end

---@param handle integer
---@param processid integer?
function closeRemoteHandle(handle, processid) end

---@param handle integer
---@param modeOrFromPID integer?
---@param toPID integer?
---@return integer
function duplicateHandle(handle, modeOrFromPID, toPID) end

-- ============================================================
-- EVENT HANDLER STUBS (define globally to receive callbacks)
-- ============================================================

--- Called whenever CE opens a process
---@param processid integer
function onOpenProcess(processid) end

--- Called twice when a table is loaded — once before, once after
---@param before boolean
function onTableLoad(before) end

--- Called when a Lua error occurs; return string overrides the error shown
---@param errorstring string
---@return string
function onLuaError(errorstring) end

function onPointerMapGenerationStart() end
function onPointerMapGenerationFinish() end

--- Called when a breaking breakpoint hits; EAX/RBX/... globals are populated
---@return integer? 0=update UI, anything else=skip UI update
---@type (fun():integer?)|nil
debugger_onBreakpoint = nil

--- Called when a module is loaded (Windows debugger only)
---@param modulename string
---@param baseaddress integer
---@return integer? 1 to break into debugger
function debugger_onModuleLoad(modulename, baseaddress) end

--- Called before a memory record's Active state changes
---@param memoryrecord MemoryRecord
---@param newstate boolean
function onMemRecPreExecute(memoryrecord, newstate) end

--- Called after a memory record's Active state changes
---@param memoryrecord MemoryRecord
---@param newState boolean
---@param succeeded boolean
function onMemRecPostExecute(memoryrecord, newState, succeeded) end

-- ============================================================
-- PROCESS MANAGEMENT
-- ============================================================

---@return integer 0 if none open
function getOpenedProcessID() end

---@return integer handle
function getOpenedProcessHandle() end

---@param name string
---@return integer processid
function getProcessIDFromProcessName(name) end

---@param processidOrName integer|string
function openProcess(processidOrName) end

---@param filename string
---@param is64bit boolean?
---@param startaddress integer?
function openFileAsProcess(filename, is64bit, startaddress) end

---@return integer size
function getOpenedFileSize() end

---@param filename string?
function saveOpenedFile(filename) end

---@param size integer
function setPointerSize(size) end

---@return integer
function getPointerSize() end

---@param mode integer 0=32-bit 1=64-bit
function setAssemblerMode(mode) end

function pause() end
function unpause() end

---@return integer
function getCPUCount() end

---@param eax integer
---@param ecx integer
---@return {EAX: integer, EBX: integer, ECX: integer, EDX: integer}
function cpuid(eax, ecx) end

---@param state boolean
function gc_setPassive(state) end

---@param state boolean
---@param interval integer
---@param minsize integer
function gc_setActive(state, interval, minsize) end

-- ============================================================
-- SCREEN / INPUT
-- ============================================================

---@param index integer
---@return integer
function getSystemMetrics(index) end

---@return integer
function getScreenDPI() end

---@return integer
function getScreenHeight() end

---@return integer
function getScreenWidth() end

---@return integer
function getWorkAreaHeight() end

---@return integer
function getWorkAreaWidth() end

---@param color integer
---@return integer
function invertColor(color) end

---@return Canvas
function getScreenCanvas() end

---@param x integer
---@param y integer
---@return integer rgb
function getPixel(x, y) end

---@return integer x
---@return integer y
function getMousePos() end

---@param x integer
---@param y integer
function setMousePos(x, y) end

---@param key integer
---@return boolean
function isKeyPressed(key) end

---@param key integer
function keyDown(key) end

---@param key integer
function keyUp(key) end

---@param key integer
function doKeyPress(key) end

---@param flags integer
---@param x integer?
---@param y integer?
---@param data integer?
---@param extra integer?
function mouse_event(flags, x, y, data, extra) end

---@param shortcut integer
---@return string
function shortCutToText(shortcut) end

---@param shortcutstring string
---@return integer
function textToShortCut(shortcutstring) end

---@param ... integer
---@return string
function convertKeyComboToString(...) end

---@param text string
function outputDebugString(text) end

---@param command string
---@param parameters string?
---@param folder string?
---@param showcommand string?
function shellExecute(command, parameters, folder, showcommand) end

---@param exepath string
---@param parameters string|string[]
---@param pathtoexecutein string?
---@return string output
---@return integer exitcode
function runCommand(exepath, parameters, pathtoexecutein) end

---@return integer milliseconds since Windows start
function getTickCount() end

---@return string H:M:S.ms
function getTimeStamp() end

function processMessages() end
function processMessagesPaintOnly() end

---@return boolean
function inMainThread() end

---@param int integer
---@return userdata
function integerToUserData(int) end

---@param userData userdata
---@return integer
function userDataToInteger(userData) end

---@param f function
---@param ... any
---@return any
function synchronize(f, ...) end

---@param f function
---@param ... any
function queue(f, ...) end

---@param timeout integer?
function checkSynchronize(timeout) end

---@param text string
function writeToClipboard(text) end

---@return string
function readFromClipboard() end

-- ============================================================
-- SPEEDHACK
-- ============================================================

---@param speed number
function speedhack_setSpeed(speed) end

---@return number
function speedhack_getSpeed() end

---@param onActivate fun(): boolean, boolean?, string?
---@param onSetSpeed fun(speed: number): boolean, boolean?, string?
---@return integer callbackID
function registerSpeedhackCallbacks(onActivate, onSetSpeed) end

---@param callbackID integer
function unregisterSpeedhackCallbacks(callbackID) end

-- ============================================================
-- DLL INJECTION / CODE EXECUTION
-- ============================================================

---@param filename string
---@param skipsymbolreloadwait boolean?
---@return boolean
function injectDLL(filename, skipsymbolreloadwait) end

---@param filepath string
---@param skipsymbolreloadwait boolean?
---@return boolean
function injectLibrary(filepath, skipsymbolreloadwait) end

---@param dllpath string
---@param fullClassName string
---@param methodName string
---@param parameterstring string
---@param timeout integer?
function injectDotNetDLL(dllpath, fullClassName, methodName, parameterstring, timeout) end

---@param address integer|string
---@param parameter integer?
---@param timeout integer?
---@return integer returnValue
function executeCode(address, parameter, timeout) end

---@param address integer|string
---@param parameter integer?
---@return integer returnValue
function executeCodeLocal(address, parameter) end

---@param callmethod integer 0=stdcall 1=cdecl
---@param timeout integer?
---@param address integer|string
---@param ... {type: integer, value: any}|any
---@return integer returnValue
function executeCodeEx(callmethod, timeout, address, ...) end

---@param callmethod integer
---@param timeout integer?
---@param address integer|string
---@param instance integer|{regnr: integer, classinstance: integer}
---@param ... {type: integer, value: any}|any
---@return integer returnValue
function executeMethod(callmethod, timeout, address, instance, ...) end

---@param address integer|string
---@param ... {type: integer, value: any}|any
---@return integer returnValue
function executeCodeLocalEx(address, ...) end

-- ============================================================
-- PLUGINS / FONTS
-- ============================================================

---@param dllnameorpath string
---@return integer? nil on failure
function loadPlugin(dllnameorpath) end

---@param memorystream MemoryStream
---@return integer fontID
function loadFontFromStream(memorystream) end

---@param id integer
function unloadLoadedFont(id) end

-- ============================================================
-- AUTO GUESS
-- ============================================================

---@param address integer|string
---@param nostring boolean?
---@param nodouble boolean?
---@return string valuetype
function autoGuess(address, nostring, nodouble) end

-- ============================================================
-- CE WINDOW MANAGEMENT
-- ============================================================

function closeCE() end
function hideAllCEWindows() end
function unhideMainCEwindow() end

---@return Stringlist
function getAutoAttachList() end

-- ============================================================
-- AOB SCAN
-- ============================================================

---@param aobstring string|integer
---@param protectionflags string?
---@param alignmenttype integer?
---@param alignmentparam string?
---@return Stringlist results Don't forget to free
function AOBScan(aobstring, protectionflags, alignmenttype, alignmentparam) end

---@param aobstring string
---@param protectionflags string?
---@param alignmenttype integer?
---@param alignmentparam string?
---@return integer? address nil if not found
function AOBScanUnique(aobstring, protectionflags, alignmenttype, alignmentparam) end

---@param modulename string
---@param aobstring string
---@param protectionflags string?
---@param alignmenttype integer?
---@param alignmentparam string?
---@return integer? address nil if not found
function AOBScanModuleUnique(modulename, aobstring, protectionflags, alignmenttype, alignmentparam) end

-- ============================================================
-- MEMORY ALLOCATION
-- ============================================================

---@param size integer
---@param baseAddress integer?
---@param protection integer?
---@return integer address
function allocateMemory(size, baseAddress, protection) end

---@param address integer
---@param size integer?
function deAlloc(address, size) end

---@param name string
---@param size integer?
---@return integer address in target process
function allocateSharedMemory(name, size) end

---@param name string
---@param size integer?
---@return integer address in CE process
function allocateSharedMemoryLocal(name, size) end

---@param size integer
---@return any section
function createSection(size) end

---@param section any
---@param preferedBaseAddress integer?
---@return integer baseAddress
function mapViewOfSection(section, preferedBaseAddress) end

---@param baseaddress integer
function unMapViewOfSection(baseaddress) end

-- ============================================================
-- WINDOW QUERIES
-- ============================================================

---@return integer processID
function getForegroundProcess() end

---@param classname string?
---@param caption string?
---@return integer windowHandle
function findWindow(classname, caption) end

---@param windowhandle integer
---@param command integer
---@return integer windowHandle
function getWindow(windowhandle, command) end

---@param windowhandle integer
---@return string
function getWindowCaption(windowhandle) end

---@param windowhandle integer
---@return string
function getWindowClassName(windowhandle) end

---@param windowhandle integer
---@return integer processid
function getWindowProcessID(windowhandle) end

---@return integer windowHandle
function getForegroundWindow() end

---@param hwnd integer
---@param msg integer
---@param wparam integer
---@param lparam integer
---@return integer result
function sendMessage(hwnd, msg, wparam, lparam) end

---@param hwnd integer
---@param callback fun(hwnd: integer, msg: integer, wparam: integer, lparam: integer): integer?, integer?, integer?, integer?, integer?
---@param async boolean?
function hookWndProc(hwnd, callback, async) end

---@param hwnd integer
function unhookWndProc(hwnd) end

-- ============================================================
-- PLATFORM DETECTION
-- ============================================================

---@return boolean
function cheatEngineIs64Bit() end

---@return boolean
function targetIs64Bit() end

---@return boolean
function targetIsX86() end

---@return boolean
function targetIsArm() end

---@return boolean
function targetIsAndroid() end

---@return boolean
function targetIsRosetta() end

---@return integer 0=Windows 1=Unix
function getABI() end

---@return string path
function getCheatEngineDir() end

---@return integer processid
function getCheatEngineProcessID() end

---@return string path
function getAutorunPath() end

-- ============================================================
-- AUDIO
-- ============================================================

function beep() end

---@param stream MemoryStream|string tablefile name
---@param waittilldone boolean?
function playSound(stream, waittilldone) end

---@param text string
---@param waittilldone boolean|integer?
function speak(text, waittilldone) end

---@param text string
---@param waittilldone boolean?
function speakEnglish(text, waittilldone) end

-- ============================================================
-- MISC
-- ============================================================

---@param ... any
function printf(...) end

---@param state string tbpsNone|tbpsIndeterminate|tbpsNormal|tbpsError|tbpsPaused
function setProgressState(state) end

---@param current integer
---@param max integer
function setProgressValue(current, max) end

---@param name string
---@return string
function getUserRegistryEnvironmentVariable(name) end

---@param name string
---@param value string
function setUserRegistryEnvironmentVariable(name, value) end

---@param s string
---@return string md5hash
function stringToMD5String(s) end

---@return integer
function getFormCount() end

---@param index integer
---@return Form
function getForm(index) end

---@param callback fun(form: Form)
---@return userdata
function registerFormAddNotification(callback) end

---@param obj userdata
function unregisterFormAddNotification(obj) end

---@return Form
function getSettingsForm() end

---@return Memoryview
function getMemoryViewForm() end

---@return Form
function getMainForm() end

---@return any Application object
function getApplication() end

---@return AddressList
function getAddressList() end

---@return Timer
function getFreezeTimer() end

---@return Timer
function getUpdateTimer() end

---@param interval integer
function setGlobalKeyPollInterval(interval) end

---@param delay integer
function setGlobalDelayBetweenHotkeyActivation(delay) end

---@class XBox360State
---@field ControllerID integer
---@field PacketNumber integer
---@field GAMEPAD_DPAD_UP boolean
---@field GAMEPAD_DPAD_DOWN boolean
---@field GAMEPAD_DPAD_LEFT boolean
---@field GAMEPAD_DPAD_RIGHT boolean
---@field GAMEPAD_START boolean
---@field GAMEPAD_BACK boolean
---@field GAMEPAD_LEFT_THUMB boolean
---@field GAMEPAD_RIGHT_THUMB boolean
---@field GAMEPAD_LEFT_SHOULDER boolean
---@field GAMEPAD_RIGHT_SHOULDER boolean
---@field GAMEPAD_A boolean
---@field GAMEPAD_B boolean
---@field GAMEPAD_X boolean
---@field GAMEPAD_Y boolean
---@field LeftTrigger integer 0-255
---@field RightTrigger integer 0-255
---@field ThumbLeftX integer -32768 to 32767
---@field ThumbLeftY integer -32768 to 32767
---@field ThumbRightX integer -32768 to 32767
---@field ThumbRightY integer -32768 to 32767

---@param controllerID integer?
---@return XBox360State?
function getXBox360ControllerState(controllerID) end

---@param controllerID integer
---@param leftMotor integer 0-65535
---@param rightMotor integer 0-65535
function setXBox360ControllerVibration(controllerID, leftMotor, rightMotor) end

-- ============================================================
-- UNDEFINED PROPERTY HELPERS
-- ============================================================

---@param class any
---@return Stringlist
function getPropertyList(class) end

---@param class any
---@param propertyname string
---@param propertyvalue any
function setProperty(class, propertyname, propertyvalue) end

---@param class any
---@param propertyname string
---@return any
function getProperty(class, propertyname) end

---@param class any
---@param propertyname string
---@param func function
function setMethodProperty(class, propertyname, func) end

---@param class any
---@param propertyname string
---@return function
function getMethodProperty(class, propertyname) end

-- ============================================================
-- SYMBOL REGISTRATION
-- ============================================================

---@param symbolname string
---@param address integer
---@param donotsave boolean?
function registerSymbol(symbolname, address, donotsave) end

---@param symbolname string
function unregisterSymbol(symbolname) end

---@return {symbolname: string, address: integer}[]
function enumRegisteredSymbols() end

function deleteAllRegisteredSymbols() end

---@param address integer
---@param moduleNames boolean?
---@param symbols boolean?
---@param sections boolean?
---@return string
function getNameFromAddress(address, moduleNames, symbols, sections) end

---@param address integer
---@return boolean
function inModule(address) end

---@param address integer
---@return boolean
function inSystemModule(address) end

---@return Stringlist
function getCommonModuleList() end

-- ============================================================
-- DEBUGGING
-- ============================================================

---@param path string
---@param parameters string?
---@param debug boolean?
---@param breakonentrypoint boolean?
function createProcess(path, parameters, debug, breakonentrypoint) end

---@param interface_ integer? 0=default 1=windows 2=VEH 3=Kernel
function debugProcess(interface_) end

function detachIfPossible() end

---@return boolean
function debug_isDebugging() end

---@return integer? 1=windows 2=VEH 3=Kernel 4=mac 5=gdb nil=none
function debug_getCurrentDebuggerInterface() end

---@return boolean
function debug_canBreak() end

---@return boolean
function debug_isBroken() end

---@return boolean
function debug_isStepping() end

---@return integer[]
function debug_getBreakpointList() end

---@param threadid integer
function debug_breakThread(threadid) end

---@param threadid integer
function debug_addThreadToNoBreakList(threadid) end

---@param threadid integer
function debug_removeThreadFromNoBreakList(threadid) end

---@param threadid integer
---@param address integer|string
---@param size integer?
---@param trigger string? bptExecute|bptAccess|bptWrite
---@param breakpointmethod integer?
---@param functiontocall function?
function debug_setBreakpointForThread(threadid, address, size, trigger, breakpointmethod, functiontocall) end

---@param address integer|string
---@param sizeOrFunctionToCall integer|function?
---@param trigger integer? bptExecute|bptAccess|bptWrite
---@param breakpointmethodOrCallback integer|function?
---@param functiontocall function?
function debug_setBreakpoint(address, sizeOrFunctionToCall, trigger, breakpointmethodOrCallback, functiontocall) end

---@param address integer|string
function debug_removeBreakpoint(address) end

---@param continueMethod integer co_run|co_stepinto|co_stepover
function debug_continueFromBreakpoint(continueMethod) end

---@param xmmregnr integer 0-15 (0-7 on 32-bit)
---@return integer CE local address
function debug_getXMMPointer(xmmregnr) end

---@param state boolean
function debug_setLastBranchRecording(state) end

---@return integer -1 if none
function debug_getMaxLastBranchRecord() end

---@param index integer
---@return integer
function debug_getLastBranchRecord(index) end

---@param extraregs boolean
function debug_getContext(extraregs) end

---@param extraregs boolean
function debug_setContext(extraregs) end

function debug_updateGUI() end

-- ============================================================
-- ADDRESS COMMENTS / HEADERS
-- ============================================================

---@param address integer
---@return string
function getComment(address) end

---@param address integer
---@param text string
function setComment(address, text) end

---@param address integer
---@return string
function getHeader(address) end

---@param address integer
---@param text string
function setHeader(address, text) end

-- ============================================================
-- CLASS HELPERS
-- ============================================================

---@param object any
---@return boolean
function inheritsFromObject(object) end

---@param object any
---@return boolean
function inheritsFromComponent(object) end

---@param object any
---@return boolean
function inheritsFromControl(object) end

---@param object any
---@return boolean
function inheritsFromWinControl(object) end

---@param classname string
---@return any
function createClass(classname) end

---@param classname string
---@param owner any
---@return any
function createComponentClass(classname, owner) end

-- ============================================================
-- CLASS DEFINITIONS
-- ============================================================

-- ------------------------------------------------------------
-- Object
-- ------------------------------------------------------------

---@class Object
---@field ClassName string (readonly)
local Object = {}
---@return string
function Object:getClassName() end
---@param fieldname string
---@return integer
function Object:fieldAddress(fieldname) end
---@param methodname string
function Object:methodAddress(methodname) end
---@param address integer
function Object:methodName(address) end
function Object:destroy() end

-- ------------------------------------------------------------
-- Component
-- ------------------------------------------------------------

---@class Component: Object
---@field ComponentCount integer (readonly)
---@field Name string
---@field Tag integer Free-use integer storage
---@field Owner Component?
local Component = {}
---@return integer
function Component:getComponentCount() end
---@param index integer
---@return Component
function Component:getComponent(index) end
---@param name string
---@return Component?
function Component:findComponentByName(name) end
---@return string
function Component:getName() end
---@param newname string
function Component:setName(newname) end
---@return integer
function Component:getTag() end
---@param tagvalue integer
function Component:setTag(tagvalue) end
---@return Component?
function Component:getOwner() end

-- ------------------------------------------------------------
-- Control
-- ------------------------------------------------------------

---@class Control: Component
---@field Caption string
---@field Top integer Y position
---@field Left integer X position
---@field Width integer
---@field Height integer
---@field ClientWidth integer
---@field ClientHeight integer
---@field Align integer alNone|alTop|alBottom|alLeft|alRight|alClient
---@field Enabled boolean
---@field Visible boolean
---@field Color integer
---@field RGBColor integer
---@field Parent WinControl?
---@field PopupMenu PopupMenu?
---@field Font Font
---@field OnClick fun(sender: any)?
---@field OnDblClick fun(sender: any)?
---@field OnMouseDown fun(sender: any, button: integer, x: integer, y: integer)?
---@field OnChangeBounds fun(sender: any)?
local Control = {}
function Control:getLeft() end
---@param v integer
function Control:setLeft(v) end
function Control:getTop() end
---@param v integer
function Control:setTop(v) end
function Control:getWidth() end
---@param v integer
function Control:setWidth(v) end
function Control:getHeight() end
---@param v integer
function Control:setHeight(v) end
---@param caption string
function Control:setCaption(caption) end
---@return string
function Control:getCaption() end
---@param x integer
---@param y integer
function Control:setPosition(x, y) end
---@return integer x
---@return integer y
function Control:getPosition() end
---@param width integer
---@param height integer
function Control:setSize(width, height) end
---@return integer width
---@return integer height
function Control:getSize() end
---@param option string
function Control:setAlign(option) end
---@return string
function Control:getAlign() end
---@return boolean
function Control:getEnabled() end
---@param enabled boolean
function Control:setEnabled(enabled) end
---@return boolean
function Control:getVisible() end
---@param visible boolean
function Control:setVisible(visible) end
---@return integer
function Control:getColor() end
---@param color integer
function Control:setColor(color) end
---@return WinControl?
function Control:getParent() end
---@param parent WinControl
function Control:setParent(parent) end
function Control:getPopupMenu() end
function Control:setPopupMenu() end
---@return Font
function Control:getFont() end
function Control:setFont() end
function Control:repaint() end
function Control:refresh() end
function Control:update() end
---@param f fun(sender: any)
function Control:setOnClick(f) end
---@return fun(sender: any)?
function Control:getOnClick() end
function Control:doClick() end
function Control:bringToFront() end
function Control:sendToBack() end
---@param x integer
---@param y integer
---@return integer cx
---@return integer cy
function Control:screenToClient(x, y) end
---@param x integer
---@param y integer
---@return integer sx
---@return integer sy
function Control:clientToScreen(x, y) end

-- ------------------------------------------------------------
-- Region
-- ------------------------------------------------------------

---@class Region
local Region = {}
---@return Region
function createRegion() end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Region:addRectangle(x1, y1, x2, y2) end
---@param coords number[][]
function Region:addPolygon(coords) end

-- ------------------------------------------------------------
-- WinControl
-- ------------------------------------------------------------

---@class WinControl: Control
---@field Handle integer Windows HWND
---@field DoubleBuffered boolean
---@field ControlCount integer (readonly)
---@field OnEnter fun()?
---@field OnExit fun()?
local WinControl = {}
---@return integer
function WinControl:getControlCount() end
---@param index integer
---@return WinControl
function WinControl:getControl(index) end
---@param x integer
---@param y integer
---@return Control?
function WinControl:getControlAtPos(x, y) end
---@return boolean
function WinControl:canFocus() end
---@return boolean
function WinControl:focused() end
function WinControl:setFocus() end
---@param region Region|any
function WinControl:setShape(region) end
---@param f fun()
function WinControl:setOnEnter(f) end
---@return fun()?
function WinControl:getOnEnter() end
---@param f fun()
function WinControl:setOnExit(f) end
---@return fun()?
function WinControl:getOnExit() end
---@param key integer
---@param alpha integer
---@param flags integer LWA_ALPHA|LWA_COLORKEY
function WinControl:setLayeredAttributes(key, alpha, flags) end

-- ------------------------------------------------------------
-- ControlScrollBar
-- ------------------------------------------------------------

---@class ControlScrollBar
---@field Increment integer
---@field Page integer
---@field Smooth boolean
---@field Position integer
---@field Range integer
---@field Tracking boolean
---@field Visible boolean

-- ------------------------------------------------------------
-- ScrollingWinControl
-- ------------------------------------------------------------

---@class ScrollingWinControl: WinControl
---@field HorzScrollBar ControlScrollBar
---@field VertScrollBar ControlScrollBar

-- ------------------------------------------------------------
-- Form
-- ------------------------------------------------------------

---@class Form: ScrollingWinControl
---@field DesignTimePPI integer
---@field AllowDropFiles boolean
---@field ModalResult integer
---@field Menu MainMenu?
---@field OnClose fun(sender: any)?
---@field OnDropFiles fun(sender: any, filenames: string[])?
---@field FormState string fsCreating|fsVisible|fsShowing|fsModal|...
local Form = {}
function Form:fixDPI() end
function Form:centerScreen() end
function Form:hide() end
function Form:show() end
--- PascalCase alias for show() — CE exposes both casings
function Form:Show() end
function Form:close() end
function Form:bringToFront() end
---@return integer modalResult
function Form:showModal() end
---@return boolean
function Form:isForegroundWindow() end
---@param f fun(sender: any)
function Form:setOnClose(f) end
---@return fun(sender: any)?
function Form:getOnClose() end
---@return MainMenu?
function Form:getMenu() end
---@param menu MainMenu
function Form:setMenu(menu) end
---@param style string
function Form:setBorderStyle(style) end
---@return string
function Form:getBorderStyle() end
---@param image any RasterImage
function Form:printToRasterImage(image) end
---@param f fun(form: Form)
---@return userdata
function Form:registerCreateCallback(f) end
---@param ud userdata
function Form:unregisterCreateCallback(ud) end
---@param f fun(form: Form)
---@return userdata
function Form:registerFirstShowCallback(f) end
---@param ud userdata
function Form:unregisterFirstShowCallback(ud) end
---@param f fun(form: Form)
---@return userdata
function Form:registerCloseCallback(f) end
---@param ud userdata
function Form:unregisterCloseCallback(ud) end
function Form:dragNow() end
---@param extra integer[]?
function Form:saveFormPosition(extra) end
---@return boolean success
---@return integer[]? extra
function Form:loadFormPosition() end

-- ------------------------------------------------------------
-- CEForm
-- ------------------------------------------------------------

---@class CEForm: Form
---@field DoNotSaveInTable boolean
local CEForm = {}
---@param visible boolean?
---@return CEForm
function createForm(visible) end
---@param filename string
---@return CEForm
function createFormFromFile(filename) end
---@param stream any
---@return CEForm
function createFormFromStream(stream) end
---@param filename string
function CEForm:saveToFile(filename) end
---@param s any stream
function CEForm:saveToStream(s) end
---@return boolean
function CEForm:getDoNotSaveInTable() end
---@param v boolean
function CEForm:setDoNotSaveInTable(v) end
function CEForm:saveCurrentStateAsDesign() end

-- ------------------------------------------------------------
-- Strings / Stringlist
-- ------------------------------------------------------------

---@class Strings: Object
---@field LineBreak string
---@field Text string All strings joined
---@field Count integer
local Strings = {}
function Strings:clear() end
---@param s string
---@param data integer?
---@return integer index
function Strings:add(s, data) end
---@param strings string
function Strings:addText(strings) end
---@param index integer
function Strings:delete(index) end
---@return string
function Strings:getText() end
---@param text string
function Strings:setText(text) end
---@param s string
---@return integer -1 if not found
function Strings:indexOf(s) end
---@param index integer
---@param s string
function Strings:insert(index, s) end
---@return integer
function Strings:getCount() end
---@param s string
function Strings:remove(s) end
---@param filename string
---@param ignoreencoding boolean?
function Strings:loadFromFile(filename, ignoreencoding) end
---@param filename string
function Strings:saveToFile(filename) end
---@param index integer
---@return string
function Strings:getString(index) end
---@param index integer
---@param s string
function Strings:setString(index, s) end
---@param index integer
---@return integer
function Strings:getData(index) end
---@param index integer
---@param v integer
function Strings:setData(index, v) end
function Strings:beginUpdate() end
function Strings:endUpdate() end

---@class Stringlist: Strings
---@field Sorted boolean
---@field Duplicates string dupIgnore|dupAccept|dupError
---@field CaseSensitive boolean
local Stringlist = {}
---@return Stringlist
function createStringlist() end
---@return string
function Stringlist:getDuplicates() end
---@param dup string
function Stringlist:setDuplicates(dup) end
---@return boolean
function Stringlist:getSorted() end
---@param v boolean
function Stringlist:setSorted(v) end
---@return boolean
function Stringlist:getCaseSensitive() end
---@param v boolean
function Stringlist:setCaseSensitive(v) end

-- ------------------------------------------------------------
-- OrderedList
-- ------------------------------------------------------------

---@class OrderedList: Object
---@field Count integer
local OrderedList = {}
---@param v integer
function OrderedList:push(v) end
---@return integer
function OrderedList:pop() end
---@return integer
function OrderedList:peek() end

-- ------------------------------------------------------------
-- ImageList
-- ------------------------------------------------------------

---@class ImageList
---@field Count integer
---@field DrawingStyle string dsFocus|dsSelected|dsNormal|dsTransparent
---@field Height integer
---@field Width integer
---@field Masked boolean
---@field Scaled boolean
local ImageList = {}
---@param owner any?
---@return ImageList
function createImageList(owner) end
---@param bitmap any
---@param bitmapmask any?
---@return integer index
function ImageList:add(bitmap, bitmapmask) end
---@param canvas Canvas
---@param x integer
---@param y integer
---@param index integer
function ImageList:draw(canvas, x, y, index) end
---@param index integer
---@param bitmap any
---@param effect integer?
function ImageList:getBitmap(index, bitmap, effect) end

-- ------------------------------------------------------------
-- MenuItem / Menu
-- ------------------------------------------------------------

-- Menu Class: (Inheritance: Component->Object)
-- properties
--   Items : MenuItem - The base MenuItem class of this menu (readonly)
-- methods
--   getItems() : Returns the main MenuItem of this Menu

---@class Menu: Component
---@field Items MenuItem The base MenuItem class of this menu (readonly)
local Menu = {}
---@return MenuItem
function Menu:getItems() end

---@class MenuItem: Component
---@field Caption string
---@field Shortcut string
---@field Count integer
---@field OnClick fun(sender: any)?
local MenuItem = {}
---@param ownermenu any
---@return MenuItem
function createMenuItem(ownermenu) end
---@return string
function MenuItem:getCaption() end
---@param caption string
function MenuItem:setCaption(caption) end
---@return string
function MenuItem:getShortcut() end
---@param shortcut string e.g. "ctrl+x"
function MenuItem:setShortcut(shortcut) end
---@return integer
function MenuItem:getCount() end
---@param index integer
---@return MenuItem
function MenuItem:getItem(index) end
---@param item MenuItem
function MenuItem:add(item) end
---@param index integer
---@param item MenuItem
function MenuItem:insert(index, item) end
---@param index integer
function MenuItem:delete(index) end
function MenuItem:clear() end
---@param f fun(sender: any)
function MenuItem:setOnClick(f) end
---@return fun(sender: any)?
function MenuItem:getOnClick() end
function MenuItem:doClick() end

---@class MainMenu: Menu
local MainMenu = {}
---@param form Form
---@return MainMenu
function createMainMenu(form) end
---@return MenuItem
function MainMenu:getItems() end

---@class PopupMenu: Menu
local PopupMenu = {}
---@param owner any
---@return PopupMenu
function createPopupMenu(owner) end
---@return MenuItem
function PopupMenu:getItems() end

-- ------------------------------------------------------------
-- Label / Splitter / PaintBox
-- ------------------------------------------------------------

---@class Label: Control
local Label = {}
---@param owner WinControl
---@return Label
function createLabel(owner) end

---@class Splitter: WinControl
local Splitter = {}
---@param owner WinControl
---@return Splitter
function createSplitter(owner) end

---@class PaintBox: Control
---@field Canvas Canvas
local PaintBox = {}
---@param owner WinControl
---@return PaintBox
function createPaintBox(owner) end

-- ------------------------------------------------------------
-- Panel
-- ------------------------------------------------------------

---@class Panel: WinControl
---@field Alignment string
---@field BevelInner string
---@field BevelOuter string
---@field BevelWidth integer
---@field FullRepaint boolean
local Panel = {}
---@param owner WinControl
---@return Panel
function createPanel(owner) end
---@return string
function Panel:getAlignment() end
---@param v string
function Panel:setAlignment(v) end
function Panel:getBevelInner() end
---@param v string
function Panel:setBevelInner(v) end
function Panel:getBevelOuter() end
---@param v string
function Panel:setBevelOuter(v) end
function Panel:getBevelWidth() end
---@param v integer
function Panel:setBevelWidth(v) end
function Panel:getFullRepaint() end
---@param v boolean
function Panel:setFullRepaint(v) end

-- ------------------------------------------------------------
-- Image
-- ------------------------------------------------------------

---@class Image: Control
---@field Canvas Canvas
---@field Transparent boolean
---@field Stretch boolean
local Image = {}
---@param owner WinControl
---@return Image
function createImage(owner) end
---@param filename string
function Image:loadImageFromFile(filename) end
---@return Canvas
function Image:getCanvas() end

-- ------------------------------------------------------------
-- Edit / Memo
-- ------------------------------------------------------------

---@class Edit: WinControl
---@field Text string
---@field PasswordChar string Single char or empty
---@field SelText string
---@field SelStart integer (zero-indexed)
---@field SelLength integer
---@field OnChange fun(sender: any)?
---@field OnKeyPress fun(sender: any, key: string)?
---@field OnKeyUp fun(sender: any, key: integer, shift: any)?
---@field OnKeyDown fun(sender: any, key: integer, shift: any)?
local Edit = {}
---@param owner WinControl
---@return Edit
function createEdit(owner) end
function Edit:clear() end
function Edit:copyToClipboard() end
function Edit:cutToClipboard() end
function Edit:pasteFromClipboard() end
function Edit:selectAll() end
---@param start integer
---@param length integer?
function Edit:select(start, length) end
---@param start integer
---@param length integer?
function Edit:selectText(start, length) end
function Edit:clearSelection() end
---@return string
function Edit:getSelText() end
---@return integer
function Edit:getSelStart() end
---@return integer
function Edit:getSelLength() end
---@return fun(sender: any)?
function Edit:getOnChange() end
---@param f fun(sender: any)
function Edit:setOnChange(f) end

---@class Memo: Edit
---@field Lines Strings
---@field WordWrap boolean
---@field WantTabs boolean
---@field WantReturns boolean
---@field Scrollbars string ssNone|ssHorizontal|ssVertical|ssBoth|ssAutoHorizontal|ssAutoVertical|ssAutoBoth
local Memo = {}
---@param owner WinControl
---@return Memo
function createMemo(owner) end
---@param s string
function Memo:append(s) end
---@return Strings
function Memo:getLines() end
---@return boolean
function Memo:getWordWrap() end
---@param v boolean
function Memo:setWordWrap(v) end
---@return boolean
function Memo:getWantTabs() end
---@param v boolean
function Memo:setWantTabs(v) end
---@return boolean
function Memo:getWantReturns() end
---@param v boolean
function Memo:setWantReturns(v) end
---@return string
function Memo:getScrollbars() end
---@param v string
function Memo:setScrollbars(v) end

-- ------------------------------------------------------------
-- SynEdit
-- ------------------------------------------------------------

---@class SynEdit
---@field Lines Stringlist
---@field ReadOnly boolean
---@field SelStart integer
---@field SelEnd integer
---@field SelText string
---@field CanPaste boolean
---@field CanRedo boolean
---@field CanUndo boolean
---@field CharWidth integer (readonly)
---@field LineHeight integer (readonly)
---@field CaretX integer
---@field CaretY integer
local SynEdit = {}
---@param owner WinControl
---@param mode integer? 0=Lua 1=AutoAssembler 2=C
---@return SynEdit
function createSynEdit(owner, mode) end
function SynEdit:CopyToClipboard() end
function SynEdit:CutToClipboard() end
function SynEdit:PasteFromClipboard() end
function SynEdit:ClearUndo() end
function SynEdit:Redo() end
function SynEdit:Undo() end
function SynEdit:MarkTextAsSaved() end
function SynEdit:ClearSelection() end
function SynEdit:SelectAll() end

-- ------------------------------------------------------------
-- Button / CheckBox / ToggleBox / GroupBox / RadioGroup
-- ------------------------------------------------------------

---@class Button: WinControl
---@field ModalResult integer
local Button = {}
---@param owner WinControl
---@return Button
function createButton(owner) end

---@class CheckBox: WinControl
---@field Checked boolean
---@field AllowGrayed boolean
---@field State integer cbUnchecked=0 cbChecked=1 cbGrayed=2
---@field OnChange fun(sender: any)?
local CheckBox = {}
---@param owner WinControl
---@return CheckBox
function createCheckBox(owner) end
---@return boolean
function CheckBox:getAllowGrayed() end
---@param v boolean
function CheckBox:setAllowGrayed(v) end
---@return integer
function CheckBox:getState() end
---@param v boolean
function CheckBox:setState(v) end
---@param f fun(sender: any)
function CheckBox:onChange(f) end

---@class ToggleBox: CheckBox
local ToggleBox = {}
---@param owner WinControl
---@return ToggleBox
function createToggleBox(owner) end

---@class GroupBox: WinControl
local GroupBox = {}
---@param owner WinControl
---@return GroupBox
function createGroupBox(owner) end

---@class RadioGroup: GroupBox
---@field Items Strings
---@field Columns integer
---@field ItemIndex integer
---@field OnClick fun(sender: any)?
local RadioGroup = {}
---@param owner WinControl
---@return RadioGroup
function createRadioGroup(owner) end
---@return integer
function RadioGroup:getRows() end
---@return Strings
function RadioGroup:getItems() end
---@return integer
function RadioGroup:getColumns() end
---@param v integer
function RadioGroup:setColumns(v) end
---@return integer
function RadioGroup:getItemIndex() end
---@param v integer
function RadioGroup:setItemIndex(v) end
---@param f fun(sender: any)
function RadioGroup:setOnClick(f) end
---@return fun(sender: any)?
function RadioGroup:getOnClick() end

-- ------------------------------------------------------------
-- ListBox / ComboBox
-- ------------------------------------------------------------

---@class ListBox: WinControl
---@field MultiSelect boolean
---@field Items Strings
---@field ItemIndex integer -1 if nothing selected
---@field Canvas Canvas
local ListBox = {}
---@param owner WinControl
---@return ListBox
function createListBox(owner) end
function ListBox:clear() end
function ListBox:clearSelection() end
function ListBox:selectAll() end
---@return Strings
function ListBox:getItems() end
---@param strings Strings
function ListBox:setItems(strings) end
---@return integer
function ListBox:getItemIndex() end
---@param v integer
function ListBox:setItemIndex(v) end
---@return Canvas
function ListBox:getCanvas() end

---@class ComboBox: WinControl
---@field Items Strings
---@field ItemIndex integer -1 if nothing selected
---@field Canvas Canvas
---@field DroppedDown boolean
local ComboBox = {}
---@param owner WinControl
---@return ComboBox
function createComboBox(owner) end
function ComboBox:clear() end
---@return Strings
function ComboBox:getItems() end
function ComboBox:setItems() end
---@return integer
function ComboBox:getItemIndex() end
---@param v integer
function ComboBox:setItemIndex(v) end
---@return Canvas
function ComboBox:getCanvas() end
---@return integer
function ComboBox:getExtraWidth() end

-- ------------------------------------------------------------
-- ProgressBar / TrackBar
-- ------------------------------------------------------------

---@class ProgressBar: WinControl
---@field Min integer
---@field Max integer
---@field Position integer
---@field Step integer
local ProgressBar = {}
---@param owner WinControl
---@return ProgressBar
function createProgressBar(owner) end
function ProgressBar:stepIt() end
---@param v integer
function ProgressBar:stepBy(v) end
---@return integer
function ProgressBar:getMax() end
---@param v integer
function ProgressBar:setMax(v) end
---@return integer
function ProgressBar:getMin() end
---@param v integer
function ProgressBar:setMin(v) end
---@return integer
function ProgressBar:getPosition() end
---@param v integer
function ProgressBar:setPosition(v) end
---@param v integer Without Win7 animation
function ProgressBar:setPosition2(v) end

---@class TrackBar: WinControl
---@field Min integer
---@field Max integer
---@field Position integer
---@field OnChange fun(sender: any)?
local TrackBar = {}
---@param owner WinControl
---@return TrackBar
function createTrackBar(owner) end
function TrackBar:getMax() end
---@param v integer
function TrackBar:setMax(v) end
function TrackBar:getMin() end
---@param v integer
function TrackBar:setMin(v) end
function TrackBar:getPosition() end
---@param v integer
function TrackBar:setPosition(v) end
---@return fun(sender: any)?
function TrackBar:getOnChange() end
---@param f fun(sender: any)
function TrackBar:setOnChange(f) end

-- ------------------------------------------------------------
-- Calendar
-- ------------------------------------------------------------

---@class Calendar: WinControl
---@field Date string yyyy-mm-dd
---@field DateTime number Days since 1899-12-30
local Calendar = {}
---@param owner WinControl
---@return Calendar
function createCalendar(owner) end
---@return string date in OS local short format
function Calendar:getDateLocalFormat() end

-- ------------------------------------------------------------
-- ListView
-- ------------------------------------------------------------

---@class ListItem
---@field Caption string
---@field Checked boolean
---@field SubItems Strings
---@field Selected boolean
---@field Index integer (readonly)
---@field ImageIndex integer
---@field StateIndex integer
---@field Data integer
local ListItem = {}
function ListItem:delete() end
---@return string
function ListItem:getCaption() end
---@param s string
function ListItem:setCaption(s) end
---@return boolean
function ListItem:getChecked() end
---@param v boolean
function ListItem:setChecked(v) end
---@return Strings
function ListItem:getSubItems() end
---@param partial boolean?
function ListItem:makeVisible(partial) end

---@class ListItems
---@field Count integer
local ListItems = {}
function ListItems:clear() end
---@return integer
function ListItems:getCount() end
---@param index integer
---@return ListItem
function ListItems:getItem(index) end
---@return ListItem
function ListItems:add() end

---@class ListView: WinControl
---@field ItemIndex integer -1 if nothing selected
---@field Selected ListItem?
---@field TopItem ListItem?
---@field VisibleRowCount integer
---@field Canvas Canvas (readonly)
---@field AutoWidthLastColumn boolean
---@field HideSelection boolean
---@field RowSelect boolean
---@field OwnerData boolean
local ListView = {}
---@param owner WinControl
---@return ListView
function createListView(owner) end
function ListView:clear() end
---@return integer
function ListView:getItemIndex() end
---@param index integer
function ListView:setItemIndex(index) end
---@return ListItems
function ListView:getItems() end
---@param x integer
---@param y integer
---@return ListItem?
function ListView:getItemAt(x, y) end
---@return Canvas
function ListView:getCanvas() end
function ListView:beginUpdate() end
function ListView:endUpdate() end
---@return ListColumns
function ListView:getColumns() end

-- ------------------------------------------------------------
-- TreeView
-- ------------------------------------------------------------

---@class TreeNode
---@field Text string
---@field Parent TreeNode?
---@field Level integer
---@field HasChildren boolean
---@field Expanded boolean
---@field Count integer
---@field Index integer
---@field AbsoluteIndex integer
---@field Selected boolean
---@field MultiSelected boolean
local TreeNode = {}
function TreeNode:delete() end
function TreeNode:deleteChildren() end
function TreeNode:makeVisible() end
---@param recursive boolean?
function TreeNode:expand(recursive) end
---@param recursive boolean?
function TreeNode:collapse(recursive) end
---@return TreeNode?
function TreeNode:getNextSibling() end
---@param text string
---@return TreeNode
function TreeNode:add(text) end

---@class TreeNodes
---@field Count integer
local TreeNodes = {}
function TreeNodes:clear() end
---@return integer
function TreeNodes:getCount() end
---@param index integer
---@return TreeNode
function TreeNodes:getItem(index) end
---@param text string
---@return TreeNode
function TreeNodes:add(text) end
---@param treenode TreeNode
---@param text string
---@return TreeNode
function TreeNodes:insert(treenode, text) end
---@param treenode TreeNode
---@param text string
---@return TreeNode
function TreeNodes:insertBehind(treenode, text) end

---@class TreeView: WinControl
---@field Items TreeNodes (readonly)
---@field Selected TreeNode?
local TreeView = {}
---@param owner WinControl
---@return TreeView
function createTreeView(owner) end
function TreeView:beginUpdate() end
function TreeView:endUpdate() end
---@return TreeNodes
function TreeView:getItems() end
function TreeView:getSelected() end
function TreeView:setSelected() end
function TreeView:fullCollapse() end
function TreeView:fullExpand() end
---@param filename string
function TreeView:saveToFile(filename) end
---@param filename string
function TreeView:loadFromFile(filename) end

-- ------------------------------------------------------------
-- Canvas / Pen / Brush / Font
-- ------------------------------------------------------------

---@class Pen
---@field Color integer
---@field Width integer
local Pen = {}
---@return integer
function Pen:getColor() end
---@param color integer
function Pen:setColor(color) end
---@return integer
function Pen:getWidth() end
---@param width integer
function Pen:setWidth(width) end

---@class Brush
---@field Color integer
local Brush = {}
---@return integer
function Brush:getColor() end
---@param color integer
function Brush:setColor(color) end

---@class Font
---@field Name string
---@field Size integer
---@field Height integer
---@field Orientation integer
---@field Pitch string fpDefault|fpVariable|fpFixed
---@field Color integer
---@field CharSet integer
---@field Quality string fqDefault|fqDraft|fqProof|fqNonAntialiased|fqAntialiased|fqCleartype|fqCleartypeNatural
---@field Style string[] fsBold|fsItalic|fsStrikeOut|fsUnderline
local Font = {}
---@return Font
function createFont() end
---@return string
function Font:getName() end
---@param name string
function Font:setName(name) end
---@return integer
function Font:getSize() end
---@param size integer
function Font:setSize(size) end
---@return integer
function Font:getColor() end
---@param color integer
function Font:setColor(color) end
---@param font Font
function Font:assign(font) end

---@class Canvas
---@field Brush Brush
---@field Pen Pen
---@field Font Font
---@field Width integer
---@field Height integer
---@field Handle integer DC handle
local Canvas = {}
---@return Brush
function Canvas:getBrush() end
---@return Pen
function Canvas:getPen() end
---@return Font
function Canvas:getFont() end
function Canvas:getWidth() end
function Canvas:getHeight() end
function Canvas:getPenPosition() end
---@param x integer
---@param y integer
function Canvas:setPenPosition(x, y) end
function Canvas:clear() end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Canvas:line(x1, y1, x2, y2) end
---@param x integer
---@param y integer
function Canvas:lineTo(x, y) end
---@param x integer
---@param y integer
function Canvas:moveTo(x, y) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Canvas:rect(x1, y1, x2, y2) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Canvas:fillRect(x1, y1, x2, y2) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param rx integer
---@param ry integer
function Canvas:roundRect(x1, y1, x2, y2, rx, ry) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Canvas:drawFocusRect(x1, y1, x2, y2) end
---@param x integer
---@param y integer
---@param text string
function Canvas:textOut(x, y, text) end
---@param rect {Left: integer, Top: integer, Right: integer, Bottom: integer}
---@param x integer
---@param y integer
---@param text string
function Canvas:textRect(rect, x, y, text) end
---@param text string
---@return integer
function Canvas:getTextWidth(text) end
---@param text string
---@return integer
function Canvas:getTextHeight(text) end
---@param x integer
---@param y integer
---@return integer color
function Canvas:getPixel(x, y) end
---@param x integer
---@param y integer
---@param color integer
function Canvas:setPixel(x, y, color) end
---@param x integer
---@param y integer
---@param color integer?
---@param filltype string? fsSurface|fsBorder
function Canvas:floodFill(x, y, color, filltype) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
function Canvas:ellipse(x1, y1, x2, y2) end
---@param x1 integer
---@param y1 integer
---@param x2 integer
---@param y2 integer
---@param startcolor integer
---@param stopcolor integer
---@param direction integer 0=Vertical 1=Horizontal
function Canvas:gradientFill(x1, y1, x2, y2, startcolor, stopcolor, direction) end
---@param dx1 integer
---@param dy1 integer
---@param dx2 integer
---@param dy2 integer
---@param sourceCanvas Canvas
---@param sx1 integer
---@param sy1 integer
---@param sx2 integer
---@param sy2 integer
function Canvas:copyRect(dx1, dy1, dx2, dy2, sourceCanvas, sx1, sy1, sx2, sy2) end
---@param x integer
---@param y integer
---@param graphic any Graphic-inherited
function Canvas:draw(x, y, graphic) end
---@param rect {Left: integer, Top: integer, Right: integer, Bottom: integer}
---@param graphic any
function Canvas:stretchDraw(rect, graphic) end
---@return {Left: integer, Top: integer, Right: integer, Bottom: integer}
function Canvas:getClipRect() end

-- ------------------------------------------------------------
-- Graphic / RasterImage / Bitmap / PNG / Jpeg / Icon / Picture
-- ------------------------------------------------------------

---@class Graphic: Object
---@field Width integer
---@field Height integer
---@field Transparent boolean
local Graphic = {}
function Graphic:getWidth() end
function Graphic:setWidth() end
function Graphic:getHeight() end
function Graphic:setHeight() end
---@param filename string
function Graphic:loadFromFile(filename) end
---@param filename string
function Graphic:saveToFile(filename) end

---@class RasterImage: Graphic
---@field Canvas Canvas
---@field PixelFormat string pf32bit recommended
---@field TransparentColor integer
local RasterImage = {}
---@return Canvas
function RasterImage:getCanvas() end
---@return string
function RasterImage:getPixelFormat() end
---@param pf string
function RasterImage:setPixelFormat(pf) end
---@param color integer
function RasterImage:setTransparentColor(color) end
---@return integer
function RasterImage:getTransparentColor() end
---@param stream MemoryStream
function RasterImage:saveToStream(stream) end
---@param stream MemoryStream
function RasterImage:loadFromStream(stream) end

---@class Bitmap: RasterImage
local Bitmap = {}
---@param width integer
---@param height integer
---@return Bitmap
function createBitmap(width, height) end

---@class PortableNetworkGraphic: RasterImage
local PortableNetworkGraphic = {}
---@param width integer
---@param height integer
---@return PortableNetworkGraphic
function createPNG(width, height) end

---@class JpegImage: RasterImage
local JpegImage = {}
---@param width integer
---@param height integer
---@return JpegImage
function createJpeg(width, height) end

---@class Icon: RasterImage
local Icon = {}
---@param width integer
---@param height integer
---@return Icon
function createIcon(width, height) end

---@class Picture: Object
---@field Graphic Graphic
---@field PNG PortableNetworkGraphic
---@field Bitmap Bitmap
---@field Jpeg JpegImage
---@field Icon Icon
local Picture = {}
---@return Picture
function createPicture() end
---@param filename string
function Picture:loadFromFile(filename) end
---@param filename string
function Picture:saveToFile(filename) end
---@param stream any
---@param originalextension string?
function Picture:loadFromStream(stream, originalextension) end
---@param sourcepicture Picture
function Picture:assign(sourcepicture) end

-- ------------------------------------------------------------
-- GenericHotkey
-- ------------------------------------------------------------

---@class GenericHotkey: Object
---@field DelayBetweenActivate integer
---@field onHotkey fun()?
local GenericHotkey = {}
---@param f fun()
---@param keys integer|integer[]
---@param ... integer
---@return GenericHotkey
function createHotkey(f, keys, ...) end
---@return integer[]
function GenericHotkey:getKeys() end
---@param ... integer
function GenericHotkey:setKeys(...) end
---@param f fun()
function GenericHotkey:setOnHotkey(f) end
---@return fun()?
function GenericHotkey:getOnHotkey() end

-- ------------------------------------------------------------
-- Timer
-- ------------------------------------------------------------

---@class Timer: Component
---@field Interval integer Milliseconds between ticks
---@field Enabled boolean
---@field OnTimer fun(timer: Timer)?
local Timer = {}
---@param ownerOrDelay any
---@param funcOrEnabled any?
---@param ... any
---@return Timer
function createTimer(ownerOrDelay, funcOrEnabled, ...) end
---@return integer
function Timer:getInterval() end
---@param ms integer
function Timer:setInterval(ms) end
---@return fun(timer: Timer)?
function Timer:getOnTimer() end
---@param f fun(timer: Timer)
function Timer:setOnTimer(f) end
---@return boolean
function Timer:getEnabled() end
---@param v boolean
function Timer:setEnabled(v) end

-- ------------------------------------------------------------
-- Stream classes
-- ------------------------------------------------------------

---@class Stream: Object
---@field Size integer
---@field Position integer
local Stream = {}
---@param stream Stream
---@param count integer
function Stream:copyFrom(stream, count) end
---@param count integer
---@return integer[]
function Stream:read(count) end
---@param bytetable integer[]
---@param count integer?
function Stream:write(bytetable, count) end
---@return integer
function Stream:readByte() end
---@param v integer
function Stream:writeByte(v) end
---@return integer
function Stream:readWord() end
---@param v integer
function Stream:writeWord(v) end
---@return integer
function Stream:readDword() end
---@param v integer
function Stream:writeDword(v) end
---@return integer
function Stream:readQword() end
---@param v integer
function Stream:writeQword(v) end
---@param length integer
---@return string
function Stream:readString(length) end
---@param s string
---@param include0terminator boolean?
function Stream:writeString(s, include0terminator) end
---@return string
function Stream:readAnsiString() end
---@param s string
function Stream:writeAnsiString(s) end

---@class MemoryStream: Stream
---@field Memory integer CE process address of stream data (readonly)
local MemoryStream = {}
---@return MemoryStream
function createMemoryStream() end
---@param filename string
function MemoryStream:loadFromFile(filename) end
---@param filename string
function MemoryStream:saveToFile(filename) end
---@param filename string
---@return boolean success
---@return string? errorMessage
function MemoryStream:loadFromFileNoError(filename) end
---@param filename string
---@return boolean success
---@return string? errorMessage
function MemoryStream:saveToFileNoError(filename) end
function MemoryStream:clear() end

---@class FileStream: Stream
local FileStream = {}
---@param filename string
---@param mode integer fmCreate|fmOpenRead|fmOpenWrite|fmOpenReadWrite combined with share flags
---@return FileStream
function createFileStream(filename, mode) end

---@class StringStream: Stream
---@field DataString string
local StringStream = {}
---@param s string
---@return StringStream
function createStringStream(s) end

-- ------------------------------------------------------------
-- TableFile
-- ------------------------------------------------------------

---@class TableFile: Object
---@field Name string
---@field Stream MemoryStream
---@field DoNotSave boolean
local TableFile = {}
---@param filename string
---@return TableFile?
function findTableFile(filename) end
---@param filename string
---@param filepath string?
---@return TableFile
function createTableFile(filename, filepath) end
function TableFile:delete() end
---@param filename string
function TableFile:saveToFile(filename) end
---@return MemoryStream
function TableFile:getData() end

-- ------------------------------------------------------------
-- Thread / CriticalSection / Event / Semaphore
-- ------------------------------------------------------------

---@class Thread: Object
---@field Finished boolean
---@field Terminated boolean
---@field Result string
---@field Name string
---@field OnDestroy fun(thread: Thread)?
local Thread = {}
---@param f fun(thread: Thread, ...)
---@param ... any
---@return Thread
function createThread(f, ...) end
---@param f fun(thread: Thread, ...)
---@param ... any
---@return Thread
function createThreadSuspended(f, ...) end
---@param scripttext string
---@return Thread
function createThreadNewState(scripttext) end
---@return Thread?
function getCurrentThreadObject() end
---@param state boolean
function Thread:freeOnTerminate(state) end
---@param f fun(thread: Thread, ...)
---@param ... any
---@return any
function Thread:synchronize(f, ...) end
function Thread:waitfor() end
function Thread:suspend() end
function Thread:resume() end
function Thread:terminate() end

---@class CriticalSection: Object
local CriticalSection = {}
---@return CriticalSection
function createCriticalSection() end
function CriticalSection:enter() end
function CriticalSection:leave() end
---@return boolean
function CriticalSection:tryEnter() end

---@class Event: Object
local Event = {}
---@param manualReset boolean
---@param initialState boolean
---@return Event
function createEvent(manualReset, initialState) end
function Event:resetEvent() end
function Event:setEvent() end
---@param timeout integer
---@return integer 0=signaled 1=timeout 2=abandoned 3=error
function Event:waitFor(timeout) end

---@class Semaphore: Object
local Semaphore = {}
---@param count integer
---@return Semaphore
function createSemaphore(count) end
function Semaphore:acquire() end
function Semaphore:release() end

-- ------------------------------------------------------------
-- MemoryRecord / MemoryRecordHotkey / AddressList (class side)
-- ------------------------------------------------------------

---@class MemoryRecord
---@field ID integer (readonly)
---@field Index integer (readonly)
---@field Description string
---@field Address string Interpretable address string
---@field AddressString string Displayed address (readonly)
---@field OffsetCount integer Number of pointer offsets
---@field CurrentAddress integer Resolved address (readonly)
---@field VarType string Variable type name e.g. "vtByte"
---@field Type integer Variable type number
---@field Value string Value as string
---@field NumericalValue number? Value as number; nil if unparseable
---@field Selected boolean (readonly)
---@field Active boolean Freeze/unfreeze state
---@field Color integer
---@field ShowAsHex boolean
---@field ShowAsSigned boolean
---@field AllowIncrease boolean
---@field AllowDecrease boolean
---@field Collapsed boolean
---@field IsGroupHeader boolean
---@field IsAddressGroupHeader boolean
---@field IsReadable boolean (readonly, set after first Value access)
---@field Count integer Number of children
---@field Parent MemoryRecord?
---@field HotkeyCount integer
---@field Async boolean
---@field AsyncProcessing boolean
---@field DontSave boolean
---@field OnActivate fun(memoryrecord: MemoryRecord, before: boolean, currentstate: boolean): boolean?
---@field OnDeactivate fun(memoryrecord: MemoryRecord, before: boolean, currentstate: boolean): boolean?
---@field OnDestroy fun()?
---@field OnGetDisplayValue fun(memoryrecord: MemoryRecord, valuestring: string): boolean, string
---@field OnValueChanged fun(memoryrecord: MemoryRecord, oldvalue: string, newvalue: string)?
---@field OnValueChangedByUser fun(memoryrecord: MemoryRecord, oldvalue: string, newvalue: string)?
local MemoryRecord = {}
---@return string
function MemoryRecord:getDescription() end
function MemoryRecord:setDescription() end
---@return string address
---@return integer[]? offsets
function MemoryRecord:getAddress() end
---@param addr string
function MemoryRecord:setAddress(addr) end
---@return integer
function MemoryRecord:getOffsetCount() end
---@param count integer
function MemoryRecord:setOffsetCount(count) end
---@param index integer
---@return integer
function MemoryRecord:getOffset(index) end
---@param index integer
---@param value integer
function MemoryRecord:setOffset(index, value) end
---@return integer
function MemoryRecord:getCurrentAddress() end
---@param memrec MemoryRecord
function MemoryRecord:appendToEntry(memrec) end
---@param index integer
---@return MemoryRecordHotkey
function MemoryRecord:getHotkey(index) end
---@param id integer
---@return MemoryRecordHotkey?
function MemoryRecord:getHotkeyByID(id) end
function MemoryRecord:reinterpret() end
---@param keys integer[]
---@param action integer mrhToggleActivation=0 mrhActivate=3 mrhDeactivate=4 mrhSetValue=5 ...
---@param value string?
---@param description string?
---@return MemoryRecordHotkey
function MemoryRecord:createHotkey(keys, action, value, description) end
function MemoryRecord:disableWithoutExecute() end
function MemoryRecord:beginEdit() end
function MemoryRecord:endEdit() end

---@class MemoryRecordHotkey
---@field Owner MemoryRecord (readonly)
---@field Keys integer[]
---@field action integer
---@field value string
---@field ID integer (readonly)
---@field Active boolean
---@field Description string
---@field HotkeyString string (readonly)
---@field ActivateSound string
---@field DeactivateSound string
---@field OnHotkey fun(sender: any)?
---@field OnPostHotkey fun(sender: any)?
local MemoryRecordHotkey = {}
function MemoryRecordHotkey:doHotkey() end

-- AddressList (class methods — the global AddressList is already declared above)
---@class AddressList: Panel
---@field LoadedTableVersion integer (readonly)
---@field Count integer (readonly)
---@field SelCount integer (readonly)
---@field SelectedRecord MemoryRecord?
local AddressList_ = {}
---@return integer
function AddressList_:getCount() end
---@param index integer
---@return MemoryRecord
function AddressList_:getMemoryRecord(index) end
---@param description string
---@return MemoryRecord?
function AddressList_:getMemoryRecordByDescription(description) end
---@param description string
---@return MemoryRecord[]
function AddressList_:getMemoryRecordsWithDescription(description) end
---@param id integer
---@return MemoryRecord?
function AddressList_:getMemoryRecordByID(id) end
---@return MemoryRecord
function AddressList_:createMemoryRecord() end
---@return MemoryRecord[]
function AddressList_:getSelectedRecords() end
function AddressList_:doDescriptionChange() end
function AddressList_:doAddressChange() end
function AddressList_:doTypeChange() end
function AddressList_:doValueChange() end
---@return MemoryRecord?
function AddressList_:getSelectedRecord() end
---@param memrec MemoryRecord
function AddressList_:setSelectedRecord(memrec) end
function AddressList_:disableAllWithoutExecute() end
function AddressList_:rebuildDescriptionCache() end

-- ------------------------------------------------------------
-- MemScan / FoundList
-- ------------------------------------------------------------

---@class MemScan: Object
---@field LastScanWasRegionScan boolean
---@field LastScanValue string
---@field OnScanDone fun(memscan: MemScan)?
---@field OnlyOneResult boolean
---@field Result integer
local MemScan = {}
---@return MemScan
function getCurrentMemscan() end
---@param progressbar any?
---@return MemScan
function createMemScan(progressbar) end
function MemScan:scan() end
---@param scanoption string?
---@param vartype string?
---@param roundingtype string?
---@param input1 string?
---@param input2 string?
---@param startAddress integer?
---@param stopAddress integer?
---@param protectionflags string?
---@param alignmenttype integer?
---@param alignmentparam string?
---@param isHexadecimalInput boolean?
---@param isNotABinaryString boolean?
---@param isunicodescan boolean?
---@param iscasesensitive boolean?
function MemScan:firstScan(scanoption, vartype, roundingtype, input1, input2, startAddress, stopAddress, protectionflags, alignmenttype, alignmentparam, isHexadecimalInput, isNotABinaryString, isunicodescan, iscasesensitive) end
---@param scanoption string
---@param roundingtype string
---@param input1 string?
---@param input2 string?
---@param isHexadecimalInput boolean?
---@param isNotABinaryString boolean?
---@param isunicodescan boolean?
---@param iscasesensitive boolean?
---@param ispercentage boolean?
---@param savedResultName string?
function MemScan:nextScan(scanoption, roundingtype, input1, input2, isHexadecimalInput, isNotABinaryString, isunicodescan, iscasesensitive, ispercentage, savedResultName) end
function MemScan:newScan() end
function MemScan:waitTillDone() end
---@param name string
function MemScan:saveCurrentResults(name) end
---@return string[]
function MemScan:getSavedResultList() end
---@param name string
---@return any?
function MemScan:getSavedResultHandler(name) end
---@return FoundList?
function MemScan:getAttachedFoundlist() end
---@param state boolean
function MemScan:setOnlyOneResult(state) end
---@return integer? address
function MemScan:getOnlyResult() end
---@return {TotalAddressesToScan: integer, CurrentlyScanned: integer, ResultsFound: integer}
function MemScan:getProgress() end

---@class FoundList: Object
---@field Count integer
local FoundList = {}
---@param memscan MemScan
---@return FoundList
function createFoundList(memscan) end
function FoundList:initialize() end
function FoundList:deinitialize() end
---@return integer
function FoundList:getCount() end
---@param index integer
---@return string
function FoundList:getAddress(index) end
---@param index integer
---@return string
function FoundList:getValue(index) end

-- ------------------------------------------------------------
-- Dialogs
-- ------------------------------------------------------------

---@class CommonDialog: Component
---@field Title string
---@field OnShow fun(sender: any)?
---@field OnClose fun(sender: any)?
local CommonDialog = {}
---@return boolean
function CommonDialog:Execute() end

---@class ColorDialog: CommonDialog
---@field Color integer
local ColorDialog = {}
---@param owner any?
---@return ColorDialog
function createColorDialog(owner) end

---@class FindDialog: CommonDialog
---@field FindText string
---@field Options string[]
---@field OnFind fun(sender: any)?
local FindDialog = {}
---@param owner any?
---@return FindDialog
function createFindDialog(owner) end

---@class FileDialog: CommonDialog
---@field DefaultExt string
---@field Files Strings
---@field FileName string
---@field Filter string
---@field FilterIndex integer
---@field InitialDir string
local FileDialog = {}

---@class OpenDialog: FileDialog
---@field Options string[]
local OpenDialog = {}
---@param owner any?
---@return OpenDialog
function createOpenDialog(owner) end

---@class SaveDialog: OpenDialog
local SaveDialog = {}
---@param owner any?
---@return SaveDialog
function createSaveDialog(owner) end

---@class SelectDirectoryDialog: OpenDialog
local SelectDirectoryDialog = {}
---@param owner any?
---@return SelectDirectoryDialog
function createSelectDirectoryDialog(owner) end

-- ------------------------------------------------------------
-- LuaEngine / AutoAssembler forms
-- ------------------------------------------------------------

---@class TfrmLuaEngine: Form
---@field mOutput Memo
---@field mScript SynEdit
local TfrmLuaEngine = {}
---@return TfrmLuaEngine
function getLuaEngine() end
---@return TfrmLuaEngine
function createLuaEngine() end

---@class TfrmAutoInject: Form
---@field Assemblescreen SynEdit
---@field TabCount integer
local TfrmAutoInject = {}
---@param script string?
---@return TfrmAutoInject
function createAutoAssemblerForm(script) end
---@return integer tabIndex
function TfrmAutoInject:addTab() end
---@param index integer
function TfrmAutoInject:deleteTab(index) end

-- ------------------------------------------------------------
-- DisassemblerviewLine
-- ------------------------------------------------------------

---@class DisassemblerviewLine: Object
---@field Address integer Current address of this line
---@field Owner Disassemblerview The view that owns this line
---@field History OrderedList Navigation history
---@field OnDisassemblerViewOverride fun(address: integer, addressstring: string, bytestring: string, opcodestring: string, parameterstring: string, specialstring: string): string?, string?, string?, string?, string? Called when a line is about to be rendered; return overrides for each field
local DisassemblerviewLine = {}

-- ------------------------------------------------------------
-- Disassemblerview
-- ------------------------------------------------------------

---@class Disassemblerview: Panel
---@field SelectedAddress integer The currently selected address
---@field SelectedAddress2 integer The secondary selected address
---@field SelectionSize integer Size of the selected area
---@field TopAddress integer First address to show
---@field ShowJumplines boolean Whether jump arrows are shown
---@field HideFocusRect boolean If true the focus rectangle is hidden
---@field SpaceAboveLines integer
---@field SpaceBelowLines integer
---@field Osb Bitmap Background picture of the disassemblerview
---@field OnSelectionChange fun(sender: Disassemblerview, address: integer, address2: integer)? Called when the selection changes
---@field OnExtraLineRender fun(sender: DisassemblerviewLine, address: integer, aboveInstruction: boolean, selected: boolean): RasterImage?, integer?, integer?  Return an image plus optional x/y to draw above or below the instruction
local Disassemblerview = {}

-- ------------------------------------------------------------
-- Hexadecimal
-- ------------------------------------------------------------

---@class Hexadecimal: Panel
---@field Address integer Top address of the view
---@field HasSelection boolean True if something is selected
---@field SelectionStart integer
---@field SelectionStop integer
---@field History OrderedList Navigation history
---@field DisplayType string dtByte|dtByteDec|dtWord|dtWordDec|dtDword|dtDwordDec|dtQword|dtQwordDec|dtSingle|dtDouble
---@field OnAddressChange fun(hexview: Hexadecimal, address: integer)? Called when the top address changes
---@field OnByteSelect fun(hexview: Hexadecimal, address: integer, address2: integer)? Called when a byte range is selected
---@field OnCharacterRender fun(sender: Hexadecimal, address: integer, text: string): string Called when a character is rendered; return replacement text (slow)
---@field OnValueRender fun(sender: Hexadecimal, address: integer, text: string): string Called when a value is rendered; return replacement text (slow)
local Hexadecimal = {}

-- ------------------------------------------------------------
-- Memoryview
-- ------------------------------------------------------------

---@class Memoryview: Form
---@field DisassemblerView Disassemblerview The disassembler pane of this memory view
---@field HexadecimalView Hexadecimal The hex pane of this memory view
---@field memorypopup PopupMenu The popup/context menu of the memory view (undocumented)
local Memoryview = {}

--- Creates a new standalone memory view window.
--- Note: this window does NOT receive debug events.
--- Use getMemoryViewForm() to get the main memory view.
---@return Memoryview
function createMemoryView() end

-- ============================================================
-- CollectionItem / Collection
-- ============================================================

---@class CollectionItem: Object
---@field ID integer (readonly)
---@field Index integer (readonly)
local CollectionItem = {}

---@class Collection: Object
---@field Count integer
local Collection = {}
---@return integer
function Collection:getCount() end
---@param index integer
---@return CollectionItem
function Collection:getItem(index) end
function Collection:clear() end
---@param index integer
function Collection:delete(index) end

-- ============================================================
-- ListColumn / ListColumns
-- ============================================================

---@class ListColumn: CollectionItem
---@field AutoSize boolean
---@field Caption string
---@field MaxWidth integer
---@field MinWidth integer
---@field Width integer
---@field Visible boolean
local ListColumn = {}
function ListColumn:getAutosize() end
---@param v boolean
function ListColumn:setAutosize(v) end
function ListColumn:getCaption() end
---@param caption string
function ListColumn:setCaption(caption) end
function ListColumn:getMaxWidth() end
---@param width integer
function ListColumn:setMaxWidth(width) end
function ListColumn:getMinWidth() end
---@param width integer
function ListColumn:setMinWidth(width) end
function ListColumn:getWidth() end
---@param width integer
function ListColumn:setWidth(width) end

---@class ListColumns: Collection
local ListColumns = {}
--- Adds a new column and returns it
---@return ListColumn
function ListColumns:add() end
---@param index integer
---@return ListColumn
function ListColumns:getColumn(index) end
---@param index integer
---@param col ListColumn
function ListColumns:setColumn(index, col) end

-- ============================================================
-- HeaderSection / HeaderSections
-- ============================================================

---@class HeaderSection: CollectionItem
---@field Alignment string taLeftJustify|taRightJustify|taCenter
---@field ImageIndex integer
---@field MaxWidth integer
---@field MinWidth integer
---@field Text string
---@field Width integer
---@field Visible boolean
---@field OriginalIndex integer (readonly)
local HeaderSection = {}

---@class HeaderSections: Collection
local HeaderSections = {}
---@return HeaderSection
function HeaderSections:add() end
---@param index integer
---@return HeaderSection
function HeaderSections:insert(index) end
---@param index integer
function HeaderSections:delete(index) end

-- ============================================================
-- structure / StructureElement / StructureFrm / structColumn / structGroup
-- ============================================================

---@class StructureElement: Object
---@field Owner structure (readonly)
---@field Offset integer
---@field Name string
---@field Vartype integer See vtByte etc. constants
---@field CustomType any CustomType object if vtCustom
---@field CustomTypeName string
---@field DisplayMethod string dtUnsignedInteger|dtSignedInteger|dtHexadecimal
---@field ChildStruct structure?
---@field ChildStructStart integer
---@field Bytesize integer (readonly for basic types)
---@field BackgroundColor integer
local StructureElement = {}
---@return structure
function StructureElement:getOwnerStructure() end
function StructureElement:getOffset() end
---@param offset integer
function StructureElement:setOffset(offset) end
function StructureElement:getName() end
---@param name string
function StructureElement:setName(name) end
function StructureElement:getVartype() end
---@param vartype integer
function StructureElement:setVartype(vartype) end
---@param address integer
---@return any
function StructureElement:getValue(address) end
---@param address integer
---@param value any
function StructureElement:setValue(address, value) end
---@param baseaddress integer
---@return any
function StructureElement:getValueFromBase(baseaddress) end
---@param baseaddress integer
---@param value any
function StructureElement:setValueFromBase(baseaddress, value) end
function StructureElement:getChildStruct() end
---@param s structure
function StructureElement:setChildStruct(s) end
function StructureElement:getChildStructStart() end
---@param offset integer
function StructureElement:setChildStructStart(offset) end
function StructureElement:getBytesize() end
---@param size integer
function StructureElement:setBytesize(size) end

---@class structure: Object
---@field Name string
---@field Size integer (readonly)
---@field Count integer (readonly)
local structure = {}
function structure:getName() end
---@param name string
function structure:setName(name) end
---@param index integer
---@return StructureElement
function structure:getElement(index) end
---@param offset integer
---@return StructureElement
function structure:getElementByOffset(offset) end
---@return StructureElement
function structure:addElement() end
---@param baseaddresstoguessfrom integer
---@param offset integer
---@param size integer
function structure:autoGuess(baseaddresstoguessfrom, offset, size) end
---@param address integer
---@param changeName boolean?
function structure:fillFromDotNetAddress(address, changeName) end
function structure:beginUpdate() end
function structure:endUpdate() end
function structure:addToGlobalStructureList() end
function structure:removeFromGlobalStructureList() end

---@return integer
function getStructureCount() end
---@param index integer
---@return structure
function getStructure(index) end
---@param name string
---@return structure
function createStructure(name) end
---@param name string
---@return structure?
function createStructureFromName(name) end

---@class structColumn
---@field Address integer
---@field AddressText string
---@field Focused boolean
local structColumn = {}
function structColumn:focus() end

---@class structGroup
---@field name string
---@field box GroupBox
---@field columnCount integer
local structGroup = {}
---@return structColumn
function structGroup:addColumns() end

---@class StructureFrm: Form
---@field MainStruct structure
---@field ColumnCount integer
---@field GroupCount integer
local StructureFrm = {}
---@param address integer
---@param groupname string?
---@param structurename string?
---@return StructureFrm
function createStructureForm(address, groupname, structurename) end
---@return StructureFrm[]
function enumStructureForms() end
function StructureFrm:structChange() end
---@return structColumn
function StructureFrm:addColumn() end
---@return structGroup
function StructureFrm:addGroup() end
---@param index integer
---@return structColumn
function StructureFrm:getColumn(index) end
---@param index integer
---@return structGroup
function StructureFrm:getGroup(index) end
---@return StructureElement?, {struct: structure, element: StructureElement}[]
function StructureFrm:getSelectedStructElement() end

-- ============================================================
-- DBK (kernel driver) functions
-- ============================================================

---@return boolean loaded
function dbk_initialize() end
---@return boolean
function dbk_initialized() end
function dbk_useKernelmodeOpenProcess() end
function dbk_useKernelmodeProcessMemoryAccess() end
function dbk_useKernelmodeQueryMemoryRegions() end
function dbk_usePhysicalMemoryAccess() end
---@param state boolean
function dbk_setSaferPhysicalMemoryScanning(state) end
---@param address integer
---@param size integer
---@return integer[]
function dbk_readPhysicalMemory(address, size) end
---@param address integer
---@param bytetable integer[]
---@return boolean
function dbk_writePhysicalMemory(address, bytetable) end
---@param processid integer
---@return integer eprocessAddress
function dbk_getPEProcess(processid) end
---@param threadid integer
---@return integer ethreadAddress
function dbk_getPEThread(threadid) end
---@return integer cr0
function dbk_getCR0() end
---@return integer cr3
function dbk_getCR3() end
---@return integer cr4
function dbk_getCR4() end
---@param address integer
---@return integer? physical nil if not mapped
function dbk_getPhysicalAddress(address) end
---@param state boolean
function dbk_writesIgnoreWriteProtection(state) end
---@param msr integer
---@return integer
function dbk_readMSR(msr) end
---@param msr integer
---@param value integer
function dbk_writeMSR(msr, value) end
---@param address integer
---@param parameter integer?
---@return integer
function dbk_executeKernelMemory(address, parameter) end

---@param size integer
---@return integer address
function allocateKernelMemory(size) end
---@param address integer
function freeKernelMemory(address) end

---@param cr3 integer
---@param address integer
---@return integer? physical nil if not paged
function getPhysicalAddressCR3(cr3, address) end
---@param cr3 integer
---@param address integer
---@param size integer
---@return integer[]? nil on failure
function readProcessMemoryCR3(cr3, address, size) end
---@param cr3 integer
---@param address integer
---@param bytetable integer[]
function writeProcessMemoryCR3(cr3, address, bytetable) end

---@param address integer
---@param size integer
---@param frompid integer?
---@param topid integer?
---@return integer address
---@return integer mdl
function mapMemory(address, size, frompid, topid) end
---@param address integer
---@param mdl integer
function unmapMemory(address, mdl) end

-- ============================================================
-- DBVM (virtual machine) functions
-- ============================================================

---@param offloados boolean?
---@param reason string?
---@return boolean
function dbvm_initialize(offloados, reason) end
---@return boolean
function dbvm_initialized() end
---@param key1 integer
---@param key2 integer
---@param key3 integer
---@return boolean
function dbvm_setKeys(key1, key2, key3) end
---@return integer freemem
---@return integer fullpages
function dbvm_getMemory() end
---@param pagecount integer
function dbvm_addMemory(pagecount) end
---@param msr integer
---@return integer
function dbvm_readMSR(msr) end
---@param msr integer
---@param value integer
function dbvm_writeMSR(msr, value) end
---@return integer cr4
function dbvm_getCR4() end
---@param address integer
---@param size integer
---@return integer[]
function dbvm_readPhysicalMemory(address, size) end
---@param address integer
---@param bytetable integer[]
function dbvm_writePhysicalMemory(address, bytetable) end
---@param physicalAddress integer
---@param bytesize integer?
---@param options any?
---@param internalentrycount integer?
---@param optional1 any?
---@param optional2 any?
---@return integer watchID
function dbvm_watch_writes(physicalAddress, bytesize, options, internalentrycount, optional1, optional2) end
---@param physicalAddress integer
---@param bytesize integer?
---@param options any?
---@param internalentrycount integer?
---@param optional1 any?
---@param optional2 any?
---@return integer watchID
function dbvm_watch_reads(physicalAddress, bytesize, options, internalentrycount, optional1, optional2) end
---@param physicalAddress integer
---@param bytesize integer?
---@param options any?
---@param internalentrycount integer?
---@param optional1 any?
---@param optional2 any?
---@return integer watchID
function dbvm_watch_executes(physicalAddress, bytesize, options, internalentrycount, optional1, optional2) end
---@param watchID integer
---@return table[] entries
function dbvm_watch_retrievelog(watchID) end
---@param watchID integer
function dbvm_watch_disable(watchID) end
---@param physicalbase integer
---@param virtualbase integer?
function dbvm_cloak_activate(physicalbase, virtualbase) end
---@param physicalbase integer
function dbvm_cloak_deactivate(physicalbase) end
---@param physicalbase integer
---@return integer[]? 4096-byte table, nil on failure
function dbvm_cloak_readOriginal(physicalbase) end
---@param physicalbase integer
---@param bytetable integer[] 4096 bytes
function dbvm_cloak_writeOriginal(physicalbase, bytetable) end
---@param physicaladdress integer
---@param changereginfo any
---@param virtualaddress integer?
---@return boolean
function dbvm_changeregonbp(physicaladdress, changereginfo, virtualaddress) end
---@param physicaladdress integer
function dbvm_removechangeregonbp(physicaladdress) end
---@param physicalAddress integer
---@param stepcount integer
---@param virtualAddress integer
---@param secondaryoptions table?
function dbvm_traceonbp(physicalAddress, stepcount, virtualAddress, secondaryoptions) end
---@return integer status 0=none 1=configured 2=started 3=done
---@return integer count
---@return integer maxcount
function dbvm_traceonbp_getstatus() end
function dbvm_traceonbp_stoptrace() end
---@param pa integer
---@param force boolean
function dbvm_traceonbp_remove(pa, force) end
---@return table[] traceentries
function dbvm_traceonbp_retrievelog() end
---@return integer count Available breakpoint slots
function dbvm_bp_getBrokenThreadListSize() end
---@param id integer
---@return table
function dbvm_bp_getBrokenThreadEventShort(id) end
---@param id integer
---@return table
function dbvm_bp_getBrokenThreadEventFull(id) end
---@param id integer
---@param state any
function dbvm_bp_setBrokenThreadEventFull(id, state) end
---@param id integer
---@param continueMethod integer 0=run 1=step into
function dbvm_bp_resumeBrokenThread(id, continueMethod) end
---@param threadEvent any
---@return integer? processid nil on failure
---@return integer|string threadid
function dbvm_bp_getProcessAndThreadIDFromEvent(threadEvent) end
function dbvm_log_cr3_start() end
---@return integer[] cr3values
function dbvm_log_cr3_stop() end
---@param speed number
function dbvm_speedhack_setSpeed(speed) end
---@param enabled boolean
---@param timeout integer?
function dbvm_setTSCAdjust(enabled, timeout) end
function dbvm_startcpuidlog() end
function dbvm_stopcpuidlog() end
---@return {cr3: integer, rip: integer}[]
function dbvm_getcpuidlog() end
---@param cs integer
---@param ss integer
---@param ds integer
---@param es integer
---@param fs integer
---@param gs integer
---@return integer
function dbvm_changeselectors(cs, ss, ds, es, fs, gs) end

-- ============================================================
-- API pointer callbacks
-- ============================================================

---@param f fun(functionid: integer)
function onAPIPointerChange(f) end

---@param functionid integer
---@param address integer
function setAPIPointer(functionid, address) end

-- ============================================================
-- GDB interface
-- ============================================================

---@return boolean
function gdb_connected() end
---@return boolean
function gdb_stopped() end
---@return integer
function gdb_getCurrentInstructionpointer() end
---@param address integer
function gdb_setCurrentInstructionpointer(address) end
---@param command string
---@return string result
function gdb_command(command) end
function gdb_break() end
---@return string reason
function gdb_getcurrentstopreason() end

-- ============================================================
-- CELUA (LuaClient.dll IPC stubs — called from target process)
-- ============================================================

---@param name string Pipe name used by openLuaServer()
---@return boolean
function CELUA_Initialize(name) end

---@param luacode string Function body code
---@param parameter integer
---@return integer returnValue
function CELUA_ExecuteFunction(luacode, parameter) end

---@param luacode string Function body code
---@param parameter integer
---@return integer returnValue Async — runs in server thread
function CELUA_ExecuteFunctionAsync(luacode, parameter) end

---@param functionname string
---@return integer refid
function CELUA_GetFunctionReferenceFromName(functionname) end

---@param refid integer
---@param paramcount integer
---@param parameters integer[] Pointer array (ptrs as integers)
---@param async boolean
---@return integer returnValue
function CELUA_ExecuteFunctionByReference(refid, paramcount, parameters, async) end

---@param name string Pipe name for the LuaClient.dll to connect to
function openLuaServer(name) end

-- ============================================================
-- D3DHOOK
-- ============================================================

---@class D3DHook_Texture: Object
---@field Height integer (readonly)
---@field Width integer (readonly)
local D3DHook_Texture = {}
---@param picture any Picture or Bitmap
function D3DHook_Texture:loadTextureByPicture(picture) end

---@class D3DHook_FontMap: D3DHook_Texture
local D3DHook_FontMap = {}
---@param font Font
function D3DHook_FontMap:changeFont(font) end
---@param text string
---@return integer pixels
function D3DHook_FontMap:getTextWidth(text) end

---@class D3DHook_RenderObject: Object
---@field X number X position on screen
---@field Y number Y position on screen
---@field CenterX number Rotation center X inside object
---@field CenterY number Rotation center Y inside object
---@field Rotation number Degrees (0=360=no rotation)
---@field Alphablend number 1.0=fully visible 0.0=invisible
---@field Visible boolean
---@field ZOrder integer Higher = in front
local D3DHook_RenderObject = {}

---@class D3DHook_Sprite: D3DHook_RenderObject
---@field Width integer Pixel width (default = texture width)
---@field Height integer Pixel height (default = texture height)
---@field Texture D3DHook_Texture
local D3DHook_Sprite = {}

---@class D3Dhook_TextContainer: D3DHook_RenderObject
---@field FontMap D3DHook_FontMap
---@field Text string
local D3Dhook_TextContainer = {}

---@class D3DHOOK: Object
---@field Width integer Screen width (readonly)
---@field Height integer Screen height (readonly)
---@field DisabledZBuffer boolean Disable depth testing
---@field WireframeMode boolean Render as wireframe
---@field MouseClip boolean Clip mouse to game window
---@field OnClick fun(sprite: D3DHook_Sprite, x: integer, y: integer)? Called when a sprite is clicked
---@field OnKeyDown fun(vkey: integer, char: string): boolean? Return false to suppress key event in game
local D3DHOOK = {}

---@param textureandcommandlistsize integer? Default 16MB
---@param hookmessages boolean? Default true
---@return D3DHOOK
function createD3DHook(textureandcommandlistsize, hookmessages) end
function D3DHOOK:beginUpdate() end
function D3DHOOK:endUpdate() end
---@param virtualkey integer e.g. 0xC0 for tilde
function D3DHOOK:enableConsole(virtualkey) end
---@param filename string
---@return D3DHook_Texture
function D3DHOOK:createTexture(filename) end
---@param font Font
---@return D3DHook_FontMap
function D3DHOOK:createFontmap(font) end
---@param texture D3DHook_Texture
---@return D3DHook_Sprite
function D3DHOOK:createSprite(texture) end
---@param fontmap D3DHook_FontMap
---@param x number
---@param y number
---@param text string
---@return D3Dhook_TextContainer
function D3DHOOK:createTextContainer(fontmap, x, y, text) end

-- ============================================================
-- Disassembler (standalone object)
-- ============================================================

---@class DisassemblerData
---@field address integer
---@field opcode string
---@field parameters string
---@field description string
---@field commentsoverride string?
---@field bytes integer[]
---@field modrmValueType integer dvtNone=0 dvtAddress=1 dvtValue=2
---@field modrmValue integer
---@field parameterValueType integer
---@field parameterValue integer
---@field isJump boolean
---@field isCall boolean
---@field isRet boolean
---@field isRep boolean
---@field isConditionalJump boolean

---@class Disassembler: Object
---@field LastDisassembleData DisassemblerData
---@field syntaxhighlighting boolean
---@field OnDisassembleOverride fun(sender: Disassembler, address: integer, data: DisassemblerData): string?, string?
---@field OnPostDisassemble fun(sender: Disassembler, address: integer, data: DisassemblerData, result: string, description: string): string, string
local Disassembler = {}

---@return Disassembler
function createDisassembler() end
---@return Disassembler
function getDefaultDisassembler() end
---@return Disassembler
function getVisibleDisassembler() end

---@param f fun(sender: Disassembler, address: integer, data: DisassemblerData): string?, string?
---@return integer id
function registerGlobalDisassembleOverride(f) end
---@param id integer
function unregisterGlobalDisassembleOverride(id) end

---@param address integer
---@return string opcode
function Disassembler:disassemble(address) end
---@return string
function Disassembler:decodeLastParametersToString() end
---@return DisassemblerData
function Disassembler:getLastDisassembleData() end

-- ============================================================
-- DissectCode
-- ============================================================

---@class DissectCode: Object
local DissectCode = {}

---@return DissectCode
function getDissectCode() end
function DissectCode:clear() end
---@param modulenameOrBase string|integer Module name or base address
---@param size integer? Required when passing base address
function DissectCode:dissect(modulenameOrBase, size) end
---@param fromAddress integer
---@param toAddress integer
---@param reftype string jtCall|jtUnconditional|jtConditional|jtMemory
---@param isstring boolean?
function DissectCode:addReference(fromAddress, toAddress, reftype, isstring) end
---@param fromAddress integer
---@param toAddress integer
function DissectCode:deleteReference(fromAddress, toAddress) end
---@param address integer
---@return {address: integer, reftype: string}[]
function DissectCode:getReferences(address) end
---@return {address: integer, text: string}[]
function DissectCode:getReferencedStrings() end
---@return integer[]
function DissectCode:getReferencedFunctions() end
---@param filename string
function DissectCode:saveToFile(filename) end
---@param filename string
function DissectCode:loadFromFile(filename) end

-- ============================================================
-- RIPRelativeScanner
-- ============================================================

---@class RIPRelativeScanner: Object
---@field Count integer Number of matching instructions found
local RIPRelativeScanner = {}

---@param startOrModule integer|string Start address or module name
---@param stopOrInclude integer|boolean Stop address, or includejumpsandcalls when module name given
---@param includejumpsandcalls boolean?
---@return RIPRelativeScanner
function createRipRelativeScanner(startOrModule, stopOrInclude, includejumpsandcalls) end
---@param index integer 1-based
---@return integer address Address of the RIP-relative offset field
function RIPRelativeScanner:getAddress(index) end

-- ============================================================
-- LuaPipe / LuaPipeClient / LuaPipeServer
-- ============================================================

---@class LuaPipe: Object
---@field Connected boolean
---@field Timeout integer Seconds; 0=never
---@field OnTimeout fun(sender: LuaPipe)?
---@field OnError fun(sender: LuaPipe)?
---@field OnAboutToTimeout fun(sender: LuaPipe): boolean?
local LuaPipe = {}
function LuaPipe:lock() end
function LuaPipe:unlock() end
---@param stream Stream
---@param size integer
function LuaPipe:readIntoStream(stream, size) end
---@param stream Stream
---@param size integer?
function LuaPipe:writeFromStream(stream, size) end
---@param bytetable integer[]
---@param size integer?
---@return integer? bytessent
function LuaPipe:writeBytes(bytetable, size) end
---@param size integer
---@return integer[]? nil on failure
function LuaPipe:readBytes(size) end
---@return number?
function LuaPipe:readDouble() end
---@return number?
function LuaPipe:readFloat() end
---@return integer?
function LuaPipe:readQword() end
---@param count integer
---@return integer[]?
function LuaPipe:readQwords(count) end
---@return integer?
function LuaPipe:readDword() end
---@param count integer
---@return integer[]?
function LuaPipe:readDwords(count) end
---@return integer?
function LuaPipe:readWord() end
---@param count integer
---@return integer[]?
function LuaPipe:readWords(count) end
---@return integer?
function LuaPipe:readByte() end
---@param size integer
---@return string?
function LuaPipe:readString(size) end
---@param size integer
---@return string?
function LuaPipe:readWideString(size) end
---@param v number
---@return integer? bytessent
function LuaPipe:writeDouble(v) end
---@param v number
---@return integer? bytessent
function LuaPipe:writeFloat(v) end
---@param v integer
---@return integer? bytessent
function LuaPipe:writeQword(v) end
---@param v integer
---@return integer? bytessent
function LuaPipe:writeDword(v) end
---@param v integer
---@return integer? bytessent
function LuaPipe:writeWord(v) end
---@param v integer
---@return integer? bytessent
function LuaPipe:writeByte(v) end
---@param str string
---@param include0terminator boolean?
---@return integer? bytessent
function LuaPipe:writeString(str, include0terminator) end
---@param str string
---@param include0terminator boolean?
---@return integer? bytessent
function LuaPipe:writeWideString(str, include0terminator) end

---@class LuaPipeClient: LuaPipe
local LuaPipeClient = {}

---@param pipename string
---@param timeout integer? Milliseconds
---@return LuaPipeClient?
function connectToPipe(pipename, timeout) end

---@class LuaPipeServer: LuaPipe
---@field valid boolean True if pipe was created successfully
---@field handle integer Handle of the server-side pipe
local LuaPipeServer = {}
--- Blocks until a client connects
function LuaPipeServer:acceptConnection() end

---@param pipename string
---@param inputsize integer?
---@param outputsize integer?
---@param maxInstanceCount integer?
---@return LuaPipeServer
function createPipe(pipename, inputsize, outputsize, maxInstanceCount) end

-- ============================================================
-- SymbolList
-- ============================================================

---@class SymbolList: Object
---@field PID integer
---@field Name string
local SymbolList = {}
---@return SymbolList
function createSymbolList() end
---@return SymbolList
function getMainSymbolList() end
---@return SymbolList[]
function enumRegisteredSymbolLists() end
function SymbolList:clear() end
---@param address integer
---@return {modulename: string, searchkey: string, address: integer, symbolsize: integer}?
function SymbolList:getSymbolFromAddress(address) end
---@param searchkey string
---@return {modulename: string, searchkey: string, address: integer, symbolsize: integer}?
function SymbolList:getSymbolFromString(searchkey) end
---@param modulename string
---@param modulepath string
---@param address integer
---@param size integer
---@param is64bit boolean
function SymbolList:addModule(modulename, modulepath, address, size, is64bit) end
---@param modulenameOrAddress string|integer
function SymbolList:deleteModule(modulenameOrAddress) end
---@param modulename string
---@param searchkey string
---@param address integer
---@param symbolsize integer
---@param skipAddressToSymbolLookup boolean?
---@param extradata {returntype: string, parameters: string}?
function SymbolList:addSymbol(modulename, searchkey, address, symbolsize, skipAddressToSymbolLookup, extradata) end
---@param searchkeyOrAddress string|integer
function SymbolList:deleteSymbol(searchkeyOrAddress) end
function SymbolList:register() end
function SymbolList:unregister() end
---@return table[]
function SymbolList:getModuleList() end
---@return table
function SymbolList:getSymbolList() end

-- ============================================================
-- PageControl / TabSheet
-- ============================================================

---@class TabSheet: WinControl
---@field TabIndex integer Index in the owning page control
local TabSheet = {}

---@class PageControl: WinControl
---@field ShowTabs boolean
---@field TabIndex integer
---@field ActivePage TabSheet
---@field PageCount integer
local PageControl = {}
---@param owner WinControl
---@return PageControl
function createPageControl(owner) end
---@return TabSheet
function PageControl:addTab() end
---@param index integer
---@return {Left: integer, Top: integer, Right: integer, Bottom: integer}
function PageControl:tabRect(index) end
---@param index integer
---@return TabSheet
function PageControl:getPage(index) end

-- ============================================================
-- Internet
-- ============================================================

---@class Internet: Object
---@field Header string Additional header for next getURL request
local Internet = {}
---@param clientname string
---@return Internet
function getInternet(clientname) end
---@param path string
---@return string? nil on failure
function Internet:getURL(path) end
---@param path string
---@param urlencodeddata string
---@return string? result
function Internet:postURL(path, urlencodeddata) end

-- ============================================================
-- CustomType
-- ============================================================

---@class CustomType: Object
---@field name string
---@field functiontypename string
---@field CustomTypeType integer
---@field script string
---@field scriptUsesFloat boolean
local CustomType = {}
---@param typename string
---@param bytecount integer
---@param bytestovaluefunction fun(...): integer
---@param valuetobytesfunction fun(value: integer): ...
---@param isFloat boolean?
---@param isString boolean?
---@return CustomType
function registerCustomTypeLua(typename, bytecount, bytestovaluefunction, valuetobytesfunction, isFloat, isString) end
---@param script string
---@return CustomType
function registerCustomTypeAutoAssembler(script) end
---@param typename string
---@return CustomType?
function getCustomType(typename) end
---@param bytetable integer[]
---@param address integer?
---@return any
function CustomType:byteTableToValue(bytetable, address) end
---@param value any
---@param address integer?
---@return integer[]
function CustomType:valueToByteTable(value, address) end

-- ============================================================
-- Settings
-- ============================================================

---@class Settings: Object
---@field Path string? Current subkey (nil=CE main settings)
local Settings = {}
function reloadSettingsFromRegistry() end
---@param path string?
---@param nilResults boolean?
---@return Settings
function getSettings(path, nilResults) end
---@param name string
---@param stream Stream
function Settings:getBinaryValue(name, stream) end
---@param name string
---@param stream Stream
---@param size integer?
function Settings:setBinaryValue(name, stream, size) end

-- ============================================================
-- TFrmTracer
-- ============================================================

---@class TFrmTracer: Form
---@field Count integer Number of trace entries
---@field SelectionCount integer Number of selected entries
local TFrmTracer = {}
---@param index integer 0-based
---@return {address: integer, instruction: string, instructionSize: integer, referencedAddress: integer, referencedData: integer[], context: table}
function TFrmTracer:getEntry(index) end

-- ============================================================
-- Misc remaining global functions
-- ============================================================

---@param attachwindow Form
---@param hasclosebutton boolean
---@param width integer
---@param height integer
---@param position integer 0=Top 1=Right 2=Bottom 3=Left
---@param yoururl string?
---@param extraparameters string?
---@param percentageshown number?
function supportCheatEngine(attachwindow, hasclosebutton, width, height, position, yoururl, extraparameters, percentageshown) end

function fuckCheatEngine() end

