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

// To use "open64"
// http://www.suse.de/~aj/linux_lfs.html
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>


#include <sflib.h>
#include <sffile.h>

#include <pthread.h>

#include <getopt.h>

#include <sound_types.h>

/* MK3 */ // Include the proper header
#include <mk3lib.h>

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

sf_handle *me = 0;
sf_flow_emit *flow;
sf_error err;

int fdw = -1;
int hdstop = 1;
char fname[512];

int capture_smd = 1;
int recordmd = 0;
int mk3timed = 0;

// PThread
pthread_t threads[3]; // At max we have 3 threads running at the same time
int thread_count = 0;
pthread_mutex_t sigmutex;
pthread_cond_t sigcond;
volatile int sigset = 0;

// Ring buffer info
enum {
  sf_fc_5f   = 5,
  sf_fc_10f  = 10,
  sf_fc_20f  = 20,
  sf_fc_100f = 100,
  sf_fc_def  = sf_fc_100f,
  sf_fc_off  = -1,

  a3size_1f  = 64*3, // 1 audio sample
  a3size_5f  = 5*a3size_1f, // The amount read from each network request
  // In practice all the following values are multiple of 5 frames
  a3size_10f = 2*a3size_5f, // array3_audio_10frames
  a3size_20f = 4*a3size_5f, // array3_audio_20frames
  a3size_100f = 100*a3size_1f, // The size of the SF buffer out
  a3size_def  = a3size_100f,

  // sf_fc : SmartFlow Frame Count (variable, set by user at programe call)
  // hd_fc : HardDrive writter Frame Count (hard coded)

  hd_fc = 200, /* We write by block of 200 frames
		 (at 22KHz that makes over 100 writes per second)
		 (! needs to be a multiple of sf_fc, since the timestamps
		 are attached to each sf_fc value) */ 
};
int sf_fc = -1; // value will be determined by the programe name

int nring_mb = 16;

// Program name 
typedef struct {
  const char *name;
  int sf_fc;
  int record_speed;
} fc_entry;

enum {
  def_spd = mk3array_speed_22K,
};

fc_entry fc_names[] = {
  { "mk3cap",          sf_fc_def,  def_spd },
  // 22K
  { "mk3cap_5f_22K",   sf_fc_5f,   mk3array_speed_22K },
  { "mk3cap_10f_22K",  sf_fc_10f,  mk3array_speed_22K },
  { "mk3cap_20f_22K",  sf_fc_20f,  mk3array_speed_22K },
  { "mk3cap_100f_22K", sf_fc_100f, mk3array_speed_22K },
  { "mk3cap_22K",      sf_fc_off,  mk3array_speed_22K },
  // 44K
  { "mk3cap_5f_44K",   sf_fc_5f,   mk3array_speed_44K },
  { "mk3cap_10f_44K",  sf_fc_10f,  mk3array_speed_44K },
  { "mk3cap_20f_44K",  sf_fc_20f,  mk3array_speed_44K },
  { "mk3cap_100f_44K", sf_fc_100f, mk3array_speed_44K },
  { "mk3cap_44K",      sf_fc_off,  mk3array_speed_44K },
  // End
  { "END", 0, 0}
};

/* MK3 */ // Mk3 array handlers
mk3array *it;
mk3error aerr;

////////////////////

char *showtime()
{
  time_t tmp;

  time(&tmp);
  
  return ctime(&tmp);
}

/*****/

void running_quit()
{
  if (debug) { fprintf(stderr, "-> running_quit\n"); fflush(stderr); }
  // We want to quit but we did not catch SIGINT yet, simple: fake it
  pthread_kill(threads[0], SIGINT);
  if (debug) { fprintf(stderr, "<-running_quit\n"); fflush(stderr); }
}

void running_off()
{
  running = 0;
}

/* this is a common unix function that gets called when a process 
   receives a signal (e.g. ctrl-c) */
void* shutdown_thread(void *arg)
{
  sigset_t signals_to_catch;
  int caught;

  if (debug) { fprintf(stderr, "Signal catch start\n"); fflush(stderr); }

  /* Wait for SIGINT & SIGTERM */
  sigemptyset(&signals_to_catch);
  sigaddset(&signals_to_catch, SIGINT);
  sigaddset(&signals_to_catch, SIGTERM);

  pthread_mutex_lock(&sigmutex);
  pthread_cond_wait(&sigcond, &sigmutex);
  sigset = 1;
  pthread_mutex_unlock(&sigmutex);

  sigwait(&signals_to_catch, &caught);

  /* Got the signal: Tell the processes to quit */
  running_off();

  if (debug) { fprintf(stderr, "Signal catch quit\n"); fflush(stderr); }
  pthread_exit(0);
  
  return NULL;
}

////////////////////
// Quit functions

void okquit(char *msg)
{
  fprintf(stderr, "QUIT: %s |%s", msg, showtime());
  if (running == 0)
    exit(0);
  else
    running_quit();
}

void equit(char *msg)
{
  fprintf(stderr, "ERROR: %s |%s", msg, showtime());
  if (running == 0)
    exit(1);
  else
    running_quit();
}

void sffileerrquit(sf_file_error *err, char *msg)
{
  if (err->error_code != SFFE_OK) {
    sf_file_perror(err, msg);
    equit("Quitting");
  }
}

void sferrquit(sf_error *err, char *msg)
{
  if (err->error_code != SFE_OK) {
    sf_perror(err, msg);
    equit("Quitting");
  }
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
  fprintf(stream, "Usage : %s [options] [file]\n"
	  "  -h        --help           Prints this message\n"
	  "  -v        --verbose        Details internal workings\n"
	  " Internal Ring Buffer options:\n"
	  " -m MB      --mem MB         Use \'MB\' Mega Bytes of data for the ring buffer size (default: %d MB)\n"
	  " Capture options:\n"
	  "  -d IP     --dest IP        Listen to this \'IP\' (default: %s)\n"
	  "  -l        --link           Put microphone in slave mode (default is to turn slave off)\n"
	  "  -D X      --Drop           Drop the first X samples (default: %d)\n"
	  "  -f        --fix            Fix one sample delay problem\n"
	  "  -n meanfile --normalize    Specify the mean file required for normalization\n"
	  "  -N gainfile --Normalize    Specify the gain file required for normalization\n"
	  "      note: \'-n\' and \'-N\' need to both be provided to normalize output\n"
	  "      note: see \'mk3normalize\' to generate those files\n"
	  "  -w nfnbr  --wait_nfnbr     Wait for \'nfnbr\' (network frame number) before doing anyhting [*]\n"
	  "  -q        --quiet          Do not print lost packet messages\n"
	  " Disk capture options:\n"
	  "  -r        --raw            When capturing to a \'file\' capture the RAW data, not an SMD file\n"
	  "  -M        --Metadata       When writing a SMD file, store a metadata file with the \'network data frame\' information too (useful to synchronize multiple arrays)\n"
	  "  -c        --cont           Do not stop sending on SF in case a disk error occurs\n"
	  "  -t        --timed sec      Record \'sec\' seconds of data to disk then quit ('0' value will be ignored) (! will not try to connect to the a SF server if any)\n"
	  "\n"
	  "*: will start next step at 'nfnbr + 1' (where next step is \'Drop\' before capturing)\n"
	  , progname, nring_mb, default_dip, drop);
}

/* Options parsing */
int options(int argc, char ***argv) {
  int ra = 1;

  // ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
  //    D        MN              cd f h   lmn  qr t vw   

  static struct option optlist[] = {
    { "help",    0, 0, 'h'},
    { "dest",    1, 0, 'd'},
    { "raw",     0, 0, 'r'},
    { "quiet",   0, 0, 'q'},
    { "cont",    0, 0, 'c'},
    { "verbose", 0, 0, 'v'},
    { "timed",   1, 0, 't'},
    { "mem",     1, 0, 'm'},
    { "link",    0, 0, 'l'},
    { "Drop",    1, 0, 'D'},
    { "fix",     0, 0, 'f'},
    { "normalize", 1,0,'n'},
    { "Normalize", 1,0,'N'},
    { "Metadata",  0,0,'M'},
    { "wait_nfnbr",1,0,'w'},
    { 0,         0, 0, 0  }
  };
  
  int usage = 0, finish = 0, error = 0;
  
  dip = default_dip;
  
  for(;;) {
    int opt = getopt_long(argc, *argv, "hd:rqcvt:m:lD:fn:N:Mw:", optlist, 0);
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
    case 'r':
      capture_smd = 0;
      ra++;
      break;
    case 'q':
      quiet = 1;
      ra++;
      break;    
    case 'c':
      hdstop = 0;
      break;
      ra++;
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
    case 'n':
      meanfile = optarg;
      ra+=2;
      break;
    case 'N':
      gainfile = optarg;
      ra+=2;
      break;
    case 'M':
      recordmd = 1;
      ra++;
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

////////////////////////////////////////
// Hard Drive Writing Thread

// internal quit quick access
#define hd_equit(msg) {equit(msg);goto bail_hdwriter;}
#define hd_sfquit(X,Y) {sffileerrquit(&X, Y);if (X.error_code != SFFE_OK) goto bail_hdwriter;}

void *hdwriter(void *data)
{
  sf_file *file = NULL;
  sf_file_cursor *cursor = NULL;
  sf_file_error err;

  int smmfd = -1;
  sf_file_metadatafile *mdf = NULL;
  sf_file_metadata *md = NULL;

  /* MK3 */ // We use a cursor to access a direct read access to the memory
  mk3cursor *c;

  unsigned long long int mk3timed_ftg = 0; // Frames to get
  unsigned long long int mk3timed_fsf = 0; // Frames so far

  int keeprecording = 1;

  if (debug) { fprintf(stderr, "HD Writter starting\n"); fflush(stderr); }

  // Init
  fdw = open64(fname, O_RDWR|O_CREAT|O_TRUNC, 0666);
  if (fdw < 0) hd_equit("Could not open output file, aborting");

  if (capture_smd) {
    file = sf_file_attach(fdw, &err);
    hd_sfquit(err, "Error attaching the output file");
    
    sf_file_set_type         (file, SFF_TYPE_AUDIO, &err);
    sf_file_set_encoding     (file, SFF_ENCODING_RAW24, &err);
    sf_file_set_channel_count(file, 64, &err);
    sf_file_set_sample_rate  (file, mk3speed, &err);
    sf_file_write_header     (file, &err);
    hd_sfquit(err, "Error writing the file header");
    
    cursor = sf_file_cursor_create(file, &err);
    hd_sfquit(err, "Error creating file cursor");

    if (recordmd) {
      char tmpstr[1024], name[1024];
      char *pos;

      strcpy(tmpstr, fname);
      pos = strrchr(tmpstr, '.');
      if(pos)
	*pos = 0;

      sprintf(name, "%s.smm", tmpstr);

      smmfd = open64(name, O_RDWR|O_CREAT|O_TRUNC, 0666);
      if (smmfd < 0) hd_equit("Could not open output MetaData file, aborting");
      
      mdf = sf_file_metadatafile_attach(file, smmfd, &err);
      hd_sfquit(err, "attaching the output SMM file");

      md = sf_file_metadata_new(mdf, &err);
      hd_sfquit(err, "obtaining a new metadata");
    }
  }

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

    /* MK3 */ // Get the cursor pointer and associated capture timestamp
    mk3array_get_cursorptr_with_nfnbr(it, c, &ptr, &ts, &nfnbr, &aerr);
    mk3errquit(&aerr, "getting cursor pointer");

    if (verbose > 1)
      printf("\r'nfnbr' seen: %d                   ", nfnbr);

    // In case we encountered a disk error, simply "skip" the buffers
    if (hdstop != 99) {
      int t_hdfc = hd_fc;
      
      if ((mk3timed) && ((mk3timed_ftg - mk3timed_fsf) < t_hdfc))
	t_hdfc = mk3timed_ftg - mk3timed_fsf;
      
      if (capture_smd) {
	if (recordmd) {
	  sf_file_metadata_reset(md, &err);
	  hd_sfquit(err, "reseting metadata");
	  sf_file_metadata_add_int(md, 0, nfnbr, &err);
	  hd_sfquit(err, "adding metadata");
	  
	  sf_file_buffer_write_with_ts_and_md(cursor, ptr, t_hdfc*a3size_1f, &ts, md, &err);
	} else {
	  sf_file_buffer_write_with_ts(cursor, ptr, t_hdfc*a3size_1f, &ts, &err);
	}
	
	if (err.error_code != SFFE_OK) {
	  if (hdstop == 1) {
	    hd_sfquit(err, "Error writing the data");
	  } else {
	    sf_file_perror(&err, "Error writting the data (but 'cont' selected)");
	    hdstop = 99;
	  }
	}
      } else {
	size_t wrote;
	wrote = write(fdw, ptr, t_hdfc*a3size_1f);
	if (wrote != t_hdfc*a3size_1f) {
	  if (hdstop == 1) {
	    hd_equit("Error writing the data");
	  } else {
	    fprintf(stderr, "Error writting the data (but 'cont' selected)");
	    hdstop = 99;
	  }
	}
      }
      
      // Stop if disk writting problem and no SF
      if ((me == 0) && (hdstop == 99))
	okquit("\'cont\' selected but no SF, no reason to continue");
      
      if (mk3timed) {
	mk3timed_fsf += hd_fc;
	if (mk3timed_fsf >= mk3timed_ftg) {
	  printf("Wrote %d seconds, exiting\n", mk3timed);
	  running_quit();
	  keeprecording = 0;
	}
      }
    }
  }
  
  if (debug) { fprintf(stderr, "HD Writter quit (1)\n"); fflush(stderr); }
  // End
  if (capture_smd) {
    sf_file_cursor_delete(cursor, &err);
    sf_file_detach(file, &err);
    if (recordmd) {
      sf_file_metadata_free(md, &err);
      sf_file_metadatafile_detach(mdf, &err);
      close(smmfd);
    }
  }
  close(fdw);
 bail_hdwriter:
  fdw = -1;

  if (debug) { fprintf(stderr, "HD Writter quit (2)\n"); fflush(stderr); }
  pthread_exit(0);

  return NULL;
}

////////////////////////////////////////
// Smart Flow Writing thread

// internal quit quick access
#define sf_equit(msg) {equit(msg);goto bail_sfwriter;}
#define sf_sfquit(X,Y) {sferrquit(&X, Y);if (X.error_code != SFE_OK) goto bail_sfwriter;}

void *sfwriter(void *data)
{
  /* MK3 */ // We use a cursor to access a direct read access to the memory
  mk3cursor *c;
  int size;

  // All the SmartFlow initialization is done in the main function
  if (debug) { fprintf(stderr, "SF Writter start\n"); fflush(stderr); }

  /* MK3 */ // Get a cursor
  c = mk3cursor_create(it, sf_fc, &aerr);
  mk3errquit(&aerr, "creating cursor");

  /* MK3 */ // Get the cursor size
  size = mk3cursor_get_datasize(it, c, &aerr);
  mk3errquit(&aerr, "obtaining data size");

  /* MK3 */ // Wait for capture to be started
  // (we can not read from the cursor's pointer before that)
  mk3array_wait_capture_started(it, &aerr);
  mk3errquit(&aerr, "waiting on capture started");

  while (running) {
    char *ptr = NULL;
    struct timespec ts;
    array3_audio_100frames *buffer; // we use the biggest one
    
    /* MK3 */ // Get the cursor pointer and associated capture timestamp
    mk3array_get_cursorptr(it, c, &ptr, &ts, &aerr);
    mk3errquit(&aerr, "getting cursor pointer");
    
    buffer = (array3_audio_100frames *) sf_get_output_buffer(flow, &err);
    sf_sfquit(err, "get_output_buffer");
      
    memcpy(buffer->data, ptr, size);
      
    sf_send_buffer_with_ts(flow, size, &ts, &err);
    sf_sfquit(err, "send_buffer");
  }
  
  if (debug) { fprintf(stderr, "SF Writter quit (1)\n"); fflush(stderr); }
 bail_sfwriter:
  // End Smartflow
  sf_flow_emit_close(flow, &err);
  sf_exit(me, &err);
  me = 0;

  if (debug) { fprintf(stderr, "SF Writter quit (2)\n"); fflush(stderr); }
  pthread_exit(0);
  
  return NULL;
}

/************************************************************/

////////////////////////////////////////
// MAIN

int main(int argc, char** argv)
{
  sf_init_param iparam;
  sf_flow_param fparam;

  int i;
  pthread_attr_t attr;
  sigset_t signals_to_block;

  char prom[9];
  int id;

  FILE* nmf = NULL; // normalization mean file
  FILE* nsf = NULL; // normalization gain file

  progname = argv[0];
  shortname = strrchr(progname, '/');
  if(!shortname)
    shortname = progname;
  else
    shortname++;

  sf_fc    = sf_fc_def;
  mk3speed = def_spd;
  for(i = 0; fc_names[i].name; i++) {
    if (! strcmp(fc_names[i].name, "END")) { // No match, use default
      fprintf(stderr, "Warning: programe name (%s) not recognized, using defaults (%d frames / %d Hz)\n", shortname, sf_fc, mk3speed);
      break;
    } else if (! strcmp(shortname, fc_names[i].name)) { // Found a match
      sf_fc    = fc_names[i].sf_fc;
      mk3speed = fc_names[i].record_speed;
      break;
    }
  }

  if (sf_fc != sf_fc_off) {
    // Initialization of the SmartFlow engine
    sf_init_param_init(&iparam);
    if (mk3speed == mk3array_speed_22K)
      sf_init_param_set_name(&iparam, "Array 3.0 Audio capture (22,050 Hz)");
    else
      sf_init_param_set_name(&iparam, "Array 3.0 Audio capture (44,100 Hz)");
    sf_init_param_set_flow_count(&iparam, 0, 1);
    sf_init_param_set_flow_type(&iparam, SF_KIND_OUTPUT, 0, SF_TYPE_DATA);
    if (sf_fc == sf_fc_5f) {
      sf_init_param_set_flow_name(&iparam, SF_KIND_OUTPUT, 0, "Array3 audio in");
      sf_init_param_set_flow_desc(&iparam, SF_KIND_OUTPUT, 0, DATA_DESC(array3_audio));
    } else if (sf_fc == sf_fc_10f) {
      sf_init_param_set_flow_name(&iparam, SF_KIND_OUTPUT, 0, "Array3 10 frames audio in");
      sf_init_param_set_flow_desc(&iparam, SF_KIND_OUTPUT, 0, DATA_DESC(array3_audio_10frames));
    } else if (sf_fc == sf_fc_20f) {
      sf_init_param_set_flow_name(&iparam, SF_KIND_OUTPUT, 0, "Array3 20 frames audio in");
      sf_init_param_set_flow_desc(&iparam, SF_KIND_OUTPUT, 0, DATA_DESC(array3_audio_20frames));
    } else if (sf_fc == sf_fc_100f) {
      sf_init_param_set_flow_name(&iparam, SF_KIND_OUTPUT, 0, "Array3 100 frames audio in");
      sf_init_param_set_flow_desc(&iparam, SF_KIND_OUTPUT, 0, DATA_DESC(array3_audio_100frames));
    } else
      equit("Wrong 'sf_fc' for SmartFlow, aborting");
    
    sf_init_param_args(&iparam, &argc, argv);
  } else {
    printf("Disabling SF support\n");
  }

  i = nring_mb;
  // Quick check of "remaining" program arguments
  argc -= options(argc, &argv);

  if (argc > 1)
    equit("Leftover arguments on the command line, aborting");

  if (nring_mb < i) {
    char tmp[1024];
    sprintf(tmp, "requested 'mem' can not be under the default value of %d MB", i);
    equit(tmp);
  }

  if (mk3timed < 0)
    equit("'timed' can only be positive");

  if ((! capture_smd) && (recordmd))
    equit("can not record metadata unless recording to SMD files");

  if (argv[0] == NULL) {
    if ((mk3timed) || (sf_fc == sf_fc_off))
      equit("No file specified while in disk only mode, aborting");
    else
      fprintf(stderr, "No file specified, reverting to SmartFlow only\n");
    fdw = -1;
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
  printf("%s (%d frames / %d Hz) started: %s", progname, sf_fc, mk3speed, showtime());

  // SF init (only checking if not in disk only mode)
  if ((sf_fc != sf_fc_off) && (mk3timed == 0)) {
    me = sf_init(&iparam, &err);
    if(err.error_code != SFE_OK) {
      sf_perror(&err, "Couldn't start system");
    }
  }

  // ! (SF | fdw) ?
  if ((me == 0) && (fdw == -1)) {
    fprintf(stderr, "No SmartFlow or file support, exiting ...\n");
    exit (1);
  }

  // SF flow init
  if (me) {
    sf_flow_param_init       (&fparam);
    sf_flow_param_get_flow   (&fparam, &iparam, SF_KIND_OUTPUT, 0);
    // As long as we have no multichannel audio type defined, 
    // we will not be able to do the following:
    /*    sf_flow_param_set_frequency(&fparam, mk3speed); */
    
    flow = sf_flow_create(me, &fparam, &err);
    sferrquit(&err, "Couldn't create the flow ");
  }

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
  // Setup thread
  sigemptyset(&signals_to_block);
  sigaddset(&signals_to_block, SIGINT);
  sigaddset(&signals_to_block, SIGTERM);
  sigaddset(&signals_to_block, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &signals_to_block, NULL);

  pthread_mutex_init(&sigmutex, 0);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Some of the quit functions rely on the signal handler to work
  // properl, so in order to avoid a race condition, we wait
  pthread_cond_init(&sigcond, 0);
  pthread_create(&threads[thread_count++], &attr, shutdown_thread, 0);
  while (sigset != 1) pthread_cond_broadcast(&sigcond); 

  if (fdw != -1) {
    pthread_create(&threads[thread_count++], &attr, hdwriter, 0);
  }
  if (me != 0) {
    pthread_create(&threads[thread_count++], &attr, sfwriter, 0);
  }
  if (verbose) printf("Threads started\n");

  /* MK3 */ // 6: Start capture
  mk3array_capture_on(it, &aerr);
  mk3errquit(&aerr, "starting capture");
  if (verbose) printf("Mk3 capture started\n");

  /* the main loop */
  printf("Starting %s%s%scapture on Mk3 id:%d / Prom #%s\n"
	 " - dropping the first %d sample(s)\n"
	 " - with%s normalization of data\n"
	 " - %sfixing the 1 sample delay\n"
	 , (fdw == -1) ? "" : ((capture_smd) ? "SMD file " : "RAW file ")
	 , ((me != 0) && (fdw != -1)) ? "& " : ""
	 , (me == 0) ? "" : "SmartFlow "
	 , id, prom, drop, 
	 (normalize == mk3_true) ? "" : "OUT",
	 (fix == mk3_true) ? "" : "NOT ");
  while (running) {
    // This thread during the capture does absolutely nothing anymore,
    // since the capture method of the mk3lib takes care of it all

    // I guess it will sleep
    sleep(1);
  }  

  /* MK3 */ // 7: Stop capture
  mk3array_capture_off(it, &aerr);
  mk3errquit(&aerr, "stopping capture");
  if (verbose) printf("Mk3 capture stopped\n");

  for (i = 0; i < thread_count; i++)
    pthread_join(threads[i], NULL);

  /* MK3 */ // 8: Clean memory (including the misc cursors)
  mk3array_delete(it, &aerr);
  mk3errquit(&aerr, "cleaning");

  printf("%s ended: %s", progname, showtime());
  
  pthread_exit(NULL);
  
  return EXIT_SUCCESS;
}
