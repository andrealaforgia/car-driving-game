#include "scenery_sprite.h"

#include <string.h>

typedef struct {
  int width;
  int height;
  const char* const* rows;
} art_t;

static const char* const ROUND_TREE[32] = {
    ".......GGGGGG.......", ".....GGLLLLGGG......", "....GGLLLLLLGGGG....",
    "...GGLLLLLLLGGGGG...", "..GGLLLLLLLLGGGGGG..", ".GGLLLLLLLLLGGGGGGG.",
    ".GLLLLLLLLLGGGGGGGg.", "GGLLLLLLLLGGGGGGGGgg", "GLLLLLLLLGGGGGGGGGgg",
    "GLLLLLLLGGGGGGGGGggg", "GLLLLLLGGGGGGGGGgggg", "GGLLLLGGGGGGGGGggggg",
    "GGGLLGGGGGGGGGgggggg", ".GGGGGGGGGGGGGggggg.", ".GGGGGGGGGGGGgggggg.",
    "..GGGGGGGGGGGggggg..", "..GGGGGGGGGGggggg...", "...GGGGGGGGggggg....",
    "....GGGGGGGgggg.....", ".....GGGGGggg.......", "......GGGggg........",
    ".........TT.........", ".........TT.........", ".........TT.........",
    ".........TT.........", ".........TT.........", "........tTTt........",
    "........tTTt........", "........tTTt........", ".......ttTTtt.......",
    ".......ttTTtt.......", "......ttTTTTtt......"};

static const char* const PINE_TREE[32] = {
    ".........gg.........", "........gLgg........", "........gLgg........",
    ".......gLLggg.......", ".......gLGggg.......", "......gLLGgggg......",
    "......gLGGgggg......", ".....gLLGGggggg.....", ".....gLGGGggggg.....",
    "....gLLGGGgggggg....", "......gLGGgggg......", ".....gLLGGggggg.....",
    ".....gLGGGggggg.....", "....gLLGGGgggggg....", "....gLGGGGgggggg....",
    "...gLLGGGGggggggg...", "...gLGGGGGggggggg...", "..gLLGGGGGgggggggg..",
    ".....gLGGGggggg.....", "....gLLGGGgggggg....", "....gLGGGGgggggg....",
    "...gLLGGGGggggggg...", "...gLGGGGGggggggg...", "..gLLGGGGGgggggggg..",
    "..gLGGGGGGgggggggg..", ".gLLGGGGGGggggggggg.", ".........TT.........",
    ".........TT.........", ".........TT.........", ".........TT.........",
    "........tTTt........", ".......ttTTtt......."};

static const char* const BUSH[10] = {".....GGGGG......", "...GGLLLGGGG....",
                                     "..GLLLLLGGGGGg..", ".GLLLLLGGGGGGgg.",
                                     "GLLLLLGGGGGGGggg", "GLLLLGGGGGGGgggg",
                                     "GGLLGGGGGGGggggg", ".GGGGGGGGGggggg.",
                                     "..GGGGGGGggggg..", "....GGGgggg....."};

