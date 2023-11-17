# Arcanum
Arcanum is a free [UCI][uci-protocol] chess engine under the GPL-3.0 license.
The engine has a rating of 2450 - 2550 in bullet and blitz on [lichess][lichess].

## Building
Arcanum requires `c++17` and has only been tested using `g++`. It can be compiled on both Windows (Tested for Windows 11 using MinGW) and Linux (Tested for Ubuntu 20.04). Arcanum also takes advantage of some x86 intrinsics: `BMI1` and `POPCNT`. It is possible to build without them, but it might require some tweaking, and will be slower.

### Development build
To build a *development build*, run `make -j`. The development build allows Arcanum to print log information to the terminal. This can however interfere with the UCI protocol, thus it is not recomended to use this build when conncting to a UCI gui such as [Lucas Chess][lucas-chess].
### UCI build
To build a *UCI build*, run `make -j CFLAGS=-DPRINT_TO_FILE` or run ***snapshot.bat***. This redirects all logging to a file. All UCI communication will be written to a file in addition to be sent to *stdout* to communicate with either a terminal or UCI gui such as [Lucas Chess][lucas-chess].

## Benchmarking
To benchmark the engine, build a *UCI build* as described a above, of the two versions you want to benchmark. In *./benchmarking/scripts* run: `python main.py 'white_path' 'black_path'` e.g.
`python main.py ../../snapshots/version_1 ../../snapshots/version_2`. This will play 200 games starting from 200 different balanced (±0.25cp) with a movetime of 200ms.
The python benchmarking script requires [python-chess][python-chess] which can be installed with `pip install python-chess`. The results of all the games will be written to *./benchmarking/results/name1-name2.txt*.

## Testing
Arcanum has a number of arguments to run in different modes:
* `--play` Allows you to play against the engine in interactive mode writing uci moves in the terminal.
* `--capture-test` Test that move generation for capture moves works.
* `--zobrist-test` Testing that zobrist hashing works.
* `--perft-test` Runs [perft][perft] on a number of predefined positions with known [results][perft-results].
* `--symeval-test` A sanity test checking that a large number of random symmetrical and equal positions have the same static evaluation.
* `--search-perf` Runs a 40 ply game searching at depth 10 with [quiescence][qsearch] search starting with 4 plys of checking moves. This is a performance test, checking the speed of the search.
* `--engine-perf` Runs a search for 5 seconds on a number of difficult [test-positions][test-positions] ([Bratko-Kopec Test][bkt]). This is a performance test, checking the strength of the search.

A combination of these are available though the makefile, with `uci`, `play`, `test` and `perf` targets.
Using no arguments makes the engine default to run in *UCI mode*.

## NNUE
Arcanum has the option to use [NNUE][nnue] (Efficiently Updatable Neural Network) to evaluate positions.
To probe the NNUE, Arcanum uses an adaptation of [nnue-probe][nnue-probe] by dshawul. The NNUE probing code is re-written from scratch and uses AVX2.

This version uses a network generated by the stockfish testing framework: [nn-04cf2b4ed1da.nnue][nnue-file].

[uci-protocol]: https://backscattering.de/chess/uci/
[lucas-chess]: https://lucaschess.pythonanywhere.com/
[python-chess]: https://python-chess.readthedocs.io/en/latest/
[perft]: https://www.chessprogramming.org/Perft
[perft-results]: https://www.chessprogramming.org/Perft_Results
[qsearch]: https://www.chessprogramming.org/Quiescence_Search
[test-positions]: https://www.chessprogramming.org/Test-Positions
[bkt]: https://www.chessprogramming.org/Bratko-Kopec_Test
[chess.com]: https://www.chess.com
[nnue]: https://www.chessprogramming.org/NNUE
[nnue-probe]: https://github.com/dshawul/nnue-probe
[lichess]: https://lichess.org/@/ArcanumBot
[nnue-file]: https://tests.stockfishchess.org/nns?network_name=nn-04cf2b4ed1da&user=