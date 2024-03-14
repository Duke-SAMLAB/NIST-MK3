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

#include <time.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mk3lib.h"

mk3array* mk3array_create(int mb, int rdfpb, mk3error *err)
{
  mk3array* it;
  mk3cursor* c;
  int tmp;
  
  if (rdfpb <= 0) {
    err->error_code = MK3_RDFPB_ERROR;
    goto bail_ipb;
  }

  if (mb == 0)
    mb = _mk3array_nring_mb_default;

  if (mb < _mk3array_nring_mb_default) {
    err->error_code = MK3_MB_ERROR;
    goto bail_ipb;
  }

  it = (mk3array *) calloc(1, sizeof(*it));
  if (it == NULL) 
    goto bail_malloc;
 
  /* First let us find the least common multiple of the number of
     frames in one network frame (_mk3array_dfpnf) and the number of
     buffers a user will request when capturing (rnumber) */
  tmp = _mk3array_lcm(_mk3array_dfpnf, rdfpb);

  /* Integer arithmetic get the proper amount memory allocated 
     (closest to the requested value) */
  // 1) compute the memory in bytes
  it->nring_mb = mb;
  it->nring_mb *= 1024;
  it->nring_mb *= 1024;

  // 2) compute the max number of data frames we can put in there
  it->nring = it->nring_mb;
  it->nring /= _mk3array_dfsize;

  // 3) compute the "integer" number of times we can use it (relative to 'lcm')
  it->nring /= tmp;

  // 4) compute the real size of the ring buffer to use
  it->nring *= tmp;
  
  // 5) re-compute the amount of memory that will be used 
  it->nring_mb = it->nring;
  it->nring_mb *= _mk3array_dfsize;
  // (this value is not used in practice)

  it->dip = (char *) calloc(1, _mk3array_dip_maxsize);
  if (it->dip == NULL) 
    goto bail_malloc;

  it->prom_nb = (char *) calloc(1, _mk3array_prom_nb_maxsize);
  if (it->prom_nb == NULL) 
    goto bail_malloc;

  it->id       = _mk3array_id_default;
  it->fd       = _mk3array_fd_default;
  it->speed    = _mk3array_speed_default;
  it->slave    = _mk3array_slave_default;

  // to avoid a .h recurrence
  it->status   = mk3array_status_wait_comminit;

  it->RING_buffer = 
    (unsigned char *) calloc(it->nring, _mk3array_dfsize);
  if (it->RING_buffer == NULL) 
    goto bail_malloc;

  it->RING_bufferts = // we can only attach 1 timestamp per network frame
    (struct timespec *) calloc((it->nring / _mk3array_dfpnf),
			       sizeof(*it->RING_bufferts));
  if (it->RING_bufferts == NULL) 
    goto bail_malloc;

  it->RING_buffer_nfnbr = // network frame number
    (int *) calloc((it->nring / _mk3array_dfpnf),
			       sizeof(*it->RING_buffer_nfnbr));
  if (it->RING_buffer_nfnbr == NULL) 
    goto bail_malloc;

  it->rdfpb = rdfpb;
  it->wpos = 0;
  it->running = 0;

  for (tmp = 0; tmp < _mk3cursor_maxnbr; tmp++)
    it->cursors[tmp] = NULL;
  it->cursors_count = 0;

  it->msg_flip = _mk3msg_create(err);
  if (MK3QEC(err)) 
    goto bail_ipb;

  it->msg_flop = _mk3msg_create(err);
  if (MK3QEC(err)) 
    goto bail_ipb;

  it->wait_nfnbr = _mk3array_nfnbr_default;

  it->todrop   = _mk3array_todrop_default;
  it->samples  = 0;

  it->flXp     = mk3_true;
  it->fixdelay = mk3_false;

  it->warn     = mk3_true;
  it->total_nbp_lost  = 0;

  // Normalization
  it->do_norm = mk3_false;
  it->gain = (double *) calloc(_mk3array_nc, sizeof(double));
  if (it->gain == NULL) 
    goto bail_malloc;
  it->mean  = (int *) calloc(_mk3array_nc, sizeof(int));
  if (it->mean == NULL) 
    goto bail_malloc;

  // Everything is set, create the first cursor
  // (the 'create' function adds it to the first available entry, ie '0')
  c = mk3cursor_create(it, rdfpb, err);
  if (MK3QEC(err)) 
    goto bail_ipb;

  // All went well
  err->error_code = MK3_OK;
  return (it);

  // Problem ?
 bail_malloc:
  err->error_code = MK3_MALLOC;
 bail_ipb:
  return NULL;
}

/********************/

