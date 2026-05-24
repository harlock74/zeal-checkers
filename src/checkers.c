#include <stdint.h>

#include <zgdk/utils/rand.h>

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
#define COMPUTER_BACK_ROW 0
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
#define GAME_PLAYING 0
#define GAME_PLAYER_WIN 1
#define GAME_COMPUTER_WIN 2
#define TILE_CENTER_OFFSET 8
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
#define BOARD_PIXEL_X (CHECKERS_BOARD_TILE_X * TILE_PIXELS)
#define BOARD_PIXEL_Y (CHECKERS_BOARD_TILE_Y * TILE_PIXELS)
#define BOARD_PIXEL_MAX_X ((CHECKERS_BOARD_TILE_X + CHECKERS_BOARD_TILE_W) * TILE_PIXELS - 1U)
#define BOARD_PIXEL_MAX_Y ((CHECKERS_BOARD_TILE_Y + CHECKERS_BOARD_TILE_H) * TILE_PIXELS - 1U)
#define DRAG_PIECE_HOTSPOT_OFFSET (TILE_PIXELS + TILE_CENTER_OFFSET)

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
static uint8_t game_state;
static uint8_t ai_difficulty = CHECKERS_AI_DEFAULT_DIFFICULTY;
static uint16_t cursor_hotspot_x;
static uint16_t cursor_hotspot_y;
static uint8_t board[CHECKERS_BOARD_SQUARES][CHECKERS_BOARD_SQUARES];

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

static void draw_banner_message(const char* message)
{
    render_draw_banner_text(message);
}

static void clear_banner_message(void)
{
    render_clear_banner();
}

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

static uint8_t piece_player_flag(uint8_t piece)
{
    return piece_is_player(piece) ? PIECE_IS_PLAYER : PIECE_IS_COMPUTER;
}

static uint8_t piece_king_flag(uint8_t piece)
{
    return piece_is_king(piece) ? PIECE_IS_KING : PIECE_IS_MAN;
}

static uint16_t drag_piece_pixel_x(void)
{
    return (uint16_t)(cursor_hotspot_x - DRAG_PIECE_HOTSPOT_OFFSET);
}

static uint16_t drag_piece_pixel_y(void)
{
    return (uint16_t)(cursor_hotspot_y - DRAG_PIECE_HOTSPOT_OFFSET);
}

static void update_drag_piece(void)
{
    if (has_selection()) {
        render_move_drag_piece(drag_piece_pixel_x(), drag_piece_pixel_y());
    }
}

static void update_selector(void)
{
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);
    update_drag_piece();
}

static void restore_selected_piece_visual(void)
{
    if (has_selection() && board[selected_y][selected_x] != PIECE_EMPTY) {
        uint8_t piece = board[selected_y][selected_x];

        render_set_piece(0, selected_x, selected_y, piece_player_flag(piece), piece_king_flag(piece));
    }
    render_clear_drag_piece();
}

static void show_selected_piece_as_drag(void)
{
    uint8_t piece = board[selected_y][selected_x];

    if (piece == PIECE_EMPTY) {
        return;
    }

    render_clear_piece_square(selected_x, selected_y);
    render_set_drag_piece(
        drag_piece_pixel_x(),
        drag_piece_pixel_y(),
        piece_player_flag(piece),
        piece_king_flag(piece));
}

static void select_current_piece(void)
{
    restore_selected_piece_visual();
    selected_x = selector_x;
    selected_y = selector_y;
    show_selected_piece_as_drag();
    update_selector();
}

static void clear_selection_and_update_selector(void)
{
    restore_selected_piece_visual();
    clear_selection();
    update_selector();
}

static void set_game_state(uint8_t state)
{
    game_state = state;
    clear_selection();
    render_clear_drag_piece();
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_OFF);

    if (state == GAME_PLAYER_WIN) {
        draw_banner_message("YOU WIN PRESS ENTER TO PLAY AGAIN!");
    } else if (state == GAME_COMPUTER_WIN) {
        draw_banner_message("YOU LOSE PRESS ENTER TO PLAY AGAIN!");
    }
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

