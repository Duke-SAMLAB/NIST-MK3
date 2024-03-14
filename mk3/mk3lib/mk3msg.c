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

#include "mk3msg.h"
#include "mk3_common.h"

#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>

_mk3msg* _mk3msg_create(mk3error *err)
{
  _mk3msg* it = NULL;

  it = (_mk3msg *) calloc(1, sizeof(*it));
  if (it == NULL) {
    err->error_code = MK3_MALLOC;
    return NULL;
  }

  // Most of the messages need to be initialized to '0' : 'calloc'
  it->msg = (unsigned char *) calloc(1, _mk3msg_maxsize);
  if (it->msg == NULL)
    err->error_code = MK3_MALLOC;
  it->len = 0;
    
  err->error_code = MK3_OK;
  return it;
}

/*****/

void _mk3msg_delete(_mk3msg *msg, mk3error *err)
{
  if (msg->msg != NULL) free(msg->msg);
  free(msg);

  err->error_code = MK3_OK;
}

/*****/

void _mk3msg_send(mk3array *it, _mk3msg *msg, mk3error *err)
{
  err->error_code = MK3_OK;

  if (sendto(it->fd, msg->msg, msg->len, 0, 
	     (const struct sockaddr *) &(it->adr), sizeof(it->adr)) <0)
    err->error_code = MK3_COMM_SEND;
}

/*****/

void _mk3msg_recv(mk3array *it, _mk3msg *msg, mk3error *err)
{
  struct pollfd pfd;
  int res;

  pfd.fd = it->fd;
  pfd.events = POLLIN;
  res = poll(&pfd, 1, 100);
  
  if (! res) {
    err->error_code = MK3_COMM_RECV;
    return;
  }

  msg->len = recv(it->fd, msg->msg, _mk3msg_maxsize, 0);

  err->error_code = MK3_OK;
}

/*****/

void _mk3msg_recv_dataframe(mk3array *it, _mk3msg *msg, mk3error *err)
{
  int done = 0;

  while (done == 0) {
    _mk3msg_recv(it, msg, err);
    if (MK3QEC(err)) return;

    done = (msg->msg[0] == 0x86);
  }
}

/*****/

void _mk3msg_queryloop(mk3array *it, _mk3msg *omsg, _mk3msg *imsg, int waiton, mk3error *err)
{
  int done = 0;

  while (! done) {
    _mk3msg_send(it, omsg, err);
    if (MK3QEC(err)) return;
  
    _mk3msg_recv(it, imsg, err);
    if (MK3QEC(err)) return;

    if (! imsg->len)
      continue;

    done = (imsg->msg[0] == waiton);
  }

  err->error_code = MK3_OK;
}

/*****/

int _mk3msg_extract_framenumber(_mk3msg *msg)
{
  return ((msg->msg[1] << 8) | msg->msg[2]);
}

/*****/

char * _mk3msg_extract_dataptr(_mk3msg *msg)
{
  return (char *) (msg->msg + 4);
}
