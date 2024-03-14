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

// To use "open64"
// http://www.suse.de/~aj/linux_lfs.html
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

#include <sys/poll.h>		//library for the timout on answer

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>

#include <signal.h>

#include <sys/poll.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sflib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sffile.h>

#include <pthread.h>
#include <getopt.h>

#include <sound_types.h>

static int fd;
static unsigned char msg[2000];
static struct sockaddr_in adr;
static int len;

static const char *default_dip = "10.0.0.2";
const char *dip;
static int ID_array;
static char PROM_NB[8];
int mk3speed = 0;
int slave = -1;
char *link_val = NULL;

int cur_nb, nb_lost;

int quiet = 0;
int verbose = 0;
int stats = 0;
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
int mk3timed = 0;

// PThread
pthread_t threads[3]; // At max we have 3 threads running at the same time
int thread_count = 0;
pthread_mutex_t mutex, sigmutex;
pthread_cond_t sigcond, hdcond, sfcond;
int rsfpos, rhdpos, wpos;
volatile int sigset = 0;

// Ring buffer info
enum {
  sf_fc_5f   = 1, // 1 network data ( = 5 audio samples) 
  sf_fc_10f  = 2, // 2 nf = 10 as
  sf_fc_20f  = 4, // 4 nf = 20 as
  sf_fc_100f = 20,// 20 nf = 100 as
  sf_fc_def  = sf_fc_100f,

  a3size_1f  = 64*3, // 1 audio sample
  a3size_5f  = 5*a3size_1f, // The amount read from each network request
  // In practice all the following values are multiple of 5 frames
  a3size_10f = 2*a3size_5f, // array3_audio_10frames
  a3size_20f = 4*a3size_5f, // array3_audio_20frames
  a3size_100f = 100*a3size_1f, // The size of the SF buffer out
  a3size_def  = a3size_100f,

  // sf_fc : SmartFlow Frame Count (variable, set by user at programe call)
  // hd_fc : HardDrive writter Frame Count (hard coded)

  hd_fc = 40, /* We write by block of 200 frames
		 (at 22KHz that makes over 100 writes per second)
		 (! needs to be a multiple of sf_fc, since the timestamps
		 are attached to each sf_fc value) */ 
};
int nring = 400*hd_fc; /* big buffer (frame frop/buffer overflow safeguard)
			  (! needs to be a multiple of sf_fc and hd_fc) */

/* Martial Michel -- 12/09/2004
     The new defalut value used for 'nring' is based on tests run on a
   computer based on a AMD XP 26000+ with 256MB of RAM on a Linux 
   Fedore Core 2 (2.6 kernel), with a '/etc/sysctl.conf' modifed to match
   the recommendations made in the Mk3 Handbook (detailling: 
   "increase linux TCP buffer limits" and
   "increase linux autotuning TCP buffer limits").
     When using close to 16MB of memory for the nring buffer
   ('-m' option), recording at 44KHz, storing the data to disk 
   (with an ATA133 controller on a 200GB disk) and sending to the
   SmartFlow 20 frames at a time, no data was lost or any buffer
   overflow occurred during the entire test lasting over 6 hours
   (until the disk was full).
     We are glad that we have been using ring buffers for all our data
   collection clients for years now ;)
*/

int nring_mb = 0;
int sf_fc = -1; // value will be determined by the programe name

unsigned char *RING_buffer;
struct timespec *RING_bufferts;

// For stats purposes
int sf_fc_diff = 0;
int sf_fc_max  = 0;
int hd_fc_diff = 0;
int hd_fc_max  = 0;

// Program name 
typedef struct {
  const char *name;
  int sf_fc;
  int record_speed;
} fc_entry;

enum {
  def_spd = 22050,
};

