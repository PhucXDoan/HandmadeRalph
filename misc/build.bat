@echo off
setlocal ENABLEDELAYEDEXPANSION

set WARNINGS=/W4 /Wall /wd4201 /wd5219 /wd4668 /wd5045 /wd4711 /wd4505 /wd4100 /wd4101 /wd4514 /wd4189 /wd4191 /wd4820 /wd4710
set COMMON_COMPILER_FLAGS=/nologo /GR- /EHsc /EHa- /permissive- /std:c++20 /Od /Oi /Z7 /MTd /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DSRC_DIR="\"W:/src/\"" /DDEBUG=1
set COMMON_LINKER_FLAGS=/opt:ref /incremental:no /debug:full

IF NOT EXIST W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	REM del W:\src\meta\ /Q /S
	cl %COMMON_COMPILER_FLAGS% /WX                  W:\src\metaprogram.cpp         /link %COMMON_LINKER_FLAGS% shell32.lib
	if !ERRORLEVEL! neq 0 (
		echo Metaprogram compilation failed.
		goto ABORT
	)

	REM metaprogram.exe
	REM if !ERRORLEVEL! neq 0 (
	REM 	echo Metaprogram execution failed.
	REM 	goto ABORT
	REM )

	REM del *.pdb > NUL 2> NUL
	REM echo 0 > LOCK.temp
	REM cl %COMMON_COMPILER_FLAGS%  /LD                  W:\src\HandmadeRalph.cpp      /link %COMMON_LINKER_FLAGS% /PDB:HandmadeRalph_%RANDOM%.pdb /export:PlatformUpdate /export:PlatformSound
	REM del LOCK.temp

	REM copy NUL HandmadeRalph.exe > NUL 2>&1
	REM if !ERRORLEVEL! neq 0 (
	REM 	goto ABORT
	REM )
	REM cl %COMMON_COMPILER_FLAGS% /FeHandmadeRalph.exe W:\src\HandmadeRalph_win32.cpp /link %COMMON_LINKER_FLAGS% /subsystem:windows user32.lib gdi32.lib winmm.lib dxgi.lib

	:ABORT
	del *.obj *.lib *.exp > NUL 2> NUL
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
