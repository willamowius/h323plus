/***************************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**
**   All rights reserved.
**
***************************************************************************/

/***************************************************************************
  Filename:    decode.c    

  Purpose:     Contains the main function for the G.722.1 Annex C decoder
		
  Design Notes:

  WMOPS:     7kHz |    24kbit    |     32kbit
           -------|--------------|----------------
             AVG  |     2.73     |     2.85
           -------|--------------|----------------  
             MAX  |     2.79     |     2.88
           -------|--------------|---------------- 

            14kHz |    24kbit    |     32kbit     |     48kbit
           -------|--------------|----------------|----------------
             AVG  |     5.28     |     5.53       |     5.85   
           -------|--------------|----------------|----------------
             MAX  |     5.56     |     5.77       |     5.95   
           -------|--------------|----------------|----------------

***************************************************************************/
/***************************************************************************
 Include files                                                           
***************************************************************************/
#include "defs.h"
#include "count.h"

/************************************************************************************
 Local type declarations                                             
*************************************************************************************/
/* This object is used to control the command line input */
typedef struct 
{
    Word16  syntax;
    Word32  bit_rate;
    Word16  bandwidth;
    Word16  number_of_bits_per_frame;
    Word16  number_of_regions;
    Word16  frame_size;
    FILE    *fpout;
    FILE    *fp_bitstream;
} DECODER_CONTROL;

/************************************************************************************
 Constant definitions                                                    
*************************************************************************************/
#define MAX_SAMPLE_RATE     32000
#define MAX_FRAMESIZE   (MAX_SAMPLE_RATE/50)
#define MEASURE_WMOPS   1
#define WMOPS           1

/***************************************************************************
 Local function declarations                                             
***************************************************************************/
void parse_command_line(char *argv[],DECODER_CONTROL *control);
Word16 read_ITU_format(Word16 *, Word16 *, Word16, FILE *);


/************************************************************************************
 Function:     G722.1 Annex C main decoder function 

 Syntax:       decode <data format> <input file> <output file> <bit rate> <bandwidth>

                      <data format> - 0 for packed format/1 for ITU format
                      <input file>  - encoded bitstream file
                      <output file> - output audio file
                      <bit rate>    - 24, 32, and 48 kbps
                      <bandwidth>   - 7 or 14 kHz

 Description:  Main processing loop for the G.722.1 Annex C decoder

 Design Notes: 16kbit/sec is also supported for bandwidth of 7kHz
				
************************************************************************************/

main(Word16 argc,char *argv[])
{
    
    Word16 i;
    Word16 words;
    Word16 output[MAX_DCT_LENGTH];
    Word16 number_of_16bit_words_per_frame;
    Word16 out_words[MAX_BITS_PER_FRAME/16];
    Word16 frame_error_flag = 0;
    Word16 frame_cnt = 0;
    Word16 decoder_mlt_coefs[MAX_DCT_LENGTH];
    Word16 mag_shift;
    Word16 old_mag_shift=0;
    Word16 old_decoder_mlt_coefs[MAX_DCT_LENGTH];
    Word16 old_samples[MAX_DCT_LENGTH>>1];
    Bit_Obj bitobj;
    Rand_Obj randobj;
    DECODER_CONTROL control;

    if (argc < 6)
    {
        printf("Usage: decode  <0(packed)/1> <input-bitstream-file> <output-audio-file> <bit-rate> <bandwidth>\n\n");
        printf("Valid Rates: 48kbps = 48000\n");
        printf("             32kbps = 32000\n");
        printf("             24kbps = 24000\n");
        printf("\n");
        printf("Bandwidth:    7kHz  =  7000\n");
        printf("             14kHz  = 14000\n");
        printf("\n");
        exit(1);
    }
 	
    /* parse the command line input into the control structure */
    parse_command_line(argv,&control);
    number_of_16bit_words_per_frame = (Word16)((control.number_of_bits_per_frame)/16);
    
    /* initialize the coefs history */
    for (i=0;i<control.frame_size;i++)
        old_decoder_mlt_coefs[i] = 0;    

    for (i = 0;i < (control.frame_size >> 1);i++)
  	    old_samples[i] = 0;
    
    /* initialize the random number generator */
    randobj.seed0 = 1;
    randobj.seed1 = 1;
    randobj.seed2 = 1;
    randobj.seed3 = 1;

    /* Read first frame of samples from disk. */
    if (control.syntax == 0)
        words = (Word16 )fread(out_words, 2, number_of_16bit_words_per_frame, control.fp_bitstream);
    else
        words = read_ITU_format(out_words,
                                &frame_error_flag,
    		                    number_of_16bit_words_per_frame,
    		                    control.fp_bitstream);
    
#if MEASURE_WMOPS
    Init_WMOPS_counter ();
#endif

    while(words == number_of_16bit_words_per_frame) 
    {
        frame_cnt++;

        /* reinit the current word to point to the start of the buffer */
        bitobj.code_word_ptr = out_words;
        bitobj.current_word =  *out_words;
        bitobj.code_bit_count = 0;
        bitobj.number_of_bits_left = control.number_of_bits_per_frame;
        
#if MEASURE_WMOPS
        Reset_WMOPS_counter ();
#endif
        /* process the out_words into decoder_mlt_coefs */
        decoder(&bitobj,
                &randobj,
                control.number_of_regions,
                decoder_mlt_coefs,
                &mag_shift,
                &old_mag_shift,
                old_decoder_mlt_coefs,
                frame_error_flag);

        
        
        /* convert the decoder_mlt_coefs to samples */
        rmlt_coefs_to_samples(decoder_mlt_coefs, old_samples, output, control.frame_size, mag_shift);

        /* For ITU testing, off the 2 lsbs. */
        for (i=0; i<control.frame_size; i++)
            output[i] &= 0xfffc;
            
        /* Write frame of output samples */
        fwrite(output, 2, control.frame_size, control.fpout);

        /* Read next frame of samples from disk. */
        if (control.syntax == 0)
	        words = (Word16 )fread(out_words, 2, number_of_16bit_words_per_frame, control.fp_bitstream);
        else
	        words = read_ITU_format(out_words,
			                        &frame_error_flag,
			                        number_of_16bit_words_per_frame,
			                        control.fp_bitstream);

    }

#if MEASURE_WMOPS
    WMOPS_output (0);
#endif        
    
    fclose(control.fpout);
    fclose(control.fp_bitstream);

    return 0;        
}
/************************************************************************************
 Procedure/Function:     Parse command line 

 Syntax:        void parse_command_line(char *argv[],ENCODER_CONTROL *control)
                    
                    char *argv[]              - contains the command line arguments
                    DECODER_CONRTROL *control - contains the processed arguments

 Description:  parses the command line inputs

************************************************************************************/

