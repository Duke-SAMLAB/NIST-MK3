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

#include "mk3lib.h"

#include "string.h"
#include "stdlib.h"

char* mk3array_create_databuffers(mk3array *it, int number, int *size, mk3error *err)
{
  char *tmp;
  int tsize;
  mk3cursor *c = it->cursors[0];

  tmp = (char *) calloc(number, c->rdfpb_size);
  if (tmp == NULL) {
    err->error_code = MK3_MALLOC;
    return NULL;
  }

  tsize = number * c->rdfpb_size;

  if (size != NULL)
    *size = tsize;

  err->error_code = MK3_OK;
  return tmp;
}

/*****/

char* mk3array_create_databuffer(mk3array *it, int *size, mk3error *err)
{
  return mk3array_create_databuffers(it, 1, size, err);
}


/*****/

void mk3array_delete_databuffer(mk3array *it, char *db, mk3error *err)
{
  if (it == NULL) {
    err->error_code = MK3_NULL;
    return;
  }

  free(db);

  err->error_code = MK3_OK;
}


/*****/

int mk3array_get_one_databuffersize(mk3array *it, mk3error *err)
{
  err->error_code = MK3_OK;
  return _mk3array_dfsize;
}
