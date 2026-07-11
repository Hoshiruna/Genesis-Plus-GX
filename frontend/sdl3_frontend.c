#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "emu_runner.h"

#define EMU_WINDOW_WIDTH 640
#define EMU_WINDOW_HEIGHT 480
#define APP_AUDIO_MAX_QUEUE_MS 100

#ifdef _WIN32
#define APP_MENU_OPEN       1001
#define APP_MENU_EXIT       1002
#define APP_MENU_RESET      2001
#define APP_MENU_FULLSCREEN 3001
#define APP_MENU_ABOUT      4001
#define APP_PROP_NAME       "GenesisPlusGXSimApp"
#endif

typedef struct
{
  SDL_Window *window;
  SDL_Surface *window_surface;
  SDL_Surface *frame_surface;
  SDL_AudioStream *audio;
  const void *frame_pixels;
  int frame_width;
  int frame_height;
  int frame_pitch;
  uint32_t input_buttons;
  bool fullscreen;
  bool rom_loaded;
  bool running;
#ifdef _WIN32
  HWND hwnd;
  HMENU menu;
  WNDPROC old_wndproc;
  int pending_command;
#endif
} emu_sdl3_app_t;

static int app_load_rom(emu_sdl3_app_t *app, const char *filename);
static void app_toggle_fullscreen(emu_sdl3_app_t *app);

static void app_log(void *userdata, const char *message)
{
  (void)userdata;
  if (message)
  {
    SDL_Log("%s", message);
  }
}

static void app_clear_video(emu_sdl3_app_t *app)
{
  if (!app || !app->window)
  {
    return;
  }

  app->window_surface = SDL_GetWindowSurface(app->window);
  if (!app->window_surface)
  {
    return;
  }

  SDL_FillSurfaceRect(app->window_surface, NULL, 0);
  SDL_UpdateWindowSurface(app->window);
}

static SDL_PixelFormat app_sdl_pixel_format(emu_pixel_format_t format)
{
  switch (format)
  {
    case EMU_PIXEL_FORMAT_RGB565:
    default:
      return SDL_PIXELFORMAT_RGB565;
  }
}

static void app_recreate_frame_surface(emu_sdl3_app_t *app, const emu_video_frame_t *frame)
{
  if (app->frame_surface)
  {
    SDL_DestroySurface(app->frame_surface);
    app->frame_surface = NULL;
  }

  app->frame_surface = SDL_CreateSurfaceFrom(frame->width, frame->height,
      app_sdl_pixel_format(frame->format), (void *)frame->pixels, frame->pitch);

  app->frame_pixels = frame->pixels;
  app->frame_width = frame->width;
  app->frame_height = frame->height;
  app->frame_pitch = frame->pitch;
}

static void app_video_present(void *userdata, const emu_video_frame_t *frame)
{
  emu_sdl3_app_t *app = (emu_sdl3_app_t *)userdata;
  SDL_Rect source;
  SDL_Rect dest;
  int width;
  int height;

  if (!app || !frame || !frame->pixels)
  {
    return;
  }

  app->window_surface = SDL_GetWindowSurface(app->window);
  if (!app->window_surface)
  {
    return;
  }

  if (!app->frame_surface ||
      app->frame_pixels != frame->pixels ||
      app->frame_width != frame->width ||
      app->frame_height != frame->height ||
      app->frame_pitch != frame->pitch)
  {
    app_recreate_frame_surface(app, frame);
  }

  if (!app->frame_surface)
  {
    return;
  }

  source.x = 0;
  source.y = 0;
  source.w = frame->viewport_w + (2 * frame->viewport_x);
  source.h = frame->viewport_h + (2 * frame->viewport_y);

  if (source.w <= 0)
  {
    source.w = frame->viewport_w;
  }

  if (source.h <= 0)
  {
    source.h = frame->viewport_h;
  }

  if (source.w > frame->width)
  {
    source.w = frame->width;
  }

  if (source.h > frame->height)
  {
    source.h = frame->height;
  }

  width = source.w * 2;
  height = source.h * 2;

  if (width > app->window_surface->w)
  {
    width = app->window_surface->w;
  }

  if (height > app->window_surface->h)
  {
    height = app->window_surface->h;
  }

  dest.x = (app->window_surface->w - width) / 2;
  dest.y = (app->window_surface->h - height) / 2;
  dest.w = width;
  dest.h = height;

  SDL_FillSurfaceRect(app->window_surface, NULL, 0);
  SDL_BlitSurfaceScaled(app->frame_surface, &source, app->window_surface, &dest, SDL_SCALEMODE_NEAREST);
  SDL_UpdateWindowSurface(app->window);
}

