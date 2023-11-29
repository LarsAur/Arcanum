from model import EvalModel
from tournament import Tournament
from population import Population
import config

def main():
    fen_strings = []
    with open(config.FEN_LIST) as f:
        lines = f.readlines()
        lines = lines[1::2]
        fen_strings = [line.strip() for line in lines]

    population = Population(config.POPULATION_SIZE)
    population.initilize_population(config.MUTATION_RATE, config.MAX_MUTATION, config.INITIAL_MODEL_FILE)

    for _ in range(0, config.TRAINING_CYCLES):
        tournament = Tournament(config.ENGINE_PATH, fen_strings, population.getModels())
        tournament.run(1)
        normal_fitness = tournament.getFitness()

        population.create_next_generation(normal_fitness, config.MUTATION_RATE, config.MAX_MUTATION)
        print("Fitness:", max(normal_fitness))
        print("Hash: ", hex(population.getModels()[0].hash()))
        print(population.getModels()[0])


if __name__ == "__main__":
    main()