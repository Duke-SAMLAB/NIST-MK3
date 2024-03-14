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

#ifndef __MK3CURSOR_H__
#define __MK3CURSOR_H__

#include "mk3array.h"

#include <pthread.h>


// Type
#include "mk3cursor_type.h"

/********************/
// Internal functions

// Check that the 'mk3cursor' is really part of the 'mk3array'
int mk3cursor_isof(mk3array *it, mk3cursor *c);

// Re-initialize the values of the 'mk3cursor' as per its creation
void mk3cursor_reinit(mk3array* it, mk3cursor *c, mk3error *err);

// Get the cursor data
int mk3cursor_get_ptr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err, int blocking, int incrpos);

// Check the overflow status
int mk3cursor_check_overflow(mk3array *it, mk3cursor *c, mk3error *err);

#endif
