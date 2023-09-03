import chess.pgn
import chess.engine
import os
import sys

if(not os.path.isfile(sys.argv[1])):
    print(sys.argv[1], "is not a file")
    exit(-1)
    
if(not os.path.isfile(sys.argv[2])):
    print(sys.argv[2], "is not a file")
    exit(-1)

game_file_path = sys.argv[1]
engine_path = sys.argv[2]

game_file = open(game_file_path)

while(True):
    fen = game_file.readline()
    if(len(fen) == 0):
        break
    
    moves = game_file.readline().split(', ')
    if(len(moves) % 2 == 0):
        moves = moves[0:-8]
    else:
        moves = moves[0:-7]

    print(moves)

    # print(fen)
    # print(moves)

    engine:chess.engine.SimpleEngine = chess.engine.SimpleEngine.popen_uci(engine_path)
    board:chess.Board = chess.Board(fen)
    for move in moves:
        board.push(chess.Move.from_uci(move))

    print(board.turn)
    result = engine.analyse(board, chess.engine.Limit(time=3.0))
    engine.close()

    print(fen)
    print(result["score"])