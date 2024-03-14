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

#ifndef _MK3LIB_H_
#define _MK3LIB_H_

#include "mk3_common.h"
#include "mk3lib_int.h"
#include <stdio.h>

/****************************************/
// 'mk3array' type

/**********/
// Some functions authorized values 

enum { // Authorized 'speed' values
  mk3array_speed_22K = 22050,
  mk3array_speed_44K = 44100,
};

enum { // mk3 'status' ordered values
  mk3array_status_wait_comminit,   // Need Communication Initialization
  mk3array_status_wait_initparams, // Need to check/set speed,slave, ...
  mk3array_status_not_capturing,   // Ready to capture
  mk3array_status_capturing,       // Capturing
};

enum { // True / False
  mk3_true   = 99,
  mk3_false  = 101,
};
/* Some functions will use and/or return an 'int' that is to be compared
   to these value to check the yes/no value/status */

/*
  ******************** WARNING ********************

   It is very important to understand the difference between 
   a "data frame" and a "network frame".

   Per the Mk3 design:

   1x "network frame" = 5 x "data frame"

   therfore only 1 timestamp can be given per 5 data frames.
   Since the creation function allows the user to specifiy
   a "requested data frames per buffer" value, if this value is
   under 5 (authorized value), some timestamps will be identical,
   and the library will _not_ consider this a problem.

*/

/********************/
// Public functions (by 'status' order)

/**********/ // 0) before anything

/*
 'mk3array_create'
   arg 1 : int        the request amount of memory to use for the internal
                      storage of the collected data (0 will make the program
		      use its default value)
   arg 2 : int        when receiving data, this value allow the user to
                      request a certains amount of data frames to be delivered
		      to the 'get' functions                      
   arg 3 : mk3error*  in order to set the error state of the function (if any)
   return: mk3array*  newly created 'mk3array' (to be deleted using
                      'mk3array_delete')
 The functions create and returns an opaque handler for a 'mk3array'
 but does not initialize the connection
*/
mk3array* mk3array_create(int megabytes, int requested_data_frames_per_buffer, mk3error *err);
 
/**********/ // 1) 'mk3array_status_wait_comminit'

/*
 'mk3array_comminit'
   arg 1 : mk3array*   'mk3array' handle
   arg 2 : const char* the IP address of the Mk3 to connect to
   arg 3 : mk3error*   error state of function (if any)
   return: void
 Create the communication socket between the local system and the Mk3
 (and to 'check' that the connection is working order the Mk3 to stop capture)
*/
void mk3array_comminit(mk3array *it, const char *dip, mk3error *err);

/**********/ // 2) 'mk3array_status_wait_initparams'

/*
 'mk3array_ask_speed'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: int        one of 'mk3array_speed_22K' or 'mk3array_speed_44K'
 Query the Mk3 (if no value was set by the user) to check the current value 
 of the sampling rate (== capture 'speed')
*/
int mk3array_ask_speed(mk3array *it, mk3error *err);

/*
 'mk3array_set_speed'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        one of 'mk3array_speed_22K' or 'mk3array_speed_44K'
   arg 3 : mk3error*  error state of function (if any)
   return: void
 Set the Mk3 sampling rate
*/
void mk3array_set_speed(mk3array *it, int speed, mk3error *err);

/*
 'mk3array_ask_slave'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: int        one of 'mk3_true' or 'mk3_false'
 Query the Mk3 (if no value was set by the user) to check the current value 
 of the "slave" mode ("true", the Mk3 clock is slave to an external clock, ...)
*/
int mk3array_ask_slave(mk3array *it, mk3error *err);

/*
 'mk3array_set_slave'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        one of 'mk3_true' or 'mk3_false'
   arg 3 : mk3error*  error state of function (if any)
   return: void
 Set the "slave" mode (warning: can not check that there is an external clock)
*/
void mk3array_set_slave(mk3array *it, int slave, mk3error *err);

/*
 'mk3array_ask_id'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: int
   Return the value of the 'id' of the Mk3 (requested during comminit)
*/
int mk3array_ask_id(mk3array *it, mk3error *err);

/*
 'mk3array_ask_promnb'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : char*      address of a char[9] (at least: 8 for info + '\0')
   arg 3 : mk3error*  error state of function (if any)
   return: void
 Set 'arg 2' with the value of the 'prom_nb' (requested during comminit)
*/
void mk3array_ask_promnb(mk3array *it, char *promnb, mk3error *err);

/*
 'mk3array_initparams'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: void
 Check that 'speed' and 'slave' are set then advance automaton
*/
void mk3array_initparams(mk3array *it, mk3error *err);

/*
 'mk3array_initparams_wp' ("wp": with paramaters)
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        one of 'mk3array_speed_22K' or 'mk3array_speed_44K'
   arg 3 : int        one of 'mk3_true' or 'mk3_false'
   arg 4 : mk3error*  error state of function (if any)
   return: void
 Set 'speed' and 'slave', then advance automaton (useful as it allows to 
 initialize all the required parameters in one function. Mostly used when
 one knows the settings under which the capture will be performed)
*/
void mk3array_initparams_wp(mk3array *it, int speed, int slave, mk3error *err);

/**********/ // 3) 'mk3array_status_not_capturing'

/*
  'mk3array_wait_nfnbr'
    arg 1 : mk3array* 'mk3array' handle
    arg 2 : int       network data frame number to wait for
    arg 3 : mk3error* error state of function (if any)
  Wait until 'nfnbr' (network data frame number) is seen before allowing for
  next step of capture to start. Useful when trying to synchronize master
  and slave arrays; insure the capture on all start at the same nfnbr.
  Note: next step (drop X first frames) will start at 'nfnbr + 1'
*/
void mk3array_wait_nfnbr(mk3array *it, int nfnbr, mk3error *err);

/*
 'mk3array_fix_drop_X_first_frames'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        X value (in number of samples)
   arg 3 : mk3error*  error state of function (if any)
   return: void
 'fix': Set the internal value for dropping a certain number of frames
 Note: a problem has been noted that might require this when starting capture
 Note: the 'X' value used should be a multiple of 5 (since we collect network
 frames, not data frames from the hardware), or the closest value ad
*/
void mk3array_fix_drop_X_first_frames(mk3array *it, int X, mk3error *err);

/*
 'mk3array_fix_one_sample_delay'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        one of 'mk3_true' or 'mk3_false'
   arg 3 : mk3error*  error state of function (if any)
   return: void
 'fix': Set the internal value for correcting a one sample delay between 
 odd channels and even channels
*/
void mk3array_fix_one_sample_delay(mk3array *it, int truefalse, mk3error *err);

/*
 'mk3array_display_warnings'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        one of 'mk3_true' or 'mk3_false'
   arg 3 : mk3error*  error state of function (if any)
   return: void
 'warn': Set the internal value for printing to 'stderr' when an specific
 situation is encountered (buffer overflow, lost packets)
 odd channels and even channels
*/
void mk3array_display_warnings(mk3array *it, int truefalse, mk3error *err);

/*
 'mk3array_normalize'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : FILE*      file handle for the 'mean' information
   arg 3 : FILE*      file handle for the 'gain' information
   arg 4 : mk3error*  error state of function (if any)
   return: void
 If used, this function will force a normalization procedure to be applied on 
 the captured data using data created for this particular microphone array
 by the mean/gain file creator.
*/
void mk3array_normalize(mk3array *it, FILE* mean_fd, FILE* gain_fd, mk3error *err);

/*****/

/*
 'mk3array_capture_on'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: void
 Starts data capture on the Mk3
*/
void mk3array_capture_on(mk3array *it, mk3error *err);

/**********/ // 4) 'mk3array_status_capturing'

/*
 'mk3array_wait_capture_started'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  'mk3error' handle
   return: void
 Blocking opeation: Wait for capture 'status' to be set before returning
*/
void mk3array_wait_capture_started(mk3array *it, mk3error *err);

/**********/

/* Two possiblities are available to users when accessing the data itself */

/*****/
/* 1: Using "data buffers" (see functions definition a little further down).
      Asks the library to copy some data from its internal data types to 
      a user provided "data buffer" */

/*
  'mk3array_get_databuffer'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : char*      pointer to the "data buffer" location in which 
                      to copy the data
   arg 3 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer
   arg 4 : mk3error*  error state of function (if any)
   return: void
 Get the next available databuffer (blocking)
*/
void mk3array_get_databuffer(mk3array *it, char *databuffer, struct timespec *timestamp, mk3error *err);

/*
  'mk3array_get_databuffer_with_nfnbr'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : char*      pointer to the "data buffer" location in which 
                      to copy the data
   arg 3 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : mk3error*  error state of function (if any)
   return: void
 Get the next available databuffer (blocking), and include the 
 'network data frame number' among the returned information (usualy only
 needed when trying to synchronize multiple microphone arrays together)
*/
void mk3array_get_databuffer_nfnbr(mk3array *it, char *databuffer, struct timespec *timestamp, int *nfnbr, mk3error *err);

/*
  'mk3array_get_databuffer_nonblocking'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : char*      pointer to the "data buffer" location in which 
                      to copy the data
   arg 3 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 4 : mk3error*  error state of function (if any)
   return: int        'mk3_false' if no data was read / 'mk3_true' otherwise
 Get the next available databuffer (non-blocking), and move to cursor to next.
*/
int mk3array_get_databuffer_nonblocking(mk3array *it, char *db, struct timespec *ts, mk3error *err);

/*
  'mk3array_get_databuffer_nonblocking_with_nfnbr'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : char*      pointer to the "data buffer" location in which 
                      to copy the data
   arg 3 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : mk3error*  error state of function (if any)
   return: int        'mk3_false' if no data was read / 'mk3_true' otherwise
 Get the next available databuffer (non-blocking), and move to cursor to next.
 Include the 'network data frame number' among the returned information
 (usualy only needed when trying to synchronize multiple microphone arrays 
 together)
*/
int mk3array_get_databuffer_nonblocking_with_nfnbr(mk3array *it, char *db, struct timespec *ts, int *nfnbr, mk3error *err);

/*
 'mk3array_get_current_databuffer_timestamp'
  arg 1 : mk3array*  'mk3array' handle
  arg 2 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer
  arg 3 : mk3error*  error state of function (if any)
  return: void
 Get the next available timestamp (blocking), but do not consider the
 databuffer as read (one can either 'skip' or 'get' it there after)
*/
void mk3array_get_current_databuffer_timestamp(mk3array *it, struct timespec *ts, mk3error *err);

/*
 'mk3array_get_current_databuffer_nfnbr'
  arg 1 : mk3array*  'mk3array' handle
  arg 2 : mk3error*  error state of function (if any)
  return: int        databuffer's 'network data frame number'
 Get the next available 'network data frame number' (blocking), but do not
 consider the databuffer as read (one can either 'skip' or 'get' it there
 after)
*/
int mk3array_get_current_databuffer_nfnbr(mk3array *it, mk3error *err);

/*
 'mk3array_skip_current_databuffer'
  arg 1 : mk3array*  'mk3array' handle
  arg 2 : mk3error*  error state of function (if any)
  return: void
 Consider the next available databuffer read and move cursor to next.
*/
void mk3array_skip_current_databuffer(mk3array *it, mk3error *err);

/*
 'mk3array_check_databuffer_overflow'
  arg 1 : mk3array*  'mk3array' handle
  arg 2 : mk3error*  error state of function (if any)
  return: int        'mk3_false' or 'mk3_true' dependent on overflow of 
                     internal structure
 Check if an ring buffer overflow occured for 'databuffer' access
*/
int mk3array_check_databuffer_overflow(mk3array* it, mk3error *err);

/*****/
/* 2: Using an access cursor (see "mk3cursor" functions below)
      Gives direct access to the data from the library.
      Perfect method if one does not want to do have to worry about creating
      memory buffers in which to store the data (see 'databuffers' otehrwise)
      Also, perfect method for capture clients, since the user does not have
      to worry about maintaining its own ring buffer, the library's internal
      is used.
      ***WARNING***: This is a "direct access" to the location of the data
      in memory and therefore no modification to this data should be made
      without copying it into a temporary buffer first and working from
      this buffer ('databuffers' might be more userful then, see method 1)
*/

/*
  'mk3array_get_cursorptr'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : char**     pointer to a 'char*' that will contain the address
                      of the data (ie, for 'char *ptr', use '&ptr')
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : mk3error*  error state of function (if any)
   return: void
 Get the next available data pointer from the Mk3's cursor (blocking)
*/
void mk3array_get_cursorptr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, mk3error *err);

/*
  'mk3array_get_cursorptr_with_nfnbr'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : char**     pointer to a 'char*' that will contain the address
                      of the data (ie, for 'char *ptr', use '&ptr')
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : int *      pointer to a localy created 'int' that will contain the
                      'network data frame number' of the first element
		      in the returned pointer (a 'NULL' value means do not
		      return the corresponding 'nfnbr')
   arg 6 : mk3error*  error state of function (if any)
   return: void
 Get the next available data pointer from the Mk3's cursor (blocking), and
 include the 'network data frame number' among the returned information
 (usualy only needed when trying to synchronize multiple microphone arrays
 together)
*/
void mk3array_get_cursorptr_with_nfnbr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err);

/*
  'mk3array_get_cursorptr_nonblocking'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : char**     pointer to a 'char*' that will contain the address
                      of the data (ie, for 'char *ptr', use '&ptr')
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : mk3error*  error state of function (if any)
   return: int        'mk3_false' if no data was read / 'mk3_true' otherwise
 Get the next available data pointer from the Mk3's cursor (non-blocking)
*/
int mk3array_get_cursorptr_nonblocking(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, mk3error *err);

/*
  'mk3array_get_cursorptr_nonblocking_with_nfnbr'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : char**     pointer to a 'char*' that will contain the address
                      of the data (ie, for 'char *ptr', use '&ptr')
   arg 4 : timespec*  pointer to a localy created 'timespec' that will
                      contain the timespec detailling the capture time
		      of the requested buffer (a 'NULL' value means do
		      not return the corresponding timestamp)
   arg 5 : int *      pointer to a localy created 'int' that will contain the
                      'network data frame number' of the first element
		      in the returned pointer (a 'NULL' value means do not
		      return the corresponding 'nfnbr')
   arg 6 : mk3error*  error state of function (if any)
   return: int        'mk3_false' if no data was read / 'mk3_true' otherwise
 Get the next available data pointer from the Mk3's cursor (non-blocking), and
 include the 'network data frame number' among the returned information
 (usualy only needed when trying to synchronize multiple microphone arrays
 together)
*/
int mk3array_get_cursorptr_nonblocking_with_nfnbr(mk3array *it, mk3cursor *c, char** ptr, struct timespec *ts, int *nfnbr, mk3error *err);

/* NOTE: No function to read just the timestamp or skip a buffer is given, 
   since in this mode no data is memcpy-ed, it is simple to just skip it */

/*
 'mk3array_check_cursor_overflow'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : mk3error*  error state of function (if any)
   return: int        'mk3_false' or 'mk3_true' dependent on overflow status
 Check if an ring buffer overflow occured for specified cursor
*/
int mk3array_check_cursor_overflow(mk3array *it, mk3cursor *c, mk3error *err);

/**********/

/*
 'mk3array_check_lostdataframes'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: int        total number of data frames lost so far
 Return an 'int' containing the total number of data frames lost since the
 begining of the capture (reminder: 1 network frame = 5 data frames)
*/
int mk3array_check_lostdataframes(mk3array *it, mk3error *err);

/*
 ' mk3array_check_capture_ok'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: int        'mk3_true' if capture ok, 'mk3_false' otherwise
  Check that no error are occuring in the capture process
*/
int mk3array_check_capture_ok(mk3array *it, mk3error *err);

/**********/

/*
 'mk3array_capture_off'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3error*  error state of function (if any)
   return: void
 Stops data capture on the Mk3
*/
void mk3array_capture_off(mk3array *it, mk3error *err);

/**********/ // X) after everything

/* 
 'mk3array_delete'
   arg 1 : mk3array*   'mk3array' handle
   arg 2 : mk3error*   error state of function (if any)
   return: void
 The function frees memory used by the 'mk3array'
*/
void mk3array_delete(mk3array* it, mk3error *err);




/************************************************************/
/****************************************/
/*
  Once a 'mk3array' handler has been created (with the requested
  number of data per buffer), it is possible to have the library
  automaticaly malloc/free "data buffer(s)", where 1 "data buffer"
  contains "requested number of data per buffer" elements of "1 dataframe"

   ie:
   1 "data buffer" = 'requested_data_frames_per_buffer' "data frames"
*/

/*
  'mk3array_create_databuffers'
   arg 1 : mk3array*   'mk3array' handle
   arg 2 : int         'quantity' of "data buffers" to create
   arg 3 : int*        pointer to store the total size of the data buffer
                       created ('NULL' not to use)
   arg 4 : mk3error*   error state of function (if any)
   return: char*       pointer to the data buffer
 Create 'quantity' "data buffers" 
*/
char* mk3array_create_databuffers(mk3array *it, int quantity, int *size, mk3error *err);

/*
  'mk3array_create_databuffer'
   arg 1 : mk3array*   'mk3array' handle
   arg 2 : int*        pointer to store the total size of the data buffer 
                       created ('NULL' not to use)
   arg 3 : mk3error*   error state of function (if any)
   return: char*       pointer to the data buffer
 Create 1 "data buffer"
*/
char* mk3array_create_databuffer(mk3array *it, int *size, mk3error *err);

/*
  'mk3array_delete_databuffer'
   arg 1 : mk3array*   'mk3array' handle
   arg 2 : char*       pointer of the "data buffer"(s)
   arg 3 : mk3error*   error state of function (if any)
   return: void
 Free the memory previously allocated for a "data buffer" pointer (1 or more)
*/
void mk3array_delete_databuffer(mk3array *it, char *db, mk3error *err);

/*
 'mk3array_get_one_databuffersize'
  arg 1 : mk3array*  'mk3array' handle
  arg 2 : mk3error*   error state of function (if any)
  return: int         size of 1 "data buffer"
 Returns the actual bytes size of 1x "data buffer"
*/
int mk3array_get_databuffersize(mk3array *it, mk3error *err);




/************************************************************/
/****************************************/
/*
  'mk3cursor' are access handlers for direct access to the internal storage
  methods, and are very useful if you intend to access the data location 
  directly, and/or need to use more than one access cursor.
  WARNING: in this case, you still need to make sure that the cursors 
           'requested data frame per buffer' ('rdfpb') are multiples of
	   one another, and use the highest value in the 'mk3array_create'
	   function.
  WARNING: since this method gives direct access to the internal storage area
           _extreme_ caution must be taken _not_ to modify the content.
*/  

/*
 'mk3cursor_create'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : int        'rdfpb'
   arg 3 : mk3error   error state of function (if any)
   return: mk3cursor*
 Creates a 'mk3cursor' to directly access the internal data
*/
mk3cursor *mk3cursor_create(mk3array* it, int requested_data_frames_per_buffer, mk3error* err);

/*
 'mk3cursor_delete'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : mk3error   error state of function (if any)
   return: void
 Delete a 'mk3cursor
*/
void mk3cursor_delete(mk3array* it, mk3cursor *c, mk3error *err);

/*
 'mk3cursor_get_datasize'
   arg 1 : mk3array*  'mk3array' handle
   arg 2 : mk3cursor* 'mk3cursor' handle
   arg 3 : mk3error   error state of function (if any)
   return: int
 Returns the size of one element pointed by when obtained from a cursor
*/
int mk3cursor_get_datasize(mk3array* it, mk3cursor *c, mk3error *err);





/************************************************************/
/****************************************/

/*
 'mkarray_init_mk3error'
  arg 1 : mk3error*   error state to initialize
  return: void
 Initialize a newly created 'mk3error'
*/
void mk3array_init_mk3error(mk3error *err);

/*
 'mk3array_perror'
  arg 1 : mk3error*   error state to print
  arg 2 : char*       Comment to accompany error message
  return: void
 Prints a error message corresponding to the error object given.
*/
void mk3array_perror(const mk3error *err, const char *comment);

/*
 'mk3array_check_mk3error'
  arg 1 : mk3error*   error state to check
  return: int         one 'mk3_false' if no error, or 'mk3_true' if error
 A function to allow check of error return value without having to compare
 it to 'MK3_OK'
*/
int mk3array_check_mk3error(mk3error *err);

#endif
