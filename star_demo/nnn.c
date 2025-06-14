#define TEST_SPRITE_BANK  15
#define SFX_BANK 35
#define MUSIC_BANK 51
#define BANKED_RAM_ADDRESS 0xA000UL
#define BANK_NUM (*(unsigned char *)0x00)
#define XIST_LAYER_MAP_WIDTH 40
#define XIST_LAYER_MAP_HEIGHT 30
#include <cx16.h>
#include <cbm.h>
#include <stdio.h>
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
#define BANKED_RAM_ADDRESS 0xA000UL
#define BANK_NUM (*(unsigned char *)0x00)

#define BULLET_SPRITE_ID 34
#define BULLET_META_INDEX 5 

unsigned short ship_x = 144;
unsigned short ship_y = 200;
char clear_counter[MAX_SIGNED_INT_CHARS] = { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0 };
const unsigned char num_stars = 32;
signed short star_starting_pos[32];
unsigned short current_joypad_state;

// Bullet state
unsigned short bullet_x;
unsigned short bullet_y;
unsigned char bullet_active = 0;

void load_file_to_highram(const char *filename, unsigned char bank) {
    BANK_NUM = bank;
    cbm_k_setnam(filename);
    cbm_k_setlfs(0, 8, 2);
    cbm_k_load(0, (unsigned short)BANKED_RAM_ADDRESS);
}

void process_input() {
    if (current_joypad_state & JOYPAD_L && ship_x > 0) ship_x -= 3;
    if (current_joypad_state & JOYPAD_R && ship_x < 320 - 16) ship_x += 3;
    if (current_joypad_state & JOYPAD_U && ship_y > 0) ship_y -= 3;
    if (current_joypad_state & JOYPAD_D && ship_y < 240 - 16) ship_y += 3;

    xist_curr_sprite_idx = 1;
    xist_curr_sprite_x_pos = ship_x;
    xist_curr_sprite_y_pos = ship_y;
    xist_update_sprite_position();
}

void move_stars() {
    signed short n;
    for (n = 0; n < num_stars; ++n) {
        if (n % 3 == 0) xist_sprite_bank.y_pos[n+2] += 3;
        else xist_sprite_bank.y_pos[n+2] += 2;
        if (xist_sprite_bank.y_pos[n+2] > 250) xist_sprite_bank.y_pos[n+2] = star_starting_pos[n];

        xist_curr_sprite_idx = n + 2;
        xist_curr_sprite_x_pos = xist_sprite_bank.x_pos[n+2];
        xist_curr_sprite_y_pos = xist_sprite_bank.y_pos[n+2];
        xist_update_sprite_position();
    }
}

void fire_bullet() {
    if (!bullet_active) {
        bullet_x = ship_x + 6;
        bullet_y = ship_y;

        bullet_active = 1;

        xist_prepare_sprite(1, FALSE, XIST_VRAM_SPRITE_IMAGE_DATA, TEST_SPRITE_BANK, BULLET_META_INDEX, bullet_x, bullet_y, XIST_SPRITE_ACTIVE);
        xist_sprite_bank.x_pos[BULLET_SPRITE_ID] = bullet_x;
        xist_sprite_bank.y_pos[BULLET_SPRITE_ID] = bullet_y;

        pcm_trigger_digi(SFX_BANK, BANKED_RAM_ADDRESS);
    }
}

void update_bullet() {
    if (!bullet_active) return;

    bullet_y -= 6;
    if (bullet_y < 8) {
        bullet_active = 0;
        xist_sprite_bank.y_pos[BULLET_SPRITE_ID] = 240; // Move off screen
        return;
    }

    xist_sprite_bank.x_pos[BULLET_SPRITE_ID] = bullet_x;
    xist_sprite_bank.y_pos[BULLET_SPRITE_ID] = bullet_y;

    xist_curr_sprite_idx = BULLET_SPRITE_ID;
    xist_curr_sprite_x_pos = bullet_x;
    xist_curr_sprite_y_pos = bullet_y;
    xist_update_sprite_position();
}

