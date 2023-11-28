@ECHO OFF

if [%1] == [] echo "usage error 'snapshot.bat [filename] <version name>' "
if [%1] == [] EXIT /B -1

ECHO Creating snapshot
set PROJECT=%1

set "FLAGS=-DPRINT_TO_FILE"
if [%2] NEQ [] (set FLAGS="%FLAGS% -DARCANUM_VERSION=%2")

make clean
make -j CFLAGS="%FLAGS%"
make -j
copy ".\build\%PROJECT%.exe" /b ".\snapshots\%PROJECT%.exe" /b
make clean