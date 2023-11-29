
# README:
# This file contains the configurations for the training of the hand crafted evaluation (HCE) model.
# It optimizes the weights using genetic algorithms
# When running the test, it is adviced to use a build of Arcanum where nnue is off
# by default and the transposition table is small (e.g. 4MB).
# It is recomended to not just set this though UCI, but when building.
# This is to save time and memory when training, as training is done with
# shallow depth (large TT not required) and many instances in parallel.

# It is ok to stop the training before it is finished.
# When restarted, it will continue using the results from the previous training cycle

# Following is the configuration of the training

ENGINE_PATH = "../../snapshots/Arcanum.exe"
FEN_LIST = "data/fen_strings.txt"
INITIAL_MODEL_FILE = "data/HCE_model.txt"
MODEL_FOLDER_PATH = "tmp"
TRAINING_CYCLES = 100
POPULATION_SIZE = 200
MAX_MUTATION = 20
MUTATION_RATE = 0.01
THREAD_POOL_SIZE = 40
GAME_DEPTH = 3
TOP_MODELS_PLAYED = 10 # Play the N top models each tournament
