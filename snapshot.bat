@ECHO OFF
ECHO Creating snapshot

set CFLAGS=-DPRINT_TO_FILE
set /p PROJECT=Name of the snapshot: 

make clean
make -j
copy /b ".\build\%PROJECT%.exe" ".\snapshots\%PROJECT%.exe" /b
make clean