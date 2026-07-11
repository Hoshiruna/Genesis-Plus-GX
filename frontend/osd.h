#ifndef _OSD_H_
#define _OSD_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#include "config.h"
#include "error.h"
#include "unzip.h"
#include "fileio.h"

#define osd_input_update emu_osd_input_update

#if !defined(MAXPATHLEN) || (MAXPATHLEN < 1)
#undef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#define GG_ROM     "./ggenie.bin"
#define AR_ROM     "./areplay.bin"
#define SK_ROM     "./sk.bin"
#define SK_UPMEM   "./sk2chip.bin"
#define CD_BIOS_US "./bios_CD_U.bin"
#define CD_BIOS_EU "./bios_CD_E.bin"
#define CD_BIOS_JP "./bios_CD_J.bin"
#define MD_BIOS    "./bios_MD.bin"
#define MS_BIOS_US "./bios_U.sms"
#define MS_BIOS_EU "./bios_E.sms"
#define MS_BIOS_JP "./bios_J.sms"
#define GG_BIOS    "./bios.gg"

#endif
