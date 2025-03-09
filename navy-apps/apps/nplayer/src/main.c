#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <SDL.h>
#include <vorbis.h>
#include <fixedptc.h>

#define MUSIC_PATH "/share/music/little-star.ogg"
#define SAMPLES 4096
#define FPS 10
#define W 400
#define H 100
#define MAX_VOLUME 128

stb_vorbis *v = NULL;
stb_vorbis_info info = {};
SDL_Surface *screen = NULL;
int is_end = 0;
int16_t *stream_save = NULL;
int volume = MAX_VOLUME;

/**
 * Draws a vertical line on the screen from the starting y-coordinate (y0) to the 
 * ending y-coordinate (y1) at the specified x-coordinate. The line is drawn using 
 * the provided color.
 *
 * @param x     The x-coordinate where the vertical line will be drawn.
 * @param y0    The starting y-coordinate of the vertical line.
 * @param y1    The ending y-coordinate of the vertical line.
 * @param color The color to use for drawing the line, represented as a 32-bit 
 *              unsigned integer (e.g., in ARGB format).
 *
 * @note The function assumes that y0 is less than or equal to y1. If this condition 
 *       is not met, the program will terminate with an assertion failure.
 */
static void drawVerticalLine(int x, int y0, int y1, uint32_t color) {
  assert(y0 <= y1);
  int i;
  uint32_t *p = (void *)screen->pixels;
  for (i = y0; i <= y1; i ++) {
    p[i * W + x] = color;
  }
}

/**
 * Visualizes an audio stream as a waveform on the screen using SDL.
 * 
 * This method takes a stream of 16-bit audio samples and renders them as a waveform.
 * The waveform is drawn vertically, with each sample represented by a line extending
 * from the center of the screen. The amplitude of the sample determines the length
 * of the line. The waveform is modulated by a cosine function to create a smooth,
 * oscillating effect. The color of the lines cycles through a 24-bit color space
 * with each sample, creating a visually dynamic representation of the audio data.
 * 
 * @param stream A pointer to an array of 16-bit audio samples.
 * @param samples The number of samples in the stream to visualize.
 * 
 * The method first clears the screen by filling it with black. It then calculates
 * the vertical position for each sample based on its amplitude and the cosine
 * modulation. The waveform is drawn using vertical lines, with the color
 * incrementing for each sample and wrapping around after reaching the maximum
 * 24-bit color value. Finally, the screen is updated to display the rendered
 * waveform.
 */
static void visualize(int16_t *stream, int samples) {
  int i;
  static int color = 0;
  SDL_FillRect(screen, NULL, 0);
  int center_y = H / 2;
  for (i = 0; i < samples; i ++) {
    fixedpt multipler = fixedpt_cos(fixedpt_divi(fixedpt_muli(FIXEDPT_PI, 2 * i), samples));
    int x = i * W / samples;
    int y = center_y - fixedpt_toint(fixedpt_muli(fixedpt_divi(fixedpt_muli(multipler, stream[i]), 32768), H / 2));
    if (y < center_y) drawVerticalLine(x, y, center_y, color);
    else drawVerticalLine(x, center_y, y, color);
    color ++;
    color &= 0xffffff;
  }
  SDL_UpdateRect(screen, 0, 0, 0, 0);
}

/**
 * Adjusts the volume of an audio stream by scaling each sample based on the current volume level.
 * 
 * The method modifies the provided audio stream in-place. If the volume is set to the maximum level
 * (MAX_VOLUME), the stream remains unchanged. If the volume is set to 0, the stream is muted by
 * setting all samples to 0. For intermediate volume levels, each sample is scaled by the ratio of
 * the current volume to the maximum volume.
 *
 * @param stream Pointer to the audio stream, represented as an array of 16-bit signed integers.
 * @param samples The number of samples in the audio stream.
 */
