// evtdump.c - dump a graph of the evt
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>

#ifdef WIN32
#ifndef __BYTE_ORDER
#define __BYTE_ORDER		4321
#endif
typedef char				INT8;
typedef short				INT16;
typedef long				INT32;
typedef __int64				INT64;
typedef unsigned char		UINT8;
typedef unsigned short		UINT16;
typedef unsigned long		UINT32;
typedef unsigned __int64	UINT64;
#else
typedef char				INT8;
typedef short				INT16;
typedef long				INT32;
typedef long long			INT64;
typedef unsigned char		UINT8;
typedef unsigned short		UINT16;
typedef unsigned long		UINT32;
typedef unsigned long long	UINT64;
typedef char				BOOL;
#endif

typedef enum { EVT_AUD=1, EVT_VID=2 } EVT_TYPE;

typedef struct
{
	char	sig[4];
	char	ver[4];
} EVT_HEADER;

typedef struct
{
	INT64	timestamp;
	UINT32	reserved;
	UINT32	type;
	UINT32	value1;
	union
	{
		UINT32	value2;
		char	color[4];
	};
} EVT_EVENT;

typedef struct _fade
{
	struct _fade	*next;
	INT64			start;
	INT64			stop;
	UINT32			video;
	UINT32			audio;
	UINT32			type;
} FADE;

#if __BYTE_ORDER == __BIG_ENDIAN

#define SWAP16(v) (v)
#define SWAP32(v) (v)
#define SWAP64(v) (v)

#else

__inline UINT16 SWAP16(UINT16 val)
{
	return (val>>8)|(val<<8);
}
__inline UINT32 SWAP32(UINT32 val)
{
	char temp[4], *pc=(char*)&val;
	temp[0] = pc[3];
	temp[1] = pc[2];
	temp[2] = pc[1];
	temp[3] = pc[0];
	return *((UINT32*)temp);
}
__inline INT64 SWAP64(INT64 val)
{
	char temp[8], *pc=(char*)&val;
	temp[0] = pc[7];
	temp[1] = pc[6];
	temp[2] = pc[5];
	temp[3] = pc[4];
	temp[4] = pc[3];
	temp[5] = pc[2];
	temp[6] = pc[1];
	temp[7] = pc[0];
	return *((INT64*)temp);
}

#endif

UINT32 gProgTime	= 300;			// 5 minutes
UINT32 gMinTime		= 0;			// 0 seconds

char*
format_ts(INT64 ts)
{
	static char time[16];
	UINT32 ms = (UINT32)(ts / 1000000);
	UINT32 sec = ms / 1000;
	UINT32 min = sec / 60;
	sec -= (min*60);
	ms %= 1000;
	sprintf (time, "%03d:%02d.%03d", min, sec, ms);
	return time;
}

char*
vid_level(UINT32 level)
{
	static char vid[10];
	int i;
	level = (level + 7) / 8;
	for (i=0; i<9; i++)
	{
		if (level)
		{
			level--;
			vid[i] = '#';
		}
		else vid[i] = ' ';
	}
	vid[9] = 0;
	return vid;
}

char*
aud_level(UINT32 level)
{
	static char aud[50];
	int i;
	memset(aud,0,50);
	level = (level + 199) / 200;
	for (i=0; i<49; i++)
	{
		if (level)
		{
			level--;
			aud[i] = '=';
		}
		else aud[i] = ' ';
	}
	aud[49] = 0;
	return aud;
}

UINT32 audmax, audmin = 0xFFFFFFFF;
UINT32 factor;

void normalize(FILE* fp)		// find range to normalize the audio
{
	EVT_HEADER	header;
	EVT_EVENT	event;
	fseek(fp, 0, SEEK_SET);
	fread(&header, 1, 8, fp);	// skip header
	while (fread(&event, 1, 24, fp)==24)
	{
		event.type		= SWAP32(event.type);
		event.value1	= SWAP32(event.value1);
		event.value2	= SWAP32(event.value2);
		if (event.type == EVT_AUD)
		{
			if (event.value1 > audmax) audmax = event.value1;
			if (event.value1 < audmin) audmin = event.value1;
		}
	}
	if (audmax)	factor = 10000 / audmax;					// audio multiplier
	if (!factor) factor = 1;
//	printf("Scale factor = %d (%d, %d)\n", factor, audmin, audmax);
	audmin = (audmin + ((audmax - audmin)/10)) * factor;	// audio threshold
//	printf("Threshold = %d\n", audmin);
}

