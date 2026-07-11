#include "emu_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"

#define EMU_SOUND_FREQUENCY 48000
#define EMU_SOUND_SAMPLES_SIZE 2048
#define EMU_BITMAP_WIDTH 720
#define EMU_BITMAP_HEIGHT 576
#define EMU_SRAM_FILE "./game.srm"

#ifndef USE_16BPP_RENDERING
#error The frontend currently requires USE_16BPP_RENDERING.
#endif

int log_error = 0;
int debug_on = 0;

static emu_frontend_host_t emu_host;
static uint8 *emu_framebuffer;
static int emu_loaded;
static int16 soundframe[EMU_SOUND_SAMPLES_SIZE];

md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;

static uint8 brm_format[0x40] =
{
  0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
  0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
  0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x7f, 0x00, 0x40, 0x00, 0x7f, 0x00, 0x40,
  0x53, 0x45, 0x47, 0x41, 0x5f, 0x43, 0x44, 0x5f, 0x52, 0x4f, 0x4d, 0x00, 0x01, 0x00, 0x00, 0x00
};

static void emu_log(const char *message)
{
  if (emu_host.log)
  {
    emu_host.log(emu_host.userdata, message);
  }
}

static int emu_init_bitmap(void)
{
  if (!emu_framebuffer)
  {
    emu_framebuffer = (uint8 *)malloc(EMU_BITMAP_WIDTH * EMU_BITMAP_HEIGHT * 2);
    if (!emu_framebuffer)
    {
      emu_log("Unable to allocate video framebuffer");
      return 0;
    }
  }

  memset(&bitmap, 0, sizeof(bitmap));
  bitmap.width = EMU_BITMAP_WIDTH;
  bitmap.height = EMU_BITMAP_HEIGHT;
  bitmap.pitch = EMU_BITMAP_WIDTH * 2;
  bitmap.data = emu_framebuffer;
  bitmap.viewport.changed = 3;
  return 1;
}

static void emu_load_md_bios(void)
{
  FILE *fp = fopen(MD_BIOS, "rb");
  if (!fp)
  {
    return;
  }

  if (fread(boot_rom, 1, 0x800, fp) == 0x800)
  {
    system_bios = SYSTEM_MD;
  }

  fclose(fp);
}

static void emu_load_backup_ram(void)
{
  FILE *fp;

  if (system_hw == SYSTEM_MCD)
  {
    brm_format[0x21] = (sizeof(scd.bram) - 0x40) >> 9;
    brm_format[0x23] = (sizeof(scd.bram) - 0x40) >> 9;
    brm_format[0x25] = (sizeof(scd.bram) - 0x40) >> 9;
    memcpy(scd.bram, brm_format, sizeof(brm_format));

    fp = fopen("scd.brm", "rb");
    if (fp)
    {
      fread(scd.bram, sizeof(scd.bram), 1, fp);
      fclose(fp);
    }

    if (scd.cartridge.id)
    {
      brm_format[0x21] = (sizeof(sram.sram) - 0x40) >> 9;
      brm_format[0x23] = (sizeof(sram.sram) - 0x40) >> 9;
      brm_format[0x25] = (sizeof(sram.sram) - 0x40) >> 9;
      memcpy(sram.sram, brm_format, sizeof(brm_format));

      fp = fopen("cart.brm", "rb");
      if (fp)
      {
        fread(sram.sram, sizeof(sram.sram), 1, fp);
        fclose(fp);
      }
    }
  }
  else if (sram.on)
  {
    fp = fopen(EMU_SRAM_FILE, "rb");
    if (fp)
    {
      fread(sram.sram, 0x10000, 1, fp);
      fclose(fp);
    }
  }
}

static void emu_save_backup_ram(void)
{
  FILE *fp;

  if (system_hw == SYSTEM_MCD)
  {
    fp = fopen("scd.brm", "wb");
    if (fp)
    {
      fwrite(scd.bram, sizeof(scd.bram), 1, fp);
      fclose(fp);
    }

    if (scd.cartridge.id)
    {
      fp = fopen("cart.brm", "wb");
      if (fp)
      {
        fwrite(sram.sram, sizeof(sram.sram), 1, fp);
        fclose(fp);
      }
    }
  }
  else if (sram.on)
  {
    fp = fopen(EMU_SRAM_FILE, "wb");
    if (fp)
    {
      fwrite(sram.sram, 0x10000, 1, fp);
      fclose(fp);
    }
  }
}

