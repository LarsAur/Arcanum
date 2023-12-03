from chessgame import ChessGame
import config
import random
import math
import chess.pgn
import os
from concurrent.futures import ThreadPoolExecutor
from threading import Lock

mutex = Lock()
games_generated = 0
positions_generated = 0

def _worker(start_fen):
    print(start_fen)
    game_fens = []
    game = ChessGame(config.ENGINE_PATH, config.ENGINE_PATH, fen=start_fen)
    while not game.is_finished():
        if not game.is_mate_found():
            game_fens.append(game.get_fen())
        game.play_single_move(config.SEARCH_TIME)
    game.terminate()
    result = game.get_result()
    score = 0.5
    if result[0] == ChessGame.CHECKMATE:
        if result[1] == chess.WHITE:
            score = 1
        else:
            score = 0

    mutex.acquire()
    dataset_file = open(config.DATASET_PATH, 'a')
    num_positions = len(game_fens)
    dataset_file.write(f"GameData: {score} {num_positions}\n")
    for fen in game_fens:
        dataset_file.write(fen + '\n')
    global games_generated
    games_generated += 1
    global positions_generated
    positions_generated += num_positions
    dataset_file.close()
    print("Games:", games_generated, "Positions:", positions_generated)
    mutex.release()

def create_dataset():
    pgn_file = None
    dataset_file = None

    # Return if the dataset already exists
    if os.path.exists(config.DATASET_PATH):
        return

    try:
        pgn_file = open(config.PGN_PATH, 'r')
    except:
        print("Unable to load", config.PGN_PATH)
        exit(-1)

    try:
        dataset_file = open(config.DATASET_PATH, 'w+')
    except:
        pgn_file.close()
        print("Unable to create", config.DATASET_PATH)
        exit(-1)
    dataset_file.close()

    games_created = 0
    executor = ThreadPoolExecutor(max_workers=config.DATA_GEN_THREAD_POOL_SIZE)
    while((pgn_game := chess.pgn.read_game(pgn_file)) and games_created < config.NUM_GAMES):
        moves = pgn_game.mainline_moves()
        board = pgn_game.board()

        init_turn = math.floor(6 + 15 * random.random())
        init_turn = min(pgn_game.end().ply() - 3, init_turn)
        for i, move in enumerate(moves):
            if i > init_turn:
                break
            board.push(move)
        executor.submit(_worker, board.fen())
        games_created += 1

    print("Waiting for threads")
    executor.shutdown(wait=True)
    pgn_file.close()

