/*
 * HalOSS.h
 */
#define COPYING15 Copyright (C) Hannu Savolainen and Dev Mazumdar 1996-2005. All rights reserved.

extern int init_lynx_hal (lynxtwo_devc * devc);
extern int init_lynx_audio (lynxtwo_devc * devc);
extern int uninit_lynx_audio (lynxtwo_devc * devc);
extern int uninit_lynx_hal (lynxtwo_devc * devc);
extern int lynxhal_interrupt (lynxtwo_devc * devc);
