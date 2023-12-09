#pragma once
#include <eval.hpp>

// Weights are tuned using the trainer in the scripts/training folder.
// Everything below the marker will be replaced by the create_header_file script.

using namespace Arcanum;

// ##MARKER##

static eval_t pawnValue          = 100;
static eval_t rookValue          = 481;
static eval_t knightValue        = 311;
static eval_t bishopValue        = 306;
static eval_t queenValue         = 919;

static eval_t doublePawnScore    = -27;
static eval_t pawnSupportScore   = 1;
static eval_t pawnBackwardScore  = -5;
static eval_t mobilityBonusKnightBegin[9]  = {-43, -34, 6, 16, 21, 22, 28, 35, 33};
static eval_t mobilityBonusKnightEnd[9]    = {-61, -42, -12, 2, 14, 32, 31, 30, 20};
static eval_t mobilityBonusBishopBegin[14] = {-28, -1, 26, 33, 35, 49, 58, 71, 76, 74, 76, 72, 74, 95};
static eval_t mobilityBonusBishopEnd[14]   = {-40, -6, 11, 30, 30, 38, 42, 52, 59, 74, 61, 72, 69, 87};
static eval_t mobilityBonusRookBegin[15]   = {-41, -5, -9, 9, -8, -3, 9, 17, 28, 30, 41, 34, 42, 46, 49};
static eval_t mobilityBonusRookEnd[15]     = {-63, 4, 36, 59, 89, 107, 119, 119, 135, 145, 146, 143, 152, 152, 156};
static eval_t mobilityBonusQueenBegin[28]  = {-10, 3, -7, 11, 23, 34, 30, 27, 34, 52, 46, 55, 53, 51, 78, 86, 86, 90, 88, 89, 105, 95, 99, 93, 100, 100, 121, 111};
static eval_t mobilityBonusQueenEnd[28]    = {-34, -11, 7, 36, 53, 71, 58, 84, 93, 111, 114, 115, 139, 140, 151, 150, 153, 159, 163, 163, 166, 154, 154, 152, 159, 167, 191, 202};
static eval_t pawnRankBonusBegin[8]        = {0, -4, 2, 13, 5, 22, 16, 0};
static eval_t pawnRankBonusEnd[8]          = {0, 10, 3, 9, 31, 49, 128, 0};
static eval_t passedPawnRankBonusBegin[8]  = {0, -3, 6, 10, 44, 67, 135, 0};
static eval_t passedPawnRankBonusEnd[8]    = {0, 11, 31, 54, 88, 129, 208, 0};
static eval_t s_kingAreaAttackScore[50]    = {19, 0, 0, 14, 3, 11, -11, -1, -7, 26, 5, 3, 12, 11, 16, 20, 25, 38, 37, 55, 49, 87, 65, 66, 96, 78, 86, 94, 141, 112, 121, 169, 150, 199, 210, 221, 232, 244, 218, 267, 279, 253, 283, 295, 307, 333, 330, 342, 354, 373};
static eval_t s_whiteKingPositionBegin[64] = {8, 6, 44, 18, -7, 26, 28, 1, -11, -8, -5, 7, 3, 5, -3, -11, -2, -6, 4, 6, 6, 6, 5, -5, 2, 6, -12, -21, -13, -10, 5, 4, 5, 5, 0, -28, -24, -1, 7, 3, -2, -1, -13, -10, -27, -5, 7, 6, 0, 5, 5, 7, 7, 0, 6, 6, 6, 6, 7, 6, -11, 1, 3, -1};
static eval_t s_kingPositionEnd[64]        = {-45, -33, -14, -18, -6, -20, -41, -59, -44, -21, -11, -4, 3, 6, -6, -21, -30, -12, -4, 1, -6, -1, 7, -22, -6, 6, 8, -7, -6, 8, -1, -6, -6, 7, 16, -7, 5, -2, -4, -11, -14, -1, -1, 10, -6, 6, 0, -19, -6, -6, -6, 7, -13, -14, -13, -18, -57, -31, -15, -7, -11, -23, -37, -69};
static eval_t pawnShelterScores[4][8]      = {{16, 21, 26, 11, 15, 29, 22, 0}, {-7, 17, -4, -32, -10, 9, 14, 0}, {9, 18, 11, -7, -9, 18, -6, 0}, {-5, 14, 7, -9, -21, -29, -24, 0}};
static eval_t centerControlScoreBegin[16]  = {0, 10, 20, 30, 55, 54, 48, 77, 64, 108, 119, 125, 139, 140, 136, 132};
