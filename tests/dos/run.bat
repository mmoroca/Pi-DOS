@echo off
comtest.com >com.out
if errorlevel 8 goto fail
if not errorlevel 7 goto fail
exetest.exe >exe.out
if errorlevel 12 goto fail
if not errorlevel 11 goto fail
set PI_DOS_TEST=PI_DOS_BAT_OK
echo %PI_DOS_TEST%>bat.out
md work
cd work
echo PI_DOS_FILE_OK>source.txt
copy source.txt copy.txt >nul
if not exist copy.txt goto fail
dir copy.txt >dir.txt
cd \
echo PI_DOS_PROGRAMS_OK>result.txt
goto end
:fail
echo PI_DOS_PROGRAMS_FAILED>c:\fail.txt
:end
