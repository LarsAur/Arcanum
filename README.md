<p align="center">
  <img src="Arcanum-Logo.svg" />
</p>

## Overview
Arcanum is a free [UCI][uci-protocol] chess engine under the GPL-3.0 license.
The engine (v1.12) (w. nnue) has a rating of 2450 - 2550 in bullet and blitz on [lichess][lichess]. Without nnue, it has a rating of ~2200 in blitz on [CCRL][ccrl].

## Building
Arcanum requires `c++17` and has only been tested using `g++`. It can be compiled on Windows (Tested for Windows 11 using MinGW), and Linux (Tested for Ubuntu 22.04). Arcanum takes advantage of and requires some x86 intrinsics: `AVX2`, `FMA`, `BMI1`, `BMI2` `POPCNT` and `LZCNT`.

### Development build
To build a *development build*, run:

```
make -j
```
This will create a build directory and build a staticly linked executable. The development build allows Arcanum to log information to the terminal. This can however interfere with the UCI protocol, thus it is not recomended to use this build when connecting to a UCI gui.

To delete the build directory and its contents all the contents, run:
```
make clean
```

### UCI build
To build a *UCI build*, meant for connecting to a UCI GUI such as [Cute Chess][cute-chess] or [Lucas Chess][lucas-chess], run:

```
make -j CFLAGS="-DPRINT_TO_FILE"
```
This redirects all logging to a file. It is also possible to add `-DDISABLE_DEBUG`, `-DDISABLE_LOG`, `-DDISABLE_WARNING` and `-DDISABLE_ERROR` to the CFLAGS in this command to disable these logs.

To add a version name, which shows up in the *id* section of the UCI protocol, `-DARCANUM_VERSION=<version>` can be added to the CFLAGS. If not included, the version will be set to `dev_build`.

Optionally, a *UCI build* can be built by running:

```
snapshot.bat <executable-name> <version-name>
```
This will create a fresh *UCI build* named `<executable-name>` and copy it to the snapshots directory.

## NNUE
Arcanum has a floating point [NNUE][nnue], which is created from selfplay, starting from the HCE from Arcanum v1.12.
The architecture is `768->256->1`, where the feature set is 'flipped' based on the perspective rather than having two feature transformers.

Both the inference and backpropagation is written from scratch and requires AVX2.

The path to the NNUE file can be set by the UCI command:
```
setoption name nnuepath value <path>
```
where `<path>` is the path _relative to the Arcanum executable file_.

## Syzygy
Arcanum has an option to enable the use of [Syzygy][syzygy], an endgame table base. This is implemented using an adaptation of [Pyrrhic][pyrrhic] by [AndyGrant][andy-grant]. To enable [Syzygy][syzygy], use the UCI command:
```
setoption name syzygypath value <folder>
```
where `<folder>` is the _absolute path_ to the folder containing the tablebase.

## Testing
Arcanum has a number of arguments to validate the engine and test the performance:
* `--capture-test` Test that move generation for capture moves works.
* `--zobrist-test` Testing that zobrist hashing works.
* `--perft-test` Runs [perft][perft] on a number of predefined positions with known [results][perft-results].
* `--search-perf` Runs a 40 ply game searching at depth 10 with [quiescence][qsearch] search. This is a performance test, checking the speed of the search.
* `--engine-perf` Runs a search for 5 seconds on a number of difficult [test-positions][test-positions] ([Bratko-Kopec Test][bkt]). This is a performance test, checking the strength of the search.


[uci-protocol]: https://backscattering.de/chess/uci/
[lucas-chess]: https://lucaschess.pythonanywhere.com/
[cute-chess]: https://cutechess.com/
[python-chess]: https://python-chess.readthedocs.io/en/latest/
[perft]: https://www.chessprogramming.org/Perft
[perft-results]: https://www.chessprogramming.org/Perft_Results
[qsearch]: https://www.chessprogramming.org/Quiescence_Search
[test-positions]: https://www.chessprogramming.org/Test-Positions
[bkt]: https://www.chessprogramming.org/Bratko-Kopec_Test
[chess.com]: https://www.chess.com
[nnue]: https://www.chessprogramming.org/NNUE
[lichess]: https://lichess.org/@/ArcanumBot
[ccrl]: https://computerchess.org.uk/ccrl/404/
[syzygy]: http://tablebase.sesse.net/
[pyrrhic]: https://github.com/AndyGrant/Pyrrhic
[andy-grant]: https://github.com/AndyGrant