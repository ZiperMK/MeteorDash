#define BANKED_RAM_ADDRESS 0xA000UL
#define BANK_NUM (*(unsigned char *)0x00)
#define XIST_LAYER_MAP_WIDTH 40
#define XIST_LAYER_MAP_HEIGHT 30
#include <stdio.h>  // needed for FILE operations
#include <cx16.h>
#include <cbm.h>
#include <stdio.h>
#include <string.h>
#include "pcmplayer.h"
#include "zmsplayer.h"

#include "xist_gfx.h"
#include "xist_input.h"
#include "xist_mem.h"
#include "xist_text.h"
#include "xist_tiles.h"
#include "xist_utils.h"

#define TEST_SPRITE_BANK  15
#define SFX_BANK 35
#define MUSIC_BANK 51

// FIX: Use correct fire button constant from cx16.h
#define JOYPAD_FIRE JOYPAD_A

unsigned short ship_x = 144;
unsigned short ship_y = 200;
char score_label[] = { 83, 67, 79, 82, 69, 58, 0 };   // SCORE:
char score_value[MAX_SIGNED_INT_CHARS] = {0};
char lives_label[] = { 76, 73, 86, 69, 83, 58, 0 };   // LIVES:
char lives_value[MAX_SIGNED_INT_CHARS] = {0};
char game_over_text[] = { 71, 65, 77, 69, 32, 79, 86, 69, 82, 0 }; // "GAME OVER"
char start_text[] = { 80, 82, 69, 83, 83, 32, 70, 73, 82, 69, 32, 84, 79, 32, 83, 84, 65, 82, 84, 0 }; // "PRESS FIRE TO START"
char high_score_label[] = { 72, 73, 71, 72, 32, 83, 67, 79, 82, 69, 58, 0 };  // "HIGH SCORE:"
char high_score_value[MAX_SIGNED_INT_CHARS] = {0};
char new_high_score_label[] = {
  78, // N
  69, // E
  87, // W
  32, // [space]
  72, // H
  73, // I
  71, // G
  72, // H
  32, // [space]
  83, // S ✅ THIS WAS MISSING IN YOUR OUTPUT
  67, // C
  79, // O
  82, // R
  69, // E
  58, // :
  0   // [null terminator]
};



const unsigned char num_boulders = 32;
unsigned short current_joypad_state;

unsigned char active_boulders;
unsigned char player_lives;
unsigned char last_spawn_second;
unsigned char game_running = 0;
unsigned short high_score = 0;
unsigned char player_invincible;
unsigned short player_invincibility_timer;

unsigned short music_timer = 0;
const unsigned short music_duration_frames = 3600;

const unsigned char activation_order[32] = {
  16, 8, 24, 4, 28, 0, 31, 2, 30, 6, 26, 10, 20, 12, 18, 14,
  17, 15, 19, 13, 21, 11, 22, 9, 23, 7, 25, 5, 27, 3, 29, 1
};

void reset_game() {
    unsigned short i;
    ship_x = 144;
    ship_y = 200;
    player_lives = 3;
    last_spawn_second = 0;
    player_invincible = 0;
    player_invincibility_timer = 0;
    active_boulders = 4;
    for (i = 0; i < num_boulders; ++i) {
        xist_sprite_bank.x_pos[i+2] = i * 10;
        xist_sprite_bank.y_pos[i+2] = 255;
    }
}

int check_collision(short ax, short ay, short bx, short by) {
    return (ax < bx + 16) && (ax + 16 > bx) && (ay < by + 16) && (ay + 16 > by);
}

void load_file_to_highram(const char *filename, unsigned char bank) {
    BANK_NUM = bank;
    cbm_k_setnam(filename);
    cbm_k_setlfs(0, 8, 2);
    cbm_k_load(0, (unsigned short)BANKED_RAM_ADDRESS);
}



void save_high_score() {
    FILE* f = fopen("hiscore.dat", "wb");
    if (f) {
        fwrite(&high_score, sizeof(high_score), 1, f);
        fclose(f);
    }
}

