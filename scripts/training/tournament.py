from model import EvalModel
from chessgame import ChessGame
from typing import List
import chess
import random
import os
import config
from concurrent.futures import ThreadPoolExecutor

class Tournament:
    def __init__(self, engine_path, fen_strings:List[str], models:List[EvalModel]) -> None:
        self.engine_path = engine_path
        self.population_size = len(models)
        self.fen_strings = fen_strings
        self.models = models

    def run(self, num_games):
        num_top_models = config.TOP_MODELS_PLAYED # Play against the n top models in the tournament
        self.results = 2 * (1+num_top_models) * num_games * self.population_size * [(None, None, None)]

        for i, model in enumerate(self.models):
            model.writeToFile(f"tmp/model_{i}.txt")

        executor = ThreadPoolExecutor(max_workers=config.THREAD_POOL_SIZE)

        print("Starting Threads")
        for model_index in range(self.population_size):
            executor.submit(self._worker, model_index, num_games, num_top_models)

        print("Waiting for threads")
        executor.shutdown(wait=True)


    def _worker(self, model_index, num_games, num_top_models):
        fens = random.choices(self.fen_strings, k=num_games)
        for opponent_index in range(num_top_models):

            if(opponent_index == model_index):
                opponent_index = num_top_models

            for i, fen in enumerate(fens):
                model_options = [("UseNNUE", "false"), ("HCEWeightFile", os.path.abspath(f"tmp/model_{model_index}.txt"))]
                opponent_options = [("UseNNUE", "false"), ("HCEWeightFile", os.path.abspath(f"tmp/model_{opponent_index}.txt"))]

                # Model plays as white
                game = ChessGame(self.engine_path, self.engine_path, fen, model_options, opponent_options)
                game.play(config.GAME_DEPTH)
                result = game.get_result()
                self.results[model_index * (1+num_top_models) * num_games * 2 + opponent_index * num_games * 2 + i] = (model_index, opponent_index, result)
                # Model plays as black
                game = ChessGame(self.engine_path, self.engine_path, fen, opponent_options, model_options)
                game.play(config.GAME_DEPTH)
                result = game.get_result()
                self.results[model_index * (1+num_top_models) * num_games * 2 + opponent_index * num_games * 2 + i + 1] = (opponent_index, model_index, result)

        print("Thread", model_index, "Finished")

    def getFitness(self) -> List[float]:
        max_fitness = self.population_size * [0]
        fitness = self.population_size * [0]
        normal_fitness = self.population_size * [0]
        for w, b, result in self.results:

            if(w == None): continue

            max_fitness[w] += 1
            max_fitness[b] += 1
            if result[0] == ChessGame.CHECKMATE:
                if result[1] == chess.WHITE:
                    fitness[w] += 1
                else:
                    fitness[b] += 1
            elif result[0] == ChessGame.STALEMATE or result[0] == ChessGame.DRAW or result[0] == ChessGame.DRAW50 or result[0] == ChessGame.DRAWREPEAT:
                fitness[w] += 0.5
                fitness[b] += 0.5

        for i in range(self.population_size):
            normal_fitness[i] = float(fitness[i]) / float(max_fitness[i])

        return normal_fitness