//========================= Official Notice ===============================
//
// "This software was developed at the National Institute of Standards
// and Technology by employees of the Federal Government in the course of
// their official duties. Pursuant to Title 17 Section 105 of the United
// States Code this software is not subject to copyright protection and
// is in the public domain.
//
// The NIST Data Flow System (NDFS) is an experimental system and is
// offered AS IS. NIST assumes no responsibility whatsoever for its use
// by other parties, and makes no guarantees and NO WARRANTIES, EXPRESS
// OR IMPLIED, about its quality, reliability, fitness for any purpose,
// or any other characteristic.
//
// We would appreciate acknowledgement if the software is used.
//
// This software can be redistributed and/or modified freely provided
// that any derivative works bear some notice that they are derived from
// it, and any modified versions bear some notice that they have been
// modified from the original."
//
//=========================================================================

#include <SDL/SDL.h>
#include "font.h"
#include "draw.h"

extern int width, height;

void draw_char(SDL_Surface *screen, int x, int y, unsigned char c, unsigned char r, unsigned char g, unsigned char b)
{
  const unsigned char *src = font_8x16+16*c;
  unsigned char *dest = ((unsigned char *)screen->pixels)+4*x+screen->pitch*y;
  int xx, yy;
  for(yy=0; yy<16; yy++) {
    unsigned char *dest1 = dest;
    unsigned char pix = *src++;
    for(xx=0; xx<8; xx++) {
      if(pix & 0x80) {
	dest1[0] = b;
	dest1[1] = g;
	dest1[2] = r;
      }
      dest1 += 4;
      pix <<= 1;
    }
    dest += screen->pitch;
  }
}

void draw_string(SDL_Surface *screen, int x, int y, const char *str, unsigned char r, unsigned char g, unsigned char b)
{
  while(*str) {
    draw_char(screen, x, y, (unsigned char)*str, r, g, b);
    x+=8;
    str++;
  }
}

void draw_pixel(SDL_Surface *screen, int x, int y, Uint32 color)
{
  Uint32 *pixel;

  if ((x >= width) || (y >= height))
     return;

  pixel = (Uint32 *) screen->pixels + y*screen->pitch/4 + x;
  *pixel = color;
}

void draw_border(SDL_Surface *screen)
{
  Uint32 red;
  int x,y;
  
  red = SDL_MapRGB(screen->format, 255, 0, 0);
  
  for(x = 200; x < width; x += 200)
    for (y=0; y < height; y++)
      draw_pixel(screen, x, y, red);
  
  for (y = 150; y < height; y += 150)
    for (x = 0; x < width; x++)
      draw_pixel(screen, x, y, red);
}