fc_entry fc_names[] = {
  { "mk3cap",          sf_fc_def,  def_spd },
  // 22K
  { "mk3cap_5f_22K",   sf_fc_5f,   22050 },
  { "mk3cap_10f_22K",  sf_fc_10f,  22050 },
  { "mk3cap_20f_22K",  sf_fc_20f,  22050 },
  { "mk3cap_100f_22K", sf_fc_100f, 22050 },
  // 44K
  { "mk3cap_5f_44K",   sf_fc_5f,   44100 },
  { "mk3cap_10f_44K",  sf_fc_10f,  44100 },
  { "mk3cap_20f_44K",  sf_fc_20f,  44100 },
  { "mk3cap_100f_44K", sf_fc_100f, 44100 },
  // End
  { "END", 0, 0}
};

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
  if (debug) { fprintf(stderr, "-> running_off\n"); fflush(stderr); }
  running = 0;

  if (me != 0)
    pthread_cond_broadcast(&sfcond);

  if (fdw != -1)
    pthread_cond_broadcast(&hdcond);

  if (debug) { fprintf(stderr, "<- running_off\n"); fflush(stderr); }
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
  // Also wait for SIGUSR1 (to dump stats)
  sigaddset(&signals_to_catch, SIGUSR1);

  pthread_mutex_lock(&sigmutex);
  pthread_cond_wait(&sigcond, &sigmutex);
  sigset = 1;
  pthread_mutex_unlock(&sigmutex);

  sigwait(&signals_to_catch, &caught);
  while (caught == SIGUSR1) {
    printf("[Stats Dump] (nring=%d) | %s", nring, showtime());
    if (fdw != -1) printf("HD | current=%d | max=%d\n", hd_fc_diff, hd_fc_max);
    if (me != 0) printf("SF | current=%d | max=%d\n", sf_fc_diff, sf_fc_max);
    
    // Wait for the next signal
    sigwait(&signals_to_catch, &caught);
  }

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

////////////////////

/* Online help */
void print_usage(FILE *stream)
{
  fprintf(stream, "Usage : %s [options] [file]\n"
	  "  -h        --help           Prints this message\n"
	  "  -v        --verbose        Details internal workings\n"
	  " Internal Ring Buffer options:\n"
	  " -m MB      --mem MB         Use \'MB\' Mega Bytes of data for the ring buffer size (default: %d MB)\n"
	  "  -s        --stats          Display ring buffer stats (every time the threads \"reads\" data) [1]\n"
	  " Capture options:\n"
	  "  -d IP     --dest IP        Listen to this \'IP\' (default: %s)\n"
	  "  -l val    --link           Put microphone in slave mode (\'val\' is either \'on\' or \'off\') (default is not to change the Mk3 settings)\n"
	  "  -q        --quiet          Do not print lost packet messages\n"
	  " Disk capture options:\n"
	  "  -r        --raw            When capturing to a \'file\' capture the RAW data, not an SMD file\n"
	  "  -c        --cont           Do not stop sending on SF in case a disk error occurs\n"
	  "  -t        --timed sec      Record \'sec\' seconds of data to disk then quit ('0' value will be ignored) (! will not try to connect to the a SF server if any)\n"
	  "\nNotes:\n"
	  "[1] it is possible to get stats sent to stdout wihtout the use of this option; by sending SIGUSR1 to the running process\n"
	  , progname, nring_mb, default_dip);
}

