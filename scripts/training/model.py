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
        string += "static eval_t pawnValue          = 100;\n"
        string += "static eval_t rookValue          = " + str(self.weights[0]) + ";\n"
        string += "static eval_t knightValue        = " + str(self.weights[1]) + ";\n"
        string += "static eval_t bishopValue        = " + str(self.weights[2]) + ";\n"
        string += "static eval_t queenValue         = " + str(self.weights[3]) + ";\n\n"
        string += "static eval_t doublePawnScore    = " + str(self.weights[4]) + ";\n"
        string += "static eval_t pawnSupportScore   = " + str(self.weights[5]) + ";\n"
        string += "static eval_t pawnBackwardScore  = " + str(self.weights[6]) + ";\n"
        string += "static eval_t mobilityBonusKnightBegin[9]  = " + self._build_array(7, 9)    + ";\n"
        string += "static eval_t mobilityBonusKnightEnd[9]    = " + self._build_array(16, 9)   + ";\n"
        string += "static eval_t mobilityBonusBishopBegin[14] = " + self._build_array(25, 14)  + ";\n"
        string += "static eval_t mobilityBonusBishopEnd[14]   = " + self._build_array(39, 14)  + ";\n"
        string += "static eval_t mobilityBonusRookBegin[15]   = " + self._build_array(53, 15)  + ";\n"
        string += "static eval_t mobilityBonusRookEnd[15]     = " + self._build_array(68, 15)  + ";\n"
        string += "static eval_t mobilityBonusQueenBegin[28]  = " + self._build_array(83, 28)  + ";\n"
        string += "static eval_t mobilityBonusQueenEnd[28]    = " + self._build_array(111, 28) + ";\n"
        string += "static eval_t pawnRankBonusBegin[8]        = " + self._build_array(139, 8) + ";\n"
        string += "static eval_t pawnRankBonusEnd[8]          = " + self._build_array(147, 8) + ";\n"
        string += "static eval_t passedPawnRankBonusBegin[8]  = " + self._build_array(155, 8) + ";\n"
        string += "static eval_t passedPawnRankBonusEnd[8]    = " + self._build_array(163, 8) + ";\n"
        string += "static eval_t s_kingAreaAttackScore[50]    = " + self._build_array(171, 50) + ";\n"
        string += "static eval_t s_whiteKingPositionBegin[64] = " + self._build_array(221, 64) + ";\n"
        string += "static eval_t s_kingPositionEnd[64]        = " + self._build_array(285, 64) + ";\n"
        string += "static eval_t pawnShelterScores[4][8]      = " + self._build_2d_array(349, 4, 8) + ";\n"
        string += "static eval_t centerControlScoreBegin[16]  = " + self._build_array(381, 16) + ";\n"

        return string
