/*************************************************************************
**
**   ITU-T G.722.1 (2005-05) - Fixed point implementation for main body and Annex C
**   > Software Release 2.1 (2008-06)
**     (Simple repackaging; no change from 2005-05 Release 2.0 code)
**
**   © 2004 Polycom, Inc.
**            
**   All rights reserved.
**
*************************************************************************/

/*************************************************************************
 Filename:     encode.c    

 Description:  Contains the main function for the G.722.1 Annex C encoder

 Design Notes:
 
 WMOPS:     7kHz |    24kbit    |     32kbit
          -------|--------------|----------------
            AVG  |    2.32      |     2.43
          -------|--------------|----------------  
            MAX  |    2.59      |     2.67
          -------|--------------|---------------- 
                                                                        
           14kHz |    24kbit    |     32kbit     |     48kbit
          -------|--------------|----------------|----------------
            AVG  |    4.45      |     4.78       |     5.07   
          -------|--------------|----------------|----------------
            MAX  |    5.07      |     5.37       |     5.59   
          -------|--------------|----------------|----------------

*************************************************************************/

/************************************************************************************
 Include files                                                           
*************************************************************************************/
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
    FILE    *fpin;
    FILE    *fp_bitstream;
} ENCODER_CONTROL;

/************************************************************************************
 Constant definitions                                                    
*************************************************************************************/
#define MAX_SAMPLE_RATE 32000
#define MAX_FRAMESIZE   (MAX_SAMPLE_RATE/50)
#define MEASURE_WMOPS   1
#define WMOPS           1

/***************************************************************************
 Local function declarations                                             
***************************************************************************/
void parse_command_line(char *argv[],ENCODER_CONTROL *control);
void write_ITU_format(Word16 *out_words,
                      Word16 number_of_bits_per_frame,
                      Word16 number_of_16bit_words_per_frame,
                      FILE   *fp_bitstream);

/************************************************************************************
 Function:     G.722.1 Annex C main encoder function 

 Syntax:       encode <data format> <input file> <output file> <bit rate> <bandwidth>

                      <data format> - 0 for packed format/1 for ITU format
                      <input file>  - input audio file to be encoded
                      <output file> - encoded bitstream of the input file
                      <bit rate>    - 24, 32, and 48 kbps
                      <bandwidth>   - 7 or 14 kHz
                        
 Description:  Main processing loop for the G.722.1 Annex C encoder

 Design Notes: 16kbit/sec is also supported for bandwidth of 7kHz

*************************************************************************************/
main(Word16 argc,char *argv[])
{
    Word16  samples;
    Word16  input[MAX_FRAMESIZE];
    Word16  history[MAX_FRAMESIZE];
    Word16  number_of_16bit_words_per_frame;
    Word16  out_words[MAX_BITS_PER_FRAME/16];
    Word16  mag_shift;
    Word16  i;
    Word16  mlt_coefs[MAX_FRAMESIZE];
    Word16  frame_cnt = 0;
    ENCODER_CONTROL control;

    /* Check usage */
    if (argc < 6)
    {
        printf("Usage: encode  <0(packed)/1> <input-audio-file> <output-bitstream-file> <bit-rate> <bandwidth>\n\n");
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
    number_of_16bit_words_per_frame = (Word16)(control.number_of_bits_per_frame/16);

    /* initialize the mlt history buffer */
    for (i=0;i<control.frame_size;i++)
        history[i] = 0;

    /* Read first frame of samples from disk. */
    samples = (Word16 )fread(input,2,control.frame_size,control.fpin);

    Init_WMOPS_counter ();
    while (samples == control.frame_size)
    {

#if MEASURE_WMOPS
        Reset_WMOPS_counter ();
#endif
        frame_cnt++;

        /* Convert input samples to rmlt coefs */
        mag_shift = samples_to_rmlt_coefs(input, history, mlt_coefs, control.frame_size);


        /* Encode the mlt coefs */
        encoder(control.number_of_bits_per_frame,
                control.number_of_regions,
                mlt_coefs,
                mag_shift,
                out_words);

        /* Write output bitstream to the output file */
        if (control.syntax == 0)
            fwrite(out_words, 2, number_of_16bit_words_per_frame,control.fp_bitstream);
        else
            write_ITU_format(out_words,
                             control.number_of_bits_per_frame,
                             number_of_16bit_words_per_frame,
                             control.fp_bitstream);

        /* Read frame of samples from disk. */
        samples = (Word16 )fread(input,2,control.frame_size,control.fpin);
    }

#if MEASURE_WMOPS
        WMOPS_output (0);
#endif                
    fclose(control.fpin);
    fclose(control.fp_bitstream);
    
    return 0;
}

/************************************************************************************
 Procedure/Function:     Parse command line 

 Syntax:        void parse_command_line(char *argv[],ENCODER_CONTROL *control)
                    
                    char *argv[]              - contains the command line arguments
                    ENCODER_CONRTROL *control - contains the processed arguments

 Description:  parses the command line inputs

************************************************************************************/

void parse_command_line(char *argv[],ENCODER_CONTROL *control)    
{
    control->syntax = (Word16)(atoi(*++argv));
    if ((control->syntax != 0) && (control->syntax != 1))
    {
        printf("syntax must be 0 for packed or 1 for ITU format\n");
        exit(1);
    }

    if ((control->fpin = fopen(*++argv,"rb")) == NULL)
    {
        printf("codec: error opening %s.\n",*argv);
        exit(1);
    }
    if ((control->fp_bitstream = fopen(*++argv,"wb")) == NULL)
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

    control->number_of_bits_per_frame = (Word16)((control->bit_rate)/50);

    control->bandwidth = (Word16)(atoi(*++argv));
    if (control->bandwidth == 7000)
    {
        control->number_of_regions = NUMBER_OF_REGIONS;
        control->frame_size = MAX_FRAMESIZE >> 1;

        printf("\n*************** G.722.1 ENCODER ***************\n");
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

        printf("\n*************** G.722.1 EXTENSION ENCODER ***************\n");
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

/************************************************************************************
 Procedure/Function:     Write ITU format function 

 Syntax:                void write_ITU_format(Word16 *out_words,
                                              Word16 number_of_bits_per_frame,
                                              Word16 number_of_16bit_words_per_frame,
                                              FILE   *fp_bitstream)

 Description:           Writes file output in PACKED ITU format


************************************************************************************/

void write_ITU_format(Word16 *out_words,
                      Word16 number_of_bits_per_frame,
                      Word16 number_of_16bit_words_per_frame,
                      FILE   *fp_bitstream)

{
    Word16 frame_start = 0x6b21;
    Word16 one = 0x0081;
    Word16 zero = 0x007f;

    Word16  i,j;
    Word16  packed_word;
    Word16  bit_count;
    Word16  bit;
    Word16  out_array[MAX_BITS_PER_FRAME+2];

    j = 0;
    out_array[j++] = frame_start;
    out_array[j++] = number_of_bits_per_frame;

    for (i=0; i<number_of_16bit_words_per_frame; i++)
    {
        packed_word = out_words[i];
        bit_count = 15;
        while (bit_count >= 0)
        {
            bit = (Word16)((packed_word >> bit_count) & 1);
            bit_count--;
            if (bit == 0)
                out_array[j++] = zero;
            else
                out_array[j++] = one;
        }
    }

    fwrite(out_array, 2, number_of_bits_per_frame+2, fp_bitstream);
}