/* Options parsing */
void options(int argc, char ***argv) {

  static struct option optlist[] = {
    { "help",    0, 0, 'h'},
    { "dest",    1, 0, 'd'},
    { "raw",     0, 0, 'r'},
    { "quiet",   0, 0, 'q'},
    { "cont",    0, 0, 'c'},
    { "verbose", 0, 0, 'v'},
    { "stats",   0, 0, 's'},
    { "timed",   1, 0, 't'},
    { "mem",     1, 0, 'm'},
    { "link",    1, 0, 'l'},
    { 0,         0, 0, 0  }
  };
  
  int usage = 0, finish = 0, error = 0;
  
  dip = default_dip;
  
  for(;;) {
    int opt = getopt_long(argc, *argv, "hd:rqcvst:m:l:", optlist, 0);
    if(opt == EOF)
      break;
    switch(opt) {
    case 'h':
      usage = 1;
      finish = 1;
      error = 0;
      break;
    case 'd':
      dip = optarg;
      break;
    case 'r':
      capture_smd = 0;
      break;
    case 'q':
      quiet = 1;
      break;    
    case 'c':
      hdstop = 0;
      break;
    case 'v':
      verbose++;
      break;
    case 's':
      stats++;
      break;
    case 't':
      mk3timed = atoi(optarg);
      break;
    case 'm':
      nring_mb = atoi(optarg);
      break;
    case 'l':
      link_val = optarg;
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

////////////////////////////////////////
// Ring Buffer methods

void inc_rbval(int *val)
{
  (*val)++;
  if (*val == nring)
    *val = 0;
}

/*****/

void add_rbval(int *val, int v)
{
  *val += v;
  if (*val >= nring)
    *val -= nring;
}

/*****/

int rb_diff(int w, int r)
{
  if (w < r) w += nring;
  return (w - r);
}

/*****/

int rb_canread(int w, int r, int d)
{
  if (rb_diff(w, r) < d)
    return 0;
  else
    return 1;
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

  unsigned long long int mk3timed_ftg = 0; // Frames to get
  unsigned long long int mk3timed_fsf = 0; // Frames so far

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
  }

  if (mk3timed) 
    // the '/ 5' here is because each frame is equal to 5 samples
    mk3timed_ftg = (mk3speed / 5) * mk3timed;
  
  // Capture (Main thread code)
  while (running) {
    int lwpos;

    pthread_mutex_lock(&mutex);

    while ((running) && (rb_canread(wpos, rhdpos, hd_fc) == 0))
      pthread_cond_wait(&hdcond, &mutex);

    lwpos = wpos;

    pthread_mutex_unlock(&mutex);

    hd_fc_diff = rb_diff(lwpos, rhdpos);
    if (hd_fc_diff > hd_fc_max) hd_fc_max = hd_fc_diff;

    while ((running) && (rb_canread(lwpos, rhdpos, hd_fc) == 1)) {
      if (stats) // Print 'stats' every time we process a buffer
	printf("HD Writter Ring Buffer Diff: %d\n", rb_diff(lwpos, rhdpos));

      // In case we encountered a disk error, simply "skip" the buffers
      if (hdstop != 99) {
	struct timespec *access_ts;
	int t_hdfc = hd_fc;

	access_ts = RING_bufferts;
	access_ts += (rhdpos / sf_fc);

	if ((mk3timed) && ((mk3timed_ftg - mk3timed_fsf) < t_hdfc))
	  t_hdfc = mk3timed_ftg - mk3timed_fsf;

	if (capture_smd) {
	  sf_file_buffer_write_with_ts(cursor, RING_buffer + rhdpos*a3size_5f, t_hdfc*a3size_5f, access_ts, &err);
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
	  wrote = write(fdw, RING_buffer + rhdpos*a3size_5f, t_hdfc*a3size_5f);
	  if (wrote != t_hdfc*a3size_5f) {
	    if (hdstop == 1) {
	      hd_equit("Error writing the data");
	    } else {
	      fprintf(stderr, "Error writting the data (but 'cont' selected)");
	      hdstop = 99;
	    }
	  }
	}
      }
      
      // Stop if disk writting problem and no SF
      if ((me == 0) && (hdstop == 99))
	okquit("\'cont\' selected but no SF, no reason to continue");

      add_rbval(&rhdpos, hd_fc);

      if (mk3timed) {
	mk3timed_fsf += hd_fc;
	if (mk3timed_fsf >= mk3timed_ftg) {
	  printf("Wrote %d seconds, exiting\n", mk3timed);
	  running_quit();
	}
      }

      if (verbose) printf("HD Reader %d\n", rhdpos);
    }
  }
  
  if (debug) { fprintf(stderr, "HD Writter quit (1)\n"); fflush(stderr); }
  // End
  if (capture_smd) {
    sf_file_cursor_delete(cursor, &err);
    sf_file_detach(file, &err);
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
  // All the SmartFlow initialization is done in the main function
  if (debug) { fprintf(stderr, "SF Writter start\n"); fflush(stderr); }

  while (running) {
    int lwpos;

    pthread_mutex_lock(&mutex);

    while ((running) && (rb_canread(wpos, rsfpos, sf_fc) == 0))
      pthread_cond_wait(&sfcond, &mutex);
    
    lwpos = wpos;

    pthread_mutex_unlock(&mutex);

    sf_fc_diff = rb_diff(lwpos, rsfpos);
    if (sf_fc_diff > sf_fc_max) sf_fc_max = sf_fc_diff;

    while ((running) && (rb_canread(lwpos, rsfpos, sf_fc) == 1)) {
      array3_audio_100frames *buffer; // we use the biggest one
      struct timespec *access_ts;

      if (stats) // Print 'stats' every time we process a buffer
	printf("SF Writter Ring Buffer Diff: %d\n", rb_diff(lwpos, rsfpos));
    
      buffer = (array3_audio_100frames *) sf_get_output_buffer(flow, &err);
      sf_sfquit(err, "get_output_buffer");
      
      memcpy(buffer->data, RING_buffer + rsfpos*a3size_5f, sf_fc*a3size_5f);
      
      access_ts = RING_bufferts;
      access_ts += (rsfpos / sf_fc);

      sf_send_buffer_with_ts(flow, sf_fc*a3size_5f, access_ts, &err);
      sf_sfquit(err, "send_buffer");

      add_rbval(&rsfpos, sf_fc);
      if (verbose) printf("SF Reader %d\n", rsfpos);
    }
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

static void send_msg(void)
{
  if(sendto(fd, msg, len, 0, (const struct sockaddr *)&adr, sizeof(adr)) < 0) {
    perror("sendto");
    exit(1);
  }
}

static void recieve_msg(void)
{
  struct pollfd pfd;
  int res;
  pfd.fd = fd;
  pfd.events = POLLIN;
  res = poll(&pfd, 1, 100);
  if(!res) {
    fprintf(stderr, "Timeout on answer: %s", showtime());
    len = 0;
    exit(0);
    return;
  }

  len = recv(fd, msg, sizeof(msg), 0);
}

static int ask_status_slave(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 2;           //request number
    msg[2] = 0;
    msg[3] = 0;
    
    len = 4;
    send_msg();
    
    recieve_msg();
    if(!len)
      continue;
    done = (msg[0] == 2);
    
  } while(!done);
  printf("Your Microphone Array slave is: %i\n",(int) msg[2]);
  return (int) msg[2];
}

static void slave_on(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 1;           //request number
    msg[2] = 0xff;
    msg[3] = 0xff;  
    
    len = 4;
    send_msg();
    
    done = (ask_status_slave() == 1);
    
  } while(!done);
}

static void slave_off(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 1;           //request number
    msg[2] = 0;
    msg[3] = 0;  
    
    len = 4;
    send_msg();

    done = (ask_status_slave() == 0);

  } while(!done);
}

static void ask_id(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 3;           //request number
    msg[2] = 0;
    msg[3] = 0;
    
    len = 4;
    send_msg();
    
    recieve_msg();
    if(!len)
      continue;
    done = (msg[0] == 3);
    
  } while(!done);
  ID_array = (msg[2])|(msg[3]<<8);
  memcpy(PROM_NB,msg+6,8);
  printf("Capture on %x with PROM %s\n", ID_array, PROM_NB);
}

static int ask_status_capture(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 5;           //request number
    msg[2] = 0;
    msg[3] = 0;

    len = 4;
    send_msg();
    
    recieve_msg();
    if(!len)
      continue;
    done = (msg[0] == 5);
    
  } while(!done);
  printf("Your Microphone Array capture is: %i\n",(int) msg[2]);
  return (int) msg[2];
}

