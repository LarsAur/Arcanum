# Arcanum
Arcanum is a free [UCI][uci-protocol] chess engine under the GPL-3.0 license.
The engine (v1.12) (w. nnue) has a rating of 2450 - 2550 in bullet and blitz on [lichess][lichess]. Without nnue, it has a rating of ~2200 in blitz on [CCRL][ccrl].

## Building
Arcanum requires `c++17` and has only been tested using `g++`. It can be compiled on Windows (Tested for Windows 11 using MinGW).
With some tweaking, it should compile for Linux.
Arcanum takes advantage of and requires some x86 intrinsics: `AVX2`, `FMA`, `BMI1`, `BMI2` `POPCNT` and `LZCNT`.

### Development build
To build a *development build*, run `make -j`. The development build allows Arcanum to print log information to the terminal. This can however interfere with the UCI protocol, thus it is not recomended to use this build when conncting to a UCI gui such as [Cute Chess][cute-chess] or [Lucas Chess][lucas-chess].

### UCI build
To build a *UCI build*, run `make -j CFLAGS=-DPRINT_TO_FILE` or run ***snapshot.bat***. This redirects all logging to a file. All UCI communication will be written to a file in addition to be sent to *stdout* to communicate with either a terminal or UCI gui such as [Cute Chess][cute-chess] or [Lucas Chess][lucas-chess].

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
* `--search-perf` Runs a 40 ply game searching at depth 10 with [quiescence][qsearch] search starting with 4 plys of checking moves. This is a performance test, checking the speed of the search.
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