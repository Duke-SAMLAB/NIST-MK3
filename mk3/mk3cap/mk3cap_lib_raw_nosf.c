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

// Note:
//  documentation on use and modes can be found in 'mk3lib.h'

// Note:
//  all mk3 handling is preceded by a "/* MK3 */" comment

// Note:
//  'mk3errquit' is an internal function provided here to facilitate the
//  thread quit when an error occurs on the Mk3 handling

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>


#include <getopt.h>

/* MK3 */ // Include the proper header
#include <mk3lib.h>

enum {
a3size_1f  = 64*3, // 1 audio sample
};

static const char *default_dip = "10.0.0.2";
const char *dip;
int mk3speed = 0;
int slave = mk3_false;
int drop = 0;
int fix = mk3_false;
int normalize = mk3_false;
char* meanfile = NULL;
char* gainfile = NULL;
int wait_nfnbr = -1;

int cur_nb, nb_lost;

int quiet = 0;
int verbose = 0;
int debug = 0;

const char *progname, *shortname;
volatile int running = 0;

int fdw = -1;
char fname[512];

int mk3timed = 0;

int nring_mb = 16;


/* MK3 */ // Mk3 array handlers
mk3array *it;
mk3error aerr;

int hd_fc = 200;

////////////////////

char *showtime()
{
  time_t tmp;

  time(&tmp);
  
  return ctime(&tmp);
}

////////////////////
// Quit functions

void okquit(char *msg)
{
  fprintf(stderr, "QUIT: %s |%s", msg, showtime());
  if (running == 0)
    exit(0);
  else
    running = 0;
}

void equit(char *msg)
{
  fprintf(stderr, "ERROR: %s |%s", msg, showtime());
  if (running == 0)
    exit(1);
  else
    running = 0;
}

/* MK3 */ // Internal function using the 'mk3error' print method 
void mk3errquit(mk3error *err, char *msg)
{
  if (err->error_code != MK3_OK) {
    mk3array_perror(err, msg);
    equit("Quitting");
  }
}

////////////////////

/* Online help */
void print_usage(FILE *stream)
{
  fprintf(stream, "Usage : %s [options] file\n"
	  "  -h        --help           Prints this message\n"
	  "  -v        --verbose        Details internal workings\n"
	  " Internal Ring Buffer options:\n"
	  " -m MB      --mem MB         Use \'MB\' Mega Bytes of data for the ring buffer size (default: %d MB)\n"
	  " Capture options:\n"
	  "  -d IP     --dest IP        Listen to this \'IP\' (default: %s)\n"
	  "  -speed_44K                 Set recording frequency to 44KHz (default is 22KHz)\n"
	  "  -l        --link           Put microphone in slave mode (default is to turn slave off)\n"
	  "  -D X      --Drop           Drop the first X samples (default: %d)\n"
	  "  -f        --fix            Fix one sample delay problem\n"
	  "  -n meanfile --normalize    Specify the mean file required for normalization\n"
	  "  -N gainfile --Normalize    Specify the gain file required for normalization\n"
	  "      note: \'-n\' and \'-N\' need to both be provided to normalize output\n"
	  "      note: see \'mk3normalize\' to generate those files\n"
	  "  -w nfnbr  --wait_nfnbr     Wait for \'nfnbr\' (network frame number) before doing anyhting [*]\n"
	  "  -q        --quiet          Do not print lost packet messages\n"
	  "  -t        --timed sec      Record \'sec\' seconds of data to disk then quit ('0' value will be ignored) (! will not try to connect to the a SF server if any)\n"
	  "\n"
	  "*: will start next step at 'nfnbr + 1' (where next step is \'Drop\' before capturing)\n"
	  , progname, nring_mb, default_dip, drop);
}

