@echo off

if NOT DEFINED DevEnvDir (
	call vcvarsall.bat x64
)