static uint16_t clamp_cursor_pixel(int16_t value, uint16_t min_value, uint16_t max_value)
{
    if (value < (int16_t)min_value) {
        return min_value;
    }
    if ((uint16_t)value > max_value) {
        return max_value;
    }
    return (uint16_t)value;
}

static uint8_t board_coord_from_cursor_pixel(uint16_t pixel, uint16_t board_pixel_origin)
{
    uint8_t coord;

    if (pixel < board_pixel_origin) {
        return 0;
    }

    coord = (uint8_t)((pixel - board_pixel_origin) / (CHECKERS_SQUARE_TILE_W * TILE_PIXELS));

    if (coord >= CHECKERS_BOARD_SQUARES) {
        return (uint8_t)(CHECKERS_BOARD_SQUARES - 1U);
    }
    return coord;
}

static void sync_cursor_to_selector(void)
{
    cursor_hotspot_x = tile_center_pixel(
        (uint8_t)(CHECKERS_BOARD_TILE_X + (selector_x * CHECKERS_SQUARE_TILE_W) + 1U));
    cursor_hotspot_y = tile_center_pixel(
        (uint8_t)(CHECKERS_BOARD_TILE_Y + (selector_y * CHECKERS_SQUARE_TILE_H) + 1U));
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);
    update_drag_piece();
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
        BOARD_PIXEL_X,
        BOARD_PIXEL_MAX_X);
    cursor_hotspot_y = clamp_cursor_pixel(
        (int16_t)cursor_hotspot_y + (ev->mouse_dy * MOUSE_Y_DIRECTION),
        BOARD_PIXEL_Y,
        BOARD_PIXEL_MAX_Y);

    selector_x = board_coord_from_cursor_pixel(cursor_hotspot_x, BOARD_PIXEL_X);
    selector_y = board_coord_from_cursor_pixel(cursor_hotspot_y, BOARD_PIXEL_Y);
    render_set_selector_pixel(cursor_hotspot_x, cursor_hotspot_y, selector_tile_for_state(), FLAG_ON);
    update_drag_piece();

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

