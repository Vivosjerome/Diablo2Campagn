@echo off
setlocal
set ROOT=%~dp0
set TCC=%ROOT%tools\tcc\tcc.exe
set SRC=%ROOT%src
set OUT=%ROOT%d2maphost.exe
set MAN=%ROOT%d2maphost.manifest

if not exist "%TCC%" (
  echo [!] tcc.exe manquant dans tools\tcc
  exit /b 1
)

echo [*] Compiling d2maphost...
"%TCC%" -I"%SRC%" -luser32 -lgdi32 -lkernel32 -ladvapi32 ^
  -Wl,-subsystem=windows ^
  "%SRC%\main.c" "%SRC%\memory.c" "%SRC%\seed.c" "%SRC%\game.c" "%SRC%\mapgen.c" "%SRC%\mapdata.c" ^
  "%SRC%\levelnames.c" "%SRC%\elites.c" "%SRC%\bossstate.c" "%SRC%\settings.c" "%SRC%\overlay.c" ^
  -o "%OUT%"
if errorlevel 1 (
  echo [!] compile failed
  exit /b 1
)

if exist "%MAN%" (
  where mt >nul 2>&1
  if not errorlevel 1 (
    mt -nologo -manifest "%MAN%" -outputresource:"%OUT%;#1" >nul 2>&1
    if not errorlevel 1 echo [+] Manifest Admin embarque
  )
)

echo [+] OK: %OUT%
endlocal
