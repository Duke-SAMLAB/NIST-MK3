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

#ifndef __MK3MSG_H__
#define __MK3MSG_H__

#include "mk3array.h"
#include "mk3error.h"

/********************/
// Messages handler for communication with the mk3 (not user availble)
#include "mk3msg_type.h"

/********************/
// Functions
_mk3msg* _mk3msg_create(mk3error *err);
void _mk3msg_delete(_mk3msg *msg, mk3error *err);

// Send message to mk3
void _mk3msg_send(mk3array *it, _mk3msg *msg, mk3error *err);

// Recv message from mk3
void _mk3msg_recv(mk3array *it, _mk3msg *msg, mk3error *err);

// Recv data frame (_discard_ all other data | used while capturing)
void _mk3msg_recv_dataframe(mk3array *it, _mk3msg *msg, mk3error *err);

// Extract functions
int _mk3msg_extract_framenumber(_mk3msg *msg);
char * _mk3msg_extract_dataptr(_mk3msg *msg);

// Query Loop: 1) send omsg | 2) recv imsg | 3) wait on 'imsg[2] == waiton'
void _mk3msg_queryloop(mk3array* it, _mk3msg *omsg, _mk3msg *imsg, int waiton, mk3error *err);

#endif
