/*
testWav.c
Originally modified from arecord.c
This program convert input RAW PCM file to WAV format
Version 1.0
Licensed under GNU . SW can be freely modified and disctributed as open source.
Author: Vivek Nandan
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <asm/byteorder.h>

#include <libintl.h>

#include <endian.h>
#include <byteswap.h>
#define VERSION "1.0"
#define _(msgid) gettext (msgid)
#define gettext_noop(msgid) msgid
#define N_(msgid) gettext_noop (msgid)
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)		(v)
#define LE_INT(v)		(v)
#define BE_SHORT(v)		bswap_16(v)
#define BE_INT(v)		bswap_32(v)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)		bswap_16(v)
#define LE_INT(v)		bswap_32(v)
#define BE_SHORT(v)		(v)
#define BE_INT(v)		(v)
#else
#error "Wrong endian"
#endif
#define WAV_RIFF		COMPOSE_ID('R','I','F','F')
#define WAV_WAVE		COMPOSE_ID('W','A','V','E')
#define WAV_FMT			COMPOSE_ID('f','m','t',' ')
#define WAV_DATA		COMPOSE_ID('d','a','t','a')

/* WAVE fmt block constants from Microsoft mmreg.h header */
#define WAV_FMT_PCM             0x0001
#define WAV_FMT_IEEE_FLOAT      0x0003

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
 */
typedef struct {
	u_int magic;		/* 'RIFF' */
	u_int length;		/* filelen */
	u_int type;		/* 'WAVE' */
} WaveHeader;

typedef struct {
	u_short format;		/* see WAV_FMT_* */
	u_short channels;
	u_int sample_fq;	/* frequence of sample */
	u_int byte_p_sec;
	u_short byte_p_spl;	/* samplesize; 1 or 2 bytes */
	u_short bit_p_spl;	/* 8, 12 or 16 bit */
} WaveFmtBody;

typedef struct {
	u_int type;		/* 'data' */
	u_int length;		/* samplecount */
} WaveChunkHeader;

#ifndef LLONG_MAX
#define LLONG_MAX    9223372036854775807LL
#endif
#define FORMAT_DEFAULT		-1
#define FORMAT_RAW		0
#define FORMAT_VOC		1
#define FORMAT_WAVE		2
#define FORMAT_AU		3
static void begin_wave(int fd, size_t count);
static void end_wave(int fd);
static snd_pcm_format_t _format = SND_PCM_FORMAT_U8;
static int _rate = 48000;
static int _channel = 2;
struct fmt_capture {
	void (*start) (int fd, size_t count);
	void (*end) (int fd);
	char *what;
	long long max_filesize;
} fmt_rec_table[] = {
	{	NULL,		NULL,		N_("raw data"),		LLONG_MAX },
	{	NULL,	NULL,	N_("VOC"),		16000000LL },
	{	begin_wave,	end_wave,	N_("WAVE"),		2147483648LL },
	{	NULL,	NULL,		N_("Sparc Audio"),	LLONG_MAX }
};
int fdcount = 0;

static int formatToBits(unsigned long int format)
{
    int bits = 0;	
     switch (format) {
	case SND_PCM_FORMAT_U8:
		bits = 8;
		break;
	case SND_PCM_FORMAT_S16_LE:
		bits = 16;
		break;
	case SND_PCM_FORMAT_S32_LE:
        case SND_PCM_FORMAT_FLOAT_LE:
		bits = 32;
		break;
	case SND_PCM_FORMAT_S24_LE:
	case SND_PCM_FORMAT_S24_3LE:
		bits = 24;
		break;}
return bits;
}
/* write a WAVE-header */
static void begin_wave(int fd, size_t cnt)
{
	WaveHeader h;
	WaveFmtBody f;
	WaveChunkHeader cf, cd;
	int bits;
	u_int tmp;
	u_short tmp2;

	/* WAVE cannot handle greater than 32bit (signed?) int */
	if (cnt == (size_t)-2)
		cnt = 0x7fffff00;

	bits = 8;
        printf("cnt = %d\n",cnt);
        bits = formatToBits(_format);
	h.magic = WAV_RIFF;
	tmp = cnt + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) - 8;
	h.length = LE_INT(tmp);
	h.type = WAV_WAVE;

	cf.type = WAV_FMT;
	cf.length = LE_INT(16);

        if (_format == SND_PCM_FORMAT_FLOAT_LE)
                f.format = LE_SHORT(WAV_FMT_IEEE_FLOAT);
        else
                f.format = LE_SHORT(WAV_FMT_PCM);
	f.channels = LE_SHORT(_channel);
	f.sample_fq = LE_INT(_rate);
	tmp2 = _channel * snd_pcm_format_physical_width(_format) / 8;
	f.byte_p_spl = LE_SHORT(tmp2);
	tmp = (u_int) tmp2 * _rate;
	f.byte_p_sec = LE_INT(tmp);
	f.bit_p_spl = LE_SHORT(bits);

	cd.type = WAV_DATA;
	cd.length = LE_INT(cnt);

	if (write(fd, &h, sizeof(WaveHeader)) != sizeof(WaveHeader) ||
	    write(fd, &cf, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader) ||
	    write(fd, &f, sizeof(WaveFmtBody)) != sizeof(WaveFmtBody) ||
	    write(fd, &cd, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader))
           {
		//printf(_"write error");
		exit(EXIT_FAILURE);
	   }
}

static void end_wave(int fd)
{				/* only close output */
	WaveChunkHeader cd;
	off64_t length_seek;
	off64_t filelen;
	u_int rifflen;

	length_seek = sizeof(WaveHeader) +
		      sizeof(WaveChunkHeader) +
		      sizeof(WaveFmtBody);
	cd.type = WAV_DATA;
	cd.length = fdcount > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(fdcount);
	filelen = fdcount + 2*sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + 4;
	rifflen = filelen > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(filelen);
	if (lseek64(fd, 4, SEEK_SET) == 4)
		write(fd, &rifflen, 4);
	if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
		write(fd, &cd, sizeof(WaveChunkHeader));
	if (fd != 1)
		close(fd);
}

static int printHelp(void)
{
  printf("USAGE: \n --version , -v : print version\n"
        "--channels , -c : input channels\n"
        "--format , -f : PCM format value\n"
        "--rate , -r : sampling rate in Htz\n"
        "--output , -o : output file name\n",
        "--input , -i : input file name to raw pcm \n");
  
}
int main(char argc,char** argv)
{
     static const char short_options[] = "h:c:f:r:o:i:F:R:T:vV:O:s:S";
     static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"version", 0, 0, 'v'},
		{"channels", 1, 0, 'c'},
		{"format", 1, 0, 'f'},
		{"rate", 1, 0, 'r'},
		{"output", 1, 0 ,'o'},
                {"input",1,0,'i'},
                {"size",1,0,'s'},
		{0, 0, 0, 0}
	};
   int optionIndex = 0;
   int c = 0;
   char inputFileName[100] = "audio.pcm";
   char outputFileName[100]= "default.wav"; 
   int rate = 8000;
   int format =  SND_PCM_FORMAT_U8;
   int channel = 1;
   char *ptr;
   int inputFileDesc = 0;
   int outputFileDesc = 0;
   unsigned long long int bytesToRead = 0;
   char* buffer = 0;
   int frames = 10000;
   unsigned long long int chunk_size = 0;
   while(((c = getopt_long(argc, argv, short_options, long_options, &optionIndex)) != -1))
   {
       switch(c)
       {
          case 'h':
                   printHelp();
          break;
          case 'v':
                  printf("%s",VERSION);
          break;
          case 'c':

                 channel = strtol(optarg,&ptr,10);
                 printf("Channels = %d\n",channel); 
          break;
          case 'f':
                format = strtol(optarg,&ptr,10);
                printf("format = %d\n",format);
          break;
          case 'r':
                rate = strtol(optarg,&ptr,10);
                printf("rate = %d\n",rate);
          break;
          case 'o':
                if(optarg)
                strncpy(outputFileName,optarg,100);
                printf("Output file name = %s\n",outputFileName);
          break;
          case 'i':
                if(optarg)
                strncpy(inputFileName,optarg,100);
                printf("Input file name = %s\n",inputFileName);
          break;
          default:
          printf("Error input:%d",c);
          break;
      
      };

   }
  _channel = channel;
  _rate=rate;
  _format = format;

   bytesToRead = (formatToBits(format)/8)*channel*frames; //chunk size
   inputFileDesc = open(inputFileName,O_RDONLY);

   remove(outputFileName);
   outputFileDesc = open64(outputFileName,O_WRONLY | O_CREAT|O_APPEND, 0644);
   buffer = (char *) malloc(bytesToRead*sizeof(char));
   if(buffer == 0)
   {
      printf("Malloc failed -- not enough memory to allocate %d bytes\n",bytesToRead);
      return -1;
    }


   int long long sizeBytes= 0;
   chunk_size = bytesToRead;

   while(chunk_size == bytesToRead)
   {
       chunk_size = read(inputFileDesc,buffer,bytesToRead);
       sizeBytes +=chunk_size;
   }
  close(inputFileDesc);

   if (fmt_rec_table[FORMAT_WAVE].start)
   {
	   fmt_rec_table[FORMAT_WAVE].start(outputFileDesc,sizeBytes);
           printf("Header written\n");
   }
  inputFileDesc = open(inputFileName,O_RDONLY);
   if(inputFileDesc == -1)
   {
    printf("Error opening file: %s",inputFileName);
    return inputFileDesc;
   }
   chunk_size = bytesToRead;

   sizeBytes= 0;
   while(chunk_size == bytesToRead)
   {
       chunk_size = read(inputFileDesc,buffer,bytesToRead);
       write(outputFileDesc,buffer,chunk_size);

      sizeBytes +=chunk_size;
   }
   frames = (sizeBytes*8)/(channel*formatToBits(format));
   printf("Total bytes read = %ld\n",sizeBytes);
   printf("total pcm frames = %ld\n",(sizeBytes*8)/(channel*formatToBits(format)));

/* finish sample container */
  if (fmt_rec_table[FORMAT_WAVE].end)
   {
	fmt_rec_table[FORMAT_WAVE].end(outputFileDesc);
        printf("Header closed!\n");
	outputFileDesc = -1;
    }

   if(buffer)
   {
       free(buffer);
   }

   close(inputFileDesc);
   return 0;   
}

