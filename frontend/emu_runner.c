#include "emu_runner.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shared.h"
#include "sms_ntsc.h"
#include "md_ntsc.h"

#define SIM_SOUND_FREQUENCY 48000
#define SIM_SOUND_SAMPLES_SIZE 2048
#define SIM_BITMAP_WIDTH 720
#define SIM_BITMAP_HEIGHT 576
#define SIM_SRAM_FILE "./game.srm"

#ifndef USE_16BPP_RENDERING
#error The frontend currently requires USE_16BPP_RENDERING.
#endif

int log_error = 0;
int debug_on = 0;

static sim_frontend_host_t sim_host;
static uint8 *sim_framebuffer;
static int sim_loaded;
static int16 soundframe[SIM_SOUND_SAMPLES_SIZE];

md_ntsc_t *md_ntsc;
sms_ntsc_t *sms_ntsc;

static uint8 brm_format[0x40] =
{
  0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
  0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f, 0x5f,
  0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x40, 0x00, 0x7f, 0x00, 0x40, 0x00, 0x7f, 0x00, 0x40,
  0x53, 0x45, 0x47, 0x41, 0x5f, 0x43, 0x44, 0x5f, 0x52, 0x4f, 0x4d, 0x00, 0x01, 0x00, 0x00, 0x00
};

static void sim_log(const char *message)
{
  if (sim_host.log)
  {
    sim_host.log(sim_host.userdata, message);
  }
}

static int sim_init_bitmap(void)
{
  if (!sim_framebuffer)
  {
    sim_framebuffer = (uint8 *)malloc(SIM_BITMAP_WIDTH * SIM_BITMAP_HEIGHT * 2);
    if (!sim_framebuffer)
    {
      sim_log("Unable to allocate video framebuffer");
      return 0;
    }
  }

  memset(&bitmap, 0, sizeof(bitmap));
  bitmap.width = SIM_BITMAP_WIDTH;
  bitmap.height = SIM_BITMAP_HEIGHT;
  bitmap.pitch = SIM_BITMAP_WIDTH * 2;
  bitmap.data = sim_framebuffer;
  bitmap.viewport.changed = 3;
  return 1;
}

static void sim_load_md_bios(void)
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

static void sim_load_backup_ram(void)
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
    fp = fopen(SIM_SRAM_FILE, "rb");
    if (fp)
    {
      fread(sram.sram, 0x10000, 1, fp);
      fclose(fp);
    }
  }
}

static void sim_save_backup_ram(void)
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
    fp = fopen(SIM_SRAM_FILE, "wb");
    if (fp)
    {
      fwrite(sram.sram, 0x10000, 1, fp);
      fclose(fp);
    }
  }
}

void sim_osd_input_update(void)
{
  sim_input_state_t state;
  int i;

  memset(&state, 0, sizeof(state));
  if (sim_host.input_poll)
  {
    sim_host.input_poll(sim_host.userdata, &state);
  }

  for (i = 0; i < MAX_DEVICES; i++)
  {
    input.pad[i] = 0;
  }

  if (state.buttons & SIM_INPUT_UP)    input.pad[0] |= INPUT_UP;
  if (state.buttons & SIM_INPUT_DOWN)  input.pad[0] |= INPUT_DOWN;
  if (state.buttons & SIM_INPUT_LEFT)  input.pad[0] |= INPUT_LEFT;
  if (state.buttons & SIM_INPUT_RIGHT) input.pad[0] |= INPUT_RIGHT;
  if (state.buttons & SIM_INPUT_A)     input.pad[0] |= INPUT_A;
  if (state.buttons & SIM_INPUT_B)     input.pad[0] |= INPUT_B;
  if (state.buttons & SIM_INPUT_C)     input.pad[0] |= INPUT_C;
  if (state.buttons & SIM_INPUT_START) input.pad[0] |= INPUT_START;
  if (state.buttons & SIM_INPUT_X)     input.pad[0] |= INPUT_X;
  if (state.buttons & SIM_INPUT_Y)     input.pad[0] |= INPUT_Y;
  if (state.buttons & SIM_INPUT_Z)     input.pad[0] |= INPUT_Z;
  if (state.buttons & SIM_INPUT_MODE)  input.pad[0] |= INPUT_MODE;
}

int sim_emu_init(const sim_frontend_host_t *host)
{
  memset(&sim_host, 0, sizeof(sim_host));
  if (host)
  {
    sim_host = *host;
  }

  if (!sim_init_bitmap())
  {
    return 0;
  }

  error_init();
  set_config_defaults();
  system_bios = 0;
  return 1;
}

int sim_emu_load_rom(const char *filename)
{
  if (!filename || !filename[0])
  {
    sim_log("No ROM filename was supplied");
    return 0;
  }

  if (sim_loaded)
  {
    sim_save_backup_ram();
    audio_shutdown();
    sim_loaded = 0;
  }

  system_bios = 0;
  sim_load_md_bios();

  if (!load_rom((char *)filename))
  {
    sim_log("Error loading ROM");
    return 0;
  }

  audio_init(SIM_SOUND_FREQUENCY, 0);
  system_init();
  sim_load_backup_ram();
  system_reset();
  bitmap.viewport.changed = 3;
  sim_loaded = 1;
  return 1;
}

void sim_emu_reset(void)
{
  if (sim_loaded)
  {
    system_reset();
    bitmap.viewport.changed = 3;
  }
}

void sim_emu_run_frame(void)
{
  int samples;
  sim_video_frame_t video;
  sim_audio_frame_t audio;

  if (!sim_loaded)
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
  if (sim_host.audio_submit && samples > 0)
  {
    audio.samples = (const int16_t *)soundframe;
    audio.frame_count = samples;
    audio.channels = 2;
    audio.sample_rate = SIM_SOUND_FREQUENCY;
    sim_host.audio_submit(sim_host.userdata, &audio);
  }

  if (sim_host.video_present)
  {
    video.pixels = bitmap.data;
    video.width = bitmap.width;
    video.height = bitmap.height;
    video.pitch = bitmap.pitch;
    video.format = SIM_PIXEL_FORMAT_RGB565;
    video.viewport_x = bitmap.viewport.x;
    video.viewport_y = bitmap.viewport.y;
    video.viewport_w = bitmap.viewport.w;
    video.viewport_h = bitmap.viewport.h;
    video.viewport_changed = bitmap.viewport.changed;
    sim_host.video_present(sim_host.userdata, &video);
    bitmap.viewport.changed &= ~1;
  }
}

void sim_emu_shutdown(void)
{
  if (sim_loaded)
  {
    sim_save_backup_ram();
    audio_shutdown();
    sim_loaded = 0;
  }

  error_shutdown();
  free(sim_framebuffer);
  sim_framebuffer = NULL;
}

int sim_emu_frame_interval_ms(void)
{
  return vdp_pal ? 20 : 16;
}

const char *sim_emu_game_title(void)
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
