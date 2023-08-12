from chessgame import ChessGame
import chess
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

    white_engine_name = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    black_engine_name = os.path.splitext(os.path.basename(sys.argv[2]))[0]

    fen_string_file = open("../fen_strings.txt", 'r')
    results_file = open(f"../results/{white_engine_name}-{black_engine_name}.txt", "w+")

    wins = 0
    draws = 0
    stalemates = 0
    games = 0

    while(True):
        opening = fen_string_file.readline()
        if(len(opening) == 0):
            break
        fen = fen_string_file.readline()
        game = ChessGame(sys.argv[1], sys.argv[2], fen)
        game.play(200)
        games += 1
        
        results_file.write(opening)
        results_file.write(fen)
        result = game.get_result()
        results_file.write(result[0])
        if result[0] == ChessGame.CHECKMATE:
            if result[1] == chess.WHITE:
                results_file.write(" White")
                wins += 1
            else:
                results_file.write(" Black")
        elif result[0] == ChessGame.STALEMATE:
            stalemates += 1
        elif result[0] == ChessGame.DRAW:
            draws += 1
                
        moves = game.get_moves()
        move_list = ", ".join(moves)
        results_file.write("\n" + move_list + "\n")

        os.system('cls')
        print(f"White wins: {wins}/{games}")
        print(f"Black wins: {games-wins-stalemates-draws}/{games}")
        print(f"Stalemates: {stalemates}/{games}")
        print(f"Draws     : {draws}/{games}")
            
    fen_string_file.close()
    results_file.close()
if __name__ == "__main__":
    main()