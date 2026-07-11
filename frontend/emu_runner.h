#ifndef SIM_EMU_RUNNER_H
#define SIM_EMU_RUNNER_H

#include "frontend_host.h"

#ifdef __cplusplus
extern "C" {
#endif

int sim_emu_init(const sim_frontend_host_t *host);
int sim_emu_load_rom(const char *filename);
void sim_emu_reset(void);
void sim_emu_run_frame(void);
void sim_emu_shutdown(void);
uint64_t sim_emu_frame_period_ns(void);
const char *sim_emu_game_title(void);

#ifdef __cplusplus
}
#endif

#endif
