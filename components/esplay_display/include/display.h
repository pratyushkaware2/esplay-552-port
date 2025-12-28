#ifndef _DISPLAY_H_
#define _DISPLAY_H_
#include <stdint.h>

//*****************************************************************************
//
// Make sure all of the definitions in this header have a C binding.
//
//*****************************************************************************

#ifdef __cplusplus
extern "C"
{
#endif

void display_init();
void display_clear(uint16_t color);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void set_display_brightness(int percent);

#ifdef __cplusplus
}
#endif

#endif /*_DISPLAY_H_*/
