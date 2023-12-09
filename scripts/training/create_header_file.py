from model import EvalModel
import sys
import os

OUTPUT_FILE = os.path.dirname(__file__) + "\\..\\..\\src\\weights.hpp"
MARKER = "// ##MARKER##"

def main():
    if(len(sys.argv) != 2):
        print(f"usage: python {sys.argv[0]}", "[hce_model_file]")
        exit(-1)

    filepath = sys.argv[1]
    if(not os.path.exists(filepath)):
        print(f"Unable to find {filepath}")
        exit(-1)

    model = EvalModel()
    model.load_from_file(filepath)

    previous_content = None
    with open(OUTPUT_FILE, 'r') as f:
        previous_content = f.read().splitlines()

    if not previous_content:
        print(f"Unable to read {OUTPUT_FILE}")
        exit(-1)

    if MARKER not in previous_content:
        print(f"Could not find '{MARKER}' in {OUTPUT_FILE}")
        exit(-1)

    marker_line = previous_content.index(MARKER)

    with open(OUTPUT_FILE, 'w+') as f:
        f.writelines("\n".join(previous_content[:marker_line + 1]))
        f.write("\n\n")
        f.write(str(model))

if __name__ == "__main__":
    main()