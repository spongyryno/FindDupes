@echo off
setlocal enableextensions enabledelayedexpansion
set home=%~dp0
set home=%home:~,-1%

::=================================================================================================
:: Start the program
::=================================================================================================
call :MakeWritable

set Build_Increment=1000000
set Build_Date=%date:~10,4%%date:~4,2%%date:~7,2%

if not exist "Build_Increment.h" goto :Write_Build_Increment

for /f "tokens=3 skip=1" %%i in (Build_Increment.h) do @set Build_Increment=1%%i

set /a Build_Increment=%Build_Increment% + 1

::=================================================================================================
:: Write the results
::=================================================================================================
:Write_Build_Increment

echo #define BUILD_DATE %Build_Date%
echo #define BUILD_INCREMENT %Build_Increment:~1%

>"Build_Increment.h" echo #define BUILD_DATE %Build_Date%
>>"Build_Increment.h" echo #define BUILD_INCREMENT %Build_Increment:~1%
goto :eof

::=================================================================================================
:: If the file exists, make sure it's writable
::=================================================================================================
:MakeWritable
if not exist "Build_Increment.h" goto :eof
attrib.exe -r "Build_Increment.h"
goto :eof
