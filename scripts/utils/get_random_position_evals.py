from subprocess import Popen, PIPE, STDOUT
import chess.pgn
import chess.engine
import math
import random
import re

# Load engine
stockfish_path = input("Engine Path: ")
engine = Popen(stockfish_path, stdout=PIPE, stdin=PIPE, stderr=STDOUT)
engine.stdin.write(b"uci\n")
engine.stdin.flush()

while(True):
    out = engine.stdout.readline()
    print(bytes.decode(out, 'utf-8'), end='')
    if bytes.decode(out, 'utf-8') == "uciok\r\n":
        break

pgn_file = open("output.pgn")
fen_file = open("eval_fen_strings.txt", 'w+')

threshhold = 25
stopafter = 200
fens_added = 0

# Fetch pgns from the pgn file
while(game := chess.pgn.read_game(pgn_file)):
    moves = game.mainline_moves()
    board = game.board()

    to_play = math.floor(len(list(moves)) * random.random() * 0.7 + 1)

    played = 0
    fen = ""
    for move in moves:
        board.push(move)
        played += 1
        if played == to_play:
            fen = board.fen()
            break
    
    # print(to_play)
    print(fen)
            
    position = f"position fen {fen}\n"
    engine.stdin.write(position.encode('utf-8'))
    engine.stdin.flush()
    engine.stdin.write("eval\n".encode("utf-8"))
    engine.stdin.flush()

    while(True):
        engine.stdout.flush()
        out = bytes.decode(engine.stdout.readline(), "utf-8")
        if(out.startswith("Classical evaluation")):
            re_score = re.search(r"[-|\+]\d+\.\d+", out)
            if(re_score != None):
                print(re_score.group(), end="\n")
                fen_file.write(fen + '\n')
                fen_file.write(re_score.group() + '\n')
                fens_added += 1
            else:
                print("No score")

        if(out.startswith("Final evaluation")):
            break
    
    # If there is a positive limit, stop after finding that number of balanced fen strings
    if(fens_added >= stopafter and stopafter > 0):
        break
        
pgn_file.close()
fen_file.close()
engine.stdin.write(b"quit\n")
engine.stdin.flush()