void dump(FILE* fp)
{
	EVT_HEADER	header;
	EVT_EVENT	event;
	UINT32		lastvid = 256;
	UINT32		lastaud = 10000;
	INT64		lasttime, laststart = 0, lastvidtime, lastaudtime;

	lasttime = lastvidtime = lastaudtime = 0;

	printf("Timestamp : T Video Lvl | Audio Lvl\n");

	fseek(fp, 0, SEEK_SET);
	fread(&header, 1, 8, fp);	// skip header
	while (fread(&event, 1, 24, fp)==24)
	{
		event.timestamp = SWAP64(event.timestamp);
		event.type		= SWAP32(event.type);
		event.value1	= SWAP32(event.value1);
		event.value2	= SWAP32(event.value2);

		if ((factor == 1 && (event.timestamp - lasttime) > 1000000000))
		{
			printf("----------:\n");
			lastvid = 256;
			lastaud = 10000;
		}
		if (event.type == EVT_AUD)
		{
			if ((event.timestamp - lastvidtime) > 70000000)
				lastvid = 256;
			lastaud = event.value1 * factor;
			lastaudtime = event.timestamp;
		}
		if (event.type == EVT_VID)
		{
			int i, divisor = 0;
			if ((event.timestamp - lastaudtime) > 50000000)
				lastaud = 10000;
			lastvid = 0;
			for (i=0; i<4; i++)
			{
				if (event.color[i])
				{
					lastvid += event.color[i];
					divisor++;
				}
			}
			if (divisor) lastvid /= divisor;
			lastvidtime = event.timestamp;
		}
		if ((factor > 1 && ((!laststart && lastaud < audmin) || (laststart && lastaud >= audmin))))
		{
			if (laststart)
			{
				if ((event.timestamp - laststart) > 1000000000)
				{
					printf("----------:\n");
					laststart = 0;
				}
			}
			else
			{
				printf("----------:\n");
				laststart = event.timestamp;
			}
		}
		lasttime = event.timestamp;
		printf ("%s: %c %s | %s\n",	format_ts(event.timestamp),	(event.type == 1) ? 'A' : 'V',
					vid_level(lastvid), aud_level(lastaud));
	}
}

void find_edits(FILE* fp)
{
	EVT_HEADER	header;
	EVT_EVENT	event;
	UINT32		lastvid = 256;
	UINT32		lastaud = 10000;
	INT64		lasttime, lastvidtime, lastaudtime;
	FADE*		current;
	FADE*		root;

	lasttime = lastvidtime = lastaudtime = 0;

	root = current = (FADE*)malloc(sizeof(FADE));
	memset(current, 0, sizeof(FADE));

	// First pass: Create a list of valid fade points within the event groups.
	// We don't care what these fade points are yet.

	fseek(fp, 0, SEEK_SET);
	fread(&header, 1, 8, fp);	// skip header
	while (fread(&event, 1, 24, fp)==24)
	{
		event.timestamp = SWAP64(event.timestamp);
		event.type		= SWAP32(event.type);
		event.value1	= SWAP32(event.value1);
		event.value2	= SWAP32(event.value2);

		if (!lasttime || (factor == 1 && (event.timestamp - lasttime) > 1000000000))
		{
			if (current->start)				// re-use node if not used
			{
				if (!current->stop) current->stop = lasttime;
				current->next = (FADE*)malloc(sizeof(FADE));
				current = current->next;
				memset(current, 0, sizeof(FADE));
			}
			current->stop = 0;
			current->video = 256;
			current->audio = 10000;
			lastvid = 256;
			lastaud = 10000;
		}
		if (event.type == EVT_AUD)
		{
			if ((event.timestamp - lastvidtime) > 70000000)
				lastvid = 256;
			lastaud = event.value1 * factor;
			lastaudtime = event.timestamp;
		}
		if (event.type == EVT_VID)
		{
			int i, divisor = 0;
			if ((event.timestamp - lastaudtime) > 50000000)
				lastaud = 10000;
			lastvid = 0;
			for (i=0; i<4; i++)
			{
				if (event.color[i])
				{
					lastvid += event.color[i];
					divisor++;
				}
			}
			if (divisor) lastvid /= divisor;
			lastvidtime = event.timestamp;
		}
		if ((factor > 1 && ((!current->start && lastaud < audmin) || (current->start && lastaud >= audmin))))
		{
			if (current->start)				// re-use node if not used
			{
				if ((event.timestamp - current->start) > 1000000000)
				{
					if (!current->stop) current->stop = lasttime;
					current->next = (FADE*)malloc(sizeof(FADE));
					current = current->next;
					memset(current, 0, sizeof(FADE));
				}
			}
			current->stop = 0;
			current->video = 256;
			current->audio = 10000;
		}
		// sometimes we see errant past times at the end of the evt, filter them out
		if (event.timestamp >= lasttime)
		{
			lasttime = event.timestamp;
			if (lastvid < 32)			// A fade is only valid if BOTH vid and aud fall below
			{							// our thresholds.
				if (lastaud < audmin)
				{
					if (!current->start)
					{
						current->start = lasttime;
						current->video = lastvid;
						current->audio = lastaud;
					}
					else if (current->stop)
					{
						current->start = lasttime;
						current->video = lastvid;
						current->audio = lastaud;
						current->stop = 0;
					}
				}
				else if (current->start && !current->stop)
				{
					current->stop = lasttime;
				}
			}
			else if ((lastvid > 200) && current->start && !current->stop)
			{
				current->stop = lasttime;
			}
		}
	}

	// Second pass: Walk the list of fade points and see if anything falls out.

	// for now, just look for time between fades, if it's > 5 minutes it's likely
	// to be a program segment, else it's likely to be a commercial segment.  if
	// there is a fade in the first minute, the recording may have started on a
	// commercial.  nothing fancy.

	current = root;
	lasttime = 0;
	while (current)
	{
		if (current->start)
		{
			INT64 spottime;

			// use the middle value of the fade range as the edit time
			current->stop = (current->stop + current->start)/2;

			spottime = current->stop - lasttime;

			if (!lasttime || spottime >= ((INT64)gMinTime*1000000000))
			{
				// if it's less than gProgTime, we want to add the edit
				if (lasttime && spottime < ((INT64)gProgTime*1000000000))
				{
					current->type = 1;
				}
				// if it's the first one in the first minute, we want to add the edit
				if (!lasttime && spottime < 60000000000)						// 1 min
				{
					current->type = 1;
				}
				else lasttime = current->stop;

				printf ("%c%s\n", current->type ? 'A' : 'D', format_ts(current->stop));
/*
				printf ("%s: %d %s | %s\n",
							format_ts(current->stop),
							current->type,
							vid_level(current->video),
							aud_level(current->audio));
*/
			}
			else lasttime = current->stop;
		}
		current = current->next;
	}

	printf("E\n");

	// release the fade list
	current = root;
	while (current)
	{
		root = current;
		current = current->next;
		free(root);
	}
}

int main(int argc, char** argv)
{
	UINT8 doGraph = 0;

	while (argc > 2)
	{
		if (!memcmp(argv[1], "-p", 2))
		{
			gProgTime = atoi(&argv[1][2]);
			if (!gProgTime)
				gProgTime = 210;
		}
		else if (!memcmp(argv[1], "-i", 2))
		{
			gMinTime = atoi(&argv[1][2]);
		}
		else if (!strcmp(argv[1], "-graph"))
			doGraph = 1;
		else break;
		argc--; argv++;
	}
	if (gProgTime < gMinTime) gMinTime = 0;
	if (argc > 1)
	{
		char* ext;
		if (ext = strstr(argv[1], ".evt"))
		{
			FILE* fp = fopen(argv[1], "rb");
			if (fp)
			{
				normalize(fp);
				if (doGraph || ((argc > 2) && !strcmp(argv[2], "-graph")))
				{
					dump(fp);
					fseek(fp, 0, SEEK_SET);
					printf("\n");
				}
				sprintf(ext, ".mpg");
				printf("F%s\n", argv[1]);
				find_edits(fp);
				fclose(fp);
				return 0;
			}
		}
	}
	printf("Usage: evtdump [-p<sec>] [-i<sec>] [-graph] <evt-file>\n");
	return 0;
}

