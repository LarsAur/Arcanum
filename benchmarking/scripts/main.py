from chessgame import ChessGame
from datetime import datetime
import os
import sys

def main():
    
    if(len(sys.argv) != 3):
        print("usage:", sys.argv[0], "[UciEngine1Path] [UciEngine2Path]")
        print("UciEngine1Path is required to have the 'ischeckmate' custom command")
        exit(0)
    
    # now = datetime.now()
    # dt_string = now.strftime("%d_%m_%Y-%H_%M_%S")
    # os.mkdir(dt_string)
    
    if(not os.path.isfile(sys.argv[1])):
        print(sys.argv[1], "is not a file")
        exit(-1)
    if(not os.path.isfile(sys.argv[2])):
        print(sys.argv[2], "is not a file")
        exit(-1)
    
    for i in range(10):
        game = ChessGame(sys.argv[1], sys.argv[2])
        game.play(100)
        result = game.get_result()
        # game.save(f"./{dt_string}/game_{elo}_{result}.log")
        
    
    
if __name__ == "__main__":
    main()