void mk3array_delete(mk3array* it, mk3error *err)
{
  int i;

  if (it == NULL) {
    err->error_code = MK3_NULL;
    return;
  }

  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  free(it->dip);
  free(it->prom_nb);
  free(it->RING_buffer);
  free(it->RING_bufferts);
  free(it->RING_buffer_nfnbr);
  free(it->gain);
  free(it->mean);
  _mk3msg_delete(it->msg_flip, err);
  if (MK3QEC(err)) return;
  _mk3msg_delete(it->msg_flop, err);
  if (MK3QEC(err)) return;

  // We erase all cursors left 
  // (you need a 'mk3array' to work with a 'mk3cursor')
  for (i = 0; ((it->cursors_count > 0) && (i < _mk3cursor_maxnbr)); i++)
    if (it->cursors[i]  != NULL) {
      mk3cursor_delete(it, it->cursors[i], err);
      if (MK3QEC(err)) return;
    }
  
  free(it);

  err->error_code = MK3_OK;
}

/********************/

void mk3array_comminit(mk3array *it, const char *dip, mk3error *err)
{
  unsigned long loop = 1; /* MM (20060306) */

  // Check to see if we have already done a bind
  if (it->status != mk3array_status_wait_comminit) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Check that we have an address to bind to
  _mk3array_set_dip(it, dip, err);
  if (MK3QEC(err))
    return;

  // Create and bind the socket
  it->fd = socket(PF_INET, SOCK_DGRAM, 0);
  if (it->fd < 0) {
    err->error_code = MK3_SOCKET;
    return;
  }
  
  memset(&(it->adr), 0, sizeof(it->adr));
  it->adr.sin_family = AF_INET;
  it->adr.sin_port   = htons(32767);
  it->adr.sin_addr.s_addr = INADDR_ANY;

  /* MM (20060306): Fix send by Cedrick Rochet to enable connecting to more
     than one microphone array on the same system */
  if(setsockopt(it->fd, SOL_SOCKET, SO_REUSEADDR, &loop, sizeof(loop)) < 0) { 
    err->error_code = MK3_BIND;
    return;
  }
  /* End MM */

  if (bind(it->fd, (struct sockaddr *) &(it->adr), sizeof(it->adr)) < 0) {
    err->error_code = MK3_BIND;
    return;
  }
  
  memset(&(it->adr), 0, sizeof(it->adr));
  it->adr.sin_family = AF_INET;
  it->adr.sin_port   = htons(32767);
  inet_aton(it->dip, &(it->adr.sin_addr));

  // Force capture to off (validates communication with the Mk3)
  _mk3array_enforce_capture(it, mk3_false, err);
  if (MK3QEC(err)) return;

  // Obtain the ID and Prom Number from the Mk3
  _mk3array_query_ids(it, err);
  if (MK3QEC(err)) return;

  // Set a flag to tell that we have set up communication with the Mk3
  it->status = mk3array_status_wait_initparams;

  err->error_code = MK3_OK;
}

/********************/

int mk3array_ask_speed(mk3array *it, mk3error *err)
{
  // Can we request this ?
  if (it->status > mk3array_status_not_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return 0;
  }

  // No speed has been set yet, ask the Mk3
  if (it->speed == _mk3array_speed_default) {
    _mk3array_query_speed(it, err);
    if (MK3QEC(err)) return 0;
  }

  err->error_code = MK3_OK;
  return (it->speed);
}

/********************/

void mk3array_set_speed(mk3array *it, int speed, mk3error *err)
{
  // Can we request this ?
  if (it->status > mk3array_status_not_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Wrong speed value
  if ((speed != mk3array_speed_22K) && (speed != mk3array_speed_44K)) {
    err->error_code = MK3_SPEED_ERROR;
    return;
  }

  _mk3array_enforce_speed(it, speed, err);
  if (MK3QEC(err)) return;

  err->error_code = MK3_OK;
}
/********************/

int mk3array_ask_slave(mk3array *it, mk3error *err)
{
  // Can we request this ?
  if (it->status > mk3array_status_not_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return 0;
  }

  // No slave value has been set yet, ask the Mk3
  if (it->slave == _mk3array_slave_default) {
    _mk3array_query_slave(it, err);
    if (MK3QEC(err)) return 0;
  }

  err->error_code = MK3_OK;
  return (it->slave);
}

/********************/

void mk3array_set_slave(mk3array *it, int slave, mk3error *err)
{
  // Can we request this ?
  if (it->status > mk3array_status_not_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Wrong slave value
  if ((slave != mk3_true) && (slave != mk3_false)) {
    err->error_code = MK3_SLAVE_ERROR;
    return;
  }

  _mk3array_enforce_slave(it, slave, err);
  if (MK3QEC(err)) return;

  err->error_code = MK3_OK;
}

/********************/

int mk3array_ask_id(mk3array *it, mk3error *err)
{
  // Proper automaton call ?
  if (it->status < mk3array_status_wait_initparams) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return mk3_false;
  }

  if (it->id == _mk3array_id_default) {
    err->error_code = MK3_ID_ERROR;
    return mk3_false;
  }

  err->error_code = MK3_OK;
  return it->id;
}

/********************/

void mk3array_ask_promnb(mk3array *it, char *promnb, mk3error *err)
{
  // Proper automaton call ?
  if (it->status < mk3array_status_wait_initparams) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }
  
  if (it->prom_nb[0] == '\0') {
    err->error_code = MK3_PROMNB_ERROR;
    return;
  }

  err->error_code = MK3_OK;
  strncpy(promnb, it->prom_nb, _mk3array_prom_nb_maxsize);
}

