/*******************************************************************************
*  program pfs_hist
*  $Id$
*  This programs unpacks some data from the portable fast sampler
*  and prints and histogram of count values for all channels
*
*  usage:
*  	pfs_hist -m mode [-a (parse all data)] [-o outfile] [infile]
*
*  input:
*       the input parameters are typed in as command line arguments
*	the -m option specifies the data acquisition mode
*	the -a option specifies to parse all the data recorded
*                     (default is to parse the first megabyte)
*
*  output:
*	the -o option identifies the output file, stdout is default
*
*******************************************************************************/

/* 
   $Log$
   Revision 1.1  2000/10/30 04:46:22  margot
   Initial revision

*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "unpack.h"

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

void iq_hist(float *inbuf, int nsamples, int levels);
void iq_hist_8b(float *inbuf, int nsamples, int levels);

int main(int argc, char *argv[])
{
  int mode;
  int bufsize = 1048576;/* size of read buffer, default 1 MB */
  char *buffer;		/* buffer for packed data */
  float *rcp,*lcp;	/* buffer for unpacked data */
  int smpwd;		/* # of single pol complex samples in a 4 byte word */
  int nsamples;		/* # of complex samples in each buffer */
  int levels;		/* # of levels for given quantization mode */
  int open_flags;	/* flags required for open() call */
  int parseall;
  int i;

  /* get the command line arguments and open the files */
  processargs(argc,argv,&infile,&outfile,&mode,&parseall);

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
    perror("open input file");

  switch (mode)
    {
    case -1: smpwd = 8; levels =   4; break;  
    case  1: smpwd = 8; levels =   4; break;
    case  2: smpwd = 4; levels =  16; break;
    case  3: smpwd = 2; levels = 256; break; 
    case  5: smpwd = 4; levels =   4; break;
    case  6: smpwd = 2; levels =  16; break;
    default: fprintf(stderr,"Invalid mode\n"); exit(1);
    }

  /* allocate storage */
  nsamples = bufsize * smpwd / 4;
  rcp = (float *) malloc(2 * nsamples * sizeof(float));
  lcp = (float *) malloc(2 * nsamples * sizeof(float));
  buffer = (char *) malloc(bufsize);
  if (buffer == NULL) 
    {
      fprintf(stderr,"Malloc error\n"); 
      exit(1);
    }

  /* read first buffer */
  if (bufsize != read(fdinput, buffer, bufsize))
    fprintf(stderr,"Read error\n");
  
  switch (mode)
    { 
    case -1:
      unpack_gsb(buffer, rcp, bufsize);  
      iq_hist(rcp, nsamples, levels); 
      break;
    case 1:
      unpack_pfs_2c2b(buffer, rcp, bufsize);
      iq_hist(rcp, nsamples, levels);
      break;
    case 2: 
      unpack_pfs_2c4b(buffer, rcp, bufsize);
      iq_hist(rcp, nsamples, levels);
      break;
    case 3: 
      unpack_pfs_2c8b(buffer, rcp, bufsize);
      iq_hist_8b(rcp, nsamples, levels);
      break;
    case 5:
      unpack_pfs_4c2b(buffer, rcp, lcp, bufsize);
      fprintf(fpoutput,"RCP hist\n");
      iq_hist(rcp, nsamples, levels);
      fprintf(fpoutput,"LCP hist\n");
      iq_hist(lcp, nsamples, levels);
      break;
    case 6:
      unpack_pfs_4c4b(buffer, rcp, lcp, bufsize);
      fprintf(fpoutput,"RCP hist\n");
      iq_hist(rcp, nsamples, levels);
      fprintf(fpoutput,"LCP hist\n");
      iq_hist(lcp, nsamples, levels);
      break;
    default: fprintf(stderr,"mode not implemented yet\n"); exit(1);
    }
  
  return 0;
}

/******************************************************************************/
/*	iq_hist						         	      */
/******************************************************************************/
void iq_hist(float *inbuf, int nsamples, int levels)
{
  double i=0;
  double q=0;

  int ihist[2*levels];
  int qhist[2*levels];

  int k;

  /* initialize */  
  for (k = 0; k < 2 * levels; k += 2)
    ihist[k] = qhist[k] = 0;

  /* compute histogram */
  for (k = 0; k < 2*nsamples; k += 2)
    {
      ihist[(int)inbuf[k]   + levels - 1] += 1; 
      qhist[(int)inbuf[k+1] + levels - 1] += 1; 
    }

  /* print results */
  for (k = 0; k < 2 * levels; k += 2)
    {
      fprintf(fpoutput,"%10d %15d",k - levels + 1,ihist[k]);
      fprintf(fpoutput,"\t"); 
      fprintf(fpoutput,"%10d %15d",k - levels + 1,qhist[k]);
      fprintf(fpoutput,"\n"); 
    }

  return;
}    

/******************************************************************************/
/*	iq_hist_8b					         	      */
/******************************************************************************/
void iq_hist_8b(float *inbuf, int nsamples, int levels)
{
  double i=0;
  double q=0;

  int ihist[levels];
  int qhist[levels];

  int k;

  /* initialize */  
  for (k = 0; k < levels; k++)
    ihist[k] = qhist[k] = 0;

  /* compute histogram */
  for (k = 0; k < 2*nsamples; k += 2)
    {
      ihist[(int)inbuf[k]   + levels/2] += 1; 
      qhist[(int)inbuf[k+1] + levels/2] += 1; 
    }

  /* print results */
  for (k = 0; k < levels; k ++)
    {
      fprintf(fpoutput,"%10d %15d",k - levels/2,ihist[k]);
      fprintf(fpoutput,"\t"); 
      fprintf(fpoutput,"%10d %15d",k - levels/2,qhist[k]);
      fprintf(fpoutput,"\n"); 
    }

  return;
}    

/******************************************************************************/
/*	processargs							      */
/******************************************************************************/
void	processargs(argc,argv,infile,outfile,mode,parseall)
int	argc;
char	**argv;			 /* command line arguements */
char	**infile;		 /* input file name */
char	**outfile;		 /* output file name */
int     *mode;
int     *parseall;
{
  /* function to process a programs input command line.
     This is a template which has been customised for the pfs_hist program:
	- the outfile name is set from the -o option
	- the infile name is set from the 1st unoptioned argument
  */

  int getopt();		/* c lib function returns next opt*/ 
  extern char *optarg; 	/* if arg with option, this pts to it*/
  extern int optind;	/* after call, ind into argv for next*/
  extern int opterr;    /* if 0, getopt won't output err mesg*/

  char *myoptions = "m:o:a"; 	 /* options to search for :=> argument*/
  char *USAGE1="pfs_hist -m mode [-a (parse all data)] [-o outfile] [infile] ";
  char *USAGE2="Valid modes are\n\t-1: GSB\n\t 0: 2c1b (N/A)\n\t 1: 2c2b\n\t 2: 2c4b\n\t 3: 2c8b\n\t 4: 4c1b (N/A)\n\t 5: 4c2b\n\t 6: 4c4b\n\t 7: 4c8b (N/A)\n";
  int  c;			 /* option letter returned by getopt  */
  int  arg_count = 1;		 /* optioned argument count */

  /* default parameters */
  opterr = 0;			 /* turn off there message */
  *infile  = "-";		 /* initialise to stdin, stdout */
  *outfile = "-";

  *mode  = 0;                /* default value */
  *parseall = 0;

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
	    
      case 'a':
 	       *parseall = 1;
               arg_count += 1;
	       break;
	    
      case '?':			 /*if not in myoptions, getopt rets ? */
               goto errout;
               break;
    }
  }

  if (arg_count < argc)		 /* 1st non-optioned param is infile */
    *infile = argv[arg_count];

  /* must specify a valid mode */
  if (*mode == 0) goto errout;
  
  /* code still in development */
  if (*parseall)
    {
      fprintf(stderr,"-a option not implemented yet\n"); 
      exit(1);
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
  char 	*result;
  int	i;

  result = strcpy(command_line,argv[0]);
  result = strcat(command_line," ");

  for (i=1; i<argc; i++)
  {
    result = strcat(command_line,argv[i]);
    result = strcat(command_line," ");
  }

  return;
}	