static const char* const SIGN_RED[18] = {
    "WWWWWWWWWWWWWWWWWWWWWWWW", "WRRRRRRRRRRRRRRRRRRRRRRW",
    "WRRRRRRRRRRRRRRRRRRRRRRW", "WRRWWWRRWWWRRRWWWRRWWWRW",
    "WRRWRRRRWRWRRRWRWRRWRRRW", "WRRWWWRRWWWRRRWWWRRWWWRW",
    "WRRRRWRRWRRRRRWRRRRRRWRW", "WRRWWWRRWRRRRRWRRRRWWWRW",
    "WRRRRRRRRRRRRRRRRRRRRRRW", "WRRRRRRRRRRRRRRRRRRRRRRW",
    "WWWWWWWWWWWWWWWWWWWWWWWW", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS.........."};

static const char* const SIGN_BLUE[18] = {
    "WWWWWWWWWWWWWWWWWWWWWWWW", "WBBBBBBBBBBBBBBBBBBBBBBW",
    "WBBBBBBBBBBBBBBBBBBBBBBW", "WBBYYYBBYBYBBYYYBBYYYBBW",
    "WBBYBBBBYBYBBYBBBBYBYBBW", "WBBYYYBBYYYBBYYYBBYYYBBW",
    "WBBBBYBBYBYBBYBBBBYBBBBW", "WBBYYYBBYBYBBYYYBBYBBBBW",
    "WBBBBBBBBBBBBBBBBBBBBBBW", "WBBBBBBBBBBBBBBBBBBBBBBW",
    "WWWWWWWWWWWWWWWWWWWWWWWW", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS..........",
    "..........SSSS..........", "..........SSSS.........."};

/* Rear view of a saloon. C is the body colour, c its shaded side. */
static const char* const CAR[18] = {
    "........CCCCCCCCCCCC........", "......CCccccccccccccCC......",
    ".....CcKKKKKKKKKKKKKKcC.....", "....CcKKKKKKKKKKKKKKKKcC....",
    "....CcKKKKKKKKKKKKKKKKcC....", "....CCccccccccccccccccCC....",
    "...CCCCCCCCCCCCCCCCCCCCCC...", "..CCCCCCCCCCCCCCCCCCCCCCCC..",
    "..CRRCCCCCCCCCCCCCCCCCCRRC..", "..CRRCCCCCCCCCCCCCCCCCCRRC..",
    "..CCCCCCCCCCCCCCCCCCCCCCCC..", "..CccccccccccccccccccccccC..",
    "..CccccccSSSSSSSSSSccccccC..", ".kkkcccccSSSSSSSSSSccccckkk.",
    ".kkkkkccccccccccccccccckkkk.", ".kkkkkkkkkkkkkkkkkkkkkkkkkk.",
    "..kkkk..................kkkk", "..kkkk..................kkkk"};

/* Rear view of a box lorry: doors with a seam and ribs, marker lights,
 * a bumper with lights, and twin wheels. */
static const char* const TRUCK[36] = {
    ".cccccccccccccccccccccc.", ".cWCCCCCCCCccCCCCCCCCWc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cccccccccccccccccccccc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cccccccccccccccccccccc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cccccccccccccccccccccc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cCCCCCCCCCccCCCCCCCCCc.",
    ".cCCCCCCCCCccCCCCCCCCCc.", ".cccccccccccccccccccccc.",
    ".RRSSSSSSSSSSSSSSSSSSRR.", ".RRYSSSSSSSSSSSSSSSSYRR.",
    "..KKKKKKKKKKKKKKKKKKKK..", "..KKKKKKKKKKKKKKKKKKKK..",
    ".kkkkkk..........kkkkkk.", ".kkkkkk..........kkkkkk.",
    ".kkkkkk..........kkkkkk.", ".kkkkkk..........kkkkkk."};

static art_t art_for(billboard_kind_t kind, int variant) {
  art_t art = {0, 0, NULL};
  switch (kind) {
    case BILLBOARD_TREE:
      art.width = 20;
      art.height = 32;
      art.rows = variant % 2 == 0 ? ROUND_TREE : PINE_TREE;
      break;
    case BILLBOARD_BUSH:
      art.width = 16;
      art.height = 10;
      art.rows = BUSH;
      break;
    case BILLBOARD_SIGN:
      art.width = 24;
      art.height = 18;
      art.rows = variant % 2 == 0 ? SIGN_RED : SIGN_BLUE;
      break;
    case BILLBOARD_CAR:
      art.width = 28;
      art.height = 18;
      art.rows = CAR;
      break;
    case BILLBOARD_TRUCK:
      art.width = 24;
      art.height = 36;
      art.rows = TRUCK;
      break;
    default:
      break;
  }
  return art;
}

int billboard_variants(billboard_kind_t kind) {
  switch (kind) {
    case BILLBOARD_TREE:
    case BILLBOARD_SIGN:
      return 2;
    case BILLBOARD_CAR:
    case BILLBOARD_TRUCK:
      return BILLBOARD_VARIANTS;
    default:
      return 1;
  }
}

sprite_size_t billboard_sprite_size(billboard_kind_t kind, int variant) {
  art_t art = art_for(kind, variant);
  sprite_size_t size = {art.width, art.height};
  return size;
}

static const uint32_t CAR_COLOURS[BILLBOARD_VARIANTS] = {
    0xD02020, 0x2040C0, 0xF0F0F0, 0xE0C020, 0x208040, 0x303030};

static uint32_t to_rgba(uint32_t rgb) {
  return 0xFF000000u | ((rgb & 0xFF) << 16) | (rgb & 0xFF00) |
         ((rgb >> 16) & 0xFF);
}

static uint32_t shade(uint32_t rgb, int percent) {
  uint32_t r = ((rgb >> 16) & 0xFF) * percent / 100;
  uint32_t g = ((rgb >> 8) & 0xFF) * percent / 100;
  uint32_t b = (rgb & 0xFF) * percent / 100;
  return (r << 16) | (g << 8) | b;
}

static uint32_t colour_of(char cell, int variant) {
  uint32_t body = CAR_COLOURS[variant % BILLBOARD_VARIANTS];
  switch (cell) {
    case 'T':
      return to_rgba(0x6B4423);
    case 't':
      return to_rgba(0x4E2F16);
    case 'G':
      return to_rgba(0x2E8B3A);
    case 'g':
      return to_rgba(0x1E6B2A);
    case 'L':
      return to_rgba(0x5CC46A);
    case 'W':
      return to_rgba(0xFFFFFF);
    case 'R':
      return to_rgba(0xD02020);
    case 'B':
      return to_rgba(0x2050C0);
    case 'Y':
      return to_rgba(0xF0D000);
    case 'S':
      return to_rgba(0x909090);
    case 'K':
      return to_rgba(0x303848);
    case 'k':
      return to_rgba(0x101010);
    case 'C':
      return to_rgba(body);
    case 'c':
      return to_rgba(shade(body, 70));
    default:
      return 0;
  }
}

static bool rows_are_well_formed(art_t art) {
  for (int row = 0; row < art.height; ++row) {
    if (strlen(art.rows[row]) != (size_t)art.width) {
      return false;
    }
    for (int column = 0; column < art.width; ++column) {
      char cell = art.rows[row][column];
      if (cell != '.' && colour_of(cell, 0) == 0) {
        return false;
      }
    }
  }
  return true;
}

bool billboard_art_is_well_formed(void) {
  for (int kind = 0; kind < BILLBOARD_KINDS; ++kind) {
    for (int variant = 0; variant < billboard_variants((billboard_kind_t)kind);
         ++variant) {
      art_t art = art_for((billboard_kind_t)kind, variant);
      if (!art.rows || art.width > BILLBOARD_MAX_WIDTH ||
          art.height > BILLBOARD_MAX_HEIGHT || !rows_are_well_formed(art)) {
        return false;
      }
    }
  }
  return true;
}

void render_billboard_pixels(uint32_t* pixels, billboard_kind_t kind,
                             int variant) {
  art_t art = art_for(kind, variant);
  for (int row = 0; row < art.height; ++row) {
    for (int column = 0; column < art.width; ++column) {
      pixels[row * art.width + column] =
          colour_of(art.rows[row][column], variant);
    }
  }
}