void main() {
    signed long game_counter = 0;
    unsigned short second_counter = 0;
    unsigned short n;
    char game_counter_str[MAX_SIGNED_INT_CHARS];
    char second_counter_str[MAX_SIGNED_INT_CHARS];
    unsigned char playing;
    xist_sprite_bank.x_pos[BULLET_SPRITE_ID] = 0;
    xist_sprite_bank.y_pos[BULLET_SPRITE_ID] = 240;

    xist_seed_a = 123;
    xist_seed_b = 234;

    // Video setup
    VERA.display.hscale = 64;
    VERA.display.vscale = 64;

    // Load music and SFX
    load_file_to_highram("greenmotor.zsm", MUSIC_BANK);
    zsm_init();
    playing = zsm_startmusic(MUSIC_BANK, BANKED_RAM_ADDRESS);

    load_file_to_highram("laser.zcm", SFX_BANK);
    pcm_init();
    pcm_trigger_digi(SFX_BANK, BANKED_RAM_ADDRESS);  // test sound

    // Xist setup
    xist_initialize_tiles();
    xist_initialize_text_tiles();
    xist_load_file_to_highram("gfxmeta.dat", XIST_SPRITE_METADATA_BANK);
    xist_load_graphic(0, TRUE, TEST_SPRITE_BANK, FALSE, 0);
    xist_copy_highram_to_vram_using_metadata(XIST_LAYER_0_TILE_BASE, TEST_SPRITE_BANK, 5);

    // Clear tilemap
    for (n = 0; n < LAYER_0_MAPBASE_TOTAL; ++n) xist_map_tiles[n] = 0;
    VERA.address = XIST_LAYER_0_MAPBASE;
    VERA.address_hi = XIST_LAYER_0_MAPBASE >> 16;
    VERA.address_hi |= 0b10000;
    for (n = 0; n < LAYER_0_MAPBASE_TOTAL; ++n) {
        VERA.data0 = xist_map_tiles[n];
        VERA.data0 = 0;
    }

    // Ship sprite
    xist_prepare_sprite(0, TRUE, XIST_VRAM_SPRITE_IMAGE_DATA, TEST_SPRITE_BANK, 1, ship_x, ship_y, XIST_SPRITE_ACTIVE);

    // Starfield
    xist_rand_min = 0;
    xist_rand_max = 255;
    for (n = 0; n < num_stars; ++n) {
        xist_sprite_bank.x_pos[n+2] = n * 10;
        xist_sprite_bank.y_pos[n+2] = -(xist_rand());
        star_starting_pos[n] = xist_sprite_bank.y_pos[n+2];
        if (n == 0) {
            xist_prepare_sprite(1, TRUE, XIST_VRAM_SPRITE_IMAGE_DATA + 1024, TEST_SPRITE_BANK, n+2, xist_sprite_bank.x_pos[n+2], xist_sprite_bank.y_pos[n+2], XIST_SPRITE_ACTIVE);
        } else {
            xist_prepare_sprite(1, FALSE, XIST_VRAM_SPRITE_IMAGE_DATA + 1024, TEST_SPRITE_BANK, n+2, xist_sprite_bank.x_pos[n+2], xist_sprite_bank.y_pos[n+2], XIST_SPRITE_ACTIVE);
        }
        if (n % 2 == 0) xist_sprite_bank.current_frame[n+2] = 2;
    }
    
    xist_prepare_sprite(10, FALSE, XIST_VRAM_SPRITE_IMAGE_DATA , TEST_SPRITE_BANK, BULLET_META_INDEX, 0, 240, XIST_SPRITE_ACTIVE);
    // Game loop
    for (EVER) {
        xist_wait();

        current_joypad_state = xist_get_joypad();
        process_input();

        if (current_joypad_state & JOYPAD_A) fire_bullet();

        update_bullet();
        move_stars();

        xist_proc_sprite_start_idx = 1;
        xist_proc_sprite_end_idx = num_stars + 1;
        xist_process_sprites();

        xist_convert_long_to_ascii_array(game_counter, game_counter_str);
        xist_convert_long_to_ascii_array(second_counter, second_counter_str);

        xist_text = clear_counter;
        xist_text_tile_x = 31;
        xist_text_tile_y = 28;
        xist_draw_text();
        xist_text = second_counter_str;
        xist_draw_text();

        xist_text = clear_counter;
        xist_text_tile_x = 34;
        xist_draw_text();
        xist_text = game_counter_str;
        xist_draw_text();

        ++game_counter;
        if (game_counter % 60 == 0) ++second_counter;

        pcm_play();
        if (playing) zsm_play();
    }
}