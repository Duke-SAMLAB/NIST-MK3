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

#include "mk3cursor.h"
#include "mk3_common.h"
#include "mk3lib.h"
#include "mk3array.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>


mk3cursor *mk3cursor_create(mk3array* it, int rdfpb, mk3error* err)
{
  mk3cursor * c  = NULL;
  int i;

  if (it->cursors_count >= _mk3cursor_maxnbr) {
    err->error_code = MK3_CURSOR_COUNT;
    return NULL;
  }

  if (it->rdfpb % rdfpb != 0) {
    err->error_code = MK3_RDFPB_MODULO;
    return NULL;
  }

  c = (mk3cursor *) calloc(1, sizeof(*c));
  if (c == NULL) {
    err->error_code = MK3_MALLOC;
    return NULL;
  }

  c->rpos  = 0;
  c->rdfpb = rdfpb;
  c->rdfpb_size = rdfpb * _mk3array_dfsize;


  c->last_got_ts = 0;
  c->rb_overflow = mk3_false;

  if (pthread_cond_init(&(c->cond), 0) != 0) {
    err->error_code = MK3_INTERNAL;
    return NULL;
  }

  for (i = 0; i < _mk3cursor_maxnbr; i++)
    if (it->cursors[i] == NULL) break;

  it->cursors[i] = c;
  c->pos = i;
  it->cursors_count++;

  err->error_code = MK3_OK;

  return c;
}

/*****/

void mk3cursor_delete(mk3array* it, mk3cursor *c, mk3error *err)
{
  it->cursors[c->pos] = NULL;
  it->cursors_count--;

  free(c);

  err->error_code = MK3_OK;
}

/*****/

int mk3cursor_isof(mk3array *it, mk3cursor *c)
{
  if (it->cursors[c->pos] != c)
    return 0;

  return 1;
}

/*****/

void mk3cursor_reinit(mk3array* it, mk3cursor *c, mk3error *err)
{
  if (! mk3cursor_isof(it, c)) {
    err->error_code = MK3_CURSOR_ERROR;
    return;
  }

  c->rpos = 0;
  c->last_got_ts = 0;
  c->rb_overflow = mk3_false;

  if (pthread_cond_init(&(c->cond), 0) != 0) {
    err->error_code = MK3_INTERNAL;
    return;
  }

  err->error_code = MK3_OK;
}

/*****/

int mk3cursor_get_datasize(mk3array *it, mk3cursor *c, mk3error *err)
{
  if (! mk3cursor_isof(it, c)) {
    err->error_code = MK3_CURSOR_ERROR;
    return -1;
  }

  err->error_code = MK3_OK;
  return c->rdfpb_size;
}

/*****/


int mk3cursor_get_ptr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err, int blocking, int incrpos)
{
  unsigned long long int tmp;
  struct timespec *access_ts;
  int *access_nfnbr;
  int tell;
  
  if (it->status != mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return mk3_limbo;
  }

  if (! mk3cursor_isof(it, c)) {
    err->error_code = MK3_CURSOR_ERROR;
    return mk3_limbo;
  }
  
  tell = -1;
  // First check that we can read (or wait until we can)
  pthread_mutex_lock(&(it->mutex));
  if (blocking == mk3_false) // not blocking: set the 'tell' value
    tell = _mk3array_rb_canread(it, c);
  else // blocking: wait until we can read 
    while ((it->running) && (_mk3array_rb_canread(it, c) == 0))
      pthread_cond_wait(&(c->cond), &(it->mutex));
  pthread_mutex_unlock(&(it->mutex));
  
  // Not blocking: return if no data to read
  if ((blocking == mk3_false) && (tell == 0))
    return mk3_false;
  
  // From here, it is common to blocking and non blockin:
  // we have some data, let us process it (if we are still 'running')
  tell = mk3_false;
  if (! it->running) 
    goto out;
  
  // First the time-stamp
  access_ts = it->RING_bufferts;
  access_ts += c->rpos / _mk3array_dfpnf;
  
  if (ts != NULL) {
    ts->tv_sec  = access_ts->tv_sec;
    ts->tv_nsec = access_ts->tv_nsec;
  }
  
  // Then the network frame number
  access_nfnbr = it->RING_buffer_nfnbr;
  access_nfnbr += c->rpos / _mk3array_dfpnf;
  
  if (nfnbr != NULL)
    *nfnbr = *access_nfnbr;

  // Which allows us to check the overflow status
  tmp =  _mk3array_uint64ts(access_ts->tv_sec, access_ts->tv_nsec);
  
  if (c->last_got_ts == 0)
    c->last_got_ts = tmp;
  
  // Since time can only go forward 
  // (or "stay the same" when rdfpb < _mk3array_dfpnf)
  if (tmp < c->last_got_ts) {
    c->rb_overflow = mk3_true;
    if (it->warn == mk3_true) {
      fprintf(stderr, "[MK3LIB] Ring buffer overflow (cursor #%d) at %s"
	      , c->pos, _mk3array_showtime());
      fflush(stderr);
    }
    /* Understand that if you have a ring buffer overflow, you have faulty
       data, yet it is not the aim of this library "not" to give it to you */ 
  }
  
  // Then the data itself
  if (ptr != NULL)
    *ptr = it->RING_buffer + c->rpos * _mk3array_dfsize;
  
  // Finaly increase the read cursor (if allowed to)
  if (incrpos == mk3_true)
    _mk3array_rb_add(it, c);
  
  tell = mk3_true;

 out:
  err->error_code = MK3_OK;
  return tell;
}

/*****/

int mk3cursor_check_overflow(mk3array *it, mk3cursor *c, mk3error *err)
{
  if (! mk3cursor_isof(it, c)) {
    err->error_code = MK3_CURSOR_ERROR;
    return mk3_limbo;
  }

  err->error_code = MK3_OK;
  return c->rb_overflow;
}

