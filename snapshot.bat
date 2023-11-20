@ECHO OFF
ECHO Creating snapshot

if [%1] == [] echo "usage error 'snapshot.bat [filename] <version name>' "

set PROJECT=%1

if [%2] == [] (set FLAGS=-DPRINT_TO_FILE) else (set FLAGS="-DPRINT_TO_FILE -DARCANUM_VERSION=%2")

make clean
make -j CFLAGS=%FLAGS%
make -j
copy /b ".\build\%PROJECT%.exe" ".\snapshots\%PROJECT%.exe" /b
make clean