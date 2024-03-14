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

#ifndef __DRAW_H__
#define __DRAW_H__


void draw_char(SDL_Surface *screen, int x, int y, unsigned char c, unsigned char r, unsigned char g, unsigned char b);

void draw_string(SDL_Surface *screen, int x, int y, const char *str, unsigned char r, unsigned char g, unsigned char b);

void draw_pixel(SDL_Surface *screen, int x, int y, Uint32 color);

void draw_border(SDL_Surface *screen);

#endif
