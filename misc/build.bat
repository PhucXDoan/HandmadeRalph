@echo off
REM @TODO@ Switch to clang!

set COMMON_COMPILER_FLAGS=/nologo /std:c++20 /GR- /EHsc /EHa- /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DSRC_DIR="\"W:/src/\"" /permissive- /W4 /Wall /wd4201 /wd4668 /wd5045 /wd4711 /wd4820 /wd4710 /wd5219 /wd5039 /wd4191 /wd4061
set COMMON_LINKER_FLAGS=/opt:ref /incremental:no

set DEBUG_COMPILER_FLAGS=%COMMON_COMPILER_FLAGS% /MTd /Od /Oi /Z7 /DDEBUG=1 /wd4505 /wd4100 /wd4514 /wd4189
set DEBUG_LINKER_FLAGS=%COMMON_LINKER_FLAGS% /debug:full

IF not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	IF exist W:\src\META\ (
		del W:\src\META\ /S /Q >nul
	)

	cl W:\src\metaprogram.cpp %DEBUG_COMPILER_FLAGS% /WX /link %DEBUG_LINKER_FLAGS% shell32.lib
	if %ERRORLEVEL% neq 0 (
		echo Metaprogram compilation failed.
		goto ABORT
	)

	metaprogram.exe
	if %ERRORLEVEL% neq 0 (
		echo Metaprogram execution failed.
		goto ABORT
	)

	del *.pdb >nul 2> nul
	echo 0 > LOCK.temp
	cl W:\src\HandmadeRalph.cpp %DEBUG_COMPILER_FLAGS% /MTd /LD /link %DEBUG_LINKER_FLAGS% /PDB:HandmadeRalph_%RANDOM%.pdb /export:PlatformUpdate /export:PlatformSound
	del LOCK.temp

	copy nul HandmadeRalph.exe >nul 2>&1
	if %ERRORLEVEL% neq 0 (
		goto ABORT
	)
	cl W:\src\HandmadeRalph_win32.cpp %DEBUG_COMPILER_FLAGS% /FeHandmadeRalph.exe /link %DEBUG_LINKER_FLAGS% /subsystem:windows user32.lib gdi32.lib winmm.lib dxgi.lib

	:ABORT
	del *.obj *.lib *.exp >nul 2> nul
popd
