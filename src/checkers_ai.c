#include <stdint.h>

#include <zgdk/utils/rand.h>

#include "checkers_ai.h"

#define SCORE_NONE 0
#define SCORE_SIMPLE 10
#define SCORE_CAPTURE 40
#define SCORE_PROMOTION 30
#define SCORE_KING 5
#define SCORE_SAFE 8
#define SCORE_RISK_PENALTY 12
#define SCORE_CHAIN_CAPTURE 35
#define SCORE_CAPTURE_KING_BONUS 20
#define SCORE_ADVANCE 2
#define SCORE_CENTER 3
#define SCORE_LEAVE_BACK_ROW_PENALTY 5
#define SCORE_PLAYER_REPLY_CAPTURE 18
#define SCORE_PLAYER_REPLY_KING_BONUS 10
#define AI_CHAIN_DEPTH_MAX 6

static uint8_t ai_difficulty = CHECKERS_AI_DEFAULT_DIFFICULTY;

static uint8_t rand_bounded_u8(uint8_t max_inclusive)
{
    uint8_t range = (uint8_t)(max_inclusive + FLAG_ON);
    uint8_t limit;
    uint8_t x;

    if (range == FLAG_OFF) {
        return FLAG_OFF;
    }

    limit = (uint8_t)(U8_MAX_VALUE - (U8_MAX_VALUE % range));
    do {
        x = (uint8_t)rand8();
    } while (x > limit);

    return (uint8_t)(x % range);
}

static void store_candidate(
    CandidateMove* candidate,
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type)
{
    candidate->from_x = from_x;
    candidate->from_y = from_y;
    candidate->to_x = to_x;
    candidate->to_y = to_y;
    candidate->piece = piece;
    candidate->move_type = move_type;
    candidate->score = SCORE_NONE;
}

static uint8_t captured_piece_for_move(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y, uint8_t move_type)
{
    if (move_type == MOVE_CAPTURE) {
        uint8_t middle_x = (uint8_t)((from_x + to_x) / 2U);
        uint8_t middle_y = (uint8_t)((from_y + to_y) / 2U);

        return board[middle_y][middle_x];
    }

    return PIECE_EMPTY;
}

