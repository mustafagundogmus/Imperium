@cls
@echo off
setLocal EnableExtensions EnableDelayedExpansion

set "installfolder=%~dp0"
set "installfolder=%installfolder:~0,-1%"
set "installername=%~n0.cmd"

set "SingleNul=>nul"
set "SingleNulV1=1>nul"
set "SingleNulV2=2>nul"
set "SingleNulV3=3>nul"
set "MultiNul=1>nul 2>&1"
set "TripleNul=1>nul 2>&1 3>&1"

%MultiNul% fltmc || (
  set "_=call "%~dpfx0" %*" & powershell -nop -c start cmd -args '/d/x/r',$env:_ -verb runas || (
  >"%temp%\Elevate.vbs" echo CreateObject^("Shell.Application"^).ShellExecute "%~dpfx0", "%*" , "", "runas", 1
  %SingleNul% "%temp%\Elevate.vbs" & del /q "%temp%\Elevate.vbs" )
  exit )

cls
cd /D "%installfolder%"
for /l %%$ in (1,1,9) do set "var%%$="
for /F "tokens=*" %%a in (package.info) do (
  set /a countx=!countx! + 1
  set var!countx!=%%a
)
if %countx% LSS 5 ((echo:)&&(echo Download incomplete - Package unusable - Redo download)&&(echo:)&&(pause)&&(exit))
set "instversion=%var2%"
set "instlang=%var3%"
set "instarch1=%var4%"
set "instupdlocid=%var5%"
set "o16lang=%var6%"
set "id_LIST=%var7%"
set "oApps=%var8%"
set "exclude=%var9%"
::===============================================================================================================
if /i "!instarch1!" equ "x86" set "instarch2=32"
if /i "!instarch1!" equ "x64" set "instarch2=64"
if /i "!instarch1!" equ "x64" if not exist "%systemroot%\SysWOW64\cmd.exe" ((echo.)&&(echo ERROR: You can't install x64/64bit Office on x86/32bit Windows)&&(echo.)&&(pause)&&(exit))
if /i "!instarch1!" equ "multi" (
  set instarch2=32
  if defined PROCESSOR_ARCHITEW6432 set instarch2=64
  if /i '%PROCESSOR_ARCHITECTURE%' equ 'AMD64' set instarch2=64
  if /i '%PROCESSOR_ARCHITECTURE%' equ 'IA64'  set instarch2=64
  if !instarch2! equ 32 set "instarch1=x86"
  if !instarch2! equ 64 set "instarch1=x64"
)
echo:
echo Selected installation architecture: !instarch1! (!instarch2!-bit)
echo:
::===============================================================================================================
dir /B "%installfolder%\Office\Data\%instversion%\i!instarch2!*.*" >"%TEMP%\OfficeSetup.txt" 
for /F "tokens=* delims=" %%A in (%TEMP%\OfficeSetup.txt) DO (set "instlcidcab=%%A")
set "instlcid=%instlcidcab:~3,4%"
::=============================================================================================================== 
echo Stopping services "ClickToRunSvc" and "Windows Search"... 
sc query "WSearch" | find /i "STOPPED" 1>nul || net stop "WSearch" /y %MultiNul%
sc query "WSearch" | find /i "STOPPED" 1>nul || sc stop "WSearch" %MultiNul% 
sc query "ClickToRunSvc" | find /i "STOPPED" 1>nul || net stop "ClickToRunSvc" /y %MultiNul% 
sc query "ClickToRunSvc" | find /i "STOPPED" 1>nul || sc stop "ClickToRunSvc" %MultiNul% 
::=============================================================================================================== 
:InstallerDelete 
rd "%CommonProgramFiles%\Microsoft Shared\ClickToRun" /S /Q %MultiNul% 
if exist "%CommonProgramFiles%\Microsoft Shared\ClickToRun\OfficeClickToRun.exe" goto:InstallerDelete %MultiNul% 
md "%CommonProgramFiles%\Microsoft Shared\ClickToRun" %MultiNul% 
echo Copying new ClickToRun installer files... 
expand "%installfolder%\Office\Data\%instversion%\i!instarch2!0.cab" -F:* "%CommonProgramFiles%\Microsoft Shared\ClickToRun" %MultiNul% 
expand "%installfolder%\Office\Data\%instversion%\%instlcidcab%" -F:* "%CommonProgramFiles%\Microsoft Shared\ClickToRun" %MultiNul% 
::===============================================================================================================