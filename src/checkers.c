#include <stdint.h>

#include "main.h"
#include "render.h"
#include "checkers.h"

#define COMPUTER_START_ROW_COUNT 3
#define PLAYER_START_ROW_FIRST 5
#define PIECE_IS_COMPUTER FLAG_OFF
#define PIECE_IS_PLAYER FLAG_ON
#define PIECE_IS_MAN FLAG_OFF
#define PIECE_IS_KING FLAG_ON
#define PIECE_EMPTY 0
#define PIECE_COMPUTER_MAN 1
#define PIECE_PLAYER_MAN 2
#define PIECE_COMPUTER_KING 3
#define PIECE_PLAYER_KING 4
#define PLAYER_PROMOTION_ROW 0
#define COMPUTER_PROMOTION_ROW (CHECKERS_BOARD_SQUARES - 1)
#define SELECTOR_START_X 1
#define SELECTOR_START_Y PLAYER_START_ROW_FIRST
#define SELECTOR_MIN 0
#define SELECTOR_MAX (CHECKERS_BOARD_SQUARES - 1)
#define NO_SELECTION U8_MAX_VALUE
#define PLAYER_MOVE_DY -1
#define COMPUTER_MOVE_DY 1
#define MOVE_DISTANCE_ONE 1
#define MOVE_DISTANCE_CAPTURE 2
#define MOVE_NONE 0
#define MOVE_SIMPLE 1
#define MOVE_CAPTURE 2
#define TURN_PLAYER FLAG_ON
#define TURN_COMPUTER FLAG_OFF
#define TILE_CENTER_OFFSET 8
#define SCORE_NONE 0
#define SCORE_SIMPLE 10
#define SCORE_CAPTURE 40
#define SCORE_PROMOTION 30
#define SCORE_KING 5
#define SCORE_SAFE 8
#define SCORE_RISK_PENALTY 12

typedef struct {
    uint8_t from_x;
    uint8_t from_y;
    uint8_t to_x;
    uint8_t to_y;
    uint8_t piece;
    uint8_t move_type;
    uint8_t score;
} CandidateMove;

static uint8_t next_piece_index;
static uint8_t selector_x;
static uint8_t selector_y;
static uint8_t selected_x;
static uint8_t selected_y;
static uint8_t current_turn;
static uint16_t cursor_hotspot_x;
static uint16_t cursor_hotspot_y;
static uint8_t board[CHECKERS_BOARD_SQUARES][CHECKERS_BOARD_SQUARES];

static uint8_t square_is_computer_start(uint8_t board_x, uint8_t board_y)
{
    return ((board_x + board_y) & 1U) ? FLAG_ON : FLAG_OFF;
}

static uint8_t square_is_player_start(uint8_t board_x, uint8_t board_y)
{
    return square_is_computer_start(board_x, board_y);
}

static void add_start_piece(uint8_t board_x, uint8_t board_y, uint8_t player_piece)
{
    board[board_y][board_x] = player_piece ? PIECE_PLAYER_MAN : PIECE_COMPUTER_MAN;
    render_set_piece(next_piece_index, board_x, board_y, player_piece, PIECE_IS_MAN);
    next_piece_index++;
}

static uint8_t piece_is_player(uint8_t piece)
{
    return (piece == PIECE_PLAYER_MAN || piece == PIECE_PLAYER_KING) ? FLAG_ON : FLAG_OFF;
}

static uint8_t piece_is_computer(uint8_t piece)
{
    return (piece == PIECE_COMPUTER_MAN || piece == PIECE_COMPUTER_KING) ? FLAG_ON : FLAG_OFF;
}

static uint8_t piece_is_king(uint8_t piece)
{
    return (piece == PIECE_PLAYER_KING || piece == PIECE_COMPUTER_KING) ? FLAG_ON : FLAG_OFF;
}

static uint8_t piece_matches_turn(uint8_t piece, uint8_t turn)
{
    return (turn == TURN_PLAYER) ? piece_is_player(piece) : piece_is_computer(piece);
}

static uint8_t pieces_are_opponents(uint8_t piece, uint8_t other)
{
    if (piece_is_player(piece)) {
        return piece_is_computer(other);
    }
    if (piece_is_computer(piece)) {
        return piece_is_player(other);
    }

    return FLAG_OFF;
}

static uint8_t selector_has_player_piece(void)
{
    return piece_is_player(board[selector_y][selector_x]);
}

static uint8_t has_selection(void)
{
    return (selected_x != NO_SELECTION && selected_y != NO_SELECTION) ? FLAG_ON : FLAG_OFF;
}

static void clear_selection(void)
{
    selected_x = NO_SELECTION;
    selected_y = NO_SELECTION;
}

static uint8_t selector_tile_for_state(void)
{
    return has_selection() ? CLOSED_HAND_SELECTOR_TILE : OPEN_HAND_SELECTOR_TILE;
}

static void update_selector(void)
{
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);
}

static void select_current_piece(void)
{
    selected_x = selector_x;
    selected_y = selector_y;
    update_selector();
}

static void clear_selection_and_update_selector(void)
{
    clear_selection();
    update_selector();
}

static uint8_t abs_diff_u8(uint8_t a, uint8_t b)
{
    return (a > b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static void wait_frames(uint8_t frames)
{
    while (frames > FLAG_OFF) {
        gfx_wait_vblank(&vctx);
        render_draw_sprites();
        gfx_wait_end_vblank(&vctx);
        frames--;
    }
}

static uint8_t coord_on_board(int8_t coord)
{
    return (coord >= 0 && coord < CHECKERS_BOARD_SQUARES) ? FLAG_ON : FLAG_OFF;
}

static uint16_t tile_center_pixel(uint8_t tile)
{
    return (uint16_t)(((uint16_t)tile * TILE_PIXELS) + TILE_CENTER_OFFSET);
}

static uint16_t clamp_cursor_pixel(int16_t value, uint16_t max_value)
{
    if (value < 0) {
        return 0;
    }
    if ((uint16_t)value > max_value) {
        return max_value;
    }
    return (uint16_t)value;
}

static uint8_t board_coord_from_cursor_pixel(uint16_t pixel)
{
    uint8_t coord = (uint8_t)(pixel / (CHECKERS_SQUARE_TILE_W * TILE_PIXELS));

    if (coord >= CHECKERS_BOARD_SQUARES) {
        return (uint8_t)(CHECKERS_BOARD_SQUARES - 1U);
    }
    return coord;
}

static void sync_cursor_to_selector(void)
{
    cursor_hotspot_x = tile_center_pixel((uint8_t)((selector_x * CHECKERS_SQUARE_TILE_W) + 1U));
    cursor_hotspot_y = tile_center_pixel((uint8_t)((selector_y * CHECKERS_SQUARE_TILE_H) + 1U));
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);
}

static uint8_t apply_mouse_motion(const KeyEvents* ev)
{
    uint8_t old_x;
    uint8_t old_y;

    if ((ev->mouse_dx == 0) && (ev->mouse_dy == 0)) {
        return FLAG_OFF;
    }

    old_x = selector_x;
    old_y = selector_y;
    cursor_hotspot_x = clamp_cursor_pixel(
        (int16_t)cursor_hotspot_x + ev->mouse_dx,
        (uint16_t)(SCREEN_PIXEL_W - 1U));
    cursor_hotspot_y = clamp_cursor_pixel(
        (int16_t)cursor_hotspot_y + (ev->mouse_dy * MOUSE_Y_DIRECTION),
        (uint16_t)(SCREEN_PIXEL_H - 1U));

    selector_x = board_coord_from_cursor_pixel(cursor_hotspot_x);
    selector_y = board_coord_from_cursor_pixel(cursor_hotspot_y);
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);

    return (old_x != selector_x || old_y != selector_y) ? FLAG_ON : FLAG_OFF;
}