/* Options parsing */
int options(int argc, char ***argv) {
  int ra = 1;

  // ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
  //    D        MN              cd f h   lmn  qrst vw   

  static struct option optlist[] = {
    { "help",    0, 0, 'h'},
    { "dest",    1, 0, 'd'},
    { "quiet",   0, 0, 'q'},
    { "verbose", 0, 0, 'v'},
    { "timed",   1, 0, 't'},
    { "mem",     1, 0, 'm'},
    { "link",    0, 0, 'l'},
    { "Drop",    1, 0, 'D'},
    { "fix",     0, 0, 'f'},
    { "speed_44K", 0, 0, 's'},
    { "normalize", 1,0,'n'},
    { "Normalize", 1,0,'N'},
    { "wait_nfnbr",1,0,'w'},
    { 0,         0, 0, 0  }
  };
  
  int usage = 0, finish = 0, error = 0;
  
  dip = default_dip;
  
  for(;;) {
    int opt = getopt_long(argc, *argv, "hd:qvt:m:lD:fsn:N:Mw:", optlist, 0);
    if(opt == EOF)
      break;
    switch(opt) {
    case 'h':
      usage = 1;
      finish = 1;
      error = 0;
      ra++;
      break;
    case 'd':
      dip = optarg;
      ra+=2;
      break;
    case 'q':
      quiet = 1;
      ra++;
      break;    
    case 'v':
      verbose++;
      ra++;
      break;
    case 't':
      mk3timed = atoi(optarg);
      ra+=2;
      break;
    case 'm':
      nring_mb = atoi(optarg);
      ra+=2;
      break;
    case 'l':
      slave = mk3_true;
      ra++;
      break;
    case 'D':
      drop = atoi(optarg);
      ra+=2;
      break;
    case 'f':
      fix = mk3_true;
      ra++;
      break;
    case 's':
      mk3speed = mk3array_speed_44K;
      ra++;
      break;
    case 'n':
      meanfile = optarg;
      ra+=2;
      break;
    case 'N':
      gainfile = optarg;
      ra+=2;
      break;
    case 'w':
      wait_nfnbr = atoi(optarg);
      ra+=2;
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

  return ra;
}

/************************************************************/

////////////////////////////////////////
// MAIN

int main(int argc, char** argv)
{
  char prom[9];
  int id;
  int i;

  FILE* nmf = NULL; // normalization mean file
  FILE* nsf = NULL; // normalization gain file

  progname = argv[0];
  shortname = strrchr(progname, '/');
  if(!shortname)
    shortname = progname;
  else
    shortname++;

  mk3speed = mk3array_speed_22K;

  i = nring_mb;
  // Quick check of "remaining" program arguments
  argc -= options(argc, &argv);

  if (argc == 0) {
    print_usage(stdout);
    exit (0);
  }

  if (argc > 1)
    equit("Leftover arguments on the command line, aborting");

  if (nring_mb < i) {
    char tmp[1024];
    sprintf(tmp, "requested 'mem' can not be under the default value of %d MB", i);
    equit(tmp);
  }

  if (mk3timed < 0)
    equit("'timed' can only be positive");

  if ((argv[0] == NULL)  && (mk3timed)) {
      equit("No file specified while in disk only mode, aborting");
  } else {
    strcpy(fname, argv[0]);
    fdw = 0;
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
  printf("%s (%d frames / %d Hz) started: %s", progname, hd_fc, mk3speed, showtime());

  /* Mk3 init */
  // Notes: All information related to the parameters and notes
  //        of these functions are explained in "mk3lib.h"

  /* MK3 */ // 0: initialize the error handler
  mk3array_init_mk3error(&aerr);
  mk3errquit(&aerr, "during comminit");

 /* MK3 */  // 1: create the 'mk3array' handler 
  it = mk3array_create(nring_mb, hd_fc, &aerr);
  mk3errquit(&aerr, "during initialization");
  if (verbose) printf("Mk3 init done\n");

  /* MK3 */ // 2: initialize the communication with the Mk3
  mk3array_comminit(it, dip, &aerr);
  mk3errquit(&aerr, "during comminit");
  if (verbose) printf("Mk3 comminit done\n");

  /* MK3 */ // 3: ask the mk3 for both its 'id' and its 'prom nb'
  id = mk3array_ask_id(it, &aerr);
  mk3errquit(&aerr, "asking id");
  mk3array_ask_promnb(it, prom, &aerr);
  mk3errquit(&aerr, "asking prom nb");

  /* MK3 */ // 4: initialize the Mk3 array with its 'speed' and 'slave' mode
  mk3array_initparams_wp(it, mk3speed, slave, &aerr);
  mk3errquit(&aerr, "during initparams");
  if (verbose) printf("Mk3 paraminit done\n");

  /* MK3 */ // 5: set up misc paramaters (warning display, drop frames, fix...)
  if (quiet)
    mk3array_display_warnings(it, mk3_false, &aerr);
  else
    mk3array_display_warnings(it, mk3_true, &aerr);
  mk3errquit(&aerr, "setting \'warnings\'");

  mk3array_fix_drop_X_first_frames(it, drop, &aerr);
  mk3errquit(&aerr, "setting \'drop X first frames\'");

  mk3array_fix_one_sample_delay(it, fix, &aerr);
  mk3errquit(&aerr, "setting \'one sample delay fix\'");

  if (normalize == mk3_true) {
    mk3array_normalize(it, nmf, nsf, &aerr);
    mk3errquit(&aerr, "setting \'normalization\'");
  }

  if (wait_nfnbr != -1) {
    mk3array_wait_nfnbr(it, wait_nfnbr, &aerr);
    mk3errquit(&aerr, "setting \'wait network frame number\'");
  }

  running = 1;
  /* MK3 */ // 6: Start capture
  mk3array_capture_on(it, &aerr);
  mk3errquit(&aerr, "starting capture");
  if (verbose) printf("Mk3 capture started\n");

  /* the main loop */
  printf("Starting RAW file capture on Mk3 id:%d / Prom #%s\n"
	 " - dropping the first %d sample(s)\n"
	 " - with%s normalization of data\n"
	 " - %sfixing the 1 sample delay\n"
	 , id, prom, drop, 
	 (normalize == mk3_true) ? "" : "OUT",
	 (fix == mk3_true) ? "" : "NOT ");

  while (running) {
    /* MK3 */ // We use a cursor to access a direct read access to the memory
    mk3cursor *c;
    
    unsigned long long int mk3timed_ftg = 0; // Frames to get
    unsigned long long int mk3timed_fsf = 0; // Frames so far
    
    int keeprecording = 1;
    
    size_t wrote;
    
    // Init
    fdw = open(fname, O_RDWR|O_CREAT|O_TRUNC, 0666);
    if (fdw < 0) equit("Could not open output file, aborting");
    
    if (mk3timed) 
      mk3timed_ftg = mk3speed  * mk3timed;
    
    /* MK3 */ // Get a cursor
    c = mk3cursor_create(it, hd_fc, &aerr);
    mk3errquit(&aerr, "creating cursor");
    
    /* MK3 */ // Wait for capture to be started
    // (we can not read from the cursor's pointer before that)
    mk3array_wait_capture_started(it, &aerr);
    mk3errquit(&aerr, "waiting on capture started");
    
    // Capture (Main thread code)
    while (running && keeprecording) {
      char *ptr = NULL;
      struct timespec ts;
      int nfnbr;
      int t_hdfc = hd_fc;
  
      /* MK3 */ // Get the cursor pointer and associated capture timestamp
      mk3array_get_cursorptr_with_nfnbr(it, c, &ptr, &ts, &nfnbr, &aerr);
      mk3errquit(&aerr, "getting cursor pointer");
      
      if (verbose > 1)
	printf("\r'nfnbr' seen: %d                   ", nfnbr);
      
      if ((mk3timed) && ((mk3timed_ftg - mk3timed_fsf) < t_hdfc))
	t_hdfc = mk3timed_ftg - mk3timed_fsf;
      
      wrote = write(fdw, ptr, t_hdfc*a3size_1f);
      if (wrote != t_hdfc*a3size_1f)
	equit("Error writing the data");
      
      if (mk3timed) {
	mk3timed_fsf += hd_fc;
	if (mk3timed_fsf >= mk3timed_ftg) {
	  printf("Wrote %d seconds, exiting\n", mk3timed);
	  keeprecording = 0;
	  running = 0;
	}
      }
    }
  }
  close(fdw);
    
    /* MK3 */ // 7: Stop capture
  mk3array_capture_off(it, &aerr);
  mk3errquit(&aerr, "stopping capture");
  if (verbose) printf("Mk3 capture stopped\n");

  /* MK3 */ // 8: Clean memory (including the misc cursors)
  mk3array_delete(it, &aerr);
  mk3errquit(&aerr, "cleaning");

  printf("%s ended: %s", progname, showtime());
  
  pthread_exit(NULL);
  
  return EXIT_SUCCESS;
}
