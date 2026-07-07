#ifndef SIM_FRONTEND_HOST_H
#define SIM_FRONTEND_HOST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  SIM_PIXEL_FORMAT_RGB565 = 1
} sim_pixel_format_t;

enum
{
  SIM_INPUT_UP    = 1u << 0,
  SIM_INPUT_DOWN  = 1u << 1,
  SIM_INPUT_LEFT  = 1u << 2,
  SIM_INPUT_RIGHT = 1u << 3,
  SIM_INPUT_A     = 1u << 4,
  SIM_INPUT_B     = 1u << 5,
  SIM_INPUT_C     = 1u << 6,
  SIM_INPUT_START = 1u << 7,
  SIM_INPUT_X     = 1u << 8,
  SIM_INPUT_Y     = 1u << 9,
  SIM_INPUT_Z     = 1u << 10,
  SIM_INPUT_MODE  = 1u << 11
};

typedef struct
{
  uint32_t buttons;
} sim_input_state_t;

typedef struct
{
  const void *pixels;
  int width;
  int height;
  int pitch;
  sim_pixel_format_t format;
  int viewport_x;
  int viewport_y;
  int viewport_w;
  int viewport_h;
  int viewport_changed;
} sim_video_frame_t;

typedef struct
{
  const int16_t *samples;
  int frame_count;
  int channels;
  int sample_rate;
} sim_audio_frame_t;

typedef struct
{
  void *userdata;
  void (*video_present)(void *userdata, const sim_video_frame_t *frame);
  void (*audio_submit)(void *userdata, const sim_audio_frame_t *frame);
  void (*input_poll)(void *userdata, sim_input_state_t *state);
  void (*log)(void *userdata, const char *message);
} sim_frontend_host_t;

#ifdef __cplusplus
}
#endif

#endif
