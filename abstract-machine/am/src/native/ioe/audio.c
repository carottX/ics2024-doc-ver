#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <klib.h>
#include <SDL.h>

static int rfd = -1, wfd = -1;
static volatile int count = 0;

/**
 * Initializes the audio subsystem by creating a non-blocking pipe for communication.
 * This method sets up two file descriptors: one for reading (`rfd`) and one for writing (`wfd`).
 * The pipe is created with the `O_NONBLOCK` flag, ensuring that read and write operations
 * on the pipe will not block the calling process. If the pipe creation fails, the program
 * will terminate with an assertion error.
 *
 * The method performs the following steps:
 * 1. Declares an array `fds` to hold the two file descriptors for the pipe.
 * 2. Calls `pipe2` with the `O_NONBLOCK` flag to create the pipe and store the descriptors in `fds`.
 * 3. Asserts that the `pipe2` call was successful (returns 0).
 * 4. Assigns the read file descriptor (`fds[0]`) to `rfd` and the write file descriptor (`fds[1]`) to `wfd`.
 */
void __am_audio_init() {
  int fds[2];
  int ret = pipe2(fds, O_NONBLOCK);
  assert(ret == 0);
  rfd = fds[0];
  wfd = fds[1];
}

/**
 * Plays audio data by reading from a file descriptor and writing to an audio stream.
 *
 * This function reads audio data from a file descriptor (`rfd`) and writes it to the provided
 * audio stream (`stream`). The amount of data read is determined by the smaller of the remaining
 * audio data (`count`) and the requested length (`len`). If the remaining data is less than the
 * requested length, the remaining portion of the stream is filled with zeros.
 *
 * @param userdata A pointer to user-defined data (unused in this implementation).
 * @param stream   A pointer to the audio stream buffer where the audio data will be written.
 * @param len      The length of the audio stream buffer in bytes.
 */
static void audio_play(void *userdata, uint8_t *stream, int len) {
  int nread = len;
  if (count < len) nread = count;
  int b = 0;
  while (b < nread) {
    int n = read(rfd, stream, nread);
    if (n > 0) b += n;
  }

  count -= nread;
  if (len > nread) {
    memset(stream + nread, 0, len - nread);
  }
}

/**
 * Writes audio data to a file descriptor.
 *
 * This method writes the audio data from the provided buffer to the file descriptor
 * specified by `wfd`. It ensures that the entire buffer is written by handling partial
 * writes and retrying until all data is successfully written or an error occurs.
 *
 * @param buf Pointer to the buffer containing the audio data to be written.
 * @param len The number of bytes to write from the buffer.
 *
 * @note The method assumes that `wfd` is a valid file descriptor and that `count` is
 *       a global or external variable used to track the total number of bytes written.
 *       If the `write` system call returns -1 (indicating an error), the method treats
 *       it as if 0 bytes were written and continues attempting to write the remaining data.
 */
static void audio_write(uint8_t *buf, int len) {
  int nwrite = 0;
  while (nwrite < len) {
    int n = write(wfd, buf, len);
    if (n == -1) n = 0;
    count += n;
    nwrite += n;
  }
}

/**
 * Initializes and configures the audio subsystem using the provided control parameters.
 * This function sets up an SDL audio specification based on the given AM_AUDIO_CTRL_T structure,
 * initializes the SDL audio subsystem, and starts audio playback.
 *
 * @param ctrl A pointer to an AM_AUDIO_CTRL_T structure containing the audio control parameters.
 *             The structure should include the following fields:
 *             - freq: The audio frequency (sample rate) in Hz.
 *             - channels: The number of audio channels (e.g., 1 for mono, 2 for stereo).
 *             - samples: The number of samples per audio buffer.
 *
 * The function performs the following steps:
 * 1. Creates an SDL_AudioSpec structure and configures it with the provided parameters.
 * 2. Sets the audio format to AUDIO_S16SYS (16-bit signed system-endian).
 * 3. Assigns the audio_play function as the callback for audio playback.
 * 4. Initializes the SDL audio subsystem.
 * 5. Opens the audio device with the configured specification.
 * 6. Starts audio playback by unpausing the audio device.
 *
 * If the SDL audio subsystem initialization fails, no further actions are taken.
 */
void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  SDL_AudioSpec s = {};
  s.freq = ctrl->freq;
  s.format = AUDIO_S16SYS;
  s.channels = ctrl->channels;
  s.samples = ctrl->samples;
  s.callback = audio_play;
  s.userdata = NULL;

  count = 0;
  int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
  if (ret == 0) {
    SDL_OpenAudio(&s, NULL);
    SDL_PauseAudio(0);
  }
}

/**
 * @brief Updates the audio status structure with the current count of audio buffers.
 *
 * This function takes a pointer to an `AM_AUDIO_STATUS_T` structure and updates its `count` field
 * with the current value of the `count` variable. The `count` typically represents the number of
 * audio buffers currently available or processed.
 *
 * @param stat Pointer to the `AM_AUDIO_STATUS_T` structure to be updated.
 */
void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = count;
}

/**
 * @brief Plays audio data from the provided control structure.
 *
 * This function calculates the length of the audio data buffer by subtracting the 
 * start position from the end position of the buffer. It then writes the audio data 
 * to the audio output using the `audio_write` function.
 *
 * @param ctl Pointer to the AM_AUDIO_PLAY_T control structure containing the audio 
 *            buffer information. The structure must have a `buf` member with `start` 
 *            and `end` pointers defining the audio data range.
 */
void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  int len = ctl->buf.end - ctl->buf.start;
  audio_write(ctl->buf.start, len);
}

/**
 * Configures the audio settings by initializing the provided AM_AUDIO_CONFIG_T structure.
 * This function sets the 'present' field to true, indicating that audio is available.
 * It also determines the buffer size for the audio data by querying the pipe size
 * of the file descriptor 'rfd' using the fcntl function with the F_GETPIPE_SZ command.
 * The buffer size is then stored in the 'bufsize' field of the configuration structure.
 *
 * @param cfg Pointer to an AM_AUDIO_CONFIG_T structure that will be populated with
 *            the audio configuration details.
 */
void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = fcntl(rfd, F_GETPIPE_SZ);
}