/********************/

void mk3array_initparams(mk3array *it, mk3error *err)
{
  // Proper automaton call ?
  if (it->status != mk3array_status_wait_initparams) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Check that speed is set ?
  if (it->speed == _mk3array_speed_default) {
    err->error_code = MK3_SPEED_ERROR;
    return;
  }

  // Check that slave is set ?
  if (it->slave == _mk3array_slave_default) {
    err->error_code = MK3_SLAVE_ERROR;
    return;
  }

  // All is well, advance automaton
  it->status = mk3array_status_not_capturing;
  err->error_code = MK3_OK;
}

/********************/

void mk3array_initparams_wp(mk3array *it, int speed, int slave, mk3error *err)
{
  // Set speed
  mk3array_set_speed(it, speed, err);
  if (MK3QEC(err)) return;

  // Set slave
  mk3array_set_slave(it, slave, err);
  if (MK3QEC(err)) return;

  // Set initparams
  mk3array_initparams(it, err);
}

/********************/

void mk3array_wait_nfnbr(mk3array *it, int nfnbr, mk3error *err)
{
  // Proper automaton call ?
  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  if ((nfnbr < _mk3array_nfnbr_min) || (nfnbr > _mk3array_nfnbr_max)) {
    err->error_code = MK3_NFNBR_ERROR;
    return;
  }

  err->error_code = MK3_OK;
  it->wait_nfnbr = nfnbr;
}

/********************/

void mk3array_fix_drop_X_first_frames(mk3array *it, int X, mk3error *err)
{
  // Proper automaton call ?
  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  if (X < 0) {
    err->error_code = MK3_DROPFRAMES_ERROR;
    return;
  }

  err->error_code = MK3_OK;
  it->todrop = X;
}

/********************/

void mk3array_fix_one_sample_delay(mk3array *it, int truefalse, mk3error *err)
{
  // Proper automaton call ?
  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  if ((truefalse != mk3_true) && (truefalse != mk3_false)) {
    err->error_code = MK3_SAMPLEDELAY_ERROR;
    return;
  }

  err->error_code = MK3_OK;
  it->fixdelay = truefalse;
}

/********************/

void mk3array_display_warnings(mk3array *it, int truefalse, mk3error *err)
{
  // Proper automaton call ?
  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  if ((truefalse != mk3_true) && (truefalse != mk3_false)) {
    err->error_code = MK3_DISPLAY_ERROR;
    return;
  }

  err->error_code = MK3_OK;
  it->warn = truefalse;
}

/********************/

void mk3array_normalize(mk3array *it, FILE* mean_fd, FILE* gain_fd, mk3error *err)
{
  // Proper automaton call ?
  if (it->status == mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Mean
  if (fread(it->mean, sizeof(int), _mk3array_nc, mean_fd) != _mk3array_nc) {
    err->error_code = MK3_NORM_FERROR;
    return;
  }

  // Standard Deviation
  if (fread(it->gain, sizeof(double), _mk3array_nc, gain_fd) 
      != _mk3array_nc) {
    err->error_code = MK3_NORM_FERROR;
    return;
  }

  it->do_norm = mk3_true;
  err->error_code = MK3_OK;
}

/********************/

void mk3array_capture_on(mk3array *it, mk3error *err)
{
  int i, j;

  // Proper automaton call ?
  if (it->status != mk3array_status_not_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Misc re-init (between captures)
  it->samples = 0;

  it->wpos = 0;

  it->flXp = mk3_true;

  it->total_nbp_lost = 0;

  j = 0;
  for (i = 0; ((j < it->cursors_count) && (i < _mk3cursor_maxnbr)); i++) {
    if (it->cursors[i]  != NULL) {
      mk3cursor_reinit(it, it->cursors[i], err);
      j++;
    }
  }

  it->cerr.error_code = MK3_OK;

  // Start capture
  _mk3array_enforce_capture(it, mk3_true, err);
  if (MK3QEC(err)) return;

  it->running = 1;

  // Create the capture thread
  if (pthread_attr_init(&(it->attr)) != 0)
    goto bail;
  if (pthread_mutex_init(&(it->mutex), 0) != 0)
    goto bail;
  if (pthread_attr_setdetachstate(&(it->attr), PTHREAD_CREATE_JOINABLE) != 0)
    goto bail;
  if (pthread_create(&(it->tid), &(it->attr),
		     _mk3array_capture, (void *) it) != 0)
    goto bail;

  // All is well, advance automaton
  it->status = mk3array_status_capturing;
  err->error_code = MK3_OK;
  return;

 bail:
  err->error_code = MK3_INTERNAL;
}

/********************/

void mk3array_wait_capture_started(mk3array *it, mk3error *err)
{
  _mk3array_wait_status(it, mk3array_status_capturing, err);
}

/********************/

void mk3array_get_databuffer(mk3array *it, char *db, struct timespec *ts, mk3error *err)
{
  _mk3array_get_databuffer(it, db, ts, NULL, err, mk3_true);
}

/********************/

void mk3array_get_databuffer_with_nfnbr(mk3array *it, char *db, struct timespec *ts, int *nfnbr, mk3error *err)
{
  _mk3array_get_databuffer(it, db, ts, nfnbr, err, mk3_true);
}

/********************/

int mk3array_get_databuffer_nonblocking(mk3array *it, char *db, struct timespec *ts, mk3error *err)
{
  return _mk3array_get_databuffer(it, db, ts, NULL, err, mk3_false);
}

/********************/

int mk3array_get_databuffer_nonblocking_with_nfnbr(mk3array *it, char *db, struct timespec *ts, int *nfnbr, mk3error *err)
{
  return _mk3array_get_databuffer(it, db, ts, nfnbr, err, mk3_false);
}

/********************/

void mk3array_get_current_databuffer_timestamp(mk3array *it, struct timespec *ts, mk3error *err)
{
  _mk3array_get_current_databuffer_timestamp(it, ts, err);
}

/********************/

void mk3array_skip_current_databuffer(mk3array *it, mk3error *err)
{
  _mk3array_skip_databuffer(it, err);
}

/********************/

int mk3array_check_databuffer_overflow(mk3array* it, mk3error *err)
{
  return _mk3array_check_databuffer_overflow(it, err);
}

/********************/

void mk3array_get_cursorptr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, mk3error *err)
{
  mk3cursor_get_ptr(it, c, ptr, ts, NULL, err, mk3_true, mk3_true);
}

/********************/

void mk3array_get_cursorptr_with_nfnbr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err)
{
  mk3cursor_get_ptr(it, c, ptr, ts, nfnbr, err, mk3_true, mk3_true);
}

/********************/

int mk3array_get_cursorptr_nonblocking(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, mk3error *err)
{
  return mk3cursor_get_ptr(it, c, ptr, ts, NULL, err, mk3_false, mk3_true);
}

/********************/

int mk3array_get_cursorptr_nonblocking_with_nfnbr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err)
{
  return mk3cursor_get_ptr(it, c, ptr, ts, nfnbr, err, mk3_false, mk3_true);
}

/********************/

int mk3array_check_cursor_overflow(mk3array *it, mk3cursor *c, mk3error *err)
{
  return mk3cursor_check_overflow(it, c, err);
}

/********************/

int mk3array_check_lostdataframes(mk3array *it, mk3error *err)
{
  err->error_code = MK3_OK;
  return (it->total_nbp_lost * _mk3array_dfpnf);
}

/********************/

int mk3array_check_capture_ok(mk3array *it, mk3error *err)
{
  // Proper automaton call ?
  if (it->status != mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return mk3_false;
  }

  err->error_code = it->cerr.error_code;

  if (err->error_code != MK3_OK)
    return mk3_false;

  return mk3_true;
}

/********************/

void mk3array_capture_off(mk3array *it, mk3error *err)
{
  // Proper automaton call ?
  if (it->status != mk3array_status_capturing) {
    err->error_code = MK3_AUTOMATON_ORDER;
    return;
  }

  // Request for the thread to stop
  it->running = 0;

  // Wait for it to stop
  pthread_join(it->tid, NULL);

  // Cancel captures
  _mk3array_enforce_capture(it, mk3_false, err);
  if (MK3QEC(err)) return;

  // Reset misc thread values
  /* seems that I should not have to do anything here */

  // All is well, "advance" automaton
  it->status = mk3array_status_not_capturing;
  err->error_code = MK3_OK;
}