static void app_audio_submit(void *userdata, const emu_audio_frame_t *frame)
{
  emu_sdl3_app_t *app = (emu_sdl3_app_t *)userdata;
  int bytes;
  int max_queued_bytes;
  int queued_bytes;

  if (!app || !app->audio || !frame || !frame->samples || frame->frame_count <= 0)
  {
    return;
  }

  bytes = frame->frame_count * frame->channels * (int)sizeof(int16_t);
  max_queued_bytes = (frame->sample_rate * frame->channels * (int)sizeof(int16_t) *
      APP_AUDIO_MAX_QUEUE_MS) / 1000;
  queued_bytes = SDL_GetAudioStreamQueued(app->audio);

  if (queued_bytes > max_queued_bytes)
  {
    SDL_ClearAudioStream(app->audio);
  }

  SDL_PutAudioStreamData(app->audio, frame->samples, bytes);
}

static void app_input_poll(void *userdata, emu_input_state_t *state)
{
  emu_sdl3_app_t *app = (emu_sdl3_app_t *)userdata;

  if (!app || !state)
  {
    return;
  }

  state->buttons = app->input_buttons;
}

static void app_update_keyboard(emu_sdl3_app_t *app)
{
  const bool *keys = SDL_GetKeyboardState(NULL);
  uint32_t buttons = 0;

  if (!keys)
  {
    return;
  }

  if (keys[SDL_SCANCODE_UP])       buttons |= EMU_INPUT_UP;
  if (keys[SDL_SCANCODE_DOWN])     buttons |= EMU_INPUT_DOWN;
  if (keys[SDL_SCANCODE_LEFT])     buttons |= EMU_INPUT_LEFT;
  if (keys[SDL_SCANCODE_RIGHT])    buttons |= EMU_INPUT_RIGHT;
  if (keys[SDL_SCANCODE_A])        buttons |= EMU_INPUT_A;
  if (keys[SDL_SCANCODE_S])        buttons |= EMU_INPUT_B;
  if (keys[SDL_SCANCODE_D])        buttons |= EMU_INPUT_C;
  if (keys[SDL_SCANCODE_RETURN])   buttons |= EMU_INPUT_START;
  if (keys[SDL_SCANCODE_Q])        buttons |= EMU_INPUT_X;
  if (keys[SDL_SCANCODE_W])        buttons |= EMU_INPUT_Y;
  if (keys[SDL_SCANCODE_E])        buttons |= EMU_INPUT_Z;
  if (keys[SDL_SCANCODE_BACKSPACE]) buttons |= EMU_INPUT_MODE;

  app->input_buttons = buttons;
}

static void app_toggle_fullscreen(emu_sdl3_app_t *app)
{
  app->fullscreen = !app->fullscreen;
  SDL_SetWindowFullscreen(app->window, app->fullscreen);
  app->window_surface = SDL_GetWindowSurface(app->window);
#ifdef _WIN32
  if (app->menu)
  {
    CheckMenuItem(app->menu, APP_MENU_FULLSCREEN,
        MF_BYCOMMAND | (app->fullscreen ? MF_CHECKED : MF_UNCHECKED));
  }
#endif
}

#ifdef _WIN32
static void app_update_menu_state(emu_sdl3_app_t *app)
{
  if (!app || !app->menu)
  {
    return;
  }

  EnableMenuItem(app->menu, APP_MENU_RESET,
      MF_BYCOMMAND | (app->rom_loaded ? MF_ENABLED : MF_GRAYED));
  CheckMenuItem(app->menu, APP_MENU_FULLSCREEN,
      MF_BYCOMMAND | (app->fullscreen ? MF_CHECKED : MF_UNCHECKED));
  DrawMenuBar(app->hwnd);
}

static LRESULT CALLBACK app_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  emu_sdl3_app_t *app = (emu_sdl3_app_t *)GetPropA(hwnd, APP_PROP_NAME);

  if (app && (message == WM_COMMAND))
  {
    switch (LOWORD(wparam))
    {
      case APP_MENU_OPEN:
      case APP_MENU_EXIT:
      case APP_MENU_RESET:
      case APP_MENU_FULLSCREEN:
      case APP_MENU_ABOUT:
        app->pending_command = LOWORD(wparam);
        return 0;

      default:
        break;
    }
  }

  if (app && app->old_wndproc)
  {
    return CallWindowProcA(app->old_wndproc, hwnd, message, wparam, lparam);
  }

  return DefWindowProcA(hwnd, message, wparam, lparam);
}

