from chessgame import ChessGame
import chess
import os
import sys

def main():

    if(len(sys.argv) < 3):
        print("usage:", sys.argv[0], "[UciEngine1Path] [UciEngine2Path] <number of games> <ms movetime>")
        exit(0)

    if(not os.path.isfile(sys.argv[1])):
        print(sys.argv[1], "is not a file")
        exit(-1)
    if(not os.path.isfile(sys.argv[2])):
        print(sys.argv[2], "is not a file")
        exit(-1)

    max_games = -1
    if(len(sys.argv) >= 4):
        max_games = int(sys.argv[3])

    ms_movetime = 200
    if(len(sys.argv) >= 5):
        ms_movetime = int(sys.argv[3])

    white_engine_name = os.path.splitext(os.path.basename(sys.argv[1]))[0]
    black_engine_name = os.path.splitext(os.path.basename(sys.argv[2]))[0]

    fen_string_file = open("data/fen_strings.txt", 'r')
    results_file = open(f"results/{white_engine_name}-{black_engine_name}.txt", "w+")

    wins = 0
    draws = 0
    stalemates = 0
    games = 0

    while(True):
        opening = fen_string_file.readline()
        if(len(opening) == 0 or games == max_games):
            break
        fen = fen_string_file.readline()
        game = ChessGame(sys.argv[1], sys.argv[2], fen)
        game.play(ms_movetime)
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
        elif result[0] == ChessGame.DRAW or result[0] == ChessGame.DRAW50 or result[0] == ChessGame.DRAWREPEAT:
            draws += 1

        moves = game.get_moves()
        move_list = ", ".join(moves)
        results_file.write("\n" + move_list + "\n")

        os.system('cls')
        space = ""
        print(f"{white_engine_name:<20} (White) wins: {wins}/{games}")
        print(f"{black_engine_name:<20} (Black) wins: {games-wins-stalemates-draws}/{games}")
        print(f"{space:<20} Stalemates: {stalemates}/{games}")
        print(f"{space:<20} Draws     : {draws}/{games}")

    results_file.write(f"\n{white_engine_name:<20} (White) wins: {wins}/{games}\n")
    results_file.write(f"{black_engine_name:<20} (Black) wins: {games-wins-stalemates-draws}/{games}\n")
    results_file.write(f"{space:<20} Stalemates: {stalemates}/{games}\n")
    results_file.write(f"{space:<20} Draws     : {draws}/{games}\n")
    fen_string_file.close()
    results_file.close()
if __name__ == "__main__":
    main()