@ECHO OFF
setlocal enabledelayedexpansion

if [%1] == [] echo "usage error 'snapshot.bat [filename] <version name>' "
if [%1] == [] EXIT /B -1

ECHO Creating snapshot
set PROJECT=%1

set FLAGS=-DDISABLE_DEBUG -DDISABLE_LOG -DPRINT_TO_FILE
if NOT "%2"=="" (
    ECHO Setting version %2
    set VERSION=-DARCANUM_VERSION=%2
    set FLAGS="%FLAGS% !VERSION!"
    set FLAGS=!FLAGS:~1,-1!
) else (
    ECHO No version set, using dev_version
)

make clean
make -j CFLAGS="!FLAGS!"
copy ".\build\%PROJECT%.exe" /b ".\snapshots\%PROJECT%.exe" /b
copy "arcanum-net-v2.1.fnnue" /b ".\snapshots\arcanum-net-v2.1.fnnue" /b
make clean