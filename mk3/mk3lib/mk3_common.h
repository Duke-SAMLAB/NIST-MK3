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

#ifndef __MK3_COMMON_H__
#define __MK3_COMMON_H__

// Quick err check
#define MK3QEC(X) (X->error_code != MK3_OK)
#define MK3CQEC(X) (X.error_code != MK3_OK)

enum { // Extension to 'True / False'
  mk3_limbo  = 440, // This is only used as an initializer
};

enum { // Default values for various 'mk3_array' components
  _mk3array_speed_default          = 0,
  _mk3array_dip_maxsize            = 16,
  _mk3array_id_default             = -1,
  _mk3array_prom_nb_maxsize        = 9,
  _mk3array_fd_default             = -1,
  _mk3array_slave_default          = mk3_limbo,
  _mk3array_nring_mb_default       = 16,
  _mk3array_nfnbr_default          = -1,
  _mk3array_nfnbr_min              = 0,
  _mk3array_nfnbr_max              = 65535,
  _mk3array_todrop_default         = 0,
};

enum { // Ring Buffer
  _mk3array_dfpnf   = 5,           // 5 data frames per network frame
  _mk3array_nbpc    = 3,           // 3 bytes per channel
  _mk3array_nc      = 64,          // 64 channels
  _mk3array_dfsize  = _mk3array_nbpc * _mk3array_nc,
                      // 1x 64 channels with 3 byts audio sample
  _mk3array_nfsize  = _mk3array_dfpnf*_mk3array_dfsize, // 1 network request
};

enum { // for '_mk3msg'
  _mk3msg_maxsize = 2048, // This is wateful but allows for a small security
    /* over the TCP 1536 standard limit ... yes, I know we using are UDP :) */
};

enum { // for 'mk3cursor'
  _mk3cursor_maxnbr = 256, // maximum number of cursors allowed
};

#endif
