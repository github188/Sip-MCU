/*
   ITU-T G.729 Annex C - Reference C code for floating point
                         implementation of G.729
                         Version 1.01 of 15.September.98
*/

/*
----------------------------------------------------------------------
/*****************************************************************************/
/* bit stream manipulation routines                                          */
/*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "typedef.h"
#include "ld8a.h"
#include "tab_ld8a.h"
#include "vad.h"
#include "sid.h"
#include "dtx.h"
#include "tab_dtx.h"
#include "octet.h"

/* prototypes for local functions */
static void  int2bin(int value, int no_of_bits, INT16 *bitstream);
static int   bin2int(int no_of_bits, INT16 *bitstream);

/*----------------------------------------------------------------------------
 * prm2bits_ld8k -converts encoder parameter vector into vector of serial bits
 * bits2prm_ld8k - converts serial received bits to  encoder parameter vector
 *
 * The transmitted parameters are:
 *
 *     LPC:     1st codebook           7+1 bit
 *              2nd codebook           5+5 bit
 *
 *     1st subframe:
 *          pitch period                 8 bit
 *          parity check on 1st period   1 bit
 *          codebook index1 (positions) 13 bit
 *          codebook index2 (signs)      4 bit
 *          pitch and codebook gains   4+3 bit
 *
 *     2nd subframe:
 *          pitch period (relative)      5 bit
 *          codebook index1 (positions) 13 bit
 *          codebook index2 (signs)      4 bit
 *          pitch and codebook gains   4+3 bit
 *----------------------------------------------------------------------------
 */
void prm2bits_ld8k(
  int    prm[],           /* input : encoded parameters  (PRM_SIZE parameters)  */
  INT16 bits[]           /* output: serial bits (SERIAL_SIZE ) bits[0] = bfi
                                    bits[1] = 80 */
)
{
  int i;
  *bits++ = SYNC_WORD;     /* bit[0], at receiver this bits indicates BFI */

  switch(prm[0]){

    /* not transmitted */
  case 0 : {
    *bits = RATE_0;
    break;
  }

  case 1 : {
    *bits++ = RATE_8000;
    for (i = 0; i < PRM_SIZE; i++) {
      int2bin(prm[i+1], bitsno[i], bits);
      bits += bitsno[i];
    }
    break;
  }

  case 2 : {

#ifndef OCTET_TX_MODE
    *bits++ = RATE_SID;
    for (i = 0; i < 4; i++) {
      int2bin(prm[i+1], bitsno2[i], bits);
      bits += bitsno2[i];
    }
#else
    *bits++ = RATE_SID_OCTET;
    for (i = 0; i < 4; i++) {
      int2bin(prm[i+1], bitsno2[i], bits);
      bits += bitsno2[i];
    }
    *bits++ = BIT_0;
#endif

    break;
  }

  default : {
    printf("Unrecognized frame type\n");
    exit(-1);
  }

  }
}

/*----------------------------------------------------------------------------
 * int2bin convert integer to binary and write the bits bitstream array
 *----------------------------------------------------------------------------
 */
static void int2bin(
 int value,             /* input : decimal value */
 int no_of_bits,        /* input : number of bits to use */
 INT16 *bitstream       /* output: bitstream  */
)
{
   INT16 *pt_bitstream;
   int   i, bit;

   pt_bitstream = bitstream + no_of_bits;

   for (i = 0; i < no_of_bits; i++)
   {
     bit = value & (INT16)0x0001;      /* get lsb */
     if (bit == 0)
         *--pt_bitstream = BIT_0;
     else
         *--pt_bitstream = BIT_1;
     value >>= 1;
   }
}

/*----------------------------------------------------------------------------
 *  bits2prm_ld8k - converts serial received bits to  encoder parameter vector
 *----------------------------------------------------------------------------
 */
void bits2prm_ld8k(
 INT16 bits[],          /* input : serial bits (80)                       */
 int    prm[]            /* output: decoded parameters (11 parameters)     */
)
{
  int i;
  int nb_bits;

  nb_bits = *bits++;        /* Number of bits in this frame       */

  if(nb_bits == RATE_8000) {
    prm[1] = 1;
    for (i = 0; i < PRM_SIZE; i++) {
      prm[i+2] = bin2int(bitsno[i], bits);
      bits  += bitsno[i];
    }
  }
  else
#ifndef OCTET_TX_MODE
    if(nb_bits == RATE_SID) {
      prm[1] = 2;
      for (i = 0; i < 4; i++) {
        prm[i+2] = bin2int(bitsno2[i], bits);
        bits += bitsno2[i];
      }
    }
#else
  /* the last bit of the SID bit stream under octet mode will be discarded */
  if(nb_bits == RATE_SID_OCTET) {
    prm[1] = 2;
    for (i = 0; i < 4; i++) {
      prm[i+2] = bin2int(bitsno2[i], bits);
      bits += bitsno2[i];
    }
  }
#endif

  else {
    prm[1] = 0;
  }
}

/*----------------------------------------------------------------------------
 * bin2int - read specified bits from bit array  and convert to integer value
 *----------------------------------------------------------------------------
 */
static int bin2int(            /* output: decimal value of bit pattern */
 int no_of_bits,        /* input : number of bits to read */
 INT16 *bitstream       /* input : array containing bits */
)
{
   int   value, i;
   INT16 bit;

   value = 0;
   for (i = 0; i < no_of_bits; i++)
   {
     value <<= 1;
     bit = *bitstream++;
     if (bit == BIT_1)  value += 1;
   }
   return(value);
}

int read_frame(FILE *f_serial, int *parm)
{
  int  i;
  INT16  serial[SERIAL_SIZE];          /* Serial stream               */

  if(fread(serial, sizeof(short), 2, f_serial) != 2) {
    return(0);
  }

  if(fread(&serial[2], sizeof(INT16), (size_t)serial[1], f_serial)
     != (size_t)serial[1]) {
    return(0);
  }

  bits2prm_ld8k(&serial[1], parm);

  /* This part was modified for version V1.3 */
  /* for speech and SID frames, the hardware detects frame erasures
     by checking if all bits are set to zero */
  /* for untransmitted frames, the hardware detects frame erasures
     by testing serial[0] */

  parm[0] = 0;           /* No frame erasure */
  if(serial[1] != 0) {
   for (i=0; i < serial[1]; i++)
     if (serial[i+2] == 0 ) parm[0] = 1;  /* frame erased     */
  }
  else if(serial[0] != SYNC_WORD) parm[0] = 1;

  if(parm[1] == 1) {
    /* check parity and put 1 in parm[5] if parity error */
    parm[5] = check_parity_pitch(parm[4], parm[5]);
  }

  return(1);
}
