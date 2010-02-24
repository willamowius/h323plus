
/*
 * sbccodec.h
 *
 * Header file for SBC conversion routines.
 *
 */
#ifndef _SBCCODEC_H
#define	_SBCCODEC_H

#define	AUDIO_ENCODING_LINEAR	(3)	/* PCM 2's-complement (0-center) */

/*
 * The following is the definition of the state structure
 * used by the SBC encoder and decoder to preserve their internal
 * state between successive calls. 
 */

typedef struct sbc_state_s {
	long yl;	/* Locked or steady state step size multiplier. */
} sbc_state;

/* External function definitions. */

void sbc_init_state( sbc_state *);

int sbc_encoder(
		int sample,
		int in_coding,
		 sbc_state *state_ptr);
int sbc_decoder(
		int code,
		int out_coding,
		 sbc_state *state_ptr);

#endif /* !_SBCCODEC_H */

