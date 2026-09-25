@echo off

rem %comspec% /k "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

rem Build tm.exe. Run from a "x64 Native Tools Command Prompt for VS", or with MinGW gcc on PATH.

where cl >nul 2>nul

if %errorlevel%==0 (
    cl /nologo /O2 /GS- /W3 /std:c11 tm.c /Fe:tm.exe /link /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF user32.lib gdi32.lib
) else (
    gcc -O2 -s -mwindows -ffunction-sections -fdata-sections -Wl,--gc-sections tm.c -o tm.exe -luser32 -lgdi32
)
