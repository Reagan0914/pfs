/*******************************************************************************
*  program pfs_fft
*  $Id$
*  This programs performs spectral analysis on data acquired with
*  the portable fast sampler
*
*  usage:
*  	pfs_fft -m mode 
*               -f sampling frequency (MHz)
*              [-r desired frequency resolution (Hz)]  
*              [-d downsampling factor] 
*              [-n sum n transforms] 
*              [-l (dB output)]
*              [-t time series] 
*              [-x freqmin,freqmax (Hz)]
*              [-s scale to sigmas using smin,smax (Hz)]
*              [-c channel] 
*              [-o outfile] [infile]
*
*  input:
*       the input parameters are typed in as command line arguments
*       the -f argument specifies the data taking sampling frequency in MHz
*	the -d argument specifies the factor by which to downsample the data
*	                (coherent sum before fft simulates lower sampling fr)
*	the -r argument specifies the desired frequency resolution in Hz
*       the -n argument specifies how many transforms to add
*			(incoherent sum after fft)
*       the -l argument specifies logarithmic (dB) output
*	the -t option indicates that (sums of) transforms ought to be written
*			one after the other until EOF
*       the -x option specifies an optional range of output frequencies
*       the -c argument specifies which channel (1 or 2) to process
*
*  output:
*	the -o option identifies the output file, stdout is default
*
*  Jean-Luc Margot, Aug 2000
*******************************************************************************/

/* 
   $Log$
   Revision 3.3  2003/09/18 19:51:43  cvs
   Abort if input file cannot be opened.

   Revision 3.2  2003/09/18 02:52:44  cvs
   Improved command-line option error handling.

   Revision 3.1  2003/02/25 22:45:34  cvs
   Sizes of fftinbuf and fftoutbuf must be identical.

   Revision 3.0  2003/02/22 07:48:08  cvs
   Switch to Joseph Jao's byte unpacking.

   Revision 1.12  2002/12/28 07:42:59  cvs
   Added capability to swap I and Q prior to FFT.

   Revision 1.11  2002/06/05 16:55:10  cvs
   Corrected one error message.

   Revision 1.10  2002/05/12 13:36:02  cvs
   Cosmetic changes.

   Revision 1.9  2002/05/02 05:48:59  cvs
   Mode added to usage line

   Revision 1.8  2002/04/27 20:22:47  margot
   Removed obsolete routines specific to Golevka Sampling Box.

   Revision 1.7  2002/04/27 06:16:26  margot
   Added mode 16.

   Revision 1.6  2001/07/06 19:48:11  margot
   Added -s option for scaling to sigmas.

   Revision 1.5  2001/07/06 04:25:28  margot
   Added mode 32 for "unpacking" of 4 byte floating point quantities.

   Revision 1.4  2001/07/04 00:14:57  margot
   Added mode 8 for unpacking signed bytes.

   Revision 1.3  2000/10/30 05:38:44  margot
   Added variable nsamples.

   Revision 1.2  2000/09/15 22:10:38  margot
   Added -l and -x options.

   Revision 1.1  2000/09/15 19:36:37  margot
   Initial revision
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/fcntl.h>
#include <unistd.h>
#include "unpack.h"
#include <fftw.h>

/* revision control variable */
static char const rcsid[] = 
"$Id$";

FILE   *fpoutput;		/* pointer to output file */
int	fdinput;		/* file descriptor for input file */

char   *outfile;		/* output file name */
char   *infile;		        /* input file name */

char	command_line[200];	/* command line assembled by processargs */

void processargs();
void open_file();
void copy_cmd_line();
void vector_power(float *data, int len);
void swap_freq(float *data, int len);
void swap_iandq(float *data, int len);
void zerofill(float *data, int len);
int  no_comma_in_string();	

int main(int argc, char *argv[])
{
  int mode;
  int bufsize;		/* size of read buffer */
  char *buffer;		/* buffer for packed data */
  char *rcp;		/* buffer for unpacked data */
  float smpwd;		/* # of single pol complex samples in a 4 byte word */
  int nsamples;		/* # of complex samples in each buffer */
  int levels;		/* # of levels for given quantization mode */

  float *fftinbuf, *fftoutbuf;
  float *total;

  float freq;		/* frequency */
  float freqmin;	/* min frequency to output */
  float freqmax;	/* max frequency to output */
  float rmsmin;		/* min frequency for rms calculation */
  float rmsmax;		/* max frequency for rms calculation */
  double fsamp;		/* sampling frequency, MHz */
  double freqres;	/* frequency resolution, Hz */
  double mean = 0;	/* needed for rms computation */
  double var = 0;	/* needed for rms computation */
  double sigma = 1;	/* needed for rms computation */
  int downsample;	/* downsampling factor, dimensionless */
  int sum;		/* number of transforms to add, dimensionless */
  int timeseries;	/* process as time series, boolean */
  int dB;		/* write out results in dB */
  int fftlen;		/* transform length, complex samples */
  int chan;		/* channel to process (1 or 2) for dual pol data */
  int counter=0;	/* keeps track of number of transforms written */
  int open_flags;	/* flags required for open() call */
  int invert;		/* swap i and q before fft routine */
  int swap = 1;		/* swap frequencies at output of fft routine */

  fftw_plan p;
  int i,j,k,l,n;
  short x;

  /* get the command line arguments */
  processargs(argc,argv,&infile,&outfile,&mode,&fsamp,&freqres,&downsample,&sum,&timeseries,&chan,&freqmin,&freqmax,&rmsmin,&rmsmax,&dB,&invert);

  /* save the command line */
  copy_cmd_line(argc,argv,command_line);

  /* open output file, stdout default */
  open_file(outfile,&fpoutput);

  /* open file input */
#ifdef LARGEFILE
  open_flags = O_RDONLY|O_LARGEFILE;
#else
  open_flags = O_RDONLY;
#endif
  if((fdinput = open(infile, open_flags)) < 0 )
    {
      perror("open input file");
      exit(1);
    }

  switch (mode)
    {
    case  -1: smpwd = 8; break;  
    case   1: smpwd = 8; break;
    case   2: smpwd = 4; break;
    case   3: smpwd = 2; break; 
    case   5: smpwd = 4; break;
    case   6: smpwd = 2; break;
    case   8: smpwd = 2; break; 
    case  16: smpwd = 1; break; 
    case  32: smpwd = 0.5; break; 
    default: fprintf(stderr,"Invalid mode\n"); exit(1);
    }

  /* compute transform parameters */
  fftlen = (int) rint(fsamp / freqres * 1e6);
  bufsize = fftlen * 4 / smpwd; 
  fftlen = fftlen / downsample;

  /* compute fft plan and allocate storage */
  p = fftw_create_plan(fftlen, FFTW_FORWARD, FFTW_ESTIMATE);

  fprintf(stderr,"\n%s\n\n",command_line);
  fprintf(stderr,"FFT length                     : %d\n",fftlen);
  fprintf(stderr,"Frequency resolution           : %e Hz\n",freqres);
  fprintf(stderr,"Processed bandwidth            : %e Hz\n\n",freqres*fftlen);

  fprintf(stderr,"Data required for one transform: %d bytes\n",bufsize);
  fprintf(stderr,"Number of transforms to add    : %d\n",sum);
  fprintf(stderr,"Data required for one sum      : %d bytes\n",sum * bufsize);
  fprintf(stderr,"Integration time for one sum   : %e s\n",sum / freqres);
  fprintf(stderr,"\n");
    
  /* allocate storage */
  nsamples = bufsize * smpwd / 4;
  buffer    = (char *)  malloc(bufsize);
  fftinbuf  = (float *) malloc(2 * fftlen * sizeof(float));
  fftoutbuf = (float *) malloc(2 * fftlen * sizeof(float));
  total = (float *) malloc(fftlen * sizeof(float));
  rcp   = (char *)  malloc(2 * nsamples * sizeof(char));
  if (!buffer || !fftinbuf || !fftoutbuf || !total || !rcp)
    {
      fprintf(stderr,"Malloc error\n"); 
      exit(1);
    }

  /* label used if time series is requested */
 loop:

  /* sum transforms */
  zerofill(total, fftlen);
  for (i = 0; i < sum; i++)
    {
      /* initialize fft array to zero */
      zerofill(fftinbuf, 2 * fftlen);
      
      /* read one data buffer       */
      if (bufsize != read(fdinput, buffer, bufsize))
	{
	  fprintf(stderr,"Read error or EOF %d\n");
	  if (timeseries) fprintf(stderr,"Wrote %d transforms\n",counter);
	  exit(1);
	}

      /* unpack */
      switch (mode)
	{
	case 1:
	  unpack_pfs_2c2b(buffer, rcp, bufsize); 
	  break;
	case 2: 
	  unpack_pfs_2c4b(buffer, rcp, bufsize);
	  break;
	case 3: 
	  unpack_pfs_2c8b(buffer, rcp, bufsize);
	  break;
	case 5:
	  if (chan == 2) unpack_pfs_4c2b_lcp (buffer, rcp, bufsize);
	  else 		 unpack_pfs_4c2b_rcp (buffer, rcp, bufsize);
	  break;
	case 6: 
	  if (chan == 2) unpack_pfs_4c4b_lcp (buffer, rcp, bufsize);
	  else 		 unpack_pfs_4c4b_rcp (buffer, rcp, bufsize);
	  break;
     	case 8: 
	  memcpy (rcp, buffer, bufsize);
	  break;
	case 16: 
	  for (i = 0, j = 0; i < bufsize; i+=sizeof(short), j++)
	    {
	      memcpy(&x,&buffer[i],sizeof(short));
	      fftinbuf[j] = (float) x;
	    }
	  break;
     	case 32: 
	  memcpy(fftinbuf,buffer,bufsize);
	  break;
	default: 
	  fprintf(stderr,"Mode not implemented yet\n"); 
	  exit(-1);
	}

      /* downsample */
      if (mode != 16 && mode != 32)
	for (k = 0, l = 0; k < 2*fftlen; k += 2, l += 2*downsample)
	  {
	    for (j = 0; j < 2*downsample; j+=2)
	      {
		fftinbuf[k]   += (float) rcp[l+j];
		fftinbuf[k+1] += (float) rcp[l+j+1];
	      }
	  }

      /* transform, swap, and compute power */
      if (invert) swap_iandq(fftinbuf,fftlen); 
      fftw_one(p, (fftw_complex *)fftinbuf, (fftw_complex *)fftoutbuf);
      if (swap) swap_freq(fftoutbuf,fftlen); 
      vector_power(fftoutbuf,fftlen);
      
      /* sum transforms */
      for (j = 0; j < fftlen; j++)
	total[j] += fftoutbuf[j];
    }
  
  /* set DC to average of neighboring values  */
  total[fftlen/2] = (total[fftlen/2-1]+total[fftlen/2+1]) / 2; 
  
  /* compute rms if needed */
  if (rmsmin != 0 || rmsmax != 0)
    {
      mean = var = 0;
      n = 0;
      /* this would be cool except that we never check the bounds */
      /* imin = fftlen/2 + rmsmin/freqres; */
      /* imax = fftlen/2 + rmsmax/freqres; */
      for (i = 0; i < fftlen; i++)
	{
	  freq = (i-fftlen/2)*freqres;
	  if (freq >= rmsmin && freq <= rmsmax)
	    {
	      mean += total[i];
	      var  += total[i] * total[i];
	      n++;
	    } 
	}
      mean  = mean / n;
      var   = var / n;
      sigma = sqrt(var - mean * mean);
    }
  
  /* write output */
  /* either time series */
  if (timeseries)
    {
      for (i = 0; i < fftlen; i++) total[i] = (total[i]-mean)/sigma;
      if (fftlen != fwrite(total,sizeof(float),fftlen,fpoutput))
	fprintf(stderr,"Write error\n");
      fflush(fpoutput);
      counter++;
      goto loop;
    }
  /* or standard output */
  /* or limited frequency range */
  else
    for (i = 0; i < fftlen; i++)
      {
	  freq = (i-fftlen/2)*freqres;
    
	  if ((freqmin == 0.0 && freqmax == 0.0) || (freq >= freqmin && freq <= freqmax)) 
	  {
	    if (dB)
	      fprintf(fpoutput,"%.3f %f\n",freq,10*log10((total[i]-mean)/sigma));
	    else
	      fprintf(fpoutput,"%.3f %.1f\n",freq,(total[i]-mean)/sigma);  
	  }
      }
  
  return 0;
}

/******************************************************************************/
/*	processargs							      */
/******************************************************************************/
void	processargs(argc,argv,infile,outfile,mode,fsamp,freqres,downsample,sum,timeseries,chan,freqmin,freqmax,rmsmin,rmsmax,dB,invert)
int	argc;
char	**argv;			 /* command line arguements */
char	**infile;		 /* input file name */
char	**outfile;		 /* output file name */
int     *mode;
double   *fsamp;
double   *freqres;
int     *downsample;
int     *sum;
int     *timeseries;
int     *chan;
float   *freqmin;
float   *freqmax;
float   *rmsmin;
float   *rmsmax;
int     *dB;
int     *invert;
{
  /* function to process a programs input command line.
     This is a template which has been customised for the pfs_fft program:
	- the outfile name is set from the -o option
	- the infile name is set from the 1st unoptioned argument
  */

  int getopt();		/* c lib function returns next opt*/ 
  extern char *optarg; 	/* if arg with option, this pts to it*/
  extern int optind;	/* after call, ind into argv for next*/
  extern int opterr;    /* if 0, getopt won't output err mesg*/

  char *myoptions = "m:f:d:r:n:tc:o:lx:s:i"; /* options to search for :=> argument*/
  char *USAGE1="pfs_fft -m mode -f sampling frequency (MHz) [-r desired frequency resolution (Hz)] [-d downsampling factor] [-n sum n transforms] [-l (dB output)] [-t time series] [-x freqmin,freqmax (Hz)] [-s scale to sigmas using smin,smax (Hz)] [-c channel (1 or 2)] [-i swap IQ before transform (invert freq axis)] [-o outfile] [infile]";
  char *USAGE2="Valid modes are\n\t 0: 2c1b (N/A)\n\t 1: 2c2b\n\t 2: 2c4b\n\t 3: 2c8b\n\t 4: 4c1b (N/A)\n\t 5: 4c2b\n\t 6: 4c4b\n\t 7: 4c8b (N/A)\n\t 8: signed bytes\n\t16: signed 16bit\n\t32: 32bit floats\n";
  int  c;			 /* option letter returned by getopt  */
  int  arg_count = 1;		 /* optioned argument count */

  /* default parameters */
  opterr = 0;			 /* turn off there message */
  *infile  = "-";		 /* initialise to stdin, stdout */
  *outfile = "-";

  *mode  = 0;                /* default value */
  *fsamp = 0;
  *freqres = 1;
  *downsample = 1;
  *sum = 1;
  *timeseries = 0;
  *chan = 1;
  *dB = 0;		/* default is linear output */
  *invert = 0;		
  *freqmin = 0;		/* not set value */
  *freqmax = 0;		/* not set value */
  *rmsmin  = 0;		/* not set value */
  *rmsmax  = 0;		/* not set value */

  /* loop over all the options in list */
  while ((c = getopt(argc,argv,myoptions)) != -1)
  { 
    switch (c) 
      {
      case 'o':
	*outfile = optarg;	/* output file name */
	arg_count += 2;		/* two command line arguments */
	break;
	
      case 'm':
	sscanf(optarg,"%d",mode);
	arg_count += 2;		/* two command line arguments */
	break;
	
      case 'f':
	sscanf(optarg,"%lf",fsamp);
	arg_count += 2;
	break;
	
      case 'r':
	sscanf(optarg,"%lf",freqres);
	arg_count += 2;
	break;

      case 'd':
	sscanf(optarg,"%d",downsample);
	arg_count += 2;
	break;
	
      case 'n':
	sscanf(optarg,"%d",sum);
	arg_count += 2;
	break;

      case 'c':
	sscanf(optarg,"%d",chan);
	arg_count += 2;
	break;
	
      case 'l':
	*dB = 1;
	arg_count += 1;
	break;

      case 'i':
	*invert = 1;
	arg_count += 1;
	break;

      case 't':
	*timeseries = 1;
	arg_count += 1;
	break;

      case 'x':
	if ( no_comma_in_string(optarg) )
	  {
	    fprintf(stderr,"\nERROR: require comma between -x args\n");
	    goto errout;
	  }
	else
	  {
	    if (sscanf(optarg,"%f,%f",freqmin,freqmax) != 2)
	      goto errout;
	    arg_count += 2;          /* two command line arguments */
	  }
	break;
	
      case 's':
	if ( no_comma_in_string(optarg) )
	  {
	    fprintf(stderr,"\nERROR: require comma between -s args\n");
	    goto errout;
	  }
	else
	  {
	    if (sscanf(optarg,"%f,%f",rmsmin,rmsmax) != 2)
	      goto errout;
	    arg_count += 2;          /* two command line arguments */
	  }
	break;
	
      case '?':			 /*if not in myoptions, getopt rets ? */
	goto errout;
	break;
      }
  }
  
  if (arg_count < argc)		 /* 1st non-optioned param is infile */
    *infile = argv[arg_count];

  /* must specify a valid mode */
  if (*mode == 0)
    {
      fprintf(stderr,"Must specify sampling mode\n");
      goto errout;
    }
  /* must specify a valid sampling frequency */
  if (*fsamp == 0) 
    {
      fprintf(stderr,"Must specify sampling frequency\n");
      goto errout;
    }
  /* must specify a valid channel */
  if (*chan != 1 && *chan != 2) goto errout;
  /* some combinations not implemented yet */
  if (*timeseries && *dB) 
    {
      fprintf(stderr,"Cannot have -t and -l simultaneously yet\n");
      goto errout;
    }
  if (*timeseries && (*freqmin != 0 || *freqmax !=0)) 
    {
      fprintf(stderr,"Cannot have -t and -x simultaneously yet\n");
      goto errout;
    }
  if (*timeseries && (*rmsmin != 0 || *rmsmax !=0)) 
    {
      fprintf(stderr,"Cannot have -t and -s simultaneously yet\n");
      goto errout;
    }

  if (*downsample > 1 && (*mode == 16 || *mode == 32)) 
    {
      fprintf(stderr,"Cannot have -d with modes 16 or 32 yet\n");
      exit (1);
    }

  return;

  /* here if illegal option or argument */
  errout: fprintf(stderr,"%s\n",rcsid);
          fprintf(stderr,"Usage: %s\n",USAGE1);
          fprintf(stderr,"%s",USAGE2);
	  exit(1);
}

/******************************************************************************/
/*	open file    							      */
/******************************************************************************/
void	open_file(outfile,fpoutput)
char	*outfile;		/* output file name */
FILE    **fpoutput;		/* pointer to output file */
{
  /* opens the output file, stdout is default */
  if (outfile[0] == '-')
    *fpoutput=stdout;
  else
    {
      *fpoutput=fopen(outfile,"w");
      if (*fpoutput == NULL)
	{
	  perror("open_files: output file open error");
	  exit(1);
	}
    }
  return;
}

/******************************************************************************/
/*	copy_cmd_line    						      */
/******************************************************************************/
void	copy_cmd_line(argc,argv,command_line)
int	argc;
char	**argv;			/* command line arguements */
char	command_line[];		/* command line parameters in single string */
{
  /* copys the command line parameters in argv to the single string
     command line
  */
  int	i;

  command_line[0] = '\n';

  for (i=0; i<argc; i++)
  {
    strcat (command_line, argv[i]);
    strcat (command_line, " ");
  }

  return;
}	

/******************************************************************************/
/*	vector_power							      */
/******************************************************************************/
void vector_power(float *data, int len)
{
  /* detects the complex array data of len complex samples placing the
     result in the bottom len samples of the input array
  */

  int i,j,k;

  for (i=0, j=0, k=1; i<len; i++, j+=2, k+=2)
    data[i] = data[j]*data[j] + data[k]*data[k];

  return;
}

/******************************************************************************/
/*	swap_freq							      */
/******************************************************************************/
void swap_freq(float *data, int len)
{
  /* swaps the location of the + and - frequencies for a continuous
     spectrum, the data array is 2*len samples long
  */
  int i,j;
  float temp;

  for (i=0, j=len; i<len; i++, j++)
    {
      temp    = data[i];
      data[i] = data[j];
      data[j] = temp;
    }

  return;
}

/******************************************************************************/
/*	swap_iandq							      */
/******************************************************************************/
void swap_iandq(float *data, int len)
{
  /* swaps the i and q components of each complex word to reverse the "phase"
     direction, the data array is len complex samples long or 2*len samples long
  */

  int i,j;
  float temp;

  for (i=0, j=1; i < 2*len; i+=2, j+=2)
    {
      temp    = data[i];
      data[i] = data[j];
      data[j] = temp;
    }

  return;
}

/******************************************************************************/
/*	zerofill							      */
/******************************************************************************/
void zerofill(float *data, int len)
{
  /* zero fills array of floats of size len */

  int i;

  for (i=0; i<len; i++)
    data[i] = 0.0;

  return;
}

/******************************************************************************/
/*	no_comma_in_string						      */
/******************************************************************************/
int no_comma_in_string(params)
char    params[];               /* parameter string */
{
  /* searches for a comma in the parameter string which indicates
     multiple arguements; returns TRUE if there is no comma
  */
  int no_comma;         /* flag false if comma found */
  int index;            /* index to string */

  no_comma = 1;

  for (index = 0; no_comma && params[index]!='\0' ; index++)
    no_comma = (params[index] == ',') ? 0 : no_comma;

  return(no_comma);
}