void load_high_score() {
    FILE* f = fopen("hiscore.dat", "rb");
    if (f) {
        fread(&high_score, sizeof(high_score), 1, f);
        fclose(f);
    }
}

void draw_ui(unsigned short second_counter) {
    xist_convert_long_to_ascii_array(second_counter, score_value);
    xist_convert_long_to_ascii_array(player_lives, lives_value);

    xist_text = score_label;
    xist_text_tile_x = 1;
    xist_text_tile_y = 26;
    xist_draw_text();
    xist_text = score_value;
    xist_text_tile_x = 1;
    xist_text_tile_y = 27;
    xist_draw_text();

    xist_text = lives_label;
    xist_text_tile_x = 31;
    xist_text_tile_y = 26;
    xist_draw_text();
    xist_text = lives_value;
    xist_text_tile_x = 31;
    xist_text_tile_y = 27;
    xist_draw_text();

    xist_convert_long_to_ascii_array(high_score, high_score_value);
    xist_text = high_score_label;
    xist_text_tile_x = 17;
    xist_text_tile_y = 26;
    xist_draw_text();
    xist_text = high_score_value;
    xist_text_tile_x = 17;
    xist_text_tile_y = 27;
    xist_draw_text();

}

void process_input() {
    if (!game_running || player_lives == 0) return;

    if (current_joypad_state & JOYPAD_L && ship_x > 0) ship_x -= 3;
    if (current_joypad_state & JOYPAD_R && ship_x < 320 - 16) ship_x += 3;
    if (current_joypad_state & JOYPAD_U && ship_y > 0) ship_y -= 3;
    if (current_joypad_state & JOYPAD_D && ship_y < 240 - 16) ship_y += 3;

    xist_curr_sprite_idx = 1;
    xist_curr_sprite_x_pos = ship_x;
    xist_curr_sprite_y_pos = ship_y;
    xist_update_sprite_position();
}

void move_boulders() {
    signed short n;
    if (!game_running || player_lives == 0) return;

    for (n = 0; n < active_boulders; ++n) {
        unsigned char b = activation_order[n];

        xist_sprite_bank.y_pos[b+2] += (b % 3 == 0) ? 3 : 2;

        if (xist_sprite_bank.y_pos[b+2] > 240) {
            xist_rand_min = 16;
            xist_rand_max = 64;
            xist_sprite_bank.y_pos[b+2] = -xist_rand();
        }

        if (!player_invincible &&
            check_collision(ship_x, ship_y, xist_sprite_bank.x_pos[b+2], xist_sprite_bank.y_pos[b+2])) {

            --player_lives;
            xist_sprite_bank.y_pos[b+2] = 255;

            pcm_trigger_digi(SFX_BANK, BANKED_RAM_ADDRESS);
            player_invincible = 1;
            player_invincibility_timer = 60;
        }

        xist_curr_sprite_idx = b + 2;
        xist_curr_sprite_x_pos = xist_sprite_bank.x_pos[b+2];
        xist_curr_sprite_y_pos = xist_sprite_bank.y_pos[b+2];
        xist_update_sprite_position();
    }
}

void draw_centered_text(char* text, unsigned char row) {
    xist_text = text;
    xist_text_tile_x = (40 - (unsigned char)strlen(text)) / 2;
    xist_text_tile_y = row;
    xist_draw_text();
}

void main() {
    signed long game_counter = 0;
    unsigned short second_counter = 0;
    unsigned short n;
    unsigned char playing;

    xist_seed_a = 123;
    xist_seed_b = 234;

    VERA.display.hscale = 64;
    VERA.display.vscale = 64;

    load_file_to_highram("greenmotor.zsm", MUSIC_BANK);
    zsm_init();
    playing = zsm_startmusic(MUSIC_BANK, BANKED_RAM_ADDRESS);
    load_high_score();
    load_file_to_highram("laser.zcm", SFX_BANK);
    pcm_init();
    pcm_trigger_digi(SFX_BANK, BANKED_RAM_ADDRESS);

    xist_initialize_tiles();
    xist_initialize_text_tiles();
    xist_load_file_to_highram("gfxmeta.dat", XIST_SPRITE_METADATA_BANK);
    xist_load_graphic(0, TRUE, TEST_SPRITE_BANK, FALSE, 0);
    xist_copy_highram_to_vram_using_metadata(XIST_LAYER_0_TILE_BASE, TEST_SPRITE_BANK, 5);

    for (n = 0; n < LAYER_0_MAPBASE_TOTAL; ++n) xist_map_tiles[n] = 0;
    VERA.address = XIST_LAYER_0_MAPBASE;
    VERA.address_hi = XIST_LAYER_0_MAPBASE >> 16;
    VERA.address_hi |= 0b10000;
    for (n = 0; n < LAYER_0_MAPBASE_TOTAL; ++n) {
        VERA.data0 = xist_map_tiles[n];
        VERA.data0 = 0;
    }

    xist_prepare_sprite(0, TRUE, XIST_VRAM_SPRITE_IMAGE_DATA, TEST_SPRITE_BANK, 1, ship_x, ship_y, XIST_SPRITE_ACTIVE);

    for (n = 0; n < num_boulders; ++n) {
        xist_sprite_bank.x_pos[n+2] = n * 10;
        xist_sprite_bank.y_pos[n+2] = 255;

        xist_prepare_sprite(1, (n == 0), XIST_VRAM_SPRITE_IMAGE_DATA + 1024,
                            TEST_SPRITE_BANK, n+2,
                            xist_sprite_bank.x_pos[n+2],
                            xist_sprite_bank.y_pos[n+2],
                            XIST_SPRITE_ACTIVE);

        if (n % 2 == 0) xist_sprite_bank.current_frame[n+2] = 2;
    }

    for (;;) {
        xist_wait();

        current_joypad_state = xist_get_joypad();

        if (!game_running) {
            draw_centered_text(start_text, 12);
            xist_convert_long_to_ascii_array(high_score, high_score_value);
            draw_centered_text(high_score_label, 14);
            draw_centered_text(high_score_value, 15);
            if (current_joypad_state & JOYPAD_FIRE) {
                xist_clear_text();
                reset_game();
                second_counter = 0;
                game_running = 1;
            }
        } else {
            process_input();
            move_boulders();

            if (second_counter - last_spawn_second >= 10 && active_boulders < num_boulders) {
                ++active_boulders;
                last_spawn_second = second_counter;
            }

            xist_proc_sprite_start_idx = 1;
            xist_proc_sprite_end_idx = 32 + 2;
            xist_process_sprites();

            draw_ui(second_counter);

            if (player_invincible && player_invincibility_timer > 0) {
                --player_invincibility_timer;
                if (player_invincibility_timer == 0) {
                    player_invincible = 0;
                }
            }

        if (player_lives == 0) {
            draw_centered_text(game_over_text, 12);
            // Deactivate all boulder sprites after game over
            for (n = 0; n < num_boulders; ++n) {
                xist_prepare_sprite(1, FALSE, XIST_VRAM_SPRITE_IMAGE_DATA + 1024,
                    TEST_SPRITE_BANK, n + 2, 0, 240, 0);
            }
            if (second_counter > high_score) {
                high_score = second_counter;
                save_high_score();
                draw_centered_text(score_label, 14);
                draw_centered_text(score_value, 15);
            } else {
                draw_centered_text(score_label, 14);
                draw_centered_text(score_value, 15);
            }

            game_running = 0;
        }


        }

        ++music_timer;
        if (music_timer >= music_duration_frames) {
            zsm_startmusic(MUSIC_BANK, BANKED_RAM_ADDRESS);
            music_timer = 0;
        }

        ++game_counter;
        if (game_counter % 60 == 0 && game_running) ++second_counter;

        pcm_play();
        if (playing) zsm_play();
    }
}
