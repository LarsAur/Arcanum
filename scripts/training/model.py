import random
import math
from typing import List
class EvalModel:

    NUM_WEIGHTS = 397

    def __init__(self):
        self.weights = EvalModel.NUM_WEIGHTS * [0]

    def copy(self, model:"EvalModel"):
        self.weights = model.get_weights().copy()

    def add_delta(self, delta_weights:List[int]):
        for i in range(len(self.weights)):
            self.weights[i] += delta_weights[i]

    def _build_array(self, start_idx:int, size:int) -> str:
        arr_string = "{" + str(self.weights[start_idx:start_idx+size])[1:-1] + "}"
        return arr_string

    def _build_2d_array(self, start_idx:int, size1:int, size2) -> str:
        arr_string = "{" + ", ".join([self._build_array(start_idx + size2 * i, size2) for i in range(size1)]) + "}"
        return arr_string

    def load_from_file(self, filename):
        line = ""
        with open(filename, 'r') as f:
            line = f.readline()

        tokens = line.split(",")
        self.weights = [int(t) for t in tokens]
        print("Loaded", filename)

    def write_to_file(self, filename):
        with open(filename, 'w+') as f:
            f.write(str(self.weights)[1:-1])

    def get_weights(self):
        return self.weights

    def num_weights(self):
        return EvalModel.NUM_WEIGHTS

    def __str__(self) -> str:
        string = ""
        string += "pawnValue = 100\n"
        string += "rookValue = "   + str(self.weights[0]) + "\n"
        string += "knightValue = " + str(self.weights[1]) + "\n"
        string += "bishopValue = " + str(self.weights[2]) + "\n"
        string += "queenValue = "  + str(self.weights[3]) + "\n"

        string += "doublePawnScore = " + str(self.weights[4]) + "\n"
        string += "pawnSupportScore = " + str(self.weights[5]) + "\n"
        string += "pawnBackwardScore = " + str(self.weights[6]) + "\n"

        string += "mobilityBonusKnightBegin = " + self._build_array(7, 9)   + "\n"
        string += "mobilityBonusKnightEnd = "   + self._build_array(16, 9)  + "\n"
        string += "mobilityBonusBishopBegin = " + self._build_array(25, 14) + "\n"
        string += "mobilityBonusBishopEnd = "   + self._build_array(39, 14) + "\n"
        string += "mobilityBonusRookBegin = "   + self._build_array(53, 15) + "\n"
        string += "mobilityBonusRookEnd = "     + self._build_array(68, 15) + "\n"
        string += "mobilityBonusQueenBegin = "  + self._build_array(83, 28) + "\n"
        string += "mobilityBonusQueenEnd = "    + self._build_array(111, 28) + "\n"

        string += "pawnRankBonusBegin = "       + self._build_array(139, 8) + "\n"
        string += "pawnRankBonusEnd = "         + self._build_array(147, 8) + "\n"
        string += "passedPawnRankBonusBegin = " + self._build_array(155, 8) + "\n"
        string += "passedPawnRankBonusEnd = "   + self._build_array(163, 8) + "\n"

        string += "s_kingAreaAttackScore = "    + self._build_array(171, 50) + "\n"
        string += "s_whiteKingPositionBegin = " + self._build_array(221, 64)  + "\n"
        string += "s_blackKingPositionBegin = Mirrored ^ \n"
        string += "s_kingPositionEnd = "        + self._build_array(285, 64)  + "\n"

        string += "pawnShelterScores"           + self._build_2d_array(349, 4, 8) + "\n"
        string += "centerControlScoreBegin"     + self._build_array(381, 16) + "\n"

        return string
