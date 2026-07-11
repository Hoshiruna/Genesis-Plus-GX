#ifndef EMU_RUNNER_H
#define EMU_RUNNER_H

#include "frontend_host.h"

#ifdef __cplusplus
extern "C" {
#endif

int emu_init(const emu_frontend_host_t *host);
int emu_load_rom(const char *filename);
void emu_reset(void);
void emu_run_frame(void);
void emu_shutdown(void);
uint64_t emu_frame_period_ns(void);
const char *emu_game_title(void);

#ifdef __cplusplus
}
#endif

#endif
