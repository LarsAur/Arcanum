import chess.pgn
import chess.engine
import math
import random

# Load engine
stockfish_path = input("Engine Path: ")
engine = chess.engine.SimpleEngine.popen_uci(stockfish_path)

pgn_file = open("output.pgn")
fen_file = open("fen_strings.txt", 'w+')

threshhold = 25
stopafter = 200
fens_added = 0

# Fetch pgns from the pgn file
while(game := chess.pgn.read_game(pgn_file)):
    moves = game.mainline_moves()
    board = game.board()

    to_play = math.floor(10 + 20 * random.random()) & ~1

    played = 0
    fen = ""
    for move in moves:
        board.push(move)
        played += 1
        if played == to_play:
            fen = board.fen()
            break
            
    num_officers = sum([fen.lower().count(char) for char in "qrnb"])
    if(num_officers > 10):
        evaltime = 1 #so 5 seconds
        info = engine.analyse(board, chess.engine.Limit(time=evaltime))

        #print best move, evaluation and mainline:
        print('Evaluation: ', info['score'].white())
        if(info["score"].white() < chess.engine.Cp(threshhold) and info["score"].black() < chess.engine.Cp(threshhold)):
            
            fen_file.write(game.headers["Opening"] + '\n')    
            fen_file.write(fen + '\n')
            
            print("Added: ", fen, " with opening:", game.headers["Opening"])
            fens_added += 1
            
            # If there is a positive limit, stop after finding that number of balanced fen strings
            if(fens_added >= stopafter and stopafter > 0):
                break
            
pgn_file.close()
fen_file.close()
engine.close()