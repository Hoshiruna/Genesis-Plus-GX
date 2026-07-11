#ifndef EMU_FRONTEND_HOST_H
#define EMU_FRONTEND_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  EMU_PIXEL_FORMAT_RGB565 = 1
} emu_pixel_format_t;

enum
{
  EMU_INPUT_UP    = 1u << 0,
  EMU_INPUT_DOWN  = 1u << 1,
  EMU_INPUT_LEFT  = 1u << 2,
  EMU_INPUT_RIGHT = 1u << 3,
  EMU_INPUT_A     = 1u << 4,
  EMU_INPUT_B     = 1u << 5,
  EMU_INPUT_C     = 1u << 6,
  EMU_INPUT_START = 1u << 7,
  EMU_INPUT_X     = 1u << 8,
  EMU_INPUT_Y     = 1u << 9,
  EMU_INPUT_Z     = 1u << 10,
  EMU_INPUT_MODE  = 1u << 11
};

typedef struct
{
  uint32_t buttons;
} emu_input_state_t;

typedef struct
{
  const void *pixels;
  int width;
  int height;
  int pitch;
  emu_pixel_format_t format;
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
  int viewport_changed;
} emu_video_frame_t;

typedef struct
{
  const int16_t *samples;
  int frame_count;
  int channels;
  int sample_rate;
} emu_audio_frame_t;

typedef struct
{
  void *userdata;
  void (*video_present)(void *userdata, const emu_video_frame_t *frame);
  void (*audio_submit)(void *userdata, const emu_audio_frame_t *frame);
  void (*input_poll)(void *userdata, emu_input_state_t *state);
  void (*log)(void *userdata, const char *message);
} emu_frontend_host_t;

#ifdef __cplusplus
}
#endif

#endif