void emu_osd_input_update(void)
{
  emu_input_state_t state;
  int i;

  memset(&state, 0, sizeof(state));
  if (emu_host.input_poll)
  {
    emu_host.input_poll(emu_host.userdata, &state);
  }

  for (i = 0; i < MAX_DEVICES; i++)
  {
    input.pad[i] = 0;
  }

  if (state.buttons & EMU_INPUT_UP)    input.pad[0] |= INPUT_UP;
  if (state.buttons & EMU_INPUT_DOWN)  input.pad[0] |= INPUT_DOWN;
  if (state.buttons & EMU_INPUT_LEFT)  input.pad[0] |= INPUT_LEFT;
  if (state.buttons & EMU_INPUT_RIGHT) input.pad[0] |= INPUT_RIGHT;
  if (state.buttons & EMU_INPUT_A)     input.pad[0] |= INPUT_A;
  if (state.buttons & EMU_INPUT_B)     input.pad[0] |= INPUT_B;
  if (state.buttons & EMU_INPUT_C)     input.pad[0] |= INPUT_C;
  if (state.buttons & EMU_INPUT_START) input.pad[0] |= INPUT_START;
  if (state.buttons & EMU_INPUT_X)     input.pad[0] |= INPUT_X;
  if (state.buttons & EMU_INPUT_Y)     input.pad[0] |= INPUT_Y;
  if (state.buttons & EMU_INPUT_Z)     input.pad[0] |= INPUT_Z;
  if (state.buttons & EMU_INPUT_MODE)  input.pad[0] |= INPUT_MODE;
}

int emu_init(const emu_frontend_host_t *host)
{
  memset(&emu_host, 0, sizeof(emu_host));
  if (host)
  {
    emu_host = *host;
  }

  if (!emu_init_bitmap())
  {
    return 0;
  }

  error_init();
  set_config_defaults();
  system_bios = 0;
  return 1;
}

int emu_load_rom(const char *filename)
{
  if (!filename || !filename[0])
  {
    emu_log("No ROM filename was supplied");
    return 0;
  }

  if (emu_loaded)
  {
    emu_save_backup_ram();
    audio_shutdown();
    emu_loaded = 0;
  }

  system_bios = 0;
  emu_load_md_bios();

  if (!load_rom((char *)filename))
  {
    emu_log("Error loading ROM");
    return 0;
  }

  audio_init(EMU_SOUND_FREQUENCY, 0);
  system_init();
  emu_load_backup_ram();
  system_reset();
  bitmap.viewport.changed = 3;
  emu_loaded = 1;
  return 1;
}

void emu_reset(void)
{
  if (emu_loaded)
  {
    system_reset();
    bitmap.viewport.changed = 3;
  }
}

void emu_run_frame(void)
{
  int samples;
  emu_video_frame_t video;
  emu_audio_frame_t audio;

  if (!emu_loaded)
  {
    return;
  }

  if (system_hw == SYSTEM_MCD)
  {
    system_frame_scd(0);
  }
  else if ((system_hw & SYSTEM_PBC) == SYSTEM_MD)
  {
    system_frame_gen(0);
  }
  else
  {
    system_frame_sms(0);
  }

  samples = audio_update(soundframe);
  if (emu_host.audio_submit && samples > 0)
  {
    audio.samples = (const int16_t *)soundframe;
    audio.frame_count = samples;
    audio.channels = 2;
    audio.sample_rate = EMU_SOUND_FREQUENCY;
    emu_host.audio_submit(emu_host.userdata, &audio);
  }

  if (emu_host.video_present)
  {
    video.pixels = bitmap.data;
    video.width = bitmap.width;
    video.height = bitmap.height;
    video.pitch = bitmap.pitch;
    video.format = EMU_PIXEL_FORMAT_RGB565;
    video.viewport_x = bitmap.viewport.x;
    video.viewport_y = bitmap.viewport.y;
    video.viewport_w = bitmap.viewport.w;
    video.viewport_h = bitmap.viewport.h;
    video.viewport_changed = bitmap.viewport.changed;
    emu_host.video_present(emu_host.userdata, &video);
    bitmap.viewport.changed &= ~1;
  }
}

void emu_shutdown(void)
{
  if (emu_loaded)
  {
    emu_save_backup_ram();
    audio_shutdown();
    emu_loaded = 0;
  }

  error_shutdown();
  free(emu_framebuffer);
  emu_framebuffer = NULL;
}

uint64_t emu_frame_period_ns(void)
{
  uint64_t cycles_per_frame = (uint64_t)MCYCLES_PER_LINE * (vdp_pal ? 313u : 262u);

  return ((cycles_per_frame * UINT64_C(1000000000)) + (system_clock / 2u)) / system_clock;
}

const char *emu_game_title(void)
{
  if (rominfo.international[0] && (rominfo.international[0] != 0x20))
  {
    return rominfo.international;
  }

  if (rominfo.domestic[0] && (rominfo.domestic[0] != 0x20))
  {
    return rominfo.domestic;
  }

  return "Genesis Plus GX";
}