static void capture_on(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 4;           //request number
    msg[2] = 0xff;
    msg[3] = 0xff;  
    
    len = 4;
    send_msg();
    
    done = (ask_status_capture() == 1);
    
  } while(!done);
}

static void capture_off(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 4;           //request number
    msg[2] = 0;
    msg[3] = 0;  
    
    len = 4;
    send_msg();

    done = (ask_status_capture() == 0);
    
  } while(!done);
}
      
static int ask_status_speed(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 8;           //request number
    msg[2] = 0;
    msg[3] = 0;
    
    len = 4;
    send_msg();
    
    recieve_msg();
    if(!len)
      continue;
    
    done = (msg[0] == 7);
    
  } while(!done);
  
  printf("Your Microphone Array speed is: %i\n",(int) msg[2]);
  return (int) msg[2];
}

static void speed_on(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 7;           //request number
    msg[2] = 0xff;
    msg[3] = 0xff;  
    
    len = 4;
    send_msg();
    
    done = (ask_status_speed() == 1);
    
  } while(!done);
}

static void speed_off(void)
{
  int done = 0;
  do {
    msg[0] = 0;
    msg[1] = 7;           //request number		
    msg[2] = 0;
    msg[3] = 0;  
    
    len = 4;
    send_msg();
    
    done = (ask_status_speed() == 0);
    
  } while(!done);
}

