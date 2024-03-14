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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include <getopt.h>
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include <time.h>

#include "font.h"
#include "draw.h"

const char key16[16] = "1234qwerasdfzxcv";
const char key64[64] = "12345678qwertyuiasdfghjkzxcvbnm,!@#$%^&*QWERTYUIASDFGHJKZXCVBNM<";
int width, height, depth, cdepth;
int withsound;

int running;
const char *progname, *shortname;

/****************************************/

char *showtime()
{
  time_t tmp;

  time(&tmp);
  
  return ctime(&tmp);
}

void equit(char *msg)
{
  fprintf(stderr, "ERROR: %s |%s", msg, showtime());
  if (running == 0)
    exit(1);
  else
    running = 0;
}

// MK3 is default compilation mode, only do SF mode if selected

// -> MK3 //////////////////

#include <mk3lib.h>

int mk3df = 50; // Get 50 data frames at a time
static const char *default_dip = "10.0.0.2";
const char *dip;
int mk3speed = mk3array_speed_22K;
int slave = mk3_false;
int fix = mk3_false;
int normalize = mk3_false;
int quiet = 0;
char* meanfile = NULL;
char* gainfile = NULL;
int nring_mb = 16;

enum {
  def_spd = mk3array_speed_22K,
};

// Program name 
typedef struct {
  const char *name;
  int record_speed;
} fc_entry;

fc_entry fc_names[] = {
  { "mk3osc",       def_spd },
  { "mk3osc_22K",   mk3array_speed_22K },
  { "mk3osc_44K",   mk3array_speed_44K },
  { "END", 0}
};

/* MK3 */ // Mk3 array handlers
mk3array *it;
mk3error aerr;

void mk3errquit(mk3error *err, char *msg)
{
  if (err->error_code != MK3_OK) {
    mk3array_perror(err, msg);
    equit("Quitting");
  }
}

/****************************************/

static void print_usage(FILE *stream)
{
  fprintf(stream, "Usage : %s [options]\n"
	  "  -h        --help           Prints this message\n"
	  " Internal Ring Buffer options:\n"
	  " -m MB      --mem MB         Use \'MB\' Mega Bytes of data for the ring buffer size (default: %d MB)\n"
	  " Mk3 Capture options:\n"
	  "  -d IP     --dest IP        Listen to this \'IP\' (default: %s)\n"
	  "  -l        --link           Put microphone in slave mode (default is to turn slave off)\n"
	  "  -f        --fix            Fix one sample delay problem\n"
	  "  -n meanfile --normalize    Specify the mean file required for normalization\n"
	  "  -N gainfile --Normalize    Specify the gain file required for normalization\n"
	  "      note: \'-n\' and \'-N\' need to both be provided to normalize output\n"
	  "      note: see \'mk3normalize\' to generate those files\n"
	  "  -q        --quiet          Do not print lost packet messages\n"
	  " Program capture options:\n"
	  "  -s        --sound          Enable sound output\n"
	  , progname, nring_mb, default_dip);
}

void options(int argc, char ***argv) {

  static struct option optlist[] = {
    { "help",    0, 0, 'h'},
    { "mem",     1, 0, 'm'},
    { "dest",    1, 0, 'd'},
    { "link",    0, 0, 'l'},
    { "fix",     0, 0, 'f'},
    { "normalize", 1,0,'n'},
    { "Normalize", 1,0,'N'},
    { "quiet",   0, 0, 'q'},
    { "sound",   0, 0, 's'},
    { 0,         0, 0, 0  }
  };
  
  int usage = 0, finish = 0, error = 0;
  
  dip = default_dip;
  
  for(;;) {
    int opt = getopt_long(argc, *argv, "hm:d:lfn:N:qs", optlist, 0);
    if(opt == EOF)
      break;
    switch(opt) {
    case 'h':
      usage = 1;
      finish = 1;
      error = 0;
      break;
    case 'm':
      nring_mb = atoi(optarg);
      break;
    case 'd':
      dip = optarg;
      break;
    case 'l':
      slave = mk3_true;
      break;
    case 'f':
      fix = mk3_true;
      break;
    case 'n':
      meanfile = optarg;
      break;
    case 'N':
      gainfile = optarg;
      break;
    case 'q':
      quiet = 1;
      break;
    case 's':
      withsound = 1;
      break;
    case '?':
    case ':':
      usage = 1;
      finish = 1;
      error = 1;
      break;
    default:
      abort();
    }
    if(finish)
      break;
  }
  
  if (usage)
    print_usage(error ? stderr : stdout);
  
  if (finish)
    exit(error);
  
  *argv += optind;
}
// <- Mk3 //////////////////