static uint8_t side_has_piece(uint8_t turn)
{
    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            if (piece_matches_turn(board[y][x], turn)) {
                return FLAG_ON;
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t side_has_move_type(uint8_t turn, uint8_t required_move_type)
{
    for (uint8_t y = 0; y < CHECKERS_BOARD_SQUARES; y++) {
        for (uint8_t x = 0; x < CHECKERS_BOARD_SQUARES; x++) {
            uint8_t piece = board[y][x];

            if (piece_matches_turn(piece, turn)) {
                for (uint8_t distance = MOVE_DISTANCE_ONE; distance <= MOVE_DISTANCE_CAPTURE; distance++) {
                    for (int8_t dy = -1; dy <= 1; dy += 2) {
                        int8_t to_y = (int8_t)(y + (dy * distance));

                        if (coord_on_board(to_y)) {
                            for (int8_t dx = -1; dx <= 1; dx += 2) {
                                int8_t to_x = (int8_t)(x + (dx * distance));

                                if (coord_on_board(to_x)) {
                                    uint8_t move_type = move_type_for_piece(
                                        x,
                                        y,
                                        (uint8_t)to_x,
                                        (uint8_t)to_y,
                                        piece);

                                    if (move_type != MOVE_NONE &&
                                        (required_move_type == MOVE_NONE || move_type == required_move_type)) {
                                        return FLAG_ON;
                                    }
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

static uint8_t piece_has_move_type(uint8_t from_x, uint8_t from_y, uint8_t required_move_type)
{
    uint8_t piece = board[from_y][from_x];

    if (piece == PIECE_EMPTY) {
        return FLAG_OFF;
    }

    for (uint8_t distance = MOVE_DISTANCE_ONE; distance <= MOVE_DISTANCE_CAPTURE; distance++) {
        for (int8_t dy = -1; dy <= 1; dy += 2) {
            int8_t to_y = (int8_t)(from_y + (dy * distance));

            if (coord_on_board(to_y)) {
                for (int8_t dx = -1; dx <= 1; dx += 2) {
                    int8_t to_x = (int8_t)(from_x + (dx * distance));

                    if (coord_on_board(to_x)) {
                        uint8_t move_type = move_type_for_piece(
                            from_x,
                            from_y,
                            (uint8_t)to_x,
                            (uint8_t)to_y,
                            piece);

                        if (move_type != MOVE_NONE &&
                            (required_move_type == MOVE_NONE || move_type == required_move_type)) {
                            return FLAG_ON;
                        }
                    }
                }
            }
        }
    }

    return FLAG_OFF;
}

static uint8_t side_has_legal_move(uint8_t turn)
{
    return side_has_move_type(turn, MOVE_NONE);
}

static uint8_t side_has_capture(uint8_t turn)
{
    return side_has_move_type(turn, MOVE_CAPTURE);
}

static uint8_t piece_has_capture(uint8_t from_x, uint8_t from_y)
{
    return piece_has_move_type(from_x, from_y, MOVE_CAPTURE);
}

static uint8_t side_has_lost(uint8_t turn)
{
    return (!side_has_piece(turn) || !side_has_legal_move(turn)) ? FLAG_ON : FLAG_OFF;
}

static uint8_t update_game_over(void)
{
    if (side_has_lost(TURN_COMPUTER)) {
        set_game_state(GAME_PLAYER_WIN);
        return FLAG_ON;
    }
    if (side_has_lost(TURN_PLAYER)) {
        set_game_state(GAME_COMPUTER_WIN);
        return FLAG_ON;
    }

    return FLAG_OFF;
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

static uint8_t captured_piece_for_move(uint8_t from_x, uint8_t from_y, uint8_t to_x, uint8_t to_y, uint8_t move_type)
{
    if (move_type == MOVE_CAPTURE) {
        uint8_t middle_x = (uint8_t)((from_x + to_x) / 2U);
        uint8_t middle_y = (uint8_t)((from_y + to_y) / 2U);

        return board[middle_y][middle_x];
    }

    return PIECE_EMPTY;
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
                                move_type_for_piece(
                                    x,
                                    y,
                                    (uint8_t)reply_x,
                                    (uint8_t)reply_y,
                                    player_piece) == MOVE_CAPTURE) {
                                uint8_t captured_piece = captured_piece_for_move(
                                    x,
                                    y,
                                    (uint8_t)reply_x,
                                    (uint8_t)reply_y,
                                    MOVE_CAPTURE);
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
                    uint8_t move_type = move_type_for_piece(
                        from_x,
                        from_y,
                        (uint8_t)to_x,
                        (uint8_t)to_y,
                        piece);

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
                            score = (uint8_t)(score + score_computer_capture_chain(
                                (uint8_t)to_x,
                                (uint8_t)to_y,
                                moved_piece,
                                (uint8_t)(depth + 1U)));
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
        uint8_t player_reply_score = score_player_capture_reply_after_computer_move(
            from_x,
            from_y,
            to_x,
            to_y,
            piece,
            move_type);

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
        best->from_x = from_x;
        best->from_y = from_y;
        best->to_x = to_x;
        best->to_y = to_y;
        best->piece = piece;
        best->move_type = move_type;
        best->score = score;
    }
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

static void apply_candidate_move(const CandidateMove* candidate)
{
    apply_move(
        candidate->from_x,
        candidate->from_y,
        candidate->to_x,
        candidate->to_y,
        candidate->piece,
        candidate->move_type);
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
                    uint8_t move_type = move_type_for_piece(
                        from_x,
                        from_y,
                        (uint8_t)to_x,
                        (uint8_t)to_y,
                        piece);

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
                    uint8_t move_type = move_type_for_piece(
                        from_x,
                        from_y,
                        (uint8_t)to_x,
                        (uint8_t)to_y,
                        piece);

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
                                        store_candidate(
                                            candidate,
                                            x,
                                            y,
                                            (uint8_t)to_x,
                                            (uint8_t)to_y,
                                            piece,
                                            move_type);
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
                                        remember_candidate(
                                            best,
                                            x,
                                            y,
                                            (uint8_t)to_x,
                                            (uint8_t)to_y,
                                            piece,
                                            move_type,
                                            score_chain);
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
                    uint8_t move_type = move_type_for_piece(
                        from_x,
                        from_y,
                        (uint8_t)to_x,
                        (uint8_t)to_y,
                        piece);

                    if (move_type == MOVE_CAPTURE) {
                        remember_candidate(
                            best,
                            from_x,
                            from_y,
                            (uint8_t)to_x,
                            (uint8_t)to_y,
                            piece,
                            move_type,
                            score_chain);
                    }
                }
            }
        }
    }

    return (best->move_type != MOVE_NONE) ? FLAG_ON : FLAG_OFF;
}

static uint8_t first_or_best_piece_capture(CandidateMove* candidate, uint8_t from_x, uint8_t from_y)
{
    if (ai_difficulty == CHECKERS_AI_HARD) {
        return best_piece_capture(candidate, from_x, from_y, FLAG_ON);
    }
    if (ai_difficulty == CHECKERS_AI_MEDIUM) {
        return best_piece_capture(candidate, from_x, from_y, FLAG_OFF);
    }
    if (ai_difficulty == CHECKERS_AI_EASY) {
        return random_piece_move_for_distance(candidate, from_x, from_y, MOVE_DISTANCE_CAPTURE);
    }

    return first_piece_move_for_distance(candidate, from_x, from_y, MOVE_DISTANCE_CAPTURE);
}

static void finish_computer_turn(void)
{
    current_turn = TURN_PLAYER;
    if (!update_game_over()) {
        clear_banner_message();
    }
}

static void apply_computer_move_chain(const CandidateMove* first_move)
{
    CandidateMove move;

    store_candidate(
        &move,
        first_move->from_x,
        first_move->from_y,
        first_move->to_x,
        first_move->to_y,
        first_move->piece,
        first_move->move_type);

    while (1) {
        uint8_t promotes = piece_promotes_at(move.piece, move.to_y);

        apply_candidate_move(&move);

        if (promotes || !piece_has_capture(move.to_x, move.to_y)) {
            break;
        }

        wait_frames(CHECKERS_COMPUTER_REPLY_DELAY_FRAMES);
        if (!first_or_best_piece_capture(&move, move.to_x, move.to_y)) {
            break;
        }
    }

    finish_computer_turn();
}

static void run_computer_turn(void)
{
    CandidateMove move;

    current_turn = TURN_COMPUTER;
    clear_banner_message();
    wait_frames(CHECKERS_COMPUTER_REPLY_DELAY_FRAMES);

    if (side_has_capture(TURN_COMPUTER)) {
        if (ai_difficulty == CHECKERS_AI_HARD && best_computer_move(&move, MOVE_CAPTURE, FLAG_ON)) {
            apply_computer_move_chain(&move);
            return;
        }

        if (ai_difficulty == CHECKERS_AI_MEDIUM && best_computer_move(&move, MOVE_CAPTURE, FLAG_OFF)) {
            apply_computer_move_chain(&move);
            return;
        }

        if (random_computer_move_for_distance(&move, MOVE_DISTANCE_CAPTURE)) {
            apply_computer_move_chain(&move);
            return;
        }
    }

    if (ai_difficulty == CHECKERS_AI_EASY &&
        (random_computer_move_for_distance(&move, MOVE_DISTANCE_ONE) ||
            random_computer_move_for_distance(&move, MOVE_DISTANCE_CAPTURE))) {
        apply_candidate_move(&move);
        finish_computer_turn();
        return;
    }

    if (ai_difficulty == CHECKERS_AI_HARD && best_computer_move(&move, MOVE_SIMPLE, FLAG_ON)) {
        apply_candidate_move(&move);
        finish_computer_turn();
        return;
    }

    if (ai_difficulty == CHECKERS_AI_MEDIUM && best_computer_move(&move, MOVE_SIMPLE, FLAG_OFF)) {
        apply_candidate_move(&move);
        finish_computer_turn();
        return;
    }

    if (first_computer_move_for_distance(&move, MOVE_DISTANCE_CAPTURE) ||
        first_computer_move_for_distance(&move, MOVE_DISTANCE_ONE)) {
        apply_candidate_move(&move);
        finish_computer_turn();
        return;
    }

    current_turn = TURN_PLAYER;
    set_game_state(GAME_PLAYER_WIN);
}

static void try_select_or_move(void)
{
    uint8_t move_type;
    uint8_t landing_x;
    uint8_t landing_y;

    if (current_turn != TURN_PLAYER) {
        return;
    }

    if (!has_selection()) {
        if (selector_has_player_piece()) {
            if (side_has_capture(TURN_PLAYER) && !piece_has_capture(selector_x, selector_y)) {
                draw_banner_message("CAPTURE REQUIRED!");
                return;
            }
            select_current_piece();
        }
        return;
    }

    if (selector_x == selected_x && selector_y == selected_y) {
        if (piece_has_capture(selected_x, selected_y)) {
            draw_banner_message("KEEP CAPTURING!");
            return;
        }

        clear_selection_and_update_selector();
        return;
    }

    move_type = move_type_for_piece(selected_x, selected_y, selector_x, selector_y, board[selected_y][selected_x]);
    if (move_type != MOVE_NONE) {
        uint8_t promotes;

        landing_x = selector_x;
        landing_y = selector_y;
        promotes = piece_promotes_at(board[selected_y][selected_x], landing_y);

        if (move_type == MOVE_SIMPLE && side_has_capture(TURN_PLAYER)) {
            draw_banner_message("CAPTURE REQUIRED!");
            return;
        }

        render_clear_drag_piece();
        apply_move(selected_x, selected_y, selector_x, selector_y, board[selected_y][selected_x], move_type);
        if (update_game_over()) {
            return;
        }

        if (move_type == MOVE_CAPTURE && !promotes && piece_has_capture(landing_x, landing_y)) {
            selected_x = landing_x;
            selected_y = landing_y;
            selector_x = landing_x;
            selector_y = landing_y;
            sync_cursor_to_selector();
            show_selected_piece_as_drag();
            draw_banner_message("KEEP CAPTURING!");
            return;
        }

        clear_selection_and_update_selector();
        run_computer_turn();
    } else if (piece_has_capture(selected_x, selected_y)) {
        draw_banner_message("KEEP CAPTURING!");
    } else if (selector_has_player_piece()) {
        if (side_has_capture(TURN_PLAYER) && !piece_has_capture(selector_x, selector_y)) {
            draw_banner_message("CAPTURE REQUIRED!");
            return;
        }
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
    render_clear_drag_piece();
    render_clear_sprite_layer();
    current_turn = TURN_PLAYER;
    game_state = GAME_PLAYING;

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
    clear_banner_message();
    sync_cursor_to_selector();
}

void checkers_set_ai_difficulty(uint8_t difficulty)
{
    if (difficulty <= CHECKERS_AI_HARD) {
        ai_difficulty = difficulty;
    }
}

void checkers_handle_input(const KeyEvents* ev)
{
    if (game_state != GAME_PLAYING) {
        if (ev->start) {
            checkers_init_game();
        }
        return;
    }

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
        if (has_selection() && piece_has_capture(selected_x, selected_y)) {
            draw_banner_message("KEEP CAPTURING!");
            return;
        }

        clear_selection_and_update_selector();
    }
}
