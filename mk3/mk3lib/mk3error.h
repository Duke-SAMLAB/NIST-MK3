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

#ifndef __MK3ERROR_H__
#define __MK3ERROR_H__

/********************/
// MK3 error handler
typedef struct {
  int error_code;
} mk3error;

/********************/
// Error messages

enum {
  MK3_OK,
  MK3_CAPTURE,           // Error during capture
  MK3_RDFPB_ERROR,       // Invalid value for "req data frame per buffer"
  MK3_MB_ERROR,          // Invalid value for 'megabytes'
  MK3_NULL,              // Null value for one handler
  MK3_MALLOC,            // Problem allocating memory
  MK3_SOCKET,            // Could not create socket
  MK3_BIND,              // Could not bind socket
  MK3_DIP_ERROR,         // Destination IP error
  MK3_AUTOMATON_ORDER,   // Operation does not follow authorized order
  MK3_COMM_SEND,         // Error sending data to Mk3
  MK3_COMM_RECV,         // Error receiving data from Mk3
  MK3_GIBBERISH,         // Unexpected data / do not know how to parse
  MK3_SPEED_ERROR,       // Error with 'speed' value
  MK3_SLAVE_ERROR,       // Error with 'slave' value
  MK3_ID_ERROR,          // Error with 'id' value
  MK3_PROMNB_ERROR,      // Error with 'prom_nb' value
  MK3_NFNBR_ERROR,       // Eroor with value of "network frames number"
  MK3_DROPFRAMES_ERROR,  // Error with value of "frames to drop"
  MK3_SAMPLEDELAY_ERROR, // Error with value of "sample delay"
  MK3_INTERNAL,          // Internal error
  MK3_DISPLAY_ERROR,     // Error with value of "display warnings"
  MK3_CURSOR_COUNT,      // Too many cursors created
  MK3_RDFPB_MODULO,      // Value of 'rdfpb' is not a multiple of init value
  MK3_CURSOR_ERROR,      // Error working with this cursor
  MK3_NORM_FERROR,       // Error reading data from normalization files
};

#endif
