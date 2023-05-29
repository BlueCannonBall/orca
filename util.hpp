#pragma once

#include "logger.hpp"
#include "surge/src/position.h"
#include "surge/src/types.h"
#include <prophet.h>

#define BOTH_COLOR_CALL(function, ...)       (function<WHITE>(__VA_ARGS__) | function<BLACK>(__VA_ARGS__))
#define DYN_COLOR_CALL(function, color, ...) ({                                   \
    color == WHITE ? function<WHITE>(__VA_ARGS__) : function<BLACK>(__VA_ARGS__); \
})

extern Logger logger;

enum GameProgress {
    MIDGAME,
    ENDGAME
};

float lerp(float a, float b, float t);

GameProgress get_progress(int mv1, int mv2);

template <Color Us>
bool has_non_pawn_material(const Position& pos);

template <Color C>
MoveFlags generate_move_flags(const Position& pos, Square from, Square to);

template <Color C>
MoveFlags generate_attack_move_flags(const Position& pos, Square from, Square to);

ProphetBoard generate_prophet_board(const Position& pos);
