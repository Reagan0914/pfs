/*******************************************************************************
*  program pfs_unpack
*  $Id$
*  This programs unpacks data from the portable fast sampler
*
*  usage:
*  	pfs_unpack -m mode [-o outfile] [infile]
*
*  input:
*       the input parameters are typed in as command line arguments
*	the -m option specifies the data acquisition mode
*
*  output:
*	the -o option identifies the output file, stdout is default
*
*******************************************************************************/

/* 
   $Log$
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "unpack.h"

/* revision control variable */
static char const rcsid[] = 
"$Id$";

int     fdoutput;		/* file descriptor for output file */
int	fdinput;		/* file descriptor for input file */

char   *outfile;		/* output file name */
char   *infile;		        /* input file name */

char	command_line[200];	/* command line assembled by processargs */

void processargs();
void open_file();
void copy_cmd_line();


int main(int argc, char *argv[])
{

  int mode;
  int bufsize = 1048576;/* size of read buffer, default 1 MB */
  int outbufsize;	/* output buffer size */
  char *buffer;		/* buffer for packed data */
  float *rcp,*lcp;	/* buffer for unpacked data */
  int smpwd;		/* # of single pol complex samples in a 4 byte word */
  int nsamples;		/* # of complex samples in each buffer */
  int i;

  /* get the command line arguments and open the files */
  processargs(argc,argv,&infile,&outfile,&mode);

  /* save the command line */
  copy_cmd_line(argc,argv,command_line);

  /* open file input */
#ifdef LARGEFILE
  if((fdinput = open(infile, O_RDONLY|O_LARGEFILE)) < 0 )
    perror("open input file");
  if((fdoutput = open(outfile, O_WRONLY|O_CREAT|O_LARGEFILE, 0644)) < 0 )
    perror("open output file");
#else
  if((fdinput = open(infile, O_RDONLY)) < 0 )
    perror("open input file");
  if((fdoutput = open(outfile, O_WRONLY|O_CREAT, 0644)) < 0 )
    perror("open output file");
#endif


  switch (mode)
    {
    case -1: smpwd = 8; break;  
    case  1: smpwd = 8; break;
    case  2: smpwd = 4; break;
    case  3: smpwd = 2; break; 
    case  5: smpwd = 4; break;
    case  6: smpwd = 2; break;
    default: fprintf(stderr,"Invalid mode\n"); exit(1);
    }

  /* allocate storage */
  nsamples = bufsize * smpwd / 4;
  outbufsize = 2 * nsamples * sizeof(float);
  rcp = (float *) malloc(outbufsize);
  lcp = (float *) malloc(outbufsize);
  buffer = (char *) malloc(bufsize);
  if (buffer == NULL) 
    {
      fprintf(stderr,"Malloc error\n"); 
      exit(1);
    }

  /* infinite loop */
  while (1)
    {
      /* read one buffer */
      if (bufsize != read(fdinput, buffer, bufsize))
	{
	  perror("read");
	  fprintf(stderr,"Read error or EOF\n");
	  exit(1);
	}

      /* unpack */
      switch (mode)
	{
	case -1:
	  unpack_gsb(buffer, rcp, bufsize);
	  break;
	case 1:
	  unpack_pfs_2c2b(buffer, rcp, bufsize); 
	  if (outbufsize != write(fdoutput,rcp,outbufsize))
	    fprintf(stderr,"Write error\n");  
	  break;
	case 2: 
	  unpack_pfs_2c4b(buffer, rcp, bufsize);
	  break;
	case 3:
	  unpack_pfs_2c8b(buffer, rcp, bufsize);
	  break;
	case 5:
	  unpack_pfs_4c2b(buffer, rcp, lcp, bufsize);
	  break;
	case 6:
	  unpack_pfs_4c4b(buffer, rcp, lcp, bufsize);
	  break;
	default: 
	  fprintf(stderr,"mode not implemented yet\n"); 
	  exit(1);
	}

      /* write data to output file */
      if (outbufsize != write(fdoutput,rcp,outbufsize))
	fprintf(stderr,"Write error\n");  
    }

  return 0;
}

/******************************************************************************/
/*	processargs							      */
/******************************************************************************/
void	processargs(argc,argv,infile,outfile,mode)
int	argc;
char	**argv;			 /* command line arguements */
char	**infile;		 /* input file name */
char	**outfile;		 /* output file name */
int     *mode;
{
  /* function to process a programs input command line.
     This is a template which has been customised for the pfs_unpack program:
	- the outfile name is set from the -o option
	- the infile name is set from the 1st unoptioned argument
  */

  int getopt();		/* c lib function returns next opt*/ 
  extern char *optarg; 	/* if arg with option, this pts to it*/
  extern int optind;	/* after call, ind into argv for next*/
  extern int opterr;    /* if 0, getopt won't output err mesg*/

  char *myoptions = "m:o:"; 	 /* options to search for :=> argument*/
  char *USAGE1="pfs_unpack -m mode [-o outfile] [infile] ";
  char *USAGE2="Valid modes are\n\t-1: GSB\n\t 0: 2c1b (N/A)\n\t 1: 2c2b\n\t 2: 2c4b\n\t 3: 2c8b\n\t 4: 4c1b (N/A)\n\t 5: 4c2b\n\t 6: 4c4b\n\t 7: 4c8b (N/A)\n";
  int  c;			 /* option letter returned by getopt  */
  int  arg_count = 1;		 /* optioned argument count */

  /* default parameters */
  opterr = 0;			 /* turn off there message */
  *infile  = "-";		 /* initialise to stdin, stdout */
  *outfile = "-";

  *mode  = 0;                /* default value */

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
	    
      case '?':			 /*if not in myoptions, getopt rets ? */
               goto errout;
               break;
    }
  }

  if (arg_count < argc)		 /* 1st non-optioned param is infile */
    *infile = argv[arg_count];

  /* must specify a valid mode */
  if (*mode == 0 ) goto errout;
  
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