int main(int argc, char **argv)
{
  int i,j;

  SDL_Event event;
  Uint32 vmode;
  Uint32 color;
  SDL_Surface *screen;
  
  int *buffers[64];
  int cursor;
  int show;
  int cinc;
  int showmode = 1; // 0: 64 channels, 1: 1st 16 ...
  int multiplier = 8;
	
  unsigned char name_array[30];
	
  // Output sound
  int listenc;
  int bsize;
  char *sndbuffer = NULL;
  int sndc;

  // Capture sound
  int record;
  int recfd;
  char fname[1024];

  // Screenshot
  int sccount = 0;

  // 
  // -> MK3 //////////////////
  int ID_array;

  FILE* nmf = NULL; // normalization mean file
  FILE* nsf = NULL; // normalization gain file

  mk3cursor *mk3c;
  char prom[9];
  // <- MK3 //////////////////

  progname = argv[0];
  shortname = strrchr(progname, '/');
  if(!shortname)
    shortname = progname;
  else
    shortname++;

  withsound = 0;

  // -> MK3 //////////////////
  mk3speed = def_spd;
  for(i = 0; fc_names[i].name; i++) {
    if (! strcmp(fc_names[i].name, "END")) { // No match, use default
      fprintf(stderr, "Warning: programe name (%s) not recognized, using defaults (%d Hz)\n", shortname, mk3speed);
      break;
    } else if (! strcmp(shortname, fc_names[i].name)) { // Found a match
      mk3speed = fc_names[i].record_speed;
      break;
    }
  }
 
  i = nring_mb;
  options(argc, &argv);
  if (nring_mb < i) {
    char tmp[1024];
    sprintf(tmp, "requested 'mem' can not be under the default value of %d MB", i);
    equit(tmp);
  }

  if ((meanfile != NULL) || (gainfile != NULL)) {
    if ((meanfile == NULL) || (gainfile == NULL))
      equit("\'-n\' and \'-N\' can only be used together");

    nmf = fopen(meanfile, "r");
    if (nmf == NULL)
      equit("Could not open 'mean' file, aborting");

    nsf = fopen(gainfile, "r");
    if (nsf == NULL)
      equit("Could not open 'gain' file, aborting");

    normalize = mk3_true;
  }

  // Start
  printf("%s (%d Hz) started: %s", progname, mk3speed, showtime());

  /* Mk3 init */
  // Notes: All information related to the parameters and notes
  //        of these functions are explained in "mk3lib.h"

  mk3array_init_mk3error(&aerr);
  mk3errquit(&aerr, "during comminit");

  it = mk3array_create(nring_mb, mk3df, &aerr);
  mk3errquit(&aerr, "during initialization");

  mk3array_comminit(it, dip, &aerr);
  mk3errquit(&aerr, "during comminit");

  ID_array = mk3array_ask_id(it, &aerr);
  mk3errquit(&aerr, "asking id");
  mk3array_ask_promnb(it, prom, &aerr);
  mk3errquit(&aerr, "asking prom nb");

  mk3array_initparams_wp(it, mk3speed, slave, &aerr);
  mk3errquit(&aerr, "during initparams");

  if (quiet)
    mk3array_display_warnings(it, mk3_false, &aerr);
  else
    mk3array_display_warnings(it, mk3_true, &aerr);
  mk3errquit(&aerr, "setting \'warnings\'");

  mk3array_fix_one_sample_delay(it, fix, &aerr);
  mk3errquit(&aerr, "setting \'one sample delay fix\'");

  if (normalize == mk3_true) {
    mk3array_normalize(it, nmf, nsf, &aerr);
    mk3errquit(&aerr, "setting \'normalization\'");
  }

  sprintf(name_array,"Microphone Array ID: %x", ID_array);
  
  printf("Capture on Microphone Array ID: %x\n", ID_array);
  // <- MK3 //////////////////

  /* SDL init */
  if(SDL_Init(SDL_INIT_VIDEO | ((withsound) ? SDL_INIT_AUDIO : 0)) < 0) { 
    fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
    exit(-1);
  }

  SDL_WM_SetCaption(name_array,"Microphone Array");

  bsize = 4096;
  if (withsound) {
    printf("Initializing sound device\n");
    if (Mix_OpenAudio(mk3speed, AUDIO_S16LSB, 1, bsize/sizeof(short)) < 0) {
      char tmp[512];
      sprintf(tmp, "Couldn't open audio: %s\n", Mix_GetError());
      equit(tmp);
    }
    sndbuffer = calloc(bsize, 1);
  }
  listenc = 0;
  sndc = 0;
  
  // Get SDL ready
  running = 1;
  width = (showmode == 0) ? 1600 : 800;
  height = (showmode == 0) ? 1200 : 600;
  depth  = 32;
  cdepth = (depth/8);
  
  vmode = SDL_SWSURFACE | SDL_DOUBLEBUF;
  screen = SDL_SetVideoMode(width, height, depth, vmode);
  if(!screen) {
    fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
	    width, height, depth, SDL_GetError());
    SDL_Quit();
    exit (1);
  }
  color = SDL_MapRGB(screen->format, 0, 255, 0);
  
  // Create an empty "pic"
  SDL_LockSurface(screen);
  memset(screen->pixels, 0, width*height*cdepth);
  draw_border(screen);
  SDL_UnlockSurface(screen);
  
  // Create an empty signals
  cursor = -1;
  show = 0;
  cinc = 0;
  for (i = 0; i < 64; i++)
    buffers[i] = calloc(mk3speed, sizeof(int));

  // Set up default recording values
  record = 0;
  recfd = -1;
  
  // SDL : Ignore app focus and mouse motion events
  SDL_ShowCursor(SDL_DISABLE);
  SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EnableUNICODE(1);

  // -> MK3 //////////////////
  /* MK3 */ // Get a cursor
  mk3c = mk3cursor_create(it, mk3df, &aerr);
  mk3errquit(&aerr, "creating cursor");

  /* MK3 */ // Start capture
  mk3array_capture_on(it, &aerr);
  mk3errquit(&aerr, "starting capture");

  /* MK3 */ // Wait for capture to be started
  // (we can not read from the cursor's pointer before that)
  mk3array_wait_capture_started(it, &aerr);
  mk3errquit(&aerr, "waiting on capture started");
  // <- MK3 //////////////////

  while(running) {
    int current;
    int k;
    char *ptr = NULL;
    int imax;

    // Recording
    // Start a recording but no files yet
    if ((record == 1) && (recfd == -1)) {
      sprintf(fname, "/tmp/%02d-%d.ary", listenc, (int) time(NULL));
      recfd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, 
		   S_IRUSR | S_IWUSR | S_IRGRP);
      if (recfd < 0) {
	fprintf(stderr, "ERROR : could not open file (%s), aborting\n", fname);
	running = 0;
      }
    }
    // Stop a recording and need to close the file
    if ((record == 0) && (recfd != -1)) {
      close(recfd);
      recfd = -1;
    }

    // -> MK3 //////////////////
    // Get data
    mk3array_get_cursorptr(it, mk3c, &ptr, NULL, &aerr);
    mk3errquit(&aerr, "getting cursor pointer");

    imax = mk3df;
    // <- MK3 //////////////////

    for (i = 0; i < imax; i++) {
      // Big Endian (CMA's data) to Linux Little Endian numbers
      int out = 0x00000000;
      
      cursor = (cursor + 1) % mk3speed;

      cinc++;

      for (j = 0; j < 64; j++) {
	out = ( ( (unsigned char) ptr[i*64*3 + j*3 + 2] )
		| ( (unsigned char) (ptr[i*64*3 + j*3 + 1] << 8) )
		| ( ( (signed char) (ptr[i*64*3 + j*3])) << 16)
		);
	
	buffers[j][cursor] = out;
	
	if (j == listenc) {
	  
	  if (withsound) {
	    sndbuffer[sndc++] = ptr[i*64*3 + j*3 + 1];
	    sndbuffer[sndc++] = ptr[i*64*3 + j*3];

	    if (sndc >= bsize) {
	      Mix_Chunk *m = Mix_QuickLoad_RAW(sndbuffer, bsize);
	      while ( Mix_Playing(0) ) {
		usleep(5000);
	      };
	      Mix_PlayChannel(0, m, 0);
	      sndc = 0;
	    }
	  }
	  
	  // Recording
	  if ((record == 1) && (recfd != -1))
	    write(recfd, ptr + i*64*3 + j*3, 3);
	}
      }
    }

    if (cinc >= (mk3speed / 10)) { // 10 frames per seconds
      long lmin[64], lmax[64];
      cinc = 0;

      // SDL events
      while(SDL_PollEvent(&event)) {
	switch(event.type) {
	  // Keyboard
	case SDL_KEYDOWN: {
	  if(!(event.key.keysym.unicode & 0xFF80)) {
	    int i;
	    if (showmode == 0) {
	      for(i = 0; i < 64; i++)
		if(event.key.keysym.unicode == key64[i]) {
		  int tmp = listenc;
		  listenc = i;
		  if (listenc != tmp)
		    record = 0;
		}
	    } else {
	      for(i = 0; i < 16; i++)
		if(event.key.keysym.unicode == key16[i]) {
		  int tmp = listenc;
		  listenc = i + 16*(showmode-1);
		  if (listenc != tmp)
		    record = 0;
		}
	    }
	  }
	  switch (event.key.keysym.sym) {
	  case SDLK_SPACE:
	    if (record == 1)
	      record = 0;
	    else
	      record = 1;
	    break;
	    
	  case SDLK_F1:
	    if (showmode == 1)
	      break;
	    // Otherwise ...
	    width = 800;
	    height = 600;
	    if (listenc >= 16) {
	      record = 0;
	      listenc = 0;
	    }
	    showmode = 1;
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    memset(screen->pixels, 0, width*height*cdepth);
	    SDL_LockSurface(screen);
	    break;

	  case SDLK_F2:
	    if (showmode == 2)
	      break;
	    // Otherwise ...
	    width = 800;
	    height = 600;
	    if ((listenc < 16) || (listenc >= 32)) {
	      record = 0;
	      listenc = 16;
	    }
	    showmode = 2;
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    SDL_LockSurface(screen);
	    memset(screen->pixels, 0, width*height*cdepth);
	    break;
	                                                        
	  case SDLK_F3:
	    if (showmode == 3)
	      break;
	    // Otherwise ...
	    width = 800;
	    height = 600;
	    if ((listenc < 32) || (listenc >= 48)) {
	      record = 0;
	      listenc = 32;
	    }
	    showmode = 3;
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    SDL_LockSurface(screen);
	    memset(screen->pixels, 0, width*height*cdepth);
	    break;
	    
	  case SDLK_F4:
	    if (showmode == 4)
	      break;
	    // Otherwise ...
	    width = 800;
	    height = 600;
	    if (listenc < 48) {
	      record = 0;
	      listenc = 48;
	    }
	    showmode = 4;
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    SDL_LockSurface(screen);
	    memset(screen->pixels, 0, width*height*cdepth);
	    break;
	    
	  case SDLK_F5:
	    if (showmode == 0)
	      break;
	    // Otherwise ...
	    width = 1600;
	    height = 1200;
	    showmode = 0;
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    SDL_LockSurface(screen);
	    memset(screen->pixels, 0, width*height*cdepth);
	    break;
	    	 
	  case SDLK_PRINT:
	  case SDLK_BACKSLASH:
	    {
	      char scf[256];
	      sprintf(scf, "/tmp/cma64-sc-%03d.bmp", sccount++);
	      if (SDL_SaveBMP(screen, scf) == 0) {
		printf("Wrote screenshot file: %s\n", scf);
	      } else {
		printf("Warning: Could not write screenshot file (%s), skipping\n", scf);
	      }
	    }
	    break;
       
	  case SDLK_TAB:
	    if (vmode == (SDL_SWSURFACE | SDL_DOUBLEBUF))
	      vmode = SDL_SWSURFACE | SDL_DOUBLEBUF | SDL_FULLSCREEN;
	    else
	      vmode = SDL_SWSURFACE | SDL_DOUBLEBUF;
	    SDL_UnlockSurface(screen);
	    SDL_FreeSurface(screen);
	    screen = SDL_SetVideoMode(width, height, depth, vmode);
	    if(!screen) {
	      fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
		      width, height, depth, SDL_GetError());
	      exit(1);
	    }
	    SDL_LockSurface(screen);
	    memset(screen->pixels, 0, width*height*(depth/8));
	    break;
	  case SDLK_ESCAPE:
	    printf("Quitting\n");
	    running = 0;
	    break;
	  default:
	    break;
	  }
	  break;
	}
	case SDL_QUIT:
	  running = 0;
	  break;
	default:
	  break;
	}
      }
      
      // Reactualize the pic
      SDL_LockSurface(screen);
      memset(screen->pixels, 0, width*height*cdepth);
      multiplier = (showmode == 0) ? 8 : 4;
      for (i = 0; i < 64; i++) {
	lmin[i] = 0;
	lmax[i] = 0;
      }
      for (k = 0; k < 200; k++) {
	int x, y, z, t;
	int current;
	int oldy[64];

	// 200 rows ... 10 refresh per second
	t = (show + k*(mk3speed/(200 * 10))) % mk3speed;

	for (i = 0; i < multiplier; i++) {
	  for (j = 0; j < multiplier; j++) {
	    current = ((showmode == 0) ? 0 : (showmode - 1) * 16)
	      + i*multiplier + j;
	
	    if ( buffers[current][t] < lmin[current])
	      lmin[current] = buffers[current][t];
	    if ( buffers[current][t] > lmax[current])
	      lmax[current] = buffers[current][t];

	    y = 75 + 150*i 
	      - ((buffers[current][t] * 75) / 0x800000);
	    x = 200*j + k;

	    if (k == 0)
	      oldy[current] = 75 + 150*i;

	    if (oldy[current] >= y)
	      for (z = y - 1; z < oldy[current]; z++)
		draw_pixel(screen, x, z, color);
	    else
	      for (z = oldy[current]; z < y + 1; z++)
		draw_pixel(screen, x, z, color);
	   
	    oldy[current] = y;
	  }
	}
      }
      draw_border(screen);
      for (i = 0; i < multiplier; i++) {
	for (j = 0; j < multiplier; j++) {
	  char tmp[20];

	  current = ((showmode == 0) ? 0 : (showmode - 1) * 16)
	    + i*multiplier + j;

	  sprintf(tmp, "%ld", lmin[current]);
	  draw_string(screen, 200*j + 2, 150*i + (150 - 16 - 2),
			 tmp, 255, 101, 13);

	  sprintf(tmp, "%ld", lmax[current]);
	  draw_string(screen, 200*j + 2, 150*i + 2, tmp, 255, 101, 13);

	  sprintf(tmp, "%d/%c", current, 
		  (showmode == 0) ? key64[current] : key16[i*multiplier + j]);
	  draw_string(screen, 200*j + (200 - (current > 9 ? 4 : 3)*8 - 2),
			 150*i + (150 - 16 - 2), tmp,
			 (listenc == current) ? 131 : 247,
			 (listenc == current) ? 206 : 236,
			 (listenc == current) ? 248 : 0);

	  if ((listenc == current) && (record == 1) && (recfd != -1))
	    draw_string(screen, 200*j + (200 - strlen(fname)*8 -2),
			150*i + 18, fname, 131, 206, 248);
	}
      }
      SDL_UnlockSurface(screen);
      SDL_UpdateRect(screen, 0, 0, 0, 0);
      show = cursor;
    }
    
  }

  // -> MK3 //////////////////
  /* MK3 */ // Stop capture
  mk3array_capture_off(it, &aerr);
  mk3errquit(&aerr, "stopping capture");
  // <- MK3 //////////////////

  printf("%s ended: %s", progname, showtime());
  
  SDL_Quit();
  return 0;
}