/************************************************************/

////////////////////////////////////////
// MAIN

int main(int argc, char** argv)
{
  int tmp_nring_mb;

  sf_init_param iparam;
  sf_flow_param fparam;

  int i;
  pthread_attr_t attr;
  sigset_t signals_to_block;
  
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
      fprintf(stderr, "Warning: programe name (%s) not recognized, using defaults (%d frames / %d Hz)\n", shortname, 5*sf_fc, mk3speed);
      break;
    } else if (! strcmp(shortname, fc_names[i].name)) { // Found a match
      sf_fc    = fc_names[i].sf_fc;
      mk3speed = fc_names[i].record_speed;
      break;
    }
  }

  // Initialization of the SmartFlow engine
  sf_init_param_init(&iparam);
  if (mk3speed == 22050)
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

  // Integer arithmetic
  nring_mb = nring;
  nring_mb *= a3size_5f;
  nring_mb /= 1024;
  nring_mb /= 1024;
  tmp_nring_mb = nring_mb;

  // Quick check of "remaining" program arguments
  options(argc, &argv);
  
  if (link_val != NULL) {
    if (strncmp(link_val, "on", 2) == 0) {
      slave = 1;
    } else if (strncmp(link_val, "off", 3) == 0) {
      slave = 0;
    } else {
      char tmp[1024];
      sprintf(tmp, "bad value for \'link\' (%s); one of \'on\' or \'off\' must be selected", link_val);
      equit(tmp);
    }
  }

  if (nring_mb < tmp_nring_mb) {
    char tmp[1024];
    sprintf(tmp, "requested 'mem' can not be under the default value of %d MB", tmp_nring_mb);
    equit(tmp);
  }
  if (nring_mb != tmp_nring_mb) {
    // 'nring_mb' was changed, let use recompute the proper 'nring' value
    nring = nring_mb;
    nring *= 1024;
    nring *= 1024;
    nring /= a3size_5f;
    nring /= hd_fc;
    nring *= hd_fc;
  }

  if (mk3timed < 0)
    equit("Error: 'timed' can only be positive\n");

  if (argv[0] == NULL) {
    if (mk3timed)
      equit("Error: No file specified while in 'timed' mode, aborting\n");
    else
      fprintf(stderr, "No file specified, reverting to SmartFlow only\n");
    fdw = -1;
  } else {
    strcpy(fname, argv[0]);
    fdw = 0;
  }

  printf("%s (%d frames / %d Hz) started: %s", progname, 5*sf_fc, mk3speed, showtime());

  // SF init (only checking if not using 'timed')
  if (mk3timed == 0) {
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

  /* Network initialization */
  fd = socket(PF_INET, SOCK_DGRAM, 0);
  if(fd<0) equit("socket");
  
  memset(&adr, 0, sizeof(adr));
  adr.sin_family = AF_INET;
  adr.sin_port = htons(32767);
  adr.sin_addr.s_addr = INADDR_ANY;

  if(bind(fd, (struct sockaddr *)&adr, sizeof(adr)) < 0)
    equit("bind error");

  memset(&adr, 0, sizeof(adr));
  adr.sin_family = AF_INET;
  adr.sin_port = htons(32767);
  inet_aton(dip, &adr.sin_addr);

  /* MK3 initialization */
  capture_off();

  if (slave == 0)
    slave_off();
  else if (slave == 1)
    slave_on();
  
  if (mk3speed == 22050)
    speed_off();
  else
    speed_on();

  ask_id();

  // Setup ring (before thread setup)
  wpos = rsfpos = rhdpos = 0;
  RING_buffer = (unsigned char *) calloc(nring, a3size_5f);
  if (RING_buffer == NULL)
    equit("Could not \'malloc\' memory for ring buffer (buffer)");
  RING_bufferts = (struct timespec *) calloc((nring / sf_fc), sizeof(*RING_bufferts));
  if (RING_bufferts == NULL)
    equit("Could not \'malloc\' memory for ring buffer (size)");

  // Start capture
  capture_on();

  running = 1;
  // Setup thread
  sigemptyset(&signals_to_block);
  sigaddset(&signals_to_block, SIGINT);
  sigaddset(&signals_to_block, SIGTERM);
  sigaddset(&signals_to_block, SIGUSR1);
  pthread_sigmask(SIG_BLOCK, &signals_to_block, NULL);

  pthread_mutex_init(&mutex, 0);
  pthread_mutex_init(&sigmutex, 0);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  // Some of the quit functions rely on the signal handler to work
  // properl, so in order to avoid a race condition, we wait
  pthread_cond_init(&sigcond, 0);
  pthread_create(&threads[thread_count++], &attr, shutdown_thread, 0);
  while (sigset != 1) pthread_cond_broadcast(&sigcond); 

  if (fdw != -1) {
    pthread_cond_init(&hdcond, 0);
    pthread_create(&threads[thread_count++], &attr, hdwriter, 0);
  }
  if (me != 0) {
    pthread_cond_init(&sfcond, 0);
    pthread_create(&threads[thread_count++], &attr, sfwriter, 0);
  }

  /* the main loop */
  cur_nb = -1;
  nb_lost = 0;
  while (running) {
    int nb_frame_rcvd = 0;
    struct timeval tv;
    struct timespec *access_ts;

    gettimeofday(&tv, 0);
    access_ts = RING_bufferts;
    access_ts += (wpos / sf_fc);
    access_ts->tv_sec  = tv.tv_sec;
    access_ts->tv_nsec = 1000 * tv.tv_usec;

    while ((running) && (nb_frame_rcvd < sf_fc)) {
      if ( nb_lost > 0){
	nb_lost = nb_lost - 1;
	memset(RING_buffer + wpos*a3size_5f + nb_frame_rcvd*a3size_5f,0, a3size_5f);
      } else {
	recieve_msg();
	if(msg[0] == 0x86) {
	  int nb = (msg[1]<<8) | msg[2];
	  if(cur_nb != -1 && nb != cur_nb){
	    if ( nb > cur_nb) nb_lost = nb - cur_nb;
	    if ( nb < cur_nb) nb_lost = (65535 - cur_nb) + nb;
	    if (! quiet) {
	      fprintf(stderr, "Lost %d-%d=>%d at %s"
		      , cur_nb, (nb-1) & 0xffff, nb_lost, showtime());
	      fflush(stderr);
	    }
	  }
	  cur_nb = (nb + 1) & 0xffff;
	  memcpy(RING_buffer + wpos*a3size_5f + nb_frame_rcvd*a3size_5f, msg+4, a3size_5f);
	}
      }
      nb_frame_rcvd++;
    }

    if (running) {
      // Mutex
      // we increase the value to the writting pointer only once data has
      // been added to the buffer, indicating that it is safe to read it now
      pthread_mutex_lock(&mutex);
      
      if (verbose) printf("Write %d\n", wpos);
      add_rbval(&wpos, sf_fc);
      
      if ((me != 0) && (wpos == rsfpos)) {
	fprintf(stderr,	"***** WARNING ***** Ring buffer overflow on Flow Emit [wpos:%d|rsfpos:%d] : %s", wpos, rsfpos, showtime());
	fflush(stderr);
      }
      if ((fdw != -1) && (wpos == rhdpos)) {
	fprintf(stderr,	"***** WARNING ***** Ring buffer overflow on Disk Writing [wpos:%d|rhdpos:%d] : %s", wpos, rhdpos, showtime());
	fflush(stderr);
	//      equit("");
      }
      pthread_mutex_unlock(&mutex);

      if (me != 0)
	pthread_cond_broadcast(&sfcond);
      if ((fdw != -1) && (wpos % hd_fc == 0))
	pthread_cond_broadcast(&hdcond);
    }
  }
  
  capture_off();
  close(fd);
  
  for (i = 0; i < thread_count; i++)
    pthread_join(threads[i], NULL);
  
  printf("%s ended: %s", progname, showtime());
  
  pthread_exit(NULL);
  
  return EXIT_SUCCESS;
}
