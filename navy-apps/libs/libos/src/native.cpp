#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <dlfcn.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

//#define MODE_800x600

#define FPS 60
#define WINDOW_W 800
#define WINDOW_H 600
#ifdef MODE_800x600
const int disp_w = WINDOW_W, disp_h = WINDOW_H;
#else
const int disp_w = 400, disp_h = 300;
#endif
static int pipe_size = 0;
#define FB_SIZE (disp_w * disp_h * sizeof(uint32_t))
#define RMASK 0x00ff0000
#define GMASK 0x0000ff00
#define BMASK 0x000000ff
#define AMASK 0x00000000

static FILE *(*glibc_fopen)(const char *path, const char *mode) = NULL;
static int (*glibc_open)(const char *path, int flags, ...) = NULL;
static ssize_t (*glibc_read)(int fd, void *buf, size_t count) = NULL;
static ssize_t (*glibc_write)(int fd, const void *buf, size_t count) = NULL;
static int (*glibc_execve)(const char *filename, char *const argv[], char *const envp[]) = NULL;

static SDL_Window *window = NULL;
static SDL_Surface *surface = NULL;
static int dummy_fd = -1;
static int dispinfo_fd = -1;
static int fb_memfd = -1;
static int evt_fd = -1;
static int sb_fifo[2] = {-1, -1};
static int sbctl_fd = -1;
static uint32_t *fb = NULL;
static char fsimg_path[512] = "";

/**
 * @brief Constructs a new file system image path by concatenating a base path with a given relative path.
 *
 * This method takes a base path (`fsimg_path`) and a relative path (`path`), and concatenates them
 * to form a new full path. The resulting path is stored in the `newpath` buffer. The method assumes
 * that `fsimg_path` is a null-terminated string representing the base directory of the file system
 * image, and `path` is a null-terminated string representing the relative path within the file system
 * image. The caller is responsible for ensuring that `newpath` has sufficient space to hold the
 * concatenated result.
 *
 * @param newpath A pointer to the buffer where the concatenated path will be stored.
 * @param path A pointer to the null-terminated string representing the relative path to be appended
 *             to the base path.
 */
static inline void get_fsimg_path(char *newpath, const char *path) {
  sprintf(newpath, "%s%s", fsimg_path, path);
}

#define _KEYS(_) \
  _(ESCAPE) _(F1) _(F2) _(F3) _(F4) _(F5) _(F6) _(F7) _(F8) _(F9) _(F10) _(F11) _(F12) \
  _(GRAVE) _(1) _(2) _(3) _(4) _(5) _(6) _(7) _(8) _(9) _(0) _(MINUS) _(EQUALS) _(BACKSPACE) \
  _(TAB) _(Q) _(W) _(E) _(R) _(T) _(Y) _(U) _(I) _(O) _(P) _(LEFTBRACKET) _(RIGHTBRACKET) _(BACKSLASH) \
  _(CAPSLOCK) _(A) _(S) _(D) _(F) _(G) _(H) _(J) _(K) _(L) _(SEMICOLON) _(APOSTROPHE) _(RETURN) \
  _(LSHIFT) _(Z) _(X) _(C) _(V) _(B) _(N) _(M) _(COMMA) _(PERIOD) _(SLASH) _(RSHIFT) \
  _(LCTRL) _(APPLICATION) _(LALT) _(SPACE) _(RALT) _(RCTRL) \
  _(UP) _(DOWN) _(LEFT) _(RIGHT) _(INSERT) _(DELETE) _(HOME) _(END) _(PAGEUP) _(PAGEDOWN)

#define COND(k) \
  if (scancode == SDL_SCANCODE_##k) name = #k;

#define KEY_QUEUE_LEN 64
static SDL_Event key_queue[KEY_QUEUE_LEN] = {};
static int key_f = 0, key_r = 0;
static SDL_mutex *key_queue_lock = NULL;

/**
 * @brief Event handling thread that processes SDL events in an infinite loop.
 * 
 * This method runs in a separate thread and continuously waits for SDL events.
 * When an event is received, it processes the event based on its type:
 * - If the event is of type `SDL_QUIT`, the program exits with a status code of 0.
 * - If the event is of type `SDL_KEYDOWN` or `SDL_KEYUP`, the event is added to a circular
 *   queue (`key_queue`) for further processing. The queue is protected by a mutex
 *   (`key_queue_lock`) to ensure thread safety. The method also ensures that the queue
 *   does not overflow by asserting that the read and write indices (`key_r` and `key_f`)
 *   are not equal.
 * 
 * @param args A pointer to optional arguments passed to the thread (unused in this implementation).
 * @return int Always returns 0, although the method runs in an infinite loop and does not return
 *             under normal circumstances.
 */
static int event_thread(void *args) {
  SDL_Event event;
  while (1) {
    SDL_WaitEvent(&event);

    switch (event.type) {
      case SDL_QUIT: exit(0); break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        SDL_LockMutex(key_queue_lock);
        key_queue[key_r] = event;
        key_r = (key_r + 1) % KEY_QUEUE_LEN;
        assert(key_r != key_f);
        SDL_UnlockMutex(key_queue_lock);
        break;
    }
  }
  return 0;
}

/**
 * @brief Synchronizes the texture by blitting the source surface onto the window's surface and updating the window.
 *
 * This method performs the following operations:
 * 1. Blits the contents of the `surface` onto the window's surface using `SDL_BlitScaled`. 
 *    The entire `surface` is scaled to fit the entire window surface.
 * 2. Updates the window's surface to reflect the changes using `SDL_UpdateWindowSurface`.
 * 3. Returns the `interval` parameter unchanged, allowing the method to be used as a callback 
 *    for SDL timers or other interval-based mechanisms.
 *
 * @param interval The interval (in milliseconds) at which the synchronization should occur. 
 *                 This value is returned unchanged.
 * @param param A pointer to user-defined data. This parameter is unused in this method.
 * @return The `interval` value passed to the method, unchanged.
 */
static Uint32 texture_sync(Uint32 interval, void *param) {
  SDL_BlitScaled(surface, NULL, SDL_GetWindowSurface(window), NULL);
  SDL_UpdateWindowSurface(window);
  return interval;
}

/**
 * @brief Fills the audio stream buffer with data from the FIFO.
 *
 * This method reads audio data from the specified FIFO (First In First Out) buffer
 * and writes it into the provided stream buffer. If the amount of data read is less
 * than the requested length, the remaining portion of the stream buffer is filled
 * with zeros to ensure the buffer is fully populated.
 *
 * @param userdata A pointer to user-specific data (unused in this implementation).
 * @param stream A pointer to the audio stream buffer that needs to be filled.
 * @param len The length of the audio stream buffer in bytes.
 *
 * @note This method uses `glibc_read` to read data from the FIFO. If the read operation
 * fails (returns -1), the method assumes no data was read and fills the entire stream
 * buffer with zeros.
 */
static void audio_fill(void *userdata, uint8_t *stream, int len) {
  int nread = glibc_read(sb_fifo[0], stream, len);
  if (nread == -1) nread = 0;
  if (nread < len) memset(stream + nread, 0, len - nread);
}

/**
 * @brief Initializes and opens the display for the simulated Nanos application.
 *
 * This method performs the following operations:
 * 1. Creates a memory file descriptor (memfd) for the framebuffer using `memfd_create`.
 * 2. Truncates the memfd to the size of the framebuffer (`FB_SIZE`).
 * 3. Maps the framebuffer into the process's address space using `mmap`.
 * 4. Initializes the framebuffer memory to zero using `memset`.
 * 5. Initializes the SDL video and timer subsystems.
 * 6. Creates an SDL window with the specified dimensions (`WINDOW_W`, `WINDOW_H`).
 * 7. Creates an SDL RGB surface from the framebuffer with the specified color masks.
 * 8. Starts an SDL thread for handling events.
 * 9. Adds an SDL timer to synchronize the texture at the specified frame rate (`FPS`).
 *
 * Asserts are used to ensure critical operations succeed, such as creating the memfd,
 * truncating it, mapping the framebuffer, and initializing SDL components.
 */
static void open_display() {
  fb_memfd = memfd_create("fb", 0);
  assert(fb_memfd != -1);
  int ret = ftruncate(fb_memfd, FB_SIZE);
  assert(ret == 0);
  fb = (uint32_t *)mmap(NULL, FB_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fb_memfd, 0);
  assert(fb != (void *)-1);
  memset(fb, 0, FB_SIZE);
  lseek(fb_memfd, 0, SEEK_SET);

  SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
  window = SDL_CreateWindow("Simulated Nanos Application",
      SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL);
  surface = SDL_CreateRGBSurfaceFrom(fb, disp_w, disp_h, 32, disp_w * sizeof(uint32_t),
      RMASK, GMASK, BMASK, AMASK);
  SDL_CreateThread(event_thread, "event thread", nullptr);
  SDL_AddTimer(1000 / FPS, texture_sync, NULL);
}

/**
 * @brief Opens an event by initializing necessary resources.
 *
 * This method performs two main operations:
 * 1. Creates a mutex using SDL_CreateMutex() and assigns it to `key_queue_lock`.
 *    This mutex is used to synchronize access to the key queue, ensuring thread safety.
 * 2. Duplicates the file descriptor `dummy_fd` using dup() and assigns it to `evt_fd`.
 *    This allows for the creation of a new file descriptor that refers to the same
 *    file or resource as `dummy_fd`, enabling independent operations on the event file descriptor.
 *
 * @note Ensure that `dummy_fd` is a valid file descriptor before calling this method.
 * @warning The caller is responsible for managing the lifecycle of the created mutex
 *          and the duplicated file descriptor to avoid resource leaks.
 */
static void open_event() {
  key_queue_lock = SDL_CreateMutex();
  evt_fd = dup(dummy_fd);
}

/**
 * @brief Opens an audio stream by creating a non-blocking pipe and configuring it for audio data.
 *
 * This method performs the following operations:
 * 1. Creates a non-blocking pipe using `pipe2` with the `O_NONBLOCK` flag, which allows the pipe to
 *    operate without blocking on read or write operations. The pipe is stored in the `sb_fifo` array.
 * 2. Asserts that the pipe creation was successful by checking the return value of `pipe2`.
 * 3. Duplicates the file descriptor `dummy_fd` and stores the result in `sbctl_fd`. This is likely
 *    used for controlling the audio stream.
 * 4. Retrieves the size of the pipe buffer using `fcntl` with the `F_GETPIPE_SZ` command and stores
 *    it in `pipe_size`. This value represents the capacity of the pipe for buffering audio data.
 *
 * @note This method assumes that `sb_fifo`, `dummy_fd`, `sbctl_fd`, and `pipe_size` are defined
 *       elsewhere in the code.
 * @warning If the pipe creation fails, the program will terminate due to the `assert` statement.
 */
static void open_audio() {
  int ret = pipe2(sb_fifo, O_NONBLOCK);
  assert(ret == 0);
  sbctl_fd = dup(dummy_fd);
  pipe_size = fcntl(sb_fifo[0], F_GETPIPE_SZ);
}

/**
 * @brief Redirects the file path to a new path if the file exists at the new location.
 *
 * This method takes an original file path and attempts to redirect it to a new path by calling
 * `get_fsimg_path` to generate the new path. If the file exists at the new path (checked using `access`),
 * the method logs the redirection to `stderr` and returns the new path. If the file does not exist at the
 * new path, the original path is returned.
 *
 * @param newpath A buffer to store the generated new path. This buffer must be large enough to hold the path.
 * @param path The original file path to be redirected.
 * @return const char* The new path if the file exists at the new location, otherwise the original path.
 */
static const char* redirect_path(char *newpath, const char *path) {
  get_fsimg_path(newpath, path);
  if (0 == access(newpath, 0)) {
    fprintf(stderr, "Redirecting file open: %s -> %s\n", path, newpath);
    return newpath;
  }
  return path;
}

extern "C" FILE *fopen(const char *path, const char *mode);
extern "C" int open(const char *path, int flags, ...);
extern "C" ssize_t read(int fd, void *buf, size_t count);
extern "C" ssize_t write(int fd, const void *buf, size_t count);
extern "C" int execve(const char *filename, char *const argv[], char *const envp[]);

/**
 * @brief Opens a file with the specified path and mode.
 *
 * This function is a wrapper around the standard `fopen` function from the glibc library.
 * It dynamically loads the `fopen` function from the glibc library using `dlsym` if it hasn't
 * been loaded already. The function then redirects the path using the `redirect_path` function,
 * which modifies the path and stores it in the `newpath` buffer. Finally, it calls the glibc `fopen`
 * function with the redirected path and the specified mode.
 *
 * @param path The path to the file to be opened.
 * @param mode The mode in which the file should be opened (e.g., "r" for read, "w" for write).
 * @return A pointer to the FILE object associated with the opened file. If the file cannot be opened,
 *         NULL is returned.
 */
FILE *fopen(const char *path, const char *mode) {
  char newpath[512];
  if (glibc_fopen == NULL) {
    glibc_fopen = (FILE*(*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
    assert(glibc_fopen != NULL);
  }
  return glibc_fopen(redirect_path(newpath, path), mode);
}

/**
 * @brief Opens a file or device based on the provided path and flags.
 *
 * This method checks the provided path against a set of predefined paths and returns
 * the corresponding file descriptor or memory file descriptor. If the path does not
 * match any of the predefined paths, it redirects the path using the `redirect_path`
 * function and calls the `glibc_open` function to open the file.
 *
 * @param path The path to the file or device to be opened.
 * @param flags The flags to be used when opening the file or device.
 * @param ... Optional arguments that may be required by the `glibc_open` function.
 *
 * @return int The file descriptor or memory file descriptor corresponding to the path.
 *             If the path is not recognized, the return value is the result of the
 *             `glibc_open` function.
 *
 * @note The predefined paths and their corresponding file descriptors are:
 *       - "/proc/dispinfo" -> `dispinfo_fd`
 *       - "/dev/events" -> `evt_fd`
 *       - "/dev/fb" -> `fb_memfd`
 *       - "/dev/sb" -> `sb_fifo[1]`
 *       - "/dev/sbctl" -> `sbctl_fd`
 */
int open(const char *path, int flags, ...) {
  if (strcmp(path, "/proc/dispinfo") == 0) {
    return dispinfo_fd;
  } else if (strcmp(path, "/dev/events") == 0) {
    return evt_fd;
  } else if (strcmp(path, "/dev/fb") == 0) {
    return fb_memfd;
  } else if (strcmp(path, "/dev/sb") == 0) {
    return sb_fifo[1];
  } else if (strcmp(path, "/dev/sbctl") == 0) {
    return sbctl_fd;
  } else {
    char newpath[512];
    return glibc_open(redirect_path(newpath, path), flags);
  }
}

/**
 * @brief Reads data from a file descriptor into a buffer.
 *
 * This method reads data from the specified file descriptor `fd` into the buffer `buf` 
 * with a maximum size of `count` bytes. The behavior of this method depends on the type 
 * of file descriptor:
 *
 * - If `fd` is `dispinfo_fd`, it writes the display width and height into the buffer 
 *   in the format "WIDTH: <width>\nHEIGHT: <height>\n". This does not strictly conform 
 *   to the specification in `navy-apps/README.md` but is sufficient for real usage.
 *
 * - If `fd` is `evt_fd`, it checks for a key event in the key queue. If a key event is 
 *   found, it writes the key event details into the buffer in the format "k<type> <key>\n", 
 *   where `<type>` is 'd' for key down or 'u' for key up, and `<key>` is the key name.
 *
 * - If `fd` is `sbctl_fd`, it writes the free space of the sound buffer FIFO into the 
 *   buffer as a string.
 *
 * - For any other file descriptor, it delegates the read operation to the `glibc_read` 
 *   function.
 *
 * @param fd The file descriptor to read from.
 * @param buf The buffer to store the read data.
 * @param count The maximum number of bytes to read.
 * @return The number of bytes read, or 0 if no data is available (for `evt_fd`), or 
 *         the number of bytes written to the buffer (for `dispinfo_fd` and `sbctl_fd`).
 */
ssize_t read(int fd, void *buf, size_t count) {
  if (fd == dispinfo_fd) {
    // This does not strictly conform to `navy-apps/README.md`.
    // But it should be enough for real usage. Modify it if necessary.
    return snprintf((char *)buf, count, "WIDTH: %d\nHEIGHT: %d\n", disp_w, disp_h);
  } else if (fd == evt_fd) {
    int has_key = 0;
    SDL_Event ev = {};
    SDL_LockMutex(key_queue_lock);
    if (key_f != key_r) {
      ev = key_queue[key_f];
      key_f = (key_f + 1) % KEY_QUEUE_LEN;
      has_key = 1;
    }
    SDL_UnlockMutex(key_queue_lock);

    if (has_key) {
      SDL_Keysym k = ev.key.keysym;
      int keydown = ev.key.type == SDL_KEYDOWN;
      int scancode = k.scancode;

      const char *name = NULL;
      _KEYS(COND);
      if (name) return snprintf((char *)buf, count, "k%c %s\n", keydown ? 'd' : 'u', name);
    }
    return 0;
  } else if (fd == sbctl_fd) {
    // return the free space of sb_fifo
    int used;
    ioctl(sb_fifo[0], FIONREAD, &used);
    int free = pipe_size - used;
    return snprintf((char *)buf, count, "%d", free);
  }
  return glibc_read(fd, buf, count);
}

/**
 * @brief Writes data to a file descriptor or initializes audio if the file descriptor matches `sbctl_fd`.
 *
 * This method writes `count` bytes of data from the buffer `buf` to the file descriptor `fd`. 
 * If the file descriptor `fd` matches `sbctl_fd`, it initializes an audio subsystem using SDL (Simple DirectMedia Layer). 
 * In this case, the buffer `buf` is expected to contain at least three integers specifying the audio frequency, 
 * number of channels, and number of samples. The audio subsystem is then initialized with these parameters, 
 * and the audio playback is started.
 *
 * @param fd The file descriptor to write to or check against `sbctl_fd`.
 * @param buf The buffer containing the data to write or audio parameters.
 * @param count The number of bytes to write or the size of the buffer.
 * @return On success, the number of bytes written is returned. If `fd` matches `sbctl_fd`, `count` is returned 
 *         after initializing the audio subsystem. On error, the return value depends on the underlying `glibc_write` 
 *         implementation.
 */
ssize_t write(int fd, const void *buf, size_t count) {
  if (fd == sbctl_fd) {
    // open audio
    const int *args = (const int *)buf;
    assert(count >= sizeof(int) * 3);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_AudioSpec spec = {0};
    spec.freq = args[0];
    spec.channels = args[1];
    spec.samples = args[2];
    spec.userdata = NULL;
    spec.callback = audio_fill;
    SDL_OpenAudio(&spec, NULL);
    SDL_PauseAudio(0);
    return count;
  }
  return glibc_write(fd, buf, count);
}

/**
 * @brief Executes a program by replacing the current process image with a new one.
 *
 * This method is a wrapper around the `glibc_execve` function, which is used to execute
 * a program specified by `filename`. The method first redirects the path of the executable
 * using the `redirect_path` function, which modifies the path and stores it in `newpath`.
 * The modified path, along with the argument vector `argv` and the environment vector `envp`,
 * is then passed to `glibc_execve` to execute the program.
 *
 * @param filename The path to the executable file to be executed.
 * @param argv     An array of argument strings passed to the new program. The first element
 *                 should be the name of the program, and the last element must be a null pointer.
 * @param envp     An array of environment strings, typically in the form "key=value". The last
 *                 element must be a null pointer.
 *
 * @return This method always returns -1, indicating that an error occurred during execution.
 *         The actual execution of the program is handled by `glibc_execve`, and if successful,
 *         this method does not return as the current process is replaced by the new program.
 */
int execve(const char *filename, char *const argv[], char *const envp[]) {
  char newpath[512];
  glibc_execve(redirect_path(newpath, filename), argv, envp);
  return -1;
}

struct Init {
  /**
   * @brief Initializes the system by dynamically loading standard library functions, setting up file descriptors,
   *        configuring environment variables, and initializing SDL subsystems.
   *
   * This method performs the following operations:
   * 1. Dynamically loads the following standard library functions using `dlsym`:
   *    - `fopen`: For file opening.
   *    - `open`: For file opening with additional options.
   *    - `read`: For reading from file descriptors.
   *    - `write`: For writing to file descriptors.
   *    - `execve`: For executing programs.
   * 2. Creates a dummy file descriptor using `memfd_create` and assigns it to `dummy_fd` and `dispinfo_fd`.
   * 3. Retrieves the `NAVY_HOME` environment variable and constructs the path to the `fsimg` directory.
   * 4. Updates the `PATH` environment variable to include the `bin` directory within the `fsimg` path.
   * 5. Initializes the SDL library.
   * 6. If the `NWM_APP` environment variable is not set, opens the display and event subsystems.
   * 7. Opens the audio subsystem.
   *
   * @note This method asserts the success of critical operations, such as function loading and file descriptor creation,
   *       and will terminate the program if any of these operations fail.
   */
  Init() {
      glibc_fopen = (FILE*(*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
      assert(glibc_fopen != NULL);
      glibc_open = (int(*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");
      assert(glibc_open != NULL);
      glibc_read = (ssize_t (*)(int fd, void *buf, size_t count))dlsym(RTLD_NEXT, "read");
      assert(glibc_read != NULL);
      glibc_write = (ssize_t (*)(int fd, const void *buf, size_t count))dlsym(RTLD_NEXT, "write");
      assert(glibc_write != NULL);
      glibc_execve = (int(*)(const char*, char *const [], char *const []))dlsym(RTLD_NEXT, "execve");
      assert(glibc_execve != NULL);
  
      dummy_fd = memfd_create("dummy", 0);
      assert(dummy_fd != -1);
      dispinfo_fd = dummy_fd;
  
      char *navyhome = getenv("NAVY_HOME");
      assert(navyhome);
      sprintf(fsimg_path, "%s/fsimg", navyhome);
  
      char newpath[512];
      get_fsimg_path(newpath, "/bin");
      setenv("PATH", newpath, 1); // overwrite the current PATH
  
      SDL_Init(0);
      if (!getenv("NWM_APP")) {
        open_display();
        open_event();
      }
      open_audio();
    }
  /**
   * @brief Destructor for the Init class.
   * 
   * This method is automatically called when an instance of the Init class goes out of scope
   * or is explicitly deleted. It is responsible for cleaning up any resources that were allocated
   * by the Init class during its lifetime. In this implementation, the destructor does not perform
   * any specific actions, as no dynamic resources are managed by the class. However, it can be
   * overridden in derived classes to handle custom cleanup logic.
   */
  ~Init() {
  }
};

Init init;
