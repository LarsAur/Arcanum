
# README:
# This file contains the configurations for the training of the hand crafted evaluation (HCE) model.
# The training uses logistic regression and is based on the following article: https://www.chessprogramming.org/Texel%27s_Tuning_Method
# When training, it is recomended to use a build which has nnue off by default, a small default transposition table and logging disabled.
# This is mainly to save some time and memory.

# Training configurations
ENGINE_PATH = "../../snapshots/Arcanum.exe" # Path to the Arcanum engine, or other uci engine supporting an 'eval' command.
PGN_PATH = "data/games.pgn"                 # File location for the pgn file used to generate initial positions. Can be downloaded from https://database.lichess.org/
DATASET_PATH = "tmp/dataset.txt"            # File location for the dataset. When creating a new dataset, the previous file has to be deleted or the name has to be changed.
INITIAL_MODEL_FILE = "data/HCE_model.txt"   # Initial model to start regression.
REGRESSION_THREAD_POOL_SIZE = 2             # Number of threads used when performing regression.
DATA_GEN_THREAD_POOL_SIZE = 10              # Number of threads used when generating dataset.
SEARCH_TIME = 50                            # Number of ms used to play each move while generating the dataset.
NUM_GAMES = 10000                           # Number of grames to generate positions from. (10K games results in ~400K positions)
