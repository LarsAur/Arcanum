<div align="center">
  <h1>Arcanum</h1>
</div>

## Overview
Arcanum is a free [UCI][uci-protocol] chess engine under the GPL-3.0 license.
Arcanum v2.6 has a rating of 3331 Elo in blitz on [CCRL][ccrl].

## Building
Arcanum requires `c++17` and has only been tested using `clang++`. It can be compiled on Windows (Tested for Windows 11 using MinGW), and Linux (Tested for Ubuntu 22.04). Arcanum takes advantage of and requires some x86 intrinsics: `AVX2`, `FMA`, `BMI1`, `BMI2` `POPCNT` and `LZCNT`.

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

### Release build
To build a *release build*, meant for connecting to a UCI GUI such as [Cute Chess][cute-chess] or [Lucas Chess][lucas-chess], `-DLOG_FILE_NAME=<filename>` should be added to `CFLAGS`. This will redirect all logging to a file named `<filename>_[date]_[time].log`. Note that this file is not created unless something is logged. It is also possible to disable separate log-levels by adding `-DDISABLE_DEBUG`, `-DDISABLE_LOG`, `-DDISABLE_WARNING` and `-DDISABLE_ERROR` to `CFLAGS`.

To add a version name, which shows up in the *id* section of the UCI protocol, `-DARCANUM_VERSION=<version>` can be added to the `CFLAGS`. If not included, the version will be set to `dev_build`.

A simpler approach is to run:

```
make release -j NAME=<executable-name> VERSION=<version>
```

This creates a clean build named `<executable-name>` with version `<version>` which logs to file, and only have warnings and errors enabled. The build will be copied to the *releases* directory.

## NNUE
Arcanum has a floating point [NNUE][nnue] which is quantized at runtime. \
The architecture is `768->8x(512->16->1)`, where the feature set is 'flipped' based on the perspective rather than having two feature transformers.
The output buckets are selected based on the number of pieces left on the board.

Both the inference and backpropagation is written from scratch and requires AVX2.

The path to the NNUE file can be set by the UCI command:
```
setoption name nnuepath value <path>
```
where `<path>` is the path _relative to the Arcanum executable file_. Note that the default net is embedded in the executable.

## Syzygy
Arcanum has an option to enable the use of [Syzygy][syzygy], an endgame table base. This is implemented using an adaptation of [Pyrrhic][pyrrhic] by [AndyGrant][andy-grant]. To enable [Syzygy][syzygy], use the UCI command:
```
setoption name syzygypath value <folder>
```
where `<folder>` is the _absolute path_ to the folder containing the tablebase.

## Incbin
From v2.4 and onward, [Incbin][incbin] by [graphitemaster][graphitemaster] is used to embed the default nnue net into the executable. If the nnue path is equal to the default path, the embedded net will be used.

This can be disabled by removing `-DENABLE_INCBIN` from the makefile when building.


## Options
All of the following UCI options are available in Arcanum.
| Name           | Type   | Default                | Description                                                                                                                                                                                    |
|----------------|--------|------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| Hash           | Spin   | 32                     | Number of MBs to allocate for the transposition table. If 0, the transposition table will be disabled.                                                                                         |
| ClearHash      | Button |                        | Clears the transposition table.                                                                                                                                                                |
| SyzygyPath     | String | \<empty\>              | Absolute path to the Syzygy directory. If \<empty\>, Syzygy will be disabled.                                                                                                                  |
| NNUEPath       | String | arcanum&#8209;net&#8209;v5.0.fnnue | Path to the NNUE net relative to the executable. If the default value is used, the net embedded in the executable will be used.                                                                                                                                  |
| MoveOverhead   | Spin   | 10                     | Number of ms to assume as move overhead. MoveOverhead is subtracted from the remaining time before doing time management. If MoveOverhead is larger than the remaining time, 1ms will be used. |
| NormalizeScore | Check  | True                   | Normalize the score reported in UCI info such that 100cp equates to a ~50% chance to win                                                                                                       |

## Rating Progression

| Version | CCRL[^1] Blitz | CCRL[^1] 40/15 | CEGT[^2] 40/20 |
|---------|------------|------------|------------|
| 2.6     | 3331	     | 3325       | 3250       |
| 2.5     | 3278       | 3273       | 3181       |
| 2.4     | 3198       | 3210       |            |
| 2.3.*   | 3129       | 3107       |            |
| 2.2     | 2926       | 2929       |            |
| 2.1     | 2724       |            |            |
| 2.0     | 2456       |            |            |
| 1.12    | 2227       | 2228       |            |
| 1.11.*  | 2140       |            |            |

[^1]: CCRL: https://computerchess.org.uk/ccrl/404/
[^2]: CEGT: http://www.cegt.net/

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
[cegt]: http://www.cegt.net/
[syzygy]: http://tablebase.sesse.net/
[pyrrhic]: https://github.com/AndyGrant/Pyrrhic
[andy-grant]: https://github.com/AndyGrant
[incbin]: https://github.com/graphitemaster/incbin
[graphitemaster]: https://github.com/graphitemaster