static uint8_t move_type_for_piece(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y, uint8_t piece)
{
    uint8_t dx = abs_diff_u8(to_x, from_x);
    int8_t dy = (int8_t)(to_y - from_y);
    int8_t expected_dy = (piece == PIECE_PLAYER_MAN) ? PLAYER_MOVE_DY : COMPUTER_MOVE_DY;

    if (board[to_y][to_x] != PIECE_EMPTY) {
        return MOVE_NONE;
    }
    if (dx == MOVE_DISTANCE_ONE && (piece_is_king(piece) || dy == expected_dy)) {
        return MOVE_SIMPLE;
    }
    if (dx == MOVE_DISTANCE_CAPTURE &&
        (piece_is_king(piece) || dy == (int8_t)(expected_dy * MOVE_DISTANCE_CAPTURE))) {
        uint8_t middle_x = (uint8_t)((from_x + to_x) / 2U);
        uint8_t middle_y = (uint8_t)((from_y + to_y) / 2U);
        uint8_t middle_piece = board[middle_y][middle_x];

        if (pieces_are_opponents(piece, middle_piece)) {
            return MOVE_CAPTURE;
        }
    }

    return MOVE_NONE;
}

static void apply_move(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y, uint8_t piece, uint8_t move_type)
{
    uint8_t moved_piece = piece;
    uint8_t player_piece = piece_is_player(piece) ? PIECE_IS_PLAYER : PIECE_IS_COMPUTER;

    board[from_y][from_x] = PIECE_EMPTY;
    render_clear_piece_square(from_x, from_y);

    if (move_type == MOVE_CAPTURE) {
        uint8_t middle_x = (uint8_t)((from_x + to_x) / 2U);
        uint8_t middle_y = (uint8_t)((from_y + to_y) / 2U);

        board[middle_y][middle_x] = PIECE_EMPTY;
        render_clear_piece_square(middle_x, middle_y);
    }

    if (piece == PIECE_PLAYER_MAN && to_y == PLAYER_PROMOTION_ROW) {
        moved_piece = PIECE_PLAYER_KING;
    } else if (piece == PIECE_COMPUTER_MAN && to_y == COMPUTER_PROMOTION_ROW) {
        moved_piece = PIECE_COMPUTER_KING;
    }

    board[to_y][to_x] = moved_piece;
    render_set_piece(0, to_x, to_y, player_piece, piece_is_king(moved_piece));
}

