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

#ifndef __MK3ARRAY_H__
#define __MK3ARRAY_H__

#include "mk3cursor_type.h"
#include "mk3msg_type.h"
#include "mk3error.h"
#include "mk3_misc.h"
#include "mk3_common.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

/********************/
// MK3 array handler
typedef struct {

  /********************/
  // "public" side of the mk3
  // (values 'read' using user avilable functions)

  char *dip;            // The destination IP
  int id;               // The mk3 id
  char *prom_nb;        // The mk3 Prom Number
  int speed;            // The capture speed
  int slave;            // The slave mode

  int status;           // The mk3 capture status

  /********************/
  // "private" side of the mk3

  // Normalization
  int    do_norm;
  double *gain;
  int    *mean;

  // socket related
  int fd;
  struct sockaddr_in adr;

  // Capture error information
  mk3error   cerr;

  // Capture thread
  pthread_t       tid;
  pthread_attr_t  attr;
  pthread_mutex_t mutex;
  volatile int   running;

  // Capture Ring buffer
  int nring;
  int nring_mb;
  unsigned char *RING_buffer;
  struct timespec *RING_bufferts;
  int *RING_buffer_nfnbr;
  int wpos;
  int rdfpb;

  // Default access cursor is the '0'th one (the one used when using 
  mk3cursor *cursors[_mk3cursor_maxnbr];
  int cursors_count;

  // Capture buffers (for re-ordering of data)
  _mk3msg* msg_flip;
  _mk3msg* msg_flop;
  int flXp; // Selector
  // Fix the Mk3 sample delay 
  int fixdelay;

  // Wait until specficic netword data frame number
  int wait_nfnbr;

  // Drop the X first samples
  int todrop;
  int samples;

  // Print warnings
  int warn;

  // Total number of packets lost
  int total_nbp_lost;

  // Note that ring buffer overflows are related to cursors only, not global
} mk3array;

/********************/
// Functions

// capture
int _mk3array_query_capture(mk3array *it, mk3error *err);
void _mk3array_enforce_capture(mk3array *it, int truefalse, mk3error *err);

// The capture thread itself
void *_mk3array_capture(mk3array *it);

// ID
void _mk3array_query_ids(mk3array *it, mk3error *err);

// DIP
void _mk3array_set_dip(mk3array *it, const char *dip, mk3error *err);

// speed
void _mk3array_query_speed(mk3array *it, mk3error *err);
void _mk3array_enforce_speed(mk3array *it, int speed, mk3error *err);

// slave
void _mk3array_query_slave(mk3array *it, mk3error *err);
void _mk3array_enforce_slave(mk3array *it, int truefalse, mk3error *err);

// Wait on status
void _mk3array_wait_status(mk3array *it, int status, mk3error *err);

// databuffer 
int _mk3array_get_databuffer(mk3array *it, char *db, struct timespec *ts, int *nfnbr, mk3error *err, int blocking);
void _mk3array_get_current_databuffer_timestamp(mk3array* it, struct timespec *ts, mk3error *err);
int _mk3array_get_current_databuffer_nfnbr(mk3array* it, mk3error *err);
void _mk3array_skip_databuffer(mk3array *it, mk3error *err);
int _mk3array_check_databuffer_overflow(mk3array* it, mk3error* err);

/********************/
// Ring buffer methods

void _mk3array_rb_add_w(mk3array* it);
void _mk3array_rb_add(mk3array* it, mk3cursor *c);
int _mk3array_rb_diff(mk3array *it, mk3cursor *c);
int _mk3array_rb_canread(mk3array * it, mk3cursor *c);

#endif
