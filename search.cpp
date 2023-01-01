#include "search.hpp"
#include <cmath>

GameProgress get_progress(int mv1, int mv2) {
    return (mv1 <= 1300 && mv2 <= 1300) ? ENDGAME : MIDGAME;
}

template <Color Us>
int evaluate(const Position& pos) {
    // Material value
    int mv;
    int mv_us = 0;
    int mv_them = 0;
    for (size_t i = 0; i < NPIECE_TYPES - 1; i++) {
        mv_us += __builtin_popcountll(pos.bitboard_of(Us, (PieceType) i)) * piece_values[i];
    }
    for (size_t i = 0; i < NPIECE_TYPES - 1; i++) {
        mv_them += __builtin_popcountll(pos.bitboard_of(~Us, (PieceType) i)) * piece_values[i];
    }
    mv = mv_us - mv_them;
    GameProgress progress = get_progress(mv_us, mv_them);

    // Color advantage
    int ca = 0;
    if (progress == MIDGAME) {
        ca = (Us == WHITE) ? 15 : -15;
    }

    // Center control
    int cc = 0;
    if (progress == MIDGAME) {
        cc += (color_of(pos.at(d5)) == Us && type_of(pos.at(d5)) != KING) ? 25 : -25;
        cc += (color_of(pos.at(e5)) == Us && type_of(pos.at(e5)) != KING) ? 25 : -25;
        cc += (color_of(pos.at(d4)) == Us && type_of(pos.at(d4)) != KING) ? 25 : -25;
        cc += (color_of(pos.at(e4)) == Us && type_of(pos.at(e4)) != KING) ? 25 : -25;
    } else if (progress == ENDGAME) {
        cc += (color_of(pos.at(d5)) == Us) ? 25 : -25;
        cc += (color_of(pos.at(e5)) == Us) ? 25 : -25;
        cc += (color_of(pos.at(d4)) == Us) ? 25 : -25;
        cc += (color_of(pos.at(e4)) == Us) ? 25 : -25;
    } else {
        throw std::logic_error("Invalid progress value");
    }

    // Knight placement
    int np = 0;
    np -= __builtin_popcountll(pos.bitboard_of(Us, KNIGHT) & MASK_FILE[AFILE] & MASK_RANK[RANK1] & MASK_FILE[HFILE] & MASK_RANK[RANK8]) * 50;
    np += __builtin_popcountll(pos.bitboard_of(~Us, KNIGHT) & MASK_FILE[AFILE] & MASK_RANK[RANK1] & MASK_FILE[HFILE] & MASK_RANK[RANK8]) * 50;

    // King placement
    /*
    function distance(x1, y1, x2, y2) {
        return Math.hypot(x2 - x1, y2 - y1);
    }

    let table = [[], []];
    for (let y = 0; y < 8; y++) {
        for (let x = 0; x < 8; x++) {
            table[0].push(Math.round(-Math.min(distance(x, y, 0, 7), distance(x, y, 7, 7)) * 4));
            table[1].push(Math.round(-Math.min(distance(x, y, 0, 0), distance(x, y, 7, 0)) * 4));
        }
    }
    */
    static constexpr int king_pcsq_table[2][64] = {
        {-28, -28, -29, -30, -30, -29, -28, -28, -24, -24, -25, -27, -27, -25, -24, -24, -20, -20, -22, -23, -23, -22, -20, -20, -16, -16, -18, -20, -20, -18, -16, -16, -12, -13, -14, -17, -17, -14, -13, -12, -8, -9, -11, -14, -14, -11, -9, -8, -4, -6, -9, -13, -13, -9, -6, -4, 0, -4, -8, -12, -12, -8, -4, 0},
        {0, -4, -8, -12, -12, -8, -4, 0, -4, -6, -9, -13, -13, -9, -6, -4, -8, -9, -11, -14, -14, -11, -9, -8, -12, -13, -14, -17, -17, -14, -13, -12, -16, -16, -18, -20, -20, -18, -16, -16, -20, -20, -22, -23, -23, -22, -20, -20, -24, -24, -25, -27, -27, -25, -24, -24, -28, -28, -29, -30, -30, -29, -28, -28},
    };
    int kp = 0;
    kp += king_pcsq_table[Us][__builtin_clzll(pos.bitboard_of(Us, KING))];
    kp -= king_pcsq_table[~Us][__builtin_clzll(pos.bitboard_of(~Us, KING))];

    return std::pow(mv + ca + cc + np + kp, 1.0075);
}

template <Color Us>
int negamax(Position& pos, int alpha, int beta, unsigned int depth) {
    if (depth == 0) {
        return evaluate<Us>(pos);
    }

    int ret;
    MoveList<Us> children(pos);
    if (children.size() == 0) {
        std::cout << depth << std::endl;
        ret = -piece_values[KING];
    } else {
        for (Move child : children) {
            pos.play<Us>(child);
            int score = -negamax<~Us>(pos, -beta, -alpha, depth - 1);
            pos.undo<Us>(child);

            if (score >= beta) {
                ret = beta;
                goto cutoff;
            } else if (score > alpha) {
                alpha = score;
            }
        }
        ret = alpha;
    }

cutoff:
    return ret;
}

template <Color Us>
Move find_best_move(Position& pos, unsigned int depth, tp::ThreadPool& pool, int* best_move_score_ret) {
    MoveList<Us> children(pos);

    Move best_move;
    int best_move_score = -piece_values[KING];

    std::mutex mtx;
    std::vector<std::shared_ptr<tp::Task>> tasks;

    for (Move child : children) {
        tasks.push_back(pool.schedule([pos, depth, child, &best_move, &best_move_score, &mtx](void*) mutable {
            pos.play<Us>(child);
            int score = -negamax<~Us>(pos, -piece_values[KING], piece_values[KING], depth - 1);
            mtx.lock();
            if (score > best_move_score) {
                best_move = child;
                best_move_score = score;
            }
            mtx.unlock();
        }));
    }

    for (auto& task : tasks) {
        task->await();
    }

    *best_move_score_ret = best_move_score;
    return best_move;
}

template int evaluate<WHITE>(const Position& pos);
template int evaluate<BLACK>(const Position& pos);

template int negamax<WHITE>(Position& pos, int alpha, int beta, unsigned int depth);
template int negamax<BLACK>(Position& pos, int alpha, int beta, unsigned int depth);

template Move find_best_move<WHITE>(Position& pos, unsigned int depth, tp::ThreadPool& pool, int* best_move_score_ret);
template Move find_best_move<BLACK>(Position& pos, unsigned int depth, tp::ThreadPool& pool, int* best_move_score_ret);