static void AdjustVolume(int16_t *stream, int samples) {
  if (volume == MAX_VOLUME) return;
  if (volume == 0) {
    memset(stream, 0, samples * sizeof(stream[0]));
    return;
  }
  int i;
  for (i = 0; i < samples; i ++) {
    stream[i] = stream[i] * volume / MAX_VOLUME;
  }
}

/**
 * Fills the provided audio stream buffer with audio data from a Vorbis file.
 * 
 * This function decodes audio data from a Vorbis file using the stb_vorbis library
 * and fills the given stream buffer with the decoded samples. If the decoded audio
 * data is shorter than the requested length, the remaining buffer is filled with
 * silence (zeroes). The function also adjusts the volume of the audio samples
 * using the `AdjustVolume` function. If the end of the audio file is reached, the
 * `is_end` flag is set to indicate completion. The final audio stream is saved to
 * a global buffer `stream_save` for further use.
 *
 * @param userdata A pointer to user-specific data (unused in this implementation).
 * @param stream   A pointer to the audio stream buffer to be filled with audio data.
 * @param len      The length of the audio stream buffer in bytes.
 */
void FillAudio(void *userdata, uint8_t *stream, int len) {
  int nbyte = 0;
  int samples_per_channel = stb_vorbis_get_samples_short_interleaved(v,
      info.channels, (int16_t *)stream, len / sizeof(int16_t));
  if (samples_per_channel != 0 || len < sizeof(int16_t)) {
    int samples = samples_per_channel * info.channels;
    nbyte = samples * sizeof(int16_t);
    AdjustVolume((int16_t *)stream, samples);
  } else {
    is_end = 1;
  }
  if (nbyte < len) memset(stream + nbyte, 0, len - nbyte);
  memcpy(stream_save, stream, len);
}

/**
 * @brief Main entry point for the application.
 *
 * This function initializes the SDL library, sets up the video mode, and loads
 * an audio file specified by MUSIC_PATH. It decodes the audio file using the
 * stb_vorbis library and plays it through the SDL audio system. The application
 * also visualizes the audio data in real-time and allows the user to adjust the
 * volume using the '-' and '=' keys. The application runs in a loop until the
 * user exits, at which point it cleans up resources and terminates.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return int Returns 0 on successful execution.
 */
int main(int argc, char *argv[]) {
  SDL_Init(0);
  screen = SDL_SetVideoMode(W, H, 32, SDL_HWSURFACE);
  SDL_FillRect(screen, NULL, 0);
  SDL_UpdateRect(screen, 0, 0, 0, 0);

  FILE *fp = fopen(MUSIC_PATH, "r");
  assert(fp);
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  void *buf = malloc(size);
  assert(size);
  fseek(fp, 0, SEEK_SET);
  int ret = fread(buf, size, 1, fp);
  assert(ret == 1);
  fclose(fp);

  int error;
  v = stb_vorbis_open_memory(buf, size, &error, NULL);
  assert(v);
  info = stb_vorbis_get_info(v);

  SDL_AudioSpec spec;
  spec.freq = info.sample_rate;
  spec.channels = info.channels;
  spec.samples = SAMPLES;
  spec.format = AUDIO_S16SYS;
  spec.userdata = NULL;
  spec.callback = FillAudio;
  SDL_OpenAudio(&spec, NULL);

  stream_save = malloc(SAMPLES * info.channels * sizeof(*stream_save));
  assert(stream_save);
  printf("Playing %s(freq = %d, channels = %d)...\n", MUSIC_PATH, info.sample_rate, info.channels);
  SDL_PauseAudio(0);

  while (!is_end) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_KEYDOWN) {
        switch (ev.key.keysym.sym) {
          case SDLK_MINUS:  if (volume >= 8) volume -= 8; break;
          case SDLK_EQUALS: if (volume <= MAX_VOLUME - 8) volume += 8; break;
        }
      }
    }
    SDL_Delay(1000 / FPS);
    visualize(stream_save, SAMPLES * info.channels);
  }

  SDL_CloseAudio();
  stb_vorbis_close(v);
  SDL_Quit();
  free(stream_save);
  free(buf);

  return 0;
}
