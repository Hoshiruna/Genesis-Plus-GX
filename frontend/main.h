#ifndef EMU_MAIN_H
#define EMU_MAIN_H

#include "frontend_host.h"

#define MAX_INPUTS 8

extern int log_error;
extern int debug_on;

void emu_osd_input_update(void);

#endif
