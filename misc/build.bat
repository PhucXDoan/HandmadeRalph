@echo off
setlocal EnableDelayedExpansion

set COMMON_COMPILER_FLAGS=^
	-std=c++20 -Xlinker /incremental:no -pedantic -Weverything -ferror-limit=1 -Xlinker /debug^
	-DDATA_DIR="\"W:/data/\""^
	-DEXE_DIR="\"W:/build/\""^
	-DSRC_DIR="\"W:/src/\""^
	-Wno-c++17-extensions                        -Wno-c++20-designator -Wno-c++98-compat         -Wno-c++98-compat-pedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-duplicate-enum^
	-Wno-deprecated-copy-with-user-provided-dtor -Wno-missing-braces   -Wno-gnu-anonymous-struct -Wno-nested-anon-types     -Wno-cast-function-type                -Wno-disabled-macro-expansion^
	-Wno-switch-enum -Wno-zero-as-null-pointer-constant

set DEBUG_COMPILER_FLAGS=%COMMON_COMPILER_FLAGS% -O0 -g -gcodeview -DDEBUG=1 -Wno-unused-parameter -Wno-unused-command-line-argument -Wno-unused-function -Wno-unused-variable

if not exist W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
	set t0=%time: =0%

	IF exist W:\src\META\ (
		del W:\src\META\ /S /Q >nul
	)

	clang %DEBUG_COMPILER_FLAGS% -E -CC W:\src\metaprogram.cpp > metaprogram.new.cpp
	fc /b metaprogram.new.cpp metaprogram.old.cpp > nul 2>&1
	if !ERRORLEVEL! neq 0 (
		echo :: metaprogam.cpp
		clang -o metaprogram.exe %DEBUG_COMPILER_FLAGS% -Werror W:\src\metaprogram.cpp -l shell32.lib
		if !ERRORLEVEL! neq 0 (
			echo :: Metaprogram compilation failed
			del metaprogram.new.cpp
			del metaprogram.old.cpp
			goto ABORT
		) else (
			move /y metaprogram.new.cpp metaprogram.old.cpp > nul
		)
	)

	echo :: metaprogram.exe
	metaprogram.exe
	if !ERRORLEVEL! neq 0 (
		echo :: Metaprogram execution failed
		goto ABORT
	)

	echo :: HandmadeRalph.cpp
	del HandmadeRalph_*.pdb
	echo > LOCK.temp
	clang -o HandmadeRalph.dll %DEBUG_COMPILER_FLAGS% W:\src\HandmadeRalph.cpp -shared -Xlinker /PDB:HandmadeRalph_%RANDOM%.pdb -Xlinker /export:PlatformUpdate -Xlinker /export:PlatformSound
	del LOCK.temp
	if !ERRORLEVEL! neq 0 (
		echo :: HandmadeRalph compilation failed
		goto ABORT
	)

	clang %DEBUG_COMPILER_FLAGS% -E -CC W:\src\HandmadeRalph_win32.cpp > HandmadeRalph_win32.new.cpp
	fc /b HandmadeRalph_win32.new.cpp HandmadeRalph_win32.old.cpp > nul 2>&1
	if !ERRORLEVEL! neq 0 (
		echo :: HandmadeRalph_win32.cpp
		clang -o HandmadeRalph.exe %DEBUG_COMPILER_FLAGS% W:\src\HandmadeRalph_win32.cpp -l user32.lib -l gdi32.lib -l winmm.lib -l dxgi.lib -Xlinker /subsystem:windows
		if !ERRORLEVEL! neq 0 (
			echo :: HandmadeRalph_win32 compilation failed
			del HandmadeRalph_win32.new.cpp
			del HandmadeRalph_win32.old.cpp
			goto ABORT
		) else (
			move /y HandmadeRalph_win32.new.cpp HandmadeRalph_win32.old.cpp > nul
		)
	)

	:ABORT
	del *.obj *.lib *.exp >nul 2> nul

	set t=%time: =0%
	set /a h=1%t0:~0,2%-100
	set /a m=1%t0:~3,2%-100
	set /a s=1%t0:~6,2%-100
	set /a c=1%t0:~9,2%-100
	set /a starttime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%
	set /a h=1%t:~0,2%-100
	set /a m=1%t:~3,2%-100
	set /a s=1%t:~6,2%-100
	set /a c=1%t:~9,2%-100
	set /a endtime = %h% * 360000 + %m% * 6000 + 100 * %s% + %c%
	set /a runtime = %endtime% - %starttime%
	set runtime = %s%.%c%
	echo :: %runtime%0ms
popd W:\build\
