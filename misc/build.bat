@echo off
setlocal ENABLEDELAYEDEXPANSION

set DEBUG=1
set WARNINGS=/W4 /Wall /wd4201 /wd5219 /wd4668 /wd5045 /wd4711
set DEBUG_WARNINGS=%WARNINGS% /wd4505 /wd4100 /wd4101 /wd4514 /wd4189 /wd4191 /wd4820 /wd4710
set LIBRARIES=user32.lib gdi32.lib winmm.lib

IF NOT EXIST W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	if %DEBUG% equ 0 (
		echo Release build
	) else (
		echo Debug build
		cl /nologo /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DSRC_DIR="\"W:/src/\"" /std:c++20 /Od /Oi /DDEBUG=1 /Z7 /MTd /GR- /EHsc /EHa- %DEBUG_WARNINGS% /permissive- /LD                  W:\src\HandmadeRalph.cpp       /link %LIBRARIES% /debug:FULL /opt:ref /incremental:no /export:PlatformUpdate /export:PlatformSound
		cl /nologo /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DSRC_DIR="\"W:/src/\"" /std:c++20 /Od /Oi /DDEBUG=1 /Z7 /MTd /GR- /EHsc /EHa- %DEBUG_WARNINGS% /permissive- /FeHandmadeRalph.exe W:\src\HandmadeRalph_win32.cpp /link %LIBRARIES% /debug:FULL /opt:ref /incremental:no /subsystem:windows
	)
popd

REM C4201       : "nonstandard extension used : nameless struct/union"
REM C5219       : "implicit conversion from 'type-1' to 'type-2', possible loss of data"
REM C4668       : "'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'"
REM C5045       : "Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified"
REM C4711       : "function 'function' selected for inline expansion"

REM C4505       : "'function' : unreferenced local function has been removed"
REM C4100       : "'identifier' : unreferenced formal parameter"
REM C4101       : "'identifier' : unreferenced local variable"
REM C4514       : "'function' : unreferenced inline function has been removed"
REM C4189       : "'identifier' : local variable is initialized but not referenced"
REM C4191       : "'operator/operation' : unsafe conversion from 'type of expression' to 'type required'"
REM C4710       : "'function' : function not inlined"

REM C4820       : "'bytes' bytes padding added after construct 'member_name'"
REM /Z7         : "The /Z7 option produces object files that also contain full symbolic debugging information for use with the debugger."
REM /MTd        : "Defines _DEBUG and _MT. This option also causes the compiler to place the library name LIBCMTD.lib into the .obj file so that the linker will use LIBCMTD.lib to resolve external symbols."
REM /GR         : "Adds code to check object types at run time."
REM /EHsc       : "When used with /EHs, the compiler assumes that functions declared as extern "C" never throw a C++ exception."
REM /EHa        : "Enables standard C++ stack unwinding."
REM /permissive : "Specify standards conformance mode to the compiler."
REM /LD         : "Creates a DLL. Passes the /DLL option to the linker. The linker looks for, but does not require, a DllMain function. If you do not write a DllMain function, the linker inserts a DllMain function that returns TRUE."