static uint8_t piece_promotes_at(uint8_t piece, uint8_t y)
{
    if (piece == PIECE_COMPUTER_MAN && y == COMPUTER_PROMOTION_ROW) {
        return FLAG_ON;
    }
    if (piece == PIECE_PLAYER_MAN && y == PLAYER_PROMOTION_ROW) {
        return FLAG_ON;
    }
    return FLAG_OFF;
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

    if (piece_promotes_at(piece, to_y)) {
        score = (uint8_t)(score + SCORE_PROMOTION);
    }
    if (piece_is_king(piece)) {
        score = (uint8_t)(score + SCORE_KING);
    }
    if (computer_lands_in_danger(from_x, from_y, to_x, to_y, piece, move_type)) {
        score = (score > SCORE_RISK_PENALTY) ? (uint8_t)(score - SCORE_RISK_PENALTY) : SCORE_NONE;
    } else {
        score = (uint8_t)(score + SCORE_SAFE);
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
    uint8_t move_type)
{
    uint8_t score = score_computer_move(from_x, from_y, to_x, to_y, piece, move_type);

    if (best->move_type == MOVE_NONE || score > best->score) {
        best->from_x = from_x;
        best->from_y = from_y;
        best->to_x = to_x;
        best->to_y = to_y;
        best->piece = piece;
        best->move_type = move_type;
        best->score = score;
    }
}

static uint8_t first_computer_move_for_distance(uint8_t distance)
{
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
                                    apply_move(x, y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type);
                                    return FLAG_ON;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t best_computer_move(CandidateMove* best)
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
                                    if (move_type != MOVE_NONE) {
                                        remember_candidate(best, x, y, (uint8_t)to_x, (uint8_t)to_y, piece, move_type);
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

static void run_computer_turn(void)
{
    current_turn = TURN_COMPUTER;

#if CHECKERS_AI_DIFFICULTY == CHECKERS_AI_EASY
    if (first_computer_move_for_distance(MOVE_DISTANCE_ONE) ||
        first_computer_move_for_distance(MOVE_DISTANCE_CAPTURE)) {
        current_turn = TURN_PLAYER;
        return;
    }
#elif CHECKERS_AI_DIFFICULTY == CHECKERS_AI_HARD
    {
        CandidateMove best;

        if (best_computer_move(&best)) {
            apply_move(best.from_x, best.from_y, best.to_x, best.to_y, best.piece, best.move_type);
            current_turn = TURN_PLAYER;
            return;
        }
    }
#else
    if (first_computer_move_for_distance(MOVE_DISTANCE_CAPTURE) ||
        first_computer_move_for_distance(MOVE_DISTANCE_ONE)) {
        current_turn = TURN_PLAYER;
        return;
    }
#endif

    current_turn = TURN_PLAYER;
}

static void try_select_or_move(void)
{
    uint8_t move_type;

    if (current_turn != TURN_PLAYER) {
        return;
    }

    if (!has_selection()) {
        if (selector_has_player_piece()) {
            select_current_piece();
        }
        return;
    }

    if (selector_x == selected_x && selector_y == selected_y) {
        clear_selection_and_update_selector();
        return;
    }

    move_type = move_type_for_piece(selected_x, selected_y, selector_x, selector_y, board[selected_y][selected_x]);
    if (move_type != MOVE_NONE) {
        apply_move(selected_x, selected_y, selector_x, selector_y, board[selected_y][selected_x], move_type);
        clear_selection_and_update_selector();
        wait_frames(CHECKERS_COMPUTER_REPLY_DELAY_FRAMES);
        run_computer_turn();
    } else if (selector_has_player_piece()) {
        select_current_piece();
    }
}

static void move_selector(int8_t dx, int8_t dy)
{
    if (dx < 0 && selector_x > SELECTOR_MIN) {
        selector_x--;
    } else if (dx > 0 && selector_x < SELECTOR_MAX) {
        selector_x++;
    }

    if (dy < 0 && selector_y > SELECTOR_MIN) {
        selector_y--;
    } else if (dy > 0 && selector_y < SELECTOR_MAX) {
        selector_y++;
    }

    sync_cursor_to_selector();
}

void checkers_init_game(void)
{
    next_piece_index = 0;
    clear_selection();
    current_turn = TURN_PLAYER;

    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            board[y][x] = PIECE_EMPTY;
        }
    }

    /*
     * Match docs/template.png:
     * computer pieces start on dark board squares, player pieces on light
     * board squares.
     */
    for (uint8_t y = 0; y < COMPUTER_START_ROW_COUNT; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            if (square_is_computer_start(x, y)) {
                add_start_piece(x, y, PIECE_IS_COMPUTER);
            }
        }
    }

    for (uint8_t y = PLAYER_START_ROW_FIRST; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            if (square_is_player_start(x, y)) {
                add_start_piece(x, y, PIECE_IS_PLAYER);
            }
        }
    }

    selector_x = SELECTOR_START_X;
    selector_y = SELECTOR_START_Y;
    sync_cursor_to_selector();
}

void checkers_handle_input(const KeyEvents* ev)
{
    apply_mouse_motion(ev);

    if (ev->left) {
        move_selector(-1, 0);
    } else if (ev->right) {
        move_selector(1, 0);
    } else if (ev->up) {
        move_selector(0, -1);
    } else if (ev->down) {
        move_selector(0, 1);
    } else if (ev->accept) {
        try_select_or_move();
    } else if (ev->cancel) {
        clear_selection_and_update_selector();
    }
}
