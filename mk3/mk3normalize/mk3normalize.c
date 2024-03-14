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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

enum {
  ccount = 64,
  bcount = 3,
  ibcount = 4,
  dfsize = ccount*bcount,
  idfsize = ccount*ibcount,
  readc = 200,
};

void equit(char *tmp)
{
  fprintf(stderr, "ERROR: %s\n", tmp);
  exit(1);
}

int main(int argc, char** argv)
{
  int i;

  char *in;
  char *out_mean;
  char *out_gain;
  int inf, outmf, outsf;

  int nbe = 0; // nb elements
  char *tbuffer = (char *) calloc(readc, dfsize);
  
  int keepreading;
  int smean[ccount];
  double mean[ccount];
  double stddev[ccount];
  double gain[ccount];
  double mean_stddev = 0.0;

  if (argc != 4) equit("program inputfile.raw meanfile gainfile");

  in = argv[1];
  out_mean  = argv[2];
  out_gain = argv[3];

  inf = open(in, O_RDONLY);
  if (inf < 0) equit("Could not open input file, aborting");
  {
    struct stat buf;
    if (stat(in, &buf) != 0)
      equit("could not get info about input file, aborting");
    if (buf.st_size % dfsize != 0)
      equit("file size is not a multiple of the mk3 capture size, aborting");
    nbe = buf.st_size / dfsize;
  }

  outmf = open(out_mean, O_RDWR|O_CREAT|O_TRUNC, 0666);
  if (outmf < 0) equit("Could not open mean output file, aborting");
  outsf = open(out_gain, O_RDWR|O_CREAT|O_TRUNC, 0666);
  if (outsf < 0) equit("Could not open gain output file, aborting");

  {
    for (i=0; i<ccount; i++) {
      mean[i]  = 0.0;
      stddev[i] = 0.0;
      gain[i] = 0.0;
    }
  }

  /**********/
  // Pass 1: calculate the mean value
  printf("Pass 1: Calculating the mean value\n");
  keepreading = 1;
  while (keepreading) {
    int toread = dfsize * readc;
    int aread = 0;

    int dsf = 0;
    unsigned char *ptr = tbuffer;

    aread = read(inf, tbuffer, toread);
    if (aread != toread) keepreading = 0;

    while (dsf < aread) {
      for (i=0; i<ccount; i++) {
	// cma 24 to intel 32
	int buffer = ptr[2] | (ptr[1] << 8)  | ((signed char) ptr[0] << 16);
	// Mean sum
	mean[i] += (double) buffer / (double) nbe;
	// advance in data
	ptr += 3;
      }
      
      dsf += dfsize;
    }
  }
  // Finalize mean
  for (i=0;i<ccount;i++) smean[i] = (int) mean[i];

  // Go back to the beginning of the file
  if (lseek(inf, 0, SEEK_SET) != 0)
    equit("Could not rewind infile, aborting");

  /**********/
  // Pass 2: calculate the gain factor
  printf("Pass 2: Calculating the gain factor\n");
  keepreading = 1;
  while (keepreading) {
    int toread = dfsize * readc;
    int aread = 0;

    int dsf = 0;
    unsigned char *ptr = tbuffer;

    aread = read(inf, tbuffer, toread);
    if (aread != toread) keepreading = 0;

    while (dsf < aread) {
      double tmp;

      for (i=0; i<ccount; i++) {
	// cma 24 to intel 32
	int buffer = ptr[2] | (ptr[1] << 8)  | ((signed char) ptr[0] << 16);
	tmp = (((double) buffer) - mean[i]);
	stddev[i] += (tmp * tmp) / (double) (nbe - 1);
	// advance in data
	ptr += 3;
      }
      
      dsf += dfsize;
    }
  }
  // Finalize stddev
  for (i=0; i<ccount; i++) stddev[i] = sqrt(stddev[i]);
  // Mean stddev
  for (i=0; i<ccount; i++) mean_stddev += stddev[i];
  mean_stddev = mean_stddev / (double) ccount;
  // Gain factor
  for (i=0; i<ccount; i++) gain[i] = stddev[i] / mean_stddev;

  // Step 3: Writing to disk
  printf("Writing to disk\n");
  write(outmf, smean, ccount * sizeof(int));
  close(outmf);
  write(outsf, gain, ccount * sizeof(double));
  close(outsf);

  // Ok
  printf("Exiting with success\n");
  exit(0);
}
