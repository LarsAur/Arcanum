@ECHO OFF

if [%1] == [] echo "usage error 'snapshot.bat [filename] <version name>' "
if [%1] == [] EXIT /B -1

ECHO Creating snapshot
set PROJECT=%1

set "FLAGS=-DDISABLE_DEBUG -DDISABLE_LOG -DDISABLE_WARNING -DDISABLE_ERROR -DDISABLE_SUCCESS"
if [%2] NEQ [] (set FLAGS="%FLAGS% -DARCANUM_VERSION=%2")

make clean
make -j CFLAGS="%FLAGS%"
make -j
copy ".\build\%PROJECT%.exe" /b ".\snapshots\%PROJECT%.exe" /b
copy "nn-04cf2b4ed1da.nnue" /b ".\snapshots\nn-04cf2b4ed1da.nnue" /b
copy "hceWeights.dat" /b ".\snapshots\hceWeights.dat" /b
make clean