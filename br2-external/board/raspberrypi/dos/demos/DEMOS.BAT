@ECHO OFF
IF NOT EXIST C:\DEMOS\MENU.BAT GOTO missing
PUSHD C:\DEMOS
CALL MENU.BAT
POPD
GOTO end
:missing
ECHO Demoscene pack not found in C:\DEMOS.
:end