static void app_open_rom_dialog(emu_sdl3_app_t *app)
{
  OPENFILENAMEA ofn;
  char filename[4096];

  memset(filename, 0, sizeof(filename));
  memset(&ofn, 0, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = app->hwnd;
  ofn.lpstrFilter =
      "ROM images (*.md;*.gen;*.bin;*.smd;*.sms;*.gg;*.sg;*.zip;*.gz;*.cue;*.iso;*.chd)\0"
      "*.md;*.gen;*.bin;*.smd;*.sms;*.gg;*.sg;*.zip;*.gz;*.cue;*.iso;*.chd\0"
      "All files (*.*)\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = sizeof(filename);
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

  if (GetOpenFileNameA(&ofn))
  {
    app_load_rom(app, filename);
  }
}

static void app_process_menu_command(emu_sdl3_app_t *app)
{
  int command;

  if (!app || !app->pending_command)
  {
    return;
  }

  command = app->pending_command;
  app->pending_command = 0;

  switch (command)
  {
    case APP_MENU_OPEN:
      app_open_rom_dialog(app);
      break;

    case APP_MENU_EXIT:
      app->running = false;
      break;

    case APP_MENU_RESET:
      if (app->rom_loaded)
      {
        emu_reset();
      }
      break;

    case APP_MENU_FULLSCREEN:
      app_toggle_fullscreen(app);
      break;

    case APP_MENU_ABOUT:
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Genesis Plus GX",
          "Genesis Plus GX frontend", app->window);
      break;

    default:
      break;
  }

  app_update_menu_state(app);
}

static int app_install_menu(emu_sdl3_app_t *app)
{
  SDL_PropertiesID props;
  HMENU menu;
  HMENU file_menu;
  HMENU genesis_menu;
  HMENU config_menu;
  HMENU tools_menu;
  HMENU debug_menu;
  HMENU help_menu;

  props = SDL_GetWindowProperties(app->window);
  app->hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
  if (!app->hwnd)
  {
    return 0;
  }

  menu = CreateMenu();
  file_menu = CreatePopupMenu();
  genesis_menu = CreatePopupMenu();
  config_menu = CreatePopupMenu();
  tools_menu = CreatePopupMenu();
  debug_menu = CreatePopupMenu();
  help_menu = CreatePopupMenu();

  if (!menu || !file_menu || !genesis_menu || !config_menu || !tools_menu || !debug_menu || !help_menu)
  {
    return 0;
  }

  AppendMenuA(file_menu, MF_STRING, APP_MENU_OPEN, "&Open ROM...");
  AppendMenuA(file_menu, MF_SEPARATOR, 0, NULL);
  AppendMenuA(file_menu, MF_STRING, APP_MENU_EXIT, "E&xit");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)file_menu, "&File");

  AppendMenuA(genesis_menu, MF_STRING | MF_GRAYED, APP_MENU_RESET, "&Reset");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)genesis_menu, "&Genesis");

  AppendMenuA(config_menu, MF_STRING, APP_MENU_FULLSCREEN, "&Fullscreen");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)config_menu, "&Config");

  AppendMenuA(tools_menu, MF_STRING | MF_GRAYED, 0, "(none)");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)tools_menu, "&Tools");

  AppendMenuA(debug_menu, MF_STRING | MF_GRAYED, 0, "(none)");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)debug_menu, "&Debug");

  AppendMenuA(help_menu, MF_STRING, APP_MENU_ABOUT, "&About");
  AppendMenuA(menu, MF_POPUP, (UINT_PTR)help_menu, "&Help");

  SetPropA(app->hwnd, APP_PROP_NAME, app);
  app->old_wndproc = (WNDPROC)SetWindowLongPtrA(app->hwnd, GWLP_WNDPROC, (LONG_PTR)app_window_proc);
  SetMenu(app->hwnd, menu);
  DrawMenuBar(app->hwnd);

  app->menu = menu;
  app_update_menu_state(app);
  return 1;
}

static void app_destroy_menu(emu_sdl3_app_t *app)
{
  if (!app || !app->hwnd)
  {
    return;
  }

  if (app->old_wndproc)
  {
    SetWindowLongPtrA(app->hwnd, GWLP_WNDPROC, (LONG_PTR)app->old_wndproc);
    app->old_wndproc = NULL;
  }

  RemovePropA(app->hwnd, APP_PROP_NAME);

  if (app->menu)
  {
    SetMenu(app->hwnd, NULL);
    DestroyMenu(app->menu);
    app->menu = NULL;
  }
}
#else
static void app_process_menu_command(emu_sdl3_app_t *app)
{
  (void)app;
}

static int app_install_menu(emu_sdl3_app_t *app)
{
  (void)app;
  return 1;
}