static uint8_t square_can_be_captured_by_player(uint8_t x, uint8_t y)
{
    for (int8_t dy = -1; dy <= 1; dy += 2) {
        int8_t enemy_y = (int8_t)(y + dy);
        int8_t landing_y = (int8_t)(y - dy);

        if (coord_on_board(enemy_y) && coord_on_board(landing_y)) {
            for (int8_t dx = -1; dx <= 1; dx += 2) {
                int8_t enemy_x = (int8_t)(x + dx);
                int8_t landing_x = (int8_t)(x - dx);

                if (coord_on_board(enemy_x) && coord_on_board(landing_x)) {
                    uint8_t enemy_piece = board[(uint8_t)enemy_y][(uint8_t)enemy_x];

                    if (piece_is_player(enemy_piece) &&
                        board[(uint8_t)landing_y][(uint8_t)landing_x] == PIECE_EMPTY &&
                        move_type_for_piece(
                            (uint8_t)enemy_x,
                            (uint8_t)enemy_y,
                            (uint8_t)landing_x,
                            (uint8_t)landing_y,
                            enemy_piece) == MOVE_CAPTURE) {
                        return FLAG_ON;
                    }
                }
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t computer_lands_in_danger(
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type)
{
    uint8_t from_piece = board[from_y][from_x];
    uint8_t to_piece = board[to_y][to_x];
    uint8_t middle_x = 0;
    uint8_t middle_y = 0;
    uint8_t middle_piece = PIECE_EMPTY;
    uint8_t in_danger;

    board[from_y][from_x] = PIECE_EMPTY;

    if (move_type == MOVE_CAPTURE) {
        middle_x = (uint8_t)((from_x + to_x) / 2U);
        middle_y = (uint8_t)((from_y + to_y) / 2U);
        middle_piece = board[middle_y][middle_x];
        board[middle_y][middle_x] = PIECE_EMPTY;
    }

    board[to_y][to_x] = piece_promotes_at(piece, to_y) ? PIECE_COMPUTER_KING : piece;
    in_danger = square_can_be_captured_by_player(to_x, to_y);

    board[to_y][to_x] = to_piece;
    if (move_type == MOVE_CAPTURE) {
        board[middle_y][middle_x] = middle_piece;
    }
    board[from_y][from_x] = from_piece;

    return in_danger;
}

static uint8_t score_computer_move(
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type)
{
    uint8_t score = (move_type == MOVE_CAPTURE) ? SCORE_CAPTURE : SCORE_SIMPLE;
    uint8_t captured_piece = captured_piece_for_move(from_x, from_y, to_x, to_y, move_type);

    if (piece_is_king(captured_piece)) {
        score = (uint8_t)(score + SCORE_CAPTURE_KING_BONUS);
    }
    if (piece_promotes_at(piece, to_y)) {
        score = (uint8_t)(score + SCORE_PROMOTION);
    }
    if (piece == PIECE_COMPUTER_MAN) {
        score = (uint8_t)(score + (to_y * SCORE_ADVANCE));
    }
    if (piece_is_king(piece)) {
        score = (uint8_t)(score + SCORE_KING);
    }
    if (to_x >= 2U && to_x <= 5U) {
        score = (uint8_t)(score + SCORE_CENTER);
    }
    if (piece == PIECE_COMPUTER_MAN && from_y == COMPUTER_BACK_ROW && move_type == MOVE_SIMPLE) {
        score = (score > SCORE_LEAVE_BACK_ROW_PENALTY) ?
            (uint8_t)(score - SCORE_LEAVE_BACK_ROW_PENALTY) :
            SCORE_NONE;
    }
    if (computer_lands_in_danger(from_x, from_y, to_x, to_y, piece, move_type)) {
        score = (score > SCORE_RISK_PENALTY) ? (uint8_t)(score - SCORE_RISK_PENALTY) : SCORE_NONE;
    } else {
        score = (uint8_t)(score + SCORE_SAFE);
    }

    return score;
}

static uint8_t piece_after_move(uint8_t piece, uint8_t to_y)
{
    if (piece == PIECE_PLAYER_MAN && to_y == PLAYER_PROMOTION_ROW) {
        return PIECE_PLAYER_KING;
    }
    if (piece == PIECE_COMPUTER_MAN && to_y == COMPUTER_PROMOTION_ROW) {
        return PIECE_COMPUTER_KING;
    }
    return piece;
}

static uint8_t score_player_best_capture_reply_on_board(void)
{
    uint8_t best_reply_score = SCORE_NONE;

    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            uint8_t player_piece = board[y][x];

            if (piece_is_player(player_piece)) {
                for (int8_t dy = -1; dy <= 1; dy += 2) {
                    int8_t reply_y = (int8_t)(y + (dy * MOVE_DISTANCE_CAPTURE));

                    if (coord_on_board(reply_y)) {
                        for (int8_t dx = -1; dx <= 1; dx += 2) {
                            int8_t reply_x = (int8_t)(x + (dx * MOVE_DISTANCE_CAPTURE));

                            if (coord_on_board(reply_x) &&
                                move_type_for_piece(x, y, (uint8_t)reply_x, (uint8_t)reply_y, player_piece) == MOVE_CAPTURE) {
                                uint8_t captured_piece = captured_piece_for_move(x, y, (uint8_t)reply_x, (uint8_t)reply_y, MOVE_CAPTURE);
                                uint8_t reply_score = SCORE_PLAYER_REPLY_CAPTURE;

                                if (piece_is_king(captured_piece)) {
                                    reply_score = (uint8_t)(reply_score + SCORE_PLAYER_REPLY_KING_BONUS);
                                }
                                if (reply_score > best_reply_score) {
                                    best_reply_score = reply_score;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return best_reply_score;
}

static uint8_t score_player_capture_reply_after_computer_move(
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type)
{
    uint8_t from_piece = board[from_y][from_x];
    uint8_t to_piece = board[to_y][to_x];
    uint8_t middle_x = 0;
    uint8_t middle_y = 0;
    uint8_t middle_piece = PIECE_EMPTY;
    uint8_t reply_score;

    board[from_y][from_x] = PIECE_EMPTY;
    if (move_type == MOVE_CAPTURE) {
        middle_x = (uint8_t)((from_x + to_x) / 2U);
        middle_y = (uint8_t)((from_y + to_y) / 2U);
        middle_piece = board[middle_y][middle_x];
        board[middle_y][middle_x] = PIECE_EMPTY;
    }
    board[to_y][to_x] = piece_after_move(piece, to_y);
    reply_score = score_player_best_capture_reply_on_board();

    board[to_y][to_x] = to_piece;
    if (move_type == MOVE_CAPTURE) {
        board[middle_y][middle_x] = middle_piece;
    }
    board[from_y][from_x] = from_piece;

    return reply_score;
}

static uint8_t score_computer_capture_chain(uint8_t from_x, uint8_t from_y, uint8_t piece, uint8_t depth)
{
    uint8_t best_score = SCORE_NONE;

    if (depth >= AI_CHAIN_DEPTH_MAX) {
        return SCORE_NONE;
    }

    for (int8_t dy = -1; dy <= 1; dy += 2) {
        int8_t to_y = (int8_t)(from_y + (dy * MOVE_DISTANCE_CAPTURE));

        if (coord_on_board(to_y)) {
            for (int8_t dx = -1; dx <= 1; dx += 2) {
                int8_t to_x = (int8_t)(from_x + (dx * MOVE_DISTANCE_CAPTURE));

                if (coord_on_board(to_x)) {
                    uint8_t move_type = move_type_for_piece(from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece);

                    if (move_type == MOVE_CAPTURE) {
                        uint8_t middle_x = (uint8_t)((from_x + (uint8_t)to_x) / 2U);
                        uint8_t middle_y = (uint8_t)((from_y + (uint8_t)to_y) / 2U);
                        uint8_t captured_piece = board[middle_y][middle_x];
                        uint8_t moved_piece = piece_after_move(piece, (uint8_t)to_y);
                        uint8_t score = SCORE_CHAIN_CAPTURE;
                        uint8_t promotes = piece_promotes_at(piece, (uint8_t)to_y);

                        board[from_y][from_x] = PIECE_EMPTY;
                        board[middle_y][middle_x] = PIECE_EMPTY;
                        board[(uint8_t)to_y][(uint8_t)to_x] = moved_piece;

                        if (!promotes) {
                            score = (uint8_t)(score + score_computer_capture_chain((uint8_t)to_x, (uint8_t)to_y, moved_piece, (uint8_t)(depth + 1U)));
                        }

                        board[(uint8_t)to_y][(uint8_t)to_x] = PIECE_EMPTY;
                        board[middle_y][middle_x] = captured_piece;
                        board[from_y][from_x] = piece;

                        if (score > best_score) {
                            best_score = score;
                        }
                    }
                }
            }
        }
    }

    return best_score;
}

static uint8_t score_computer_move_with_chain(
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type)
{
    uint8_t score = score_computer_move(from_x, from_y, to_x, to_y, piece, move_type);

    if (move_type == MOVE_SIMPLE) {
        uint8_t player_reply_score = score_player_capture_reply_after_computer_move(from_x, from_y, to_x, to_y, piece, move_type);

        score = (score > player_reply_score) ? (uint8_t)(score - player_reply_score) : SCORE_NONE;
    }

    if (move_type == MOVE_CAPTURE) {
        uint8_t middle_x = (uint8_t)((from_x + to_x) / 2U);
        uint8_t middle_y = (uint8_t)((from_y + to_y) / 2U);
        uint8_t captured_piece = board[middle_y][middle_x];
        uint8_t moved_piece = piece_after_move(piece, to_y);
        uint8_t promotes = piece_promotes_at(piece, to_y);

        board[from_y][from_x] = PIECE_EMPTY;
        board[middle_y][middle_x] = PIECE_EMPTY;
        board[to_y][to_x] = moved_piece;

        if (promotes || !piece_has_capture(to_x, to_y)) {
            uint8_t player_reply_score = score_player_best_capture_reply_on_board();

            score = (score > player_reply_score) ? (uint8_t)(score - player_reply_score) : SCORE_NONE;
        }
        if (!promotes) {
            score = (uint8_t)(score + score_computer_capture_chain(to_x, to_y, moved_piece, 1));
        }

        board[to_y][to_x] = PIECE_EMPTY;
        board[middle_y][middle_x] = captured_piece;
        board[from_y][from_x] = piece;
    }

    return score;
}

static void remember_candidate(
    CandidateMove* best,
    uint8_t from_x,
    uint8_t from_y,
    uint8_t to_x,
    uint8_t to_y,
    uint8_t piece,
    uint8_t move_type,
    uint8_t score_chain)
{
    uint8_t score = score_chain ?
        score_computer_move_with_chain(from_x, from_y, to_x, to_y, piece, move_type) :
        score_computer_move(from_x, from_y, to_x, to_y, piece, move_type);

    if (best->move_type == MOVE_NONE || score > best->score ||
        (score == best->score && (rand8() & 1U))) {
        store_candidate(best, from_x, from_y, to_x, to_y, piece, move_type);
        best->score = score;
    }
}

static uint8_t first_piece_move_for_distance(CandidateMove* candidate, uint8_t from_x, uint8_t from_y, uint8_t distance)
{
    uint8_t piece = board[from_y][from_x];

    for (int8_t dy = -1; dy <= 1; dy += 2) {
        int8_t to_y = (int8_t)(from_y + (dy * distance));

        if (coord_on_board(to_y)) {
            for (int8_t dx = -1; dx <= 1; dx += 2) {
                int8_t to_x = (int8_t)(from_x + (dx * distance));

                if (coord_on_board(to_x)) {
                    uint8_t move_type = move_type_for_piece(from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece);

                    if (move_type != MOVE_NONE) {
                        store_candidate(candidate, from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type);
                        return FLAG_ON;
                    }
                }
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t random_piece_move_for_distance(CandidateMove* candidate, uint8_t from_x, uint8_t from_y, uint8_t distance)
{
    uint8_t piece = board[from_y][from_x];
    uint8_t match_count = 0;

    for (int8_t dy = -1; dy <= 1; dy += 2) {
        int8_t to_y = (int8_t)(from_y + (dy * distance));

        if (coord_on_board(to_y)) {
            for (int8_t dx = -1; dx <= 1; dx += 2) {
                int8_t to_x = (int8_t)(from_x + (dx * distance));

                if (coord_on_board(to_x)) {
                    uint8_t move_type = move_type_for_piece(from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece);

                    if (move_type != MOVE_NONE) {
                        if (rand_bounded_u8(match_count) == 0U) {
                            store_candidate(candidate, from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type);
                        }
                        match_count++;
                    }
                }
            }
        }
    }

    return (match_count != 0U) ? FLAG_ON : FLAG_OFF;
}

static uint8_t first_computer_move_for_distance(CandidateMove* candidate, uint8_t distance)
{
    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            uint8_t piece = board[y][x];

            if (piece_matches_turn(piece, TURN_COMPUTER)) {
                if (first_piece_move_for_distance(candidate, x, y, distance)) {
                    return FLAG_ON;
                }
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t random_computer_move_for_distance(CandidateMove* candidate, uint8_t distance)
{
    uint8_t match_count = 0;

    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            uint8_t piece = board[y][x];

            if (piece_matches_turn(piece, TURN_COMPUTER)) {
                for (int8_t dy = -1; dy <= 1; dy += 2) {
                    int8_t to_y = (int8_t)(y + (dy * distance));

                    if (coord_on_board(to_y)) {
                        for (int8_t dx = -1; dx <= 1; dx += 2) {
                            int8_t to_x = (int8_t)(x + (dx * distance));

                            if (coord_on_board(to_x)) {
                                uint8_t move_type = move_type_for_piece(x, y, (uint8_t)to_x, (uint8_t)to_y, piece);

                                if (move_type != MOVE_NONE) {
                                    if (rand_bounded_u8(match_count) == 0U) {
                                        store_candidate(candidate, x, y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type);
                                    }
                                    match_count++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return (match_count != 0U) ? FLAG_ON : FLAG_OFF;
}

static uint8_t best_computer_move(CandidateMove* best, uint8_t required_move_type, uint8_t score_chain)
{
    best->move_type = MOVE_NONE;
    best->score = SCORE_NONE;

    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            uint8_t piece = board[y][x];

            if (piece_matches_turn(piece, TURN_COMPUTER)) {
                for (int8_t distance = MOVE_DISTANCE_CAPTURE; distance >= MOVE_DISTANCE_ONE; distance--) {
                    for (int8_t dy = -1; dy <= 1; dy += 2) {
                        int8_t to_y = (int8_t)(y + (dy * distance));

                        if (coord_on_board(to_y)) {
                            for (int8_t dx = -1; dx <= 1; dx += 2) {
                                int8_t to_x = (int8_t)(x + (dx * distance));

                                if (coord_on_board(to_x)) {
                                    uint8_t move_type = move_type_for_piece(x, y, (uint8_t)to_x, (uint8_t)to_y, piece);
                                    if (move_type != MOVE_NONE &&
                                        (required_move_type == MOVE_NONE || move_type == required_move_type)) {
                                        remember_candidate(best, x, y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type, score_chain);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return (best->move_type != MOVE_NONE) ? FLAG_ON : FLAG_OFF;
}

static uint8_t best_piece_capture(CandidateMove* best, uint8_t from_x, uint8_t from_y, uint8_t score_chain)
{
    uint8_t piece = board[from_y][from_x];

    best->move_type = MOVE_NONE;
    best->score = SCORE_NONE;

    for (int8_t dy = -1; dy <= 1; dy += 2) {
        int8_t to_y = (int8_t)(from_y + (dy * MOVE_DISTANCE_CAPTURE));

        if (coord_on_board(to_y)) {
            for (int8_t dx = -1; dx <= 1; dx += 2) {
                int8_t to_x = (int8_t)(from_x + (dx * MOVE_DISTANCE_CAPTURE));

                if (coord_on_board(to_x)) {
                    uint8_t move_type = move_type_for_piece(from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece);

                    if (move_type == MOVE_CAPTURE) {
                        remember_candidate(best, from_x, from_y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type, score_chain);
                    }
                }
            }
        }
    }

    return (best->move_type != MOVE_NONE) ? FLAG_ON : FLAG_OFF;
}

void checkers_ai_set_difficulty(uint8_t difficulty)
{
    if (difficulty <= CHECKERS_AI_HARD) {
        ai_difficulty = difficulty;
    }
}

uint8_t checkers_ai_choose_piece_capture(CandidateMove* move, uint8_t from_x, uint8_t from_y)
{
    if (ai_difficulty == CHECKERS_AI_HARD) {
        return best_piece_capture(move, from_x, from_y, FLAG_ON);
    }
    if (ai_difficulty == CHECKERS_AI_MEDIUM) {
        return best_piece_capture(move, from_x, from_y, FLAG_OFF);
    }

    return random_piece_move_for_distance(move, from_x, from_y, MOVE_DISTANCE_CAPTURE);
}

uint8_t checkers_ai_choose_computer_move(CandidateMove* move, uint8_t required_move_type)
{
    if (required_move_type == MOVE_CAPTURE) {
        if (ai_difficulty == CHECKERS_AI_HARD && best_computer_move(move, MOVE_CAPTURE, FLAG_ON)) {
            return FLAG_ON;
        }
        if (ai_difficulty == CHECKERS_AI_MEDIUM && best_computer_move(move, MOVE_CAPTURE, FLAG_OFF)) {
            return FLAG_ON;
        }

        return random_computer_move_for_distance(move, MOVE_DISTANCE_CAPTURE);
    }

    if (ai_difficulty == CHECKERS_AI_EASY) {
        return random_computer_move_for_distance(move, MOVE_DISTANCE_ONE);
    }
    if (ai_difficulty == CHECKERS_AI_HARD) {
        return best_computer_move(move, MOVE_SIMPLE, FLAG_ON);
    }
    if (ai_difficulty == CHECKERS_AI_MEDIUM) {
        return best_computer_move(move, MOVE_SIMPLE, FLAG_OFF);
    }

    return first_computer_move_for_distance(move, MOVE_DISTANCE_ONE);
}
