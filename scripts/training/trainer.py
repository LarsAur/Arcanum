from model import EvalModel
import config
from subprocess import Popen, PIPE, STDOUT
from concurrent.futures import ThreadPoolExecutor
import time
import os
import math

p_errors = None
n_errors = None

def get_set_hce_file_option(path:str) -> bytes:
    s = f'setoption name HCEWeightFile value {path}\n'
    return s.encode("utf-8")

def get_set_position_command(fen:str) -> bytes:
    s = f'position fen {fen}'
    return s.encode("utf-8")

def sigmoid(q:int) -> float:
    return 1.0 / (1 + 10**(-float(q)/400)) # k = 1

def _worker(model:EvalModel, base_error:float, index:int, delta:int):
    t = time.time()
    _model = EvalModel()
    _weight_delta = len(model.get_weights()) * [0]
    _model.copy(model)
    # Positive Delta
    _weight_delta[index] = delta
    _model.add_delta(_weight_delta)
    err = get_model_error(_model, index)
    global p_errors
    p_errors[index] = err
    # Negative Delta if positive delta increases error
    if(err > base_error):
        _weight_delta[index] = -delta * 2
        _model.add_delta(_weight_delta)
        err = get_model_error(_model, index)
        global n_errors
        n_errors[index] = err

    print(f"Finished calculating delta-error for index {index:>3}. Used {(time.time() - t):.2f} seconds.")

def get_model_error(model:EvalModel, file_index=-1) -> float:
    # Create engine subprocess
    engine_process = Popen([config.ENGINE_PATH], stdout=PIPE, stdin=PIPE, stderr=PIPE)
    engine_process.stdin.write(b'setoption name hash value 1\n')
    engine_process.stdin.write(b'setoption name usennue value false\n')

    model_file_path = "tmp/tmp_model.txt"
    if(file_index != -1):
        model_file_path = f"tmp/tmp_model_{file_index}.txt"

    model.write_to_file(model_file_path)
    engine_process.stdin.write(get_set_hce_file_option(model_file_path))

    error_sum = 0
    num_positions = 0
    with open(config.DATASET_PATH) as f:
        while(True):
            meta_data = f.readline()
            if(meta_data == ""):
                break

            # Get the numbers from the string
            header = meta_data.split()
            game_score = float(header[1])
            positions = int(header[2])
            lines = [next(f) for _ in range(positions)]

            for p in lines:
                engine_process.stdin.write(get_set_position_command(fen=p))
                engine_process.stdin.write(b'eval\n')
                engine_process.stdin.flush()
                engine_process.stdout.flush()
                q = int(engine_process.stdout.readline().decode('utf-8'))
                error_sum += (game_score - sigmoid(q))**2
                num_positions += 1

    os.remove(model_file_path)

    return float(error_sum) / num_positions

def train():

    # Initialize model
    model = EvalModel()
    model.load_from_file(config.INITIAL_MODEL_FILE)
    num_weights = model.num_weights()
    delta = 1

    while(True):
        print("Calculating current error")
        current_model_error = get_model_error(model)
        print(f"Current error: {current_model_error:.5f}")
        global p_errors
        p_errors = num_weights * [1]
        global n_errors
        n_errors = num_weights * [1]
        executor = ThreadPoolExecutor(max_workers=config.REGRESSION_THREAD_POOL_SIZE)

        for i in range(num_weights):
            executor.submit(_worker, model, current_model_error, i, delta)
        executor.shutdown(wait=True)

        delta_weights = num_weights * [0]
        for i in range(num_weights):
            if(p_errors[i] < current_model_error):
                delta_weights[i] = delta
            elif(n_errors[i] < current_model_error):
                delta_weights[i] = -delta

        model.add_delta(delta_weights)
        time_string = time.ctime().replace("  ", "_").replace(" ", "_").replace(":", "-")
        model.write_to_file(f"tmp/model_{time_string}.txt")

        print("Changed weights:", delta_weights)
        print(str(model))