static void app_destroy_menu(emu_sdl3_app_t *app)
{
  (void)app;
}
#endif

static void app_handle_events(emu_sdl3_app_t *app)
{
  SDL_Event event;

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_EVENT_QUIT:
        app->running = false;
        break;

      case SDL_EVENT_KEY_DOWN:
        switch (event.key.key)
        {
          case SDLK_ESCAPE:
            app->running = false;
            break;

          case SDLK_TAB:
            if (app->rom_loaded)
            {
              emu_reset();
            }
            break;

          case SDLK_F2:
            app_toggle_fullscreen(app);
            break;

          default:
            break;
        }
        break;

      default:
        break;
    }
  }

  app_process_menu_command(app);
}

static int app_init_audio(emu_sdl3_app_t *app)
{
  SDL_AudioSpec spec;

  SDL_zero(spec);
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;
  spec.freq = 48000;

  app->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (!app->audio)
  {
    SDL_Log("Unable to open SDL audio stream: %s", SDL_GetError());
    return 0;
  }

  SDL_ResumeAudioStreamDevice(app->audio);
  return 1;
}

static void app_destroy(emu_sdl3_app_t *app)
{
  app_destroy_menu(app);

  if (app->audio)
  {
    SDL_DestroyAudioStream(app->audio);
    app->audio = NULL;
  }

  if (app->frame_surface)
  {
    SDL_DestroySurface(app->frame_surface);
    app->frame_surface = NULL;
  }

  if (app->window)
  {
    SDL_DestroyWindow(app->window);
    app->window = NULL;
  }

  SDL_Quit();
}

static int app_load_rom(emu_sdl3_app_t *app, const char *filename)
{
  if (!filename || !filename[0])
  {
    return 0;
  }

  if (app->audio)
  {
    SDL_ClearAudioStream(app->audio);
  }

  if (!emu_load_rom(filename))
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Genesis Plus GX", "Error loading ROM", app->window);
    app->rom_loaded = false;
    SDL_SetWindowTitle(app->window, "Genesis Plus GX");
#ifdef _WIN32
    app_update_menu_state(app);
#endif
    return 0;
  }

  app->rom_loaded = true;
  SDL_SetWindowTitle(app->window, emu_game_title());
#ifdef _WIN32
  app_update_menu_state(app);
#endif
  return 1;
}

int main(int argc, char *argv[])
{
  emu_sdl3_app_t app;
  emu_frontend_host_t host;
  Uint64 next_frame_ns;

  memset(&app, 0, sizeof(app));

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS))
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Genesis Plus GX", SDL_GetError(), NULL);
    return 1;
  }

  app.window = SDL_CreateWindow("Genesis Plus GX", EMU_WINDOW_WIDTH, EMU_WINDOW_HEIGHT, 0);
  if (!app.window)
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Genesis Plus GX", SDL_GetError(), NULL);
    app_destroy(&app);
    return 1;
  }

  app.window_surface = SDL_GetWindowSurface(app.window);
  if (!app.window_surface)
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Genesis Plus GX", SDL_GetError(), app.window);
    app_destroy(&app);
    return 1;
  }

  app_install_menu(&app);
  app_clear_video(&app);

  if (!app_init_audio(&app))
  {
    app_destroy(&app);
    return 1;
  }

  memset(&host, 0, sizeof(host));
  host.userdata = &app;
  host.video_present = app_video_present;
  host.audio_submit = app_audio_submit;
  host.input_poll = app_input_poll;
  host.log = app_log;

  if (!emu_init(&host))
  {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Genesis Plus GX", "Unable to initialize emulator", app.window);
    app_destroy(&app);
    return 1;
  }

  if ((argc >= 2) && !app_load_rom(&app, argv[1]))
  {
    app_clear_video(&app);
  }

  app.running = true;
  next_frame_ns = SDL_GetTicksNS();

  while (app.running)
  {
    Uint64 now;

    app_handle_events(&app);
    app_update_keyboard(&app);

    if (app.rom_loaded)
    {
      Uint64 frame_period_ns;

      emu_run_frame();

      frame_period_ns = emu_frame_period_ns();
      next_frame_ns += frame_period_ns;
      now = SDL_GetTicksNS();
      if (next_frame_ns > now)
      {
        SDL_DelayPrecise(next_frame_ns - now);
      }
      else if ((now - next_frame_ns) > frame_period_ns)
      {
        next_frame_ns = now;
      }
    }
    else
    {
      app_clear_video(&app);
      SDL_Delay(16);
      next_frame_ns = SDL_GetTicksNS();
    }
  }

  emu_shutdown();
  app_destroy(&app);
  return 0;
}
