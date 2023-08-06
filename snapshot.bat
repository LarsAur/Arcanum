@ECHO OFF
ECHO Creating snapshot

set CFLAGS=-DDISABLE_CE2_DEBUG -DDISABLE_CE2_LOG -DDISABLE_CE2_WARNING -DDISABLE_CE2_ERROR -DDISABLE_CE2_SUCCESS
set /p PROJECT=Name of the snapshot: 

make clean
make -j
copy /b ".\build\%PROJECT%.exe" ".\snapshots\%PROJECT%.exe" /b
make clean