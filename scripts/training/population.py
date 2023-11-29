from model import EvalModel
from typing import List
import random
import math
import os
class Population:
    def __init__(self, size:int):
        self.size = size
        self.models = [EvalModel() for _ in range(size)]

    def initilize_population(self, rate, max_value, initial_model_file=""):
        for i, model in enumerate(self.models):
            if(os.path.exists(f"tmp/model_{i}.txt")):
                model.loadFromFile(f"tmp/model_{i}.txt")
            elif(os.path.exists(initial_model_file)):
                model.loadFromFile(initial_model_file)
                if(i > 0):
                    model.mutate(rate, max_value)
            else:
                model.mutate(rate, max_value)

    def create_next_generation(self, normalized_fitness:List[float], rate:float, max_value:int):
        model_fitness = list(zip(self.models, normalized_fitness))
        model_fitness.sort(key=lambda x: x[1], reverse=True) # Place largest fitness first
        kept_models = [model_fitness[i][0] for i in range(math.floor(self.size / 2))] # Keep the half of the models with the highest scores

        normalized_fitness.sort(reverse=True)
        print("Kept fitness", normalized_fitness[0:math.floor(self.size / 2)])

        # Create children
        self.models = kept_models
        for i in range(len(kept_models) - 1):
            child = EvalModel()
            child.copy(kept_models[i*2])
            child.merge(kept_models[i*2 + 1])
            child.mutate(rate, max_value)
            self.models.append(child)

        # The loop will create 1 less child than we need
        # Create a child from the best and a random model
        child = EvalModel()
        child.copy(kept_models[0])
        child.merge(kept_models[random.randint(0, (self.size / 2) - 1)])
        child.mutate(rate, max_value)
        self.models.append(child)

    def getModels(self):
        return self.models