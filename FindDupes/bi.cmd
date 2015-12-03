@echo off
:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
set Build_Increment=1000000
set Build_Date=%25date:~10,4%%25date:~4,2%%25date:~7,2%

if not exist "Build_Increment.h" goto :Write_Build_Increment

for /f "tokens=3 skip=1" %%i in (Build_Increment.h) do @set Build_Increment=1%%i

set /a Build_Increment=%Build_Increment% + 1
:Write_Build_Increment

echo #define BUILD_DATE %Build_Date%
echo #define BUILD_INCREMENT %Build_Increment:~1%

>"Build_Increment.h" echo #define BUILD_DATE %Build_Date%
>>"Build_Increment.h" echo #define BUILD_INCREMENT %Build_Increment:~1%

