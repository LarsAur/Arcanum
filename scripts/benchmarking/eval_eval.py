from subprocess import Popen, PIPE, STDOUT
import chess
import os
import sys

def main():

    if(len(sys.argv) != 2):
        print("usage:", sys.argv[0], "[UciEnginePath]")
        print("UciEngine1Path is required to have the 'ischeckmate' custom command")
        exit(0)

    engine_name = os.path.splitext(os.path.basename(sys.argv[1]))[0]

    fen_string_file = open("../eval_fen_strings.txt", 'r')
    # results_file = open(f"../results/eval_{engine_name}.txt", "w+")
    engine = Popen([sys.argv[1]], stdout=PIPE, stdin=PIPE, stderr=STDOUT)

    total_diff = 0
    percent_sum = 0
    max_diff = 0
    max_diff_fen = ""
    data_list = []

    while(True):
        fen = fen_string_file.readline()[0:-1]
        if(len(fen) == 0):
            break
        score = fen_string_file.readline()[0:-1]
        position = f"position fen {fen}\n"
        engine.stdin.write(position.encode('utf-8'))
        engine.stdin.write(b"eval\n")
        engine.stdin.flush()

        engine.stdout.flush()
        out = bytes.decode(engine.stdout.readline(), 'utf-8')[0:-1]

        diff = abs(int(out) - int(float(score)*100))
        total_diff += diff
        if diff > max_diff:
            max_diff = diff
            max_diff_fen = fen

        percent = 0 if diff == 0 else 100 * diff / (max(abs(int(out)), abs(int(float(score)*100))))
        percent_sum += percent
        data = (diff, percent, out, score, fen)
        data_list.append(data)

    print("Sort by diff:")
    data_list.sort(key= lambda x: -x[0])
    for data in data_list:
        print(f"Difference: {data[0]:>5} ({data[1]:<6.2f}%) Engine: {int(data[2]):>5}  Sf: {int(float(data[3])*100):>5}, {data[4]}")


    print("\nSort by percent:")
    data_list.sort(key= lambda x: -x[1])
    for data in data_list:
        print(f"Difference: {data[0]:>5} ({data[1]:6.2f}%) Engine: {int(data[2]):>5}  Sf: {int(float(data[3])*100):>5}, {data[4]}")


    print(f"Average Difference: {total_diff / 200}")
    print(f"Average Percentage: {percent_sum / 200:.2f}%")
    print(f"Max Difference: {max_diff} ({max_diff_fen})")
    engine.stdin.write(b"quit\n")
    engine.stdin.flush()

    fen_string_file.close()

if __name__ == "__main__":
    main()