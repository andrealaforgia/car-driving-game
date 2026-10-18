/* Renders the engine sound to a WAV file so it can be heard without
 * launching the game: six seconds flat out from rest through the gears,
 * then two seconds coasting. Usage: render_sound out.wav */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "car.h"
#include "engine_note.h"
#include "engine_synth.h"
#include "game_constants.h"

#define RATE 44100
#define CHANNELS 2
#define SECONDS 8
#define STEP 1024

static void write_le(FILE* file, uint32_t value, int bytes) {
  for (int i = 0; i < bytes; ++i) {
    fputc((value >> (8 * i)) & 0xFF, file);
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: render_sound out.wav\n");
    return 2;
  }
  FILE* file = fopen(argv[1], "wb");
  if (!file) {
    return 1;
  }
  uint32_t frames = RATE * SECONDS;
  uint32_t data_bytes = frames * CHANNELS * 2;
  fwrite("RIFF", 1, 4, file);
  write_le(file, 36 + data_bytes, 4);
  fwrite("WAVEfmt ", 1, 8, file);
  write_le(file, 16, 4);
  write_le(file, 1, 2);
  write_le(file, CHANNELS, 2);
  write_le(file, RATE, 4);
  write_le(file, RATE * CHANNELS * 2, 4);
  write_le(file, CHANNELS * 2, 2);
  write_le(file, 16, 2);
  fwrite("data", 1, 4, file);
  write_le(file, data_bytes, 4);

  engine_synth_t synth = create_engine_synth(RATE);
  car_t car = create_car();
  car_input_t throttle = {true, false, false, false};
  car_input_t coast = {false, false, false, false};
  road_conditions_t flat = {0.0, 0.0, false};
  int16_t* buffer = malloc(STEP * CHANNELS * sizeof(int16_t));
  for (uint32_t done = 0; done < frames; done += STEP) {
    double seconds = (double)done / RATE;
    car_input_t input = seconds < 6.0 ? throttle : coast;
    update_car(&car, input, (double)STEP / RATE, 1e9, flat);
    double rpm = engine_rpm_fraction(car.speed);
    render_engine_synth(&synth, rpm, input.accelerate,
                        engine_volume(input.accelerate, rpm), buffer, STEP,
                        CHANNELS);
    fwrite(buffer, sizeof(int16_t), STEP * CHANNELS, file);
  }
  free(buffer);
  fclose(file);
  return 0;
}