void parse_command_line(char *argv[],DECODER_CONTROL *control)    
{
    control->syntax = (Word16 )(atoi(*++argv));
    if ((control->syntax != 0) && (control->syntax != 1))
    {
        printf("syntax must be 0 for packed or 1 for ITU format\n");
        exit(1);
    }

    if((control->fp_bitstream = fopen(*++argv,"rb")) == NULL)
    {
        printf("codec: error opening %s.\n",*argv);
        exit(1);
    }
    if((control->fpout = fopen(*++argv,"wb")) == NULL)
    {
        printf("codec: error opening %s.\n",*argv);
        exit(1);
    }
    
    control->bit_rate = (Word32)(atoi(*++argv));
    if ((control->bit_rate < 16000) || (control->bit_rate > 48000) ||
        ((control->bit_rate/800)*800 != control->bit_rate))
    {
        printf("codec: Error. bit-rate must be multiple of 800 between 16000 and 48000\n");
        exit(1);
    }

    control->number_of_bits_per_frame = (Word16 )((control->bit_rate)/50);

    control->bandwidth = (Word16)(atoi(*++argv));
    if (control->bandwidth == 7000)
    {
        control->number_of_regions = NUMBER_OF_REGIONS;
        control->frame_size = MAX_FRAMESIZE >> 1;

        printf("\n*************** G.722.1 DECODER ***************\n");
        printf("bandwidth = 7 khz\n");
        printf("syntax = %d ",control->syntax);

        if (control->syntax == 0)
            printf(" packed bitstream\n");
        else if (control->syntax == 1)
            printf(" ITU-format bitstream\n");

        printf("sample_rate = 16000    bit_rate = %d\n",control->bit_rate);
        printf("framesize = %d samples\n",control->frame_size);
        printf("number_of_regions = %d\n",control->number_of_regions);
        printf("number_of_bits_per_frame = %d bits\n",control->number_of_bits_per_frame);
        printf("\n");
        printf("\n");
    }
    else if (control->bandwidth == 14000)
    {
        control->number_of_regions = MAX_NUMBER_OF_REGIONS;
        control->frame_size = MAX_FRAMESIZE;

        printf("\n*************** G.722.1 EXTENSION DECODER ***************\n");
        printf("bandwidth = 14 khz\n");
        printf("syntax = %d ",control->syntax);

        if (control->syntax == 0)
            printf(" packed bitstream\n");
        else if (control->syntax == 1)
            printf(" ITU-format bitstream\n");

        printf("sample_rate = 32000    bit_rate = %d\n",control->bit_rate);
        printf("framesize = %d samples\n",control->frame_size);
        printf("number_of_regions = %d\n",control->number_of_regions);
        printf("number_of_bits_per_frame = %d bits\n",control->number_of_bits_per_frame);
        printf("\n");
        printf("\n");
    }
    else
    {
        printf("codec: Error. bandwidth must be 7000 or 14000\n");
        exit(1);
    }

}

/***************************************************************************
 Procedure/Function:     Read ITU format function 

 Syntax:

 Description:  reads file input in ITU format

 Design Notes:
				
				
***************************************************************************/

Word16 read_ITU_format(Word16 *out_words,
                       Word16 *p_frame_error_flag,
                       Word16 number_of_16bit_words_per_frame,
		               FILE   *fp_bitstream)
{
    Word16 i,j;
    Word16 nsamp;
    Word16 packed_word;
    Word16 bit_count;
    Word16 bit;
    Word16 in_array[MAX_BITS_PER_FRAME+2];
    Word16 one = 0x0081;
    Word16 zero = 0x007f;
    Word16 frame_start = 0x6b21;

    nsamp = (Word16 )fread(in_array, 2, 2 + 16*number_of_16bit_words_per_frame, fp_bitstream);

    j = 0;
    bit = in_array[j++];
    if (bit != frame_start) 
    {
        *p_frame_error_flag = 1;
    }
    else 
    {
        *p_frame_error_flag = 0;
        
        /* increment j to skip over the number of bits in frame */
        j++;

        for (i=0; i<number_of_16bit_words_per_frame; i++) 
        {
            packed_word = 0;
            bit_count = 15;
            while (bit_count >= 0)
            {
    	        bit = in_array[j++];
    	        if (bit == zero) 
    	            bit = 0;
    	        else if (bit == one) 
    	            bit = 1;
    	        else 
                {
    	            *p_frame_error_flag = 1;
                    /*	  printf("read_ITU_format: bit not zero or one: %4x\n",bit); */
	            }
	            packed_word <<= 1;
	            packed_word = (Word16 )(packed_word + bit);
	            bit_count--;
            }
            out_words[i] = packed_word;
        }
    }
    return((Word16)((nsamp-1)/16));
}

