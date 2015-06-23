/*
** ReplayTV 5000
** Guide Parser
** by Lee Thompson <thompsonl@logh.net> Dan Frumin <rtv@frumin.com>, Todd Larason <jtl@molehill.org>
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of 
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
** 
** If you distribute a modified version of this program, the program MUST tell the user 
** in the usage summary how the program has been modified, and who performed the modification.
** Modified source code MUST be distributed with the modified program. 
**
** For additional detail on the Replay 4000/4500/5000 internals, protocols and structures I highly recommend
** visiting Todd's site: http://www.molehill.org/twiki/bin/view/Replay/WebHome
**
***************************************************************************************************************
**
** IMPORTANT NOTE: This is the last version of GuideParser with this codebase. 
**
** GuideParser was originally just a research project to learn the Guide Snapshot format.  This research
** phase is pretty much at a close and this code is very ugly. 
**
** Others wishing to continue development of GuideParser with this code are welcome to do so under the above
** limitations.
**
***************************************************************************************************************
**
** Revision History:
** 
** 1.02 (18)   Added "No Repeats" flag for 5.0 software.  
**             Changed UnixToTimeString so that if seconds is zero the :00 is omitted.   This will likely break
**             Rich A.'s program.
** 1.02 - LT   Updated for 5.0 Software
** 1.01 - LT   ReplayTV 5000
** 1.0 - LT    Final version with this codebase.
**             Organized the structures a bit better.
**             Fixed some annoying bugs.
**             Added -combined option which does a mini replayChannel lookup, best used with -ls
** .11 - LT    Whoops!  ReplayChannel had the wrong number for szThemeInfo, should've been 64 not 60! (Thanks Clem!)
**             Todd corrected a mis-assigned bitmask for TV rating "TV-G"
**             Todd's been knocking himself out with research... as a result no more unknowns in tagProgramInfo!
**             Added Genre lookups
** .10 - LT    Works on FreeBSD now (thanks Andy!)  
**             Fixed a minor display bug.   
**             Supports MPAA "G" ratings.
**             Added -d option for dumping unknowns as numbers.
**             Added -x (Xtra) option for displaying TMSIDs and so forth.   
**             Added -nowrap to supress word wrap.
**             Added STDIN support!
**             Added Input Source lookup.
**             Better organized sub-structures. 
**             Channel/Show Output now uses the word wrapper for everything.
**             Some minor tweaks.
**             Less unknowns.   (Thanks in large part to Todd)
**             Released on Mar 10th 2002
** .09 - LT    Wrote an int64 Big->Small Endian converter.   
**             UNIX/WIN32 stuff sorted out.  
**             Cleaned up some code.  
**             Dependency on stdafx.h removed.
**             Improved Word Wrapper.   
**             Fixed some bugs.
**             Less unknowns.
**             Released on Jan 13th 2002
** .08 - LT    More fixes!
**             Better wordwrapping!
**             Less unknowns!      
**             Released on Jan 10th 2002
** .07 - LT    Categories :)   
**             Dan added the extraction command line switches and code.
**             Released on Jan 8th 2002
** .06 - LT	   Timezone fixes
**             Released on Jan 5th 2002
** .05 - LT    Checking to see if guide file is damaged (some versions of ReplayPC have a problem in this area)
**			   Found a case where GuideHeader.replayshows is not accurate!   
**             Lots of other fixes.
**             Released on Jan 4th 2002
** .04 - LT    Day of Week implemented.
**             Categories are loaded but lookup isn't working.
** .03 - LT	   Private build.
** .02 - LT	   Private build.
** .01 - LT    Private build.  
**             Initial Version     
**             01/03/2002
**
**
***************************************************************************************************************
**
** TO DO:
**
** 1. Rewrite!
**
** 
** BUGS
**  Piping from replaypc to guideparser gets a few replaychannels in and somehow loses it's alignment -- looks
**  like the 'file' is being truncated.
**
***************************************************************************************************************
**  FreeBSD fixes by Andrew Tulloch <andrew@biliskner.demon.co.uk>
***************************************************************************************************************
*/

#ifdef WIN32
#if !defined(AFX_STDAFX_H__11AD8D81_EA54_4CD0_9A74_44F7170DE43F__INCLUDED_)
#define AFX_STDAFX_H__11AD8D81_EA54_4CD0_9A74_44F7170DE43F__INCLUDED_
#endif

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#pragma pack(1)                          // don't use byte alignments on the structs

#define WIN32_LEAN_AND_MEAN	

#include <winsock2.h> 
#include <winnt.h>
#include <winbase.h>
#endif

#if defined(__unix__) && !defined(__FreeBSD__)
#include <netinet/in.h>
#endif

#include <string.h>
#include <memory.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>


#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")      // load winsock2 (we only use it for endian conversion)
#define FILEPOS fpos_t
#endif

#ifdef __unix__
typedef unsigned char BYTE;
typedef unsigned long long DWORD64;
typedef unsigned long DWORD;
typedef unsigned short WORD;
#define FILEPOS unsigned long
#define _tzset() tzset()
#define _tzname tzname
#define _timezone timezone
#define MoveMemory(dst,src,len) memmove(dst,src,len)
#endif

extern int _daylight;
extern long _timezone;
extern char *_tzname[2];

#define SHOWSTRUCTURESIZES  1           // this is for debugging...
#undef SHOWSTRUCTURESIZES		        // ...comment this line out for structure sizes to be shown

#define REPLAYSHOW      512             // needed size of replayshow structure
#define REPLAYCHANNEL   712             // needed size of replaychannel structure
#define GUIDEHEADER     840             // needed size of guideheader structure
#define PROGRAMINFO     272             // needed size of programinfo structure
#define CHANNELINFO     80              // needed size of channelinfo structure

#define MAXCHANNELS     99

#define lpszTitle	    "ReplayTV 5000 GuideParser v1.02 (Build 18)"
#define lpszCopyright	"(C) 2002 Lee Thompson, Dan Frumin, Todd Larason, and Andrew Tulloch"
#define lpszComment     ""

//-------------------------------------------------------------------------
enum EDayOfWeek
{
        ceSunday =	    		1<<0,
        ceMonday =	    		1<<1,
        ceTuesday =	    		1<<2,
        ceWednesday =			1<<3,
        ceThursday =			1<<4,
        ceFriday =		    	1<<5,
        ceSaturday =			1<<6,
        ceUnknown =		    	1<<7
};


//-------------------------------------------------------------------------
// Data Structures
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
typedef struct tagCategoryArray {
    DWORD index;                    // Index Value
    char szName[10];                // Category Text
} CategoryArray;

//-------------------------------------------------------------------------
// This is a mini version of tagReplayChannel for doing lookups for the 
// implemented but undocumented -combine option.

typedef struct tagChannelArray {
    DWORD channel_id;               // ReplayTV Channel ID
    char szCategory[10];            // Category Text
    DWORD keep;                     // Number of Episodes to Keep
    DWORD stored;                   // Number of Episodes Recorded Thus Far
    DWORD channeltype;              // Channel Type
    char szChannelType[16];         // Channel Type (Text)
    char szShowLabel[48];	        // Show Label
} ChannelArray;

//-------------------------------------------------------------------------
//******************** COMMENTS *******************************************
//-------------------------------------------------------------------------
/*
** General ReplayTV Notes
**
**  The RTV4K/5K platforms are capable of supporting NTSC, PAL and SECAM so
**  in theory there may be one or more bytes always set a certain way as they
**  reflect NTSC.
**
**  On the subject of recorded time: The ReplayTV units appear to 
**  automatically pad shows by 3 seconds.   e.g. If you schedule a show that
**  airs at 6:00 PM, the recording will  actually begin at 5:59:57 PM.
**  Because of this, even an unpadded show will have a 3 second discrepency
**  between ReplayShow.recorded and ReplayShow.eventtime.
**
**
** Categories
**
**  There appears to be an *internal* maximum of 32 but the units appear
**  to only use 17 total.
** 
** Secondary Offset (GuideHeader)
**
**  If you jump to this absolute location this repeats the first 12 bytes of 
**  this header, null terminates and then 4 unknown DWORDs. 
**
**  You will then be at the first byte of the first ReplayChannel  record.
**
**
** Genre Codes
**  Todd Larason's been slowly figuring these out, you can see the list at
**  http://www.molehill.org/twiki/bin/view/Replay/RnsGetCG2
**
*/

/**************************************************************************
**
** GUIDE HEADER
**
**
** Most of the  unknowns in the GuideHeader are garbage/junk.   Why 
** these aren't initialized to null I have no idea.
**
** They may also be a memory structure that has no meaning on a 'remote unit'.
**
**************************************************************************/

typedef struct tagGuideSnapshotHeader { 
    WORD osversion;             // OS Version (5 for 4.5, 3 for 4.3, 0 on 5.0)
    WORD snapshotversion;       // Snapshot Version (1) (2 on 5.0)    This might be better termed as snapshot version
    DWORD structuresize;		// Should always be 64
    DWORD unknown1;		        // 0x00000002
    DWORD unknown2;             // 0x00000005
    DWORD channelcount;		    // Number of Replay Channels
    DWORD channelcountcheck;    // Should always be equal to channelcount, it's incremented as the snapshot is built.
    DWORD unknown3;             // 0x00000000
    DWORD groupdataoffset;		// Offset of Group Data
    DWORD channeloffset;		// Offset of First Replay Channel  (If you don't care about categories, jump to this)
    DWORD showoffset;			// Offset of First Replay Show (If you don't care about ReplayChannels, jump to this)
    DWORD snapshotsize;         // Total Size of the Snapshot
    DWORD unknown4;             // 0x00000001
    DWORD unknown5;             // 0x00000004
    DWORD flags;	    		// Careful, this is uninitialized, ignore invalid bits
    DWORD unknown6;             // 0x00000002
    DWORD unknown7;             // 0x00000000
} GuideSnapshotHeader; 

typedef struct tagGroupData {
    DWORD structuresize;	    // Should always be 776
    DWORD categories;			// Number of Categories (MAX: 32 [ 0 - 31 ] )
    DWORD category[32];			// category lookup 2 ^ number, position order = text
    DWORD categoryoffset[32];	// Offsets for the GuideHeader.catbuffer
    char catbuffer[512];		// GuideHeader.categoryoffset contains the starting position of each category.
} GroupData;


typedef struct tagGuideHeader {
    struct tagGuideSnapshotHeader GuideSnapshotHeader;
    struct tagGroupData GroupData;
} GuideHeader;


/*******************************************************************************
**
** SUB-STRUCTURES
**
*******************************************************************************/

//-------------------------------------------------------------------------
// Theme Info Structure

typedef struct tagThemeInfo {
    DWORD flags;                  	// Search Flags
    DWORD suzuki_id;              	// Suzuki ID
    DWORD thememinutes;           	// Minutes Allocated
    char szSearchString[48];    	// Search Text
} ThemeInfo;


//-------------------------------------------------------------------------
// Movie Extended Info Structure

typedef struct tagMovieInfo {
    WORD mpaa;                 		// MPAA Rating
    WORD stars;                		// Star Rating * 10  (i.e. value of 20 = 2 stars)
    WORD year;                 		// Release Year
    WORD runtime;              		// Strange HH:MM format
} MovieInfo;


//-------------------------------------------------------------------------
// Has Parts Info Structure

typedef struct tagPartsInfo {
    WORD partnumber;           		// Part X [Of Y]
    WORD maxparts;            		// [Part X] Of Y
} PartsInfo;


//--------------------------------------------------------------------------
// Program Info Structure

typedef struct tagProgramInfo { 
    DWORD structuresize;		// Should always be 272 (0x0110)
    DWORD autorecord;			// If non-zero it is an automatic recording, otherwise it is a manual recording.
    DWORD isvalid;		    	// Not sure what it actually means! (should always be 1 in exported guide)
    DWORD tuning;			    // Tuning (Channel Number) for the Program
    DWORD flags;		    	// Program Flags 
    DWORD eventtime;			// Scheduled Time of the Show
    DWORD tmsID;			    // Tribune Media Services ID (inherited from ChannelInfo)
    WORD minutes;			    // Minutes (add with show padding for total)
    BYTE genre1;                // Genre Code 1
    BYTE genre2;                // Genre Code 2
    BYTE genre3;                // Genre Code 3
    BYTE genre4;                // Genre Code 4
    WORD recLen;      	        // Record Length of Description Block
    BYTE titleLen;			    // Length of Title
    BYTE episodeLen;		    // Length of Episode
    BYTE descriptionLen;		// Length of Description
    BYTE actorLen;			    // Length of Actors
    BYTE guestLen;			    // Length of Guest
    BYTE suzukiLen;			    // Length of Suzuki String (Newer genre tags)
    BYTE producerLen;			// Length of Producer
    BYTE directorLen;			// Length of Director
    char szDescription[228];	// This can have parts/movie sub-structure
} ProgramInfo;	


//--------------------------------------------------------------------------
// Channel Info Structure

typedef struct tagChannelInfo {
    DWORD structuresize;		// Should always be 80 (0x50)
    DWORD usetuner;			    // If non-zero the tuner is used, otherwise it's a direct input.
    DWORD isvalid;			    // Record valid if non-zero
    DWORD tmsID;			    // Tribune Media Services ID
    WORD channel;			    // Channel Number
    BYTE device;			    // Device
    BYTE tier;                  // Cable/Satellite Service Tier
    char szChannelName[16];		// Channel Name
    char szChannelLabel[32];	// Channel Description
    char cablesystem[8];		// Cable system ID
    DWORD channelindex;			// Channel Index (USUALLY tagChannelinfo.channel repeated)
} ChannelInfo;


/*******************************************************************************
**
** REPLAY SHOW
**
*******************************************************************************/

typedef struct tagReplayShow { 
    DWORD created;			        	// ReplayChannel ID (tagReplayChannel.created)
    DWORD recorded;			        	// Filename/Time of Recording (aka ShowID)
    DWORD inputsource;		        	// Replay Input Source
    DWORD quality;			         	// Recording Quality Level
    DWORD guaranteed;		    		// (0xFFFFFFFF if guaranteed)
    DWORD playbackflags;	    		// Not well understood yet.
    struct tagChannelInfo ChannelInfo;
    struct tagProgramInfo ProgramInfo;
    DWORD ivsstatus;		    		// Always 1 in a snapshot outside of a tagReplayChannel
    DWORD guideid;		        		// The show_id on the original ReplayTV for IVS shows.  Otherwise 0
    DWORD downloadid;   	    		// Valid only during actual transfer of index or mpeg file; format/meaning still unknown 
    DWORD timessent;		    		// Times sent using IVS
    DWORD seconds;		        		// Show Duration in Seconds (this is the exact actual length of the recording)
    DWORD GOP_count;		    		// MPEG Group of Picture Count
    DWORD GOP_highest;	     			// Highest GOP number seen, 0 on a snapshot.
    DWORD GOP_last;		         		// Last GOP number seen, 0 on a snapshot.
    DWORD checkpointed;	     			// 0 in possibly-out-of-date in-memory copies; always -1 in snapshots 
    DWORD intact;				        // 0xffffffff in a snapshot; 0 means a deleted show 
    DWORD upgradeflag;	    		    // Always 0 in a snapshot
    DWORD instance;		    		    // Episode Instance Counter (0 offset)
    WORD unused;	            		// Not preserved when padding values are set, presumably not used 
    BYTE beforepadding;	    			// Before Show Padding
    BYTE afterpadding;	    			// After Show Padding
    DWORD64 indexsize;	          		// Size of NDX file (IVS Shows Only)
    DWORD64 mpegsize;	    			// Size of MPG file (IVS Shows Only)
    char szReserved[68];	    		// Show Label 
} ReplayShow; 

             
/*******************************************************************************
**
** REPLAY CHANNEL
**
*******************************************************************************/

typedef struct tagReplayChannel { 
    struct tagReplayShow ReplayShow;
    struct tagThemeInfo ThemeInfo;
    DWORD created;			    	// Timestamp Entry Created
    DWORD category;			    	// 2 ^ Category Number
    DWORD channeltype;		    	// Channel Type (Recurring, Theme, Single)
    DWORD quality;		    		// Recording Quality (High, Medium, Low)
    DWORD stored;				    // Number of Episodes Recorded
    DWORD keep;				    	// Number of Episodes to Keep
    BYTE daysofweek;		    	// Day of the Week to Record Bitmask (Sun = LSB and Sat = MSB)
    BYTE afterpadding;	 	    	// End of Show Padding
    BYTE beforepadding;	  	        // Beginning of Show Padding
    BYTE flags;		    	    	// ChannelFlags   
    DWORD64 timereserved;	        // Total Time Allocated (For Guaranteed Shows)
    char szShowLabel[48];	    	// Show Label 
    DWORD unknown1;			    	// Unknown (Reserved For Don array)
    DWORD unknown2;			    	// Unknown
    DWORD unknown3;			    	// Unknown
    DWORD unknown4;			    	// Unknown
    DWORD unknown5;			    	// Unknown
    DWORD unknown6;			    	// Unknown
    DWORD unknown7;			    	// Unknown
    DWORD unknown8;			    	// Unknown
    DWORD64 allocatedspace;	        // Total Space Allocated
    DWORD unknown9;			    	// Unknown
    DWORD unknown10;			   	// Unknown
    DWORD unknown11;			    // Unknown
    DWORD unknown12;			    // Unknown
} ReplayChannel; 


//-------------------------------------------------------------------------
// WIN32 Functions
//-------------------------------------------------------------------------

#ifdef WIN32

//-------------------------------------------------------------------------
void UnixTimeToFileTime(time_t t, LPFILETIME pft)
{
    LONGLONG ll;
    
    ll = Int32x32To64(t, 10000000) + 116444736000000000;
    pft->dwLowDateTime = (DWORD)ll;
    pft->dwHighDateTime = (DWORD)(ll >> 32);
}


//-------------------------------------------------------------------------
void UnixTimeToSystemTime(time_t t, LPSYSTEMTIME pst)
{
    FILETIME ft;
    
    UnixTimeToFileTime(t, &ft);
    FileTimeToSystemTime(&ft, pst);
}

//-------------------------------------------------------------------------
char * UnixTimeToString(time_t t)
{
    static char sbuf[17];
    char szTimeZone[32];
    SYSTEMTIME st;
    int tzbias;
    
    _tzset();
    
    memset(szTimeZone,0,sizeof(szTimeZone));
    
    MoveMemory(szTimeZone,_tzname[0],1);		
    strcat(szTimeZone,"T");
    
    tzbias = _timezone;
    
    UnixTimeToSystemTime(t - tzbias, &st);
    if (st.wSecond == 0) 
    {
        sprintf(sbuf, "%d-%.2d-%.2d %.2d:%.2d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    }
    else
    {
        sprintf(sbuf, "%d-%.2d-%.2d %.2d:%.2d:%.2d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    }
    return sbuf;
}


#endif

//-------------------------------------------------------------------------
// UNIX Functions
//-------------------------------------------------------------------------

#ifdef __unix__
char * UnixTimeToString(time_t t)
{
    static char sbuf[17];
    struct tm * tm;
    
    tm = localtime(&t);
    strftime(sbuf, sizeof(sbuf), "%Y-%m-%d %H:%M:%S", tm);
    return sbuf;
}
#endif


//-------------------------------------------------------------------------
// Common Functions
//-------------------------------------------------------------------------


//-------------------------------------------------------------------------
// Lookup Tables
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
void DisplayDaysOfWeek(int dayofweek, char *szDisplayString)
{
    strcpy( szDisplayString, "");
    
    if ( dayofweek & ceSunday )
    {
        strcat(szDisplayString, "SU ");
    }
    
    if ( dayofweek & ceMonday )
    {
        strcat(szDisplayString, "MO ");
    }
    
    if ( dayofweek & ceTuesday )
    {
        strcat(szDisplayString, "TU ");
    }
    
    if ( dayofweek & ceWednesday )
    {
        strcat(szDisplayString, "WE ");
    }
    
    if ( dayofweek & ceThursday )
    {
        strcat(szDisplayString, "TH ");
    }
    
    if ( dayofweek & ceFriday )
    {
        strcat(szDisplayString, "FR ");
    }
    
    if ( dayofweek & ceSaturday )
    {
        strcat(szDisplayString, "SA");
    }
    
    if ( dayofweek == 127) {
        // Display something less ugly!
        
        memset(szDisplayString,0,sizeof(szDisplayString));
        strcpy(szDisplayString, "Everyday");
    }
}

//-------------------------------------------------------------------------
void DisplayQuality(int quality, char *szQuality)
{
    switch( quality )	
    {
    case 0:
        strcpy( szQuality, "High" );
        break;
    case 1:
        strcpy( szQuality, "Medium" );
        break;
    case 2:
        strcpy( szQuality, "Standard" );
        break;
    default:
        strcpy( szQuality, "" );
        break;
    }
}

//-------------------------------------------------------------------------
void DisplayInputSource(int source, char *szInputSource)
{
    switch ( source )
    {
    case 0:
        strcpy( szInputSource, "Direct RF");
        break;
    case 1:
        strcpy( szInputSource, "Direct Line 1");
        break;    
    case 2:
        strcpy( szInputSource, "Direct Line 2");
        break;
    case 3:
        strcpy( szInputSource, "Tuner");
        break;
    case 16:
        strcpy( szInputSource, "Pre-loaded");
        break;
    default:
        strcpy( szInputSource, "" );
        break;
    }
}

//-------------------------------------------------------------------------
/*
** NOTE: This is a partial implementation of genre lokup.
** There are still a lot of these that aren't known.
** 
*/
void DisplayGenre(unsigned short genre, char *szGenreText, bool append)
{

    if (append == false)
    {
        strcpy( szGenreText, "");
    }
    else
    {
        if (strlen(szGenreText) > 0)
        {
            strcat( szGenreText, " ");
        }

    }

    switch ( genre )
    {
    case 1:
        strcat( szGenreText, "Action." );
        break;
    case 2:
        strcat( szGenreText, "Adult." );
        break;
    case 3:
        strcat( szGenreText, "Adventure." );
        break;
    case 4:
        strcat( szGenreText, "Animals." );
        break;
    case 5:
        strcat( szGenreText, "Animated." );
        break;
    case 8:
        strcat( szGenreText, "Art." );
        break;
    case 9:
        strcat( szGenreText, "Automotive." );
        break;
    case 10:
        strcat( szGenreText, "Award Show." );
        break;
    case 12:
        strcat( szGenreText, "Baseball." );
        break;
    case 13:
        strcat( szGenreText, "Baseketball." );
        break;
    case 14:
        strcat( szGenreText, "Beauty." );
        break;
    case 15:
        strcat( szGenreText, "Bicycling." );
        break;
    case 16:
        strcat( szGenreText, "Billiards." );
        break;
    case 17:
        strcat( szGenreText, "Biography." );
        break;
    case 18:
        strcat( szGenreText, "Boating." );
        break;
    case 19:
        strcat( szGenreText, "Body Building." );
        break;
    case 21:
        strcat( szGenreText, "Boxing." );
        break;
    case 26:
        strcat( szGenreText, "Children." );
        break;
    case 30:
        strcat( szGenreText, "Classic TV." );
        break;
    case 31:
        strcat( szGenreText, "Collectibles." );
        break;
    case 32:
        strcat( szGenreText, "Comedy." );
        break;
    case 33:
        strcat( szGenreText, "Comedy-Drama." );
        break;
    case 34:
        strcat( szGenreText, "Computers." );
        break;
    case 35:
        strcat( szGenreText, "Cooking." );
        break;
    case 36:
        strcat( szGenreText, "Crime." );
        break;
    case 37:
        strcat( szGenreText, "Crime Drama." );
        break;
    case 39:
        strcat( szGenreText, "Dance." );
        break;
    case 43:
        strcat( szGenreText, "Drama." );
        break;
    case 44:
        strcat( szGenreText, "Educational." );
        break;
    case 45:
        strcat( szGenreText, "Electronics." );
        break;
    case 47:
        strcat( szGenreText, "Exercise." );
        break;
    case 48:
        strcat( szGenreText, "Family." );
        break;
    case 49:
        strcat( szGenreText, "Fantasy." );
        break;
    case 50:
        strcat( szGenreText, "Fashion." );
        break;
    case 52:
        strcat( szGenreText, "Fishing." );
        break;
    case 53:
        strcat( szGenreText, "Football." );
        break;
    case 55:
        strcat( szGenreText, "Fundraising." );
        break;
    case 56:
        strcat( szGenreText, "Game Show." );
        break;
    case 57:
        strcat( szGenreText, "Golf." );
        break;
    case 58:
        strcat( szGenreText, "Gymnastics." );
        break;
    case 59:
        strcat( szGenreText, "Health." );
        break;
    case 60:
        strcat( szGenreText, "History." );
        break;
    case 61:
        strcat( szGenreText, "Historical Drama." );
        break;
    case 62:
        strcat( szGenreText, "Hockey." );
        break;
    case 68:
        strcat( szGenreText, "Holiday." );
        break;
    case 69:
        strcat( szGenreText, "Horror." );
        break;
    case 70:
        strcat( szGenreText, "Horses." );
        break;
    case 71:
        strcat( szGenreText, "Home & Garden." );
        break;
    case 72:
        strcat( szGenreText, "Housewares." );
        break;
    case 73:
        strcat( szGenreText, "How-To." );
        break;
    case 74:
        strcat( szGenreText, "International." );
        break;
    case 75:
        strcat( szGenreText, "Interview." );
        break;
    case 76:
        strcat( szGenreText, "Jewelry." );
        break;
    case 77:
        strcat( szGenreText, "Lacross." );
        break;
    case 78:
        strcat( szGenreText, "Magazine." );
        break;
    case 79:
        strcat( szGenreText, "Martial Arts." );
        break;
    case 80:
        strcat( szGenreText, "Medical." );
        break;
    case 83:
        strcat( szGenreText, "Motorcycles." );
        break;
    case 90:
        strcat( szGenreText, "Mystery." );
        break;
    case 91:
        strcat( szGenreText, "Nature." );
        break;
    case 92:
        strcat( szGenreText, "News." );
        break;
    case 94:
        strcat( szGenreText, "Olympics." );
        break;
    case 96:
        strcat( szGenreText, "Outdoors." );
        break;
    case 99:
        strcat( szGenreText, "Public Affairs." );
        break;
    case 100:
        strcat( szGenreText, "Racing." );
        break;
    case 102:
        strcat( szGenreText, "Reality Show." );
        break;
    case 103:
        strcat( szGenreText, "Religious." );
        break;
    case 104:
        strcat( szGenreText, "Rodeo." );
        break;
    case 105:
        strcat( szGenreText, "Romance." );
        break;
    case 106:
        strcat( szGenreText, "Romantic Comedy." );
        break;
    case 107:
        strcat( szGenreText, "Rugby." );
        break;
    case 108:
        strcat( szGenreText, "Running." );
        break;
    case 109:
        strcat( szGenreText, "Satire." );
        break;
    case 110:
        strcat( szGenreText, "Science." );
        break;
    case 111:
        strcat( szGenreText, "Science Fiction." );
        break;
    case 113:
        strcat( szGenreText, "Shopping." );
        break;
    case 114:
        strcat( szGenreText, "Sitcom." );
        break;
    case 115:
        strcat( szGenreText, "Skating." );
        break;
    case 116:
        strcat( szGenreText, "Skiing." );
        break;
    case 117:
        strcat( szGenreText, "Sleg Dog." );
        break;
    case 118:
        strcat( szGenreText, "Snow Sports." );
        break;
    case 119:
        strcat( szGenreText, "Soap Opera." );
        break;
    case 123:
        strcat( szGenreText, "Soccer." );
        break;
    case 125:
        strcat( szGenreText, "Spanish." );
        break;
    case 131:
        strcat( szGenreText, "Suspense." );
        break;
    case 132:
        strcat( szGenreText, "Suspense Comedy." );
        break;
    case 133:
        strcat( szGenreText, "Swimming." );
        break;
    case 134:
        strcat( szGenreText, "Talk Show." );
        break;
    case 135:
        strcat( szGenreText, "Tennis." );
        break;
    case 136:
        strcat( szGenreText, "Thriller." );
        break;
    case 137:
        strcat( szGenreText, "Track and Field." );
        break;
    case 138:
        strcat( szGenreText, "Travel." );
        break;
    case 139:
        strcat( szGenreText, "Variety." );
        break;
    case 141:
        strcat( szGenreText, "War." );
        break;
    case 143:
        strcat( szGenreText, "Weather." );
        break;
    case 144:
        strcat( szGenreText, "Western." );
        break;
    case 146:
        strcat( szGenreText, "Wrestling." );
        break;
    default:
        strcat( szGenreText, "" );
        break;
    }
    
}

//-------------------------------------------------------------------------
void DisplayDevice(BYTE device, char *szDeviceName)
{
    switch ( device )
    {
    case 64:
        strcpy( szDeviceName, "Standard");
        break;
    case 65:
        strcpy( szDeviceName, "A Lineup");
        break;
    case 66:
        strcpy( szDeviceName, "B Lineup");
        break;
    case 68:
        strcpy( szDeviceName, "Rebuild Lineup");
        break;
    case 71:
        strcpy( szDeviceName, "Non-Addressable Converter");
        break;
    case 72:
        strcpy( szDeviceName, "Hamlin");
        break;
    case 73:
        strcpy( szDeviceName, "Jerrold Impulse");
        break;
    case 74:
        strcpy( szDeviceName, "Jerrold");
        break;
    case 76:
        strcpy( szDeviceName, "Digital Rebuild");
        break;
    case 77:
        strcpy( szDeviceName, "Multiple Converters");
        break;
    case 78:
        strcpy( szDeviceName, "Pioneer");
        break;
    case 79:
        strcpy( szDeviceName, "Oak");
        break;
    case 80:
        strcpy( szDeviceName, "Premium");
        break;
    case 82:
        strcpy( szDeviceName, "Cable-ready-TV");
        break;
    case 83:
        strcpy( szDeviceName, "Converter Switch");
        break;
    case 84:
        strcpy( szDeviceName, "Tocom");
        break;
    case 85:
        strcpy( szDeviceName, "A Lineup Cable-ready-TV");
        break;
    case 86:
        strcpy( szDeviceName, "B Lineup Cable-ready-TV");
        break;
    case 87:
        strcpy( szDeviceName, "Scientific-Atlanta");
        break;
    case 88:
        strcpy( szDeviceName, "Digital");
        break;
    case 90:
        strcpy( szDeviceName, "Zenith");
        break;
    default:
        strcpy( szDeviceName, "Unknown" );
        break;
    }
}

//-------------------------------------------------------------------------
void DisplayServiceTier(BYTE tier, char *szTierName)
{
    switch ( tier )
    {
    case 1:
        strcpy( szTierName, "Basic");
        break;
    case 2:
        strcpy( szTierName, "Expanded Basic");
        break;
    case 3:
        strcpy( szTierName, "Premium");
        break;
    case 4:
        strcpy( szTierName, "PPV");
        break;
    case 5:
        strcpy( szTierName, "DMX/Music");
        break;
    default:
        strcpy( szTierName, "Unknown" );
        break;
    }
}

//-------------------------------------------------------------------------
void DisplayExtendedTVRating(int tvrating, char *szRating, bool append)
{

    if (append == false)
    {
        strcpy( szRating, "");
    }
    
    if (tvrating &  0x00020000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }


        strcat( szRating, "S" );    // Sex
    }
    
    if (tvrating &  0x00040000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }


        strcat( szRating, "V" );    // Violence
    }
    
    if (tvrating &  0x00080000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }


        strcat( szRating, "L" );    // Language
    }
    
    if (tvrating &  0x00100000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }


        strcat( szRating, "D" );    // Drug Use
    }
    
    if (tvrating &  0x00200000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }


        strcat( szRating, "F" );    // Fantasy Violence
    }
    
}


//-------------------------------------------------------------------------
void DisplayIVSStatus(int ivsstatus, char *szIVSStatus)
{
    switch ( ivsstatus )
    {
    case 0:
	strcpy( szIVSStatus, "Local" );
	break;
    case 1:
	strcpy( szIVSStatus, "LAN" );
	break;
    case 2:
	strcpy( szIVSStatus, "Internet Downloadable" );
	break;
    case 3:
	strcpy( szIVSStatus, "Internet Download Failed" );
	break;
    case 4:
	strcpy( szIVSStatus, "Internet Index Download Restart" );
	break;
    case 5:
	strcpy( szIVSStatus, "Internet Index Downloading" );
	break;
    case 6:
	strcpy( szIVSStatus, "Internet Index Download Complete" );
	break;
    case 7:
	strcpy( szIVSStatus, "Internet MPEG Download Restart" );
	break;
    case 8:
	strcpy( szIVSStatus, "Internet MPEG Downloading" );
	break;
    case 9:
	strcpy( szIVSStatus, "Internet MPEG Download Complete" );
	break;
    case 10:
	strcpy( szIVSStatus, "Internet File Not Found  " );
	break;
    default:
        strcpy( szIVSStatus, "" );
        break;
    }

    

}

//-------------------------------------------------------------------------
void DisplayTVRating(int tvrating, char *szRating)
{
    strcpy( szRating, "");
    
    if (tvrating &  0x00008000)
    {
        strcpy( szRating, "TV-Y" );
    }
    
    if (tvrating &  0x00010000)
    {
        strcpy( szRating, "TV-Y7" );
    }
    
    if (tvrating &  0x00001000)
    {
        strcpy( szRating, "TV-G" );
    }
    
    if (tvrating &  0x00004000)
    {
        strcpy( szRating, "TV-PG" );
    }
    
    if (tvrating &  0x00002000)
    {
        strcpy( szRating, "TV-MA" );
    }
    
    if (tvrating &  0x00000800)
    {
        strcpy( szRating, "TV-14" );
    }
    
}

//-------------------------------------------------------------------------
void DisplayExtendedMPAARating(int mpaarating, char *szRating, bool append)
{
    if (append == false)
    {
        strcpy( szRating, "");
    }
    
    if (mpaarating & 0x00400000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "AC" );    // Adult Content
    }

    if (mpaarating & 0x00800000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "BN" );   // Brief Nudity
    }

    if (mpaarating & 0x01000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "GL" );   // Graphic Language
    }

    if (mpaarating & 0x02000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "GV" );   // Graphic Violence
    }

    if (mpaarating & 0x04000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "AL" );   // Adult Language
    }

    if (mpaarating & 0x08000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "MV" );   // Mild Violence
    }

    if (mpaarating & 0x10000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "N" );    // Nudity
    }

    if (mpaarating & 0x20000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "RP" );   // Rape
    }

    if (mpaarating & 0x40000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "SC " );   // Sexual Content
    }

    if (mpaarating & 0x80000000)
    {
        if (strlen(szRating) > 0)
        {
            strcat( szRating, " ");
        }

        strcat( szRating, "V" );    // Violence
    }

}


//-------------------------------------------------------------------------
void DisplayMPAARating(int mpaarating, char *szRating)
{
    strcpy( szRating, "");

    if (mpaarating & 0x00000001)
    {
        strcat( szRating, "AO" );       // unverified
    }

    if (mpaarating & 0x00000002)
    {
        strcat( szRating, "G" );   
    }

    if (mpaarating & 0x00000004)
    {
        strcat( szRating, "NC-17" );    
    }

    if (mpaarating & 0x00000008)
    {
        strcat( szRating, "NR" );   
    }

    if (mpaarating & 0x00000010)
    {
        strcat( szRating, "PG" );   
    }

    if (mpaarating & 0x00000020)
    {
        strcat( szRating, "PG-13" );   
    }

    if (mpaarating & 0x00000040)
    {
        strcat( szRating, "R" );   
    }

}



//-------------------------------------------------------------------------
void DisplayType(int channeltype, char *szChannelType)
{
    switch( channeltype )	
    {
    case 1:
        strcpy( szChannelType, "Recurring");
        break;
    case 2:
        strcpy( szChannelType, "Theme");
        break;
    case 3:
        strcpy( szChannelType, "Single");
        break;
    case 4:
        strcpy( szChannelType, "Zone");
        break;
    default:
        strcpy( szChannelType, "");
        break;
    }
}

//-------------------------------------------------------------------------
// Data Presentation Functions
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
void ConvertCodepage(char *szString)
{
    bool exitloop = false;
    unsigned int i = 0;
    char ch;
    
    if (szString[0] == 0) 
    {
        return;
    }
    
    for( i = 0; i < strlen(szString); ++i )
    {
        ch = szString[i];   
        
        // Windows Codepage Translation to DOS/UNIX etc
        
        if (ch == (char)146)
        {
            szString[i] = '\'';
        }
        
        if (ch == (char)147)
        {
            szString[i] = '\"';
        }
        
        if (ch == (char)148)
        {
            szString[i] = '\"';
        }

        
#ifdef WIN32
        // Windows (Console) specific translations         
#endif
        
#ifdef __unix__
        // UNIX specific translations         
#endif
        
    }
    
    return;
    
}

//-------------------------------------------------------------------------
int FindLastSpace(char *szString)
{
    char szSpace[] = " ";
    char *pdest;
    int result = 0;
    
    bool exitloop = false;
    int retval = 0;
    
    if (szString[0] == 0)
    {
        return retval;
    }
    
    
    do
    {
        pdest = strstr( szString + retval, szSpace );
        if (pdest != NULL)
        {
            result = pdest - szString + 1;
            retval = result;
        }
        else
        {
            exitloop = true;
        }
        
    } while (exitloop == false);
    
    return retval;
    
}


//-------------------------------------------------------------------------
void WrapDisplay(char *szMargin, char *szString, bool fmodeWrapDisplay)
{
    unsigned int screenwidth = 80;
    unsigned int curline = 0, curchar = 0, leftmargin = 0, i = 0;
    
    char szLine[256];
    bool exitloop = false;
    
    leftmargin = strlen(szMargin);
    
    if (screenwidth < leftmargin) 
    {
        screenwidth = leftmargin;
    }
    
    if (screenwidth > 256) 
    {
        screenwidth = 256;
    }
    
    if (!fmodeWrapDisplay)
    {
        printf("%s%s\n",szMargin,szString);
        return;
    }
    
    if (szString[0] == 0) 
    {
        // Nothing to do!
        
        return;
    }
    
    do
    {
        memset(szLine,0,sizeof(szLine));
        
        // if the first character is a space, skip it
        
        if (szString[curchar] == 32) 
        {
            curchar++;
        }
        
        // copy the next chunk (this could be improved to not break up words)
        
        strncpy(szLine,szString + curchar,(screenwidth - 1 - leftmargin));
        i = FindLastSpace(szLine);
        if ((i > 0) && (strlen(szLine) == (screenwidth - 1 - leftmargin)))
        {
            szLine[i] = 0;
        }
        curchar = curchar + strlen(szLine);
        
        if (szLine[0] != 0)
        {
            
            curline++;
            printf("%s%s\n",szMargin,szLine);
            if (curline == 1)
            {
                // We only want to display the heading (szMargin) on the first line.
                
                memset(szMargin,0,sizeof(&szMargin));
                memset(szMargin,' ',leftmargin);
            }
        }
        else
        {
            exitloop = true;
        }
        
    } while (exitloop == false);
    
    return;
}

//-------------------------------------------------------------------------
// Misc. Functions
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
int CalculateMinutes( int seconds )
{
    int original = seconds;
    int retval = 0;
    double result1 = 0.0;
    double result2 = 0.0;
    
    if (seconds < 1) 
    {
        return 0;
    }
    
    result1 = seconds / 60;
    result2 = seconds % 60;
    
    if (result2 > .0)                   // this will round up if there's any remainder, 
        // could also make this .5 etc
    {
        retval = (int)result1 + 1;
    }
    else
    {
        retval = (int)result1;
    }
    
    return retval;
    
}

//-------------------------------------------------------------------------
// ENDIAN CONVERSION FUNCTIONS
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
// LONGLONG/INT64 Endian Conversion
//-------------------------------------------------------------------------

//-------------------------------------------------------------------------
DWORD64 ntohll(DWORD64 llValue)
{
    DWORD64 retval = 0;

    // This is really cheesy but it works so that's all that matters 
    // If someone out there wants to replace this with something cooler,
    // please feel free to do so!

    char szBuffer[17];
    char szInt64[17];

    memset(szBuffer,0,sizeof(szBuffer));
    memset(szInt64,0,sizeof(szInt64));

    MoveMemory(szBuffer,&llValue,sizeof(llValue));
            
    int cc = sizeof(llValue) - 1;
    int i = 0;
            
    for( i = 0; i < sizeof(llValue); ++i )
    {
        szInt64[i] = szBuffer[cc];
        --cc;
    }
            
    MoveMemory(&retval,szInt64,sizeof(retval));

    return retval;
}

//-------------------------------------------------------------------------
void ConvertThemeInfoEndian(struct tagThemeInfo * strThemeInfo)
{
    strThemeInfo->flags = ntohl(strThemeInfo->flags);
    strThemeInfo->suzuki_id = ntohl(strThemeInfo->suzuki_id);
    strThemeInfo->thememinutes = ntohl(strThemeInfo->thememinutes);

    return;
}

//-------------------------------------------------------------------------
void ConvertChannelInfoEndian(struct tagChannelInfo * strChannelInfo)
{
    strChannelInfo->channel = ntohs(strChannelInfo->channel);
    strChannelInfo->channelindex = ntohl(strChannelInfo->channelindex);
    strChannelInfo->isvalid = ntohl(strChannelInfo->isvalid);
    strChannelInfo->structuresize = ntohl(strChannelInfo->structuresize);
    strChannelInfo->tmsID = ntohl(strChannelInfo->tmsID);
    strChannelInfo->usetuner = ntohl(strChannelInfo->usetuner);

    return;
}

//-------------------------------------------------------------------------
void ConvertProgramInfoEndian(struct tagProgramInfo * strProgramInfo)
{

    strProgramInfo->autorecord = ntohl(strProgramInfo->autorecord);
    strProgramInfo->eventtime = ntohl(strProgramInfo->eventtime);
    strProgramInfo->flags = ntohl(strProgramInfo->flags);
    strProgramInfo->isvalid = ntohl(strProgramInfo->isvalid);
    strProgramInfo->minutes = ntohs(strProgramInfo->minutes);
    strProgramInfo->recLen = ntohs(strProgramInfo->recLen);
    strProgramInfo->structuresize = ntohl(strProgramInfo->structuresize);
    strProgramInfo->tuning = ntohl(strProgramInfo->tuning);
    strProgramInfo->tmsID = ntohl(strProgramInfo->tmsID);

    return;
}

//-------------------------------------------------------------------------
void ConvertMovieInfoEndian(struct tagMovieInfo * strMovieInfo)
{
    strMovieInfo->mpaa = ntohs(strMovieInfo->mpaa);
    strMovieInfo->runtime = ntohs(strMovieInfo->runtime);
    strMovieInfo->stars = ntohs(strMovieInfo->stars);
    strMovieInfo->year = ntohs(strMovieInfo->year);

    return;
}

//-------------------------------------------------------------------------
void ConvertPartsInfoEndian(struct tagPartsInfo * strPartsInfo)
{
    strPartsInfo->maxparts = ntohs(strPartsInfo->maxparts);
    strPartsInfo->partnumber = ntohs(strPartsInfo->partnumber);

    return;
}
//-------------------------------------------------------------------------
void ConvertReplayShowEndian(struct tagReplayShow * strReplayShow)
{
    strReplayShow->checkpointed = ntohl(strReplayShow->checkpointed);
    strReplayShow->created = ntohl(strReplayShow->created);
    strReplayShow->downloadid = ntohl(strReplayShow->downloadid);
    strReplayShow->GOP_count = ntohl(strReplayShow->GOP_count);    
    strReplayShow->GOP_highest = ntohl(strReplayShow->GOP_highest);    
    strReplayShow->GOP_last = ntohl(strReplayShow->GOP_last);    
    strReplayShow->guaranteed = ntohl(strReplayShow->guaranteed);
    strReplayShow->guideid = ntohl(strReplayShow->guideid);
    strReplayShow->indexsize = ntohll(strReplayShow->indexsize);
    strReplayShow->inputsource = ntohl(strReplayShow->inputsource);
    strReplayShow->instance = ntohl(strReplayShow->instance);
    strReplayShow->intact = ntohl(strReplayShow->intact);
    strReplayShow->ivsstatus = ntohl(strReplayShow->ivsstatus);
    strReplayShow->mpegsize = ntohll(strReplayShow->mpegsize);
    strReplayShow->playbackflags = ntohl(strReplayShow->playbackflags);
    strReplayShow->quality = ntohl(strReplayShow->quality);
    strReplayShow->recorded = ntohl(strReplayShow->recorded);
    strReplayShow->seconds = ntohl(strReplayShow->seconds);
    strReplayShow->timessent = ntohl(strReplayShow->timessent);
    strReplayShow->upgradeflag = ntohl(strReplayShow->upgradeflag);
    strReplayShow->unused = ntohs(strReplayShow->unused);

    return;
}

//-------------------------------------------------------------------------
void ConvertReplayChannelEndian(struct tagReplayChannel * strReplayChannel)
{
    strReplayChannel->created = ntohl(strReplayChannel->created);
    strReplayChannel->timereserved = ntohll(strReplayChannel->timereserved);
    strReplayChannel->allocatedspace = ntohll(strReplayChannel->allocatedspace);
    strReplayChannel->category = ntohl(strReplayChannel->category);
    strReplayChannel->channeltype = ntohl(strReplayChannel->channeltype);
    strReplayChannel->keep = ntohl(strReplayChannel->keep);
    strReplayChannel->stored = ntohl(strReplayChannel->stored);
    strReplayChannel->quality = ntohl(strReplayChannel->quality);

    {
        strReplayChannel->unknown1 = ntohl(strReplayChannel->unknown1);
        strReplayChannel->unknown2 = ntohl(strReplayChannel->unknown2);
        strReplayChannel->unknown3 = ntohl(strReplayChannel->unknown3);
        strReplayChannel->unknown4 = ntohl(strReplayChannel->unknown4);
        strReplayChannel->unknown5 = ntohl(strReplayChannel->unknown5);
        strReplayChannel->unknown6 = ntohl(strReplayChannel->unknown6);
        strReplayChannel->unknown7 = ntohl(strReplayChannel->unknown7);
        strReplayChannel->unknown8 = ntohl(strReplayChannel->unknown8);
        strReplayChannel->unknown9 = ntohl(strReplayChannel->unknown9);
        strReplayChannel->unknown10 = ntohl(strReplayChannel->unknown10);
        strReplayChannel->unknown11 = ntohl(strReplayChannel->unknown11);
        strReplayChannel->unknown12 = ntohl(strReplayChannel->unknown12);
    }

    return;
}

//-------------------------------------------------------------------------
void ConvertReplayGuideEndian(struct tagGuideHeader * strGuideHeader)
{
    
    strGuideHeader->GroupData.categories = ntohl(strGuideHeader->GroupData.categories);
    strGuideHeader->GroupData.structuresize = ntohl(strGuideHeader->GroupData.structuresize);

    strGuideHeader->GuideSnapshotHeader.snapshotsize = ntohl(strGuideHeader->GuideSnapshotHeader.snapshotsize);
    strGuideHeader->GuideSnapshotHeader.channeloffset = ntohl(strGuideHeader->GuideSnapshotHeader.channeloffset);
    strGuideHeader->GuideSnapshotHeader.flags = ntohl(strGuideHeader->GuideSnapshotHeader.flags);
    strGuideHeader->GuideSnapshotHeader.groupdataoffset = ntohl(strGuideHeader->GuideSnapshotHeader.groupdataoffset);
    strGuideHeader->GuideSnapshotHeader.osversion = ntohs(strGuideHeader->GuideSnapshotHeader.osversion);
    strGuideHeader->GuideSnapshotHeader.snapshotversion = ntohs(strGuideHeader->GuideSnapshotHeader.snapshotversion);
    strGuideHeader->GuideSnapshotHeader.channelcount = ntohl(strGuideHeader->GuideSnapshotHeader.channelcount);
    strGuideHeader->GuideSnapshotHeader.channelcountcheck = ntohl(strGuideHeader->GuideSnapshotHeader.channelcountcheck);
    strGuideHeader->GuideSnapshotHeader.showoffset = ntohl(strGuideHeader->GuideSnapshotHeader.showoffset);

    strGuideHeader->GuideSnapshotHeader.unknown1 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown1);
    strGuideHeader->GuideSnapshotHeader.unknown2 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown2);
    strGuideHeader->GuideSnapshotHeader.unknown3 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown3);
    strGuideHeader->GuideSnapshotHeader.unknown4 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown4);
    strGuideHeader->GuideSnapshotHeader.unknown5 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown5);
    strGuideHeader->GuideSnapshotHeader.unknown6 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown6);
    strGuideHeader->GuideSnapshotHeader.unknown7 = ntohl(strGuideHeader->GuideSnapshotHeader.unknown7);

    for( unsigned int cc = 0; cc < strGuideHeader->GroupData.categories; ++cc )
    {
        strGuideHeader->GroupData.category[cc] = ntohl(strGuideHeader->GroupData.category[cc]);
        strGuideHeader->GroupData.categoryoffset[cc] = ntohl(strGuideHeader->GroupData.categoryoffset[cc]);
    }
    

    {
        strGuideHeader->GuideSnapshotHeader.structuresize = ntohl(strGuideHeader->GuideSnapshotHeader.structuresize);
    }

    return;
}

//-------------------------------------------------------------------------
// GuideParser
//-------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    FILE *guidefile;
    FILEPOS fptr, junkptr;
    time_t endrecord, beginrecord;
    bool searchrecording, moreshows, receivedshow;
    bool isguaranteed, isrepeat, isletterbox, ismovie, hasparts, iscc, isppv, isstereo, issap, isnorepeats;
    char szCategory[10], szPathname[260], szBuffer[1024], szQuality[16];
    char szChannelType[16], szTimeZone[32], szRecordDays[32];
    char szShowDescription[256], szEpisodeTitle[128], szActors[128];
    char szSuzuki[128], szDirectors[128], szGuests[128], szProducers[128];
    char szStarRating[6], szMPAARating[10], szOriginalDescriptionBuffer[226];
    char szTVRating[10], szHeading[80], szIVSStatus[64];
    char szShowTitle[128], szOutputBuffer[256], szInputSource[64];
    char szServiceTier[64], szDeviceDescription[64], szExtTV[64];
    char szExtMPAA[64], szGenre[256];
    int i, maxshow, curshow, cc, ccount, j;
    CategoryArray strReplayCategory[32];
    ReplayShow strReplayShow;
    ReplayChannel strReplayChannel;
    ChannelArray Channels[MAXCHANNELS];
    ChannelArray strChannel;
    GuideHeader strGuideHeader;
    MovieInfo strMovieInfo;
    PartsInfo strPartsInfo;
    ThemeInfo strThemeInfo;
    
    bool fmodeSTDIN = true;
    bool fmodeExtra = false;
    bool fmodeDebug = false;
    bool fmodeList = false;
    bool fmodeListShows = false;
    bool fmodeListChannels = false;
    bool fmodeShowHelp = false;
    bool fmodeWrapDisplay = true;
    bool fmodeDetailShowListings = false;     
    
    // Initialize
    
    memset(szPathname,0,sizeof(szPathname));
    memset(szBuffer,0,sizeof(szBuffer));
    memset(szQuality,0,sizeof(szQuality));
    memset(szChannelType,0,sizeof(szChannelType));
    memset(szTimeZone,0,sizeof(szTimeZone));
    memset(szEpisodeTitle,0,sizeof(szEpisodeTitle));
    memset(szShowDescription,0,sizeof(szShowDescription));
    memset(szRecordDays,0,sizeof(szRecordDays));
    memset(szCategory,0,sizeof(szCategory));
    memset(szActors,0,sizeof(szActors));
    memset(szOriginalDescriptionBuffer,0,sizeof(szOriginalDescriptionBuffer));
    memset(szGuests,0,sizeof(szGuests));
    memset(szProducers,0,sizeof(szProducers));
    memset(szDirectors,0,sizeof(szDirectors));
    memset(szSuzuki,0,sizeof(szSuzuki));
    memset(szStarRating,0,sizeof(szStarRating));
    memset(szMPAARating,0,sizeof(szMPAARating));
    memset(szInputSource,0,sizeof(szInputSource));
    memset(szHeading,0,sizeof(szHeading));
    memset(szOutputBuffer,0,sizeof(szOutputBuffer));
    memset(szServiceTier,0,sizeof(szServiceTier));
    memset(szDeviceDescription,0,sizeof(szDeviceDescription));
    memset(szExtTV,0,sizeof(szExtTV));
    memset(szExtMPAA,0,sizeof(szExtMPAA));
    memset(szGenre,0,sizeof(szGenre));
    memset(szIVSStatus,0,sizeof(szIVSStatus));

    ccount = 0;
    j = 0;
    
    for( cc = 0; cc < 32; ++cc )
    {
        memset(strReplayCategory[cc].szName,0,sizeof(strReplayCategory[cc].szName));
        strReplayCategory[cc].index = -1;
    }
    
    
    // Display Initial Header
    
    printf("%s\n",lpszTitle);
    printf("%s\n\n",lpszCopyright);
    printf("%s\n",lpszComment);
    
    // Structure Sizes

#ifdef SHOWSTRUCTURESIZES

    printf("struct strGuideHeader is %d bytes\n",sizeof(tagGuideHeader));
    printf("struct strMovieInfo is %d bytes\n",sizeof(tagMovieInfo));
    printf("struct strPartsInfo is %d bytes\n",sizeof(tagPartsInfo));
    printf("struct strThemeInfo is %d bytes\n",sizeof(tagThemeInfo));
    printf("struct strProgramInfo is %d bytes\n",sizeof(tagProgramInfo));
    printf("struct strChannelInfo is %d bytes\n",sizeof(tagChannelInfo));
    printf("struct strReplayShow is %d bytes\n",sizeof(tagReplayShow));
    printf("struct strReplayChannel is %d bytes\n",sizeof(tagReplayChannel));
    printf("\n");
#endif
    
    // Process Command Line
    
    sprintf(szPathname, "");
    
    int iarg = 1;
    while (iarg < argc)
    {
        if (!strcmp(argv[iarg], "-f"))
        {
            fmodeSTDIN = false;
            iarg++;
            if (iarg < argc)
            {
                strcpy(szPathname, argv[iarg]);
            }
        }
        else if (!strcmp(argv[iarg], "-l"))
        {
            fmodeList = true;
            fmodeListShows = true;
            fmodeListChannels = true;
        }
        else if (!strcmp(argv[iarg], "-d"))
        {
            fmodeDebug = true;
        }
        else if (!strcmp(argv[iarg], "-x"))
        {
            fmodeExtra = true;
        }
        else if (!strcmp(argv[iarg], "-ls"))
        {
            fmodeList = true;
            fmodeListShows = true;
        }
        else if (!strcmp(argv[iarg], "-lc"))
        {
            fmodeList = true;
            fmodeListChannels = true;
        }
        else if (!strcmp(argv[iarg], "-nowrap"))
        {
            fmodeWrapDisplay = false;
        }
        else if (!strcmp(argv[iarg], "-combined"))
        {
            fmodeDetailShowListings = true;
        }
        else if (!strcmp(argv[iarg], "-?"))
        {
            fmodeListShows = false;
            fmodeList = false;
            fmodeDebug = false;
            fmodeExtra = false;
            fmodeShowHelp = true;
        }
        iarg++;
    }

    if (argc == 2) 
    {
        strcpy(szPathname, argv[1]);
    }
    
    if (!*szPathname)
    {
        if (!fmodeSTDIN)
        {
            if (argc > 1)
            {
                printf("ERROR: Guide file name not specified.\n");
            }
            fmodeShowHelp = true;
        }
    }
    
    if (!fmodeList)
    {
        fmodeList = true;
        fmodeListShows = true;
        fmodeListChannels = true;
    }

    // Show Help
    
    if (fmodeShowHelp)
    {
        printf("\n");
        printf("Usage:\n");
        printf("    -f pathname     ReplayGuide data file to use (STDIN assumed if missing)\n");
        printf("    -l              List Guide (-ls and -lc)\n");
        printf("    -ls             List ReplayShows\n");
        printf("    -lc             List ReplayChannels\n");
        printf("    -d              Dump All Unknown Information\n");
        printf("    -x              Display Extra (But Less Useful) Data\n");
        printf("\n");
        printf("Syntax:\n");
        printf("    guideparser -l -f guide.dat             Parse Guide to Screen\n");
        printf("    guideparser guide.dat                   Same as -l -f guide.dat\n");
        printf("\n");				
        return 0;
    }
    
    
    // Check to make sure structures are required sizes, this is mostly for those of you tinkering with this
    // and - of course - me :).

    if (sizeof(ProgramInfo) != PROGRAMINFO)
    {
        printf("Error in ProgramInfo structure, needs to be %u instead of %u\n",PROGRAMINFO,sizeof(ProgramInfo));
    }

    if (sizeof(ChannelInfo) != CHANNELINFO)
    {
        printf("Error in ChannelInfo structure, needs to be %u instead of %u\n",CHANNELINFO,sizeof(ChannelInfo));
    }

    
    if (sizeof(ReplayShow) != REPLAYSHOW)
    {
        printf("Error in ReplayShow structure, needs to be %u instead of %u\n",REPLAYSHOW,sizeof(ReplayShow));
    }
    
    if (sizeof(ReplayChannel) != REPLAYCHANNEL)
    {
        printf("Error in ReplayChannel structure, needs to be %u instead of %u\n",REPLAYCHANNEL,sizeof(ReplayChannel));
    }
    
    if (sizeof(GuideHeader) != GUIDEHEADER)
    {
        printf("Error in GuideHeader structure, needs to be %u instead of %u\n",GUIDEHEADER,sizeof(GuideHeader));
    }

      
    if (fmodeWrapDisplay)
    {
        printf("Word Wrap is ON\n");
    }
    else
    {
        printf("Word Wrap is OFF\n");
    }


   if (fmodeExtra)
    {
        printf("Extra Information is ON\n");
    }
    else
    {
        printf("Extra Information is OFF\n");
    }


   if (fmodeDebug)
    {
        printf("Debug Display is ON\n");
    }
    else
    {
        printf("Debug Display is OFF\n");
    }
    
    printf("\n");

    curshow = 0;
    i = 0;
    junkptr = 0;
    
    // Open the File
    
    
    if (!*szPathname)
    {
        if (fmodeSTDIN)
        {
            guidefile = stdin;
            printf("Reading STDIN\n");
        }
        else
        {
            printf("No guide file specified!\n");
            return 0;
        }
    }
    else
    {
        fmodeSTDIN = false;
        if( (guidefile = fopen( szPathname, "rb" )) == NULL )
        {
            printf("Could not open %s\n",szPathname);
            return 0;
        }
        else
        {
            printf("Reading %s\n",szPathname);
        }
    }
    
    
    if (!fmodeSTDIN)
    {
        
        fseek( guidefile, 0L, SEEK_SET );
        
        // Peek at the file, if this is a null the ASCII header block has
        // already been removed.
        
        i = fgetc( guidefile );
        
        fseek( guidefile, 0L, SEEK_SET );
    }
    else
    {
        i = 0;
    }
    
    if (i != 0) 
    {
        printf("Parsing ASCII Header\n");
        
        if (fgets( szBuffer, 512, guidefile ) == NULL )
        {
            fclose( guidefile );
            return 0;
        }		
        
        memset(szBuffer,0,sizeof(szBuffer));
        
        if (fgets( szBuffer, 512, guidefile ) == NULL )
        {
            fclose( guidefile );
            return 0;
        }		
        
        memset(szBuffer,0,sizeof(szBuffer));
        
        if (fgets( szBuffer, 512, guidefile ) == NULL )
        {
            fclose( guidefile );
            return 0;
        }
        
        memset(szBuffer,0,sizeof(szBuffer));
        
        if (fgets( szBuffer, 512, guidefile ) == NULL )
        {
            fclose( guidefile );
            return 0;
        }
        
        memset(szBuffer,0,sizeof(szBuffer));
        
        // skip to the end of the ASCII header
        
        // technically it could be done with just this but it would
        // be slower.
        
        do 
        {
            i = fgetc( guidefile );
        } while ( i != 0 );
        
        
        fptr = ftell( guidefile );
        
        fptr--;
        
        // Save the file pointer, we need it to calcuate the location
        // of the first ReplayChannel or ReplayShow record.
        
        junkptr = fptr;		
    }
    else
    {
        junkptr = 0;
        fptr = 0;
    }
    
    //-------------------------------------------------------------------------
    // Replay Header
    //-------------------------------------------------------------------------
    
    if (!fmodeSTDIN)
    {
        fseek( guidefile, (long)fptr, SEEK_SET );
    }
    
    printf("Reading ReplayTV Guide Header\n");
    
    if (fread( &strGuideHeader, sizeof(strGuideHeader), 1, guidefile ) != 1 )
    {
        printf("Error reading header!\n");
        fclose( guidefile );
        return 0;
    }

    ConvertReplayGuideEndian(&strGuideHeader);
    
    if (strGuideHeader.GuideSnapshotHeader.osversion != 0) 
    {
        if (strGuideHeader.GuideSnapshotHeader.snapshotversion != 2) 
        {
            printf("Guide file is damaged! (v %d.%d)\n",strGuideHeader.GuideSnapshotHeader.osversion,strGuideHeader.GuideSnapshotHeader.snapshotversion);
        }
        else
        {
            printf("Guide file is from an older ReplayTV.\n");
            printf("This version of GuideParser only supports ReplayTV OS 5.0.\n");
        }

    }

    if (strGuideHeader.GuideSnapshotHeader.channelcount != strGuideHeader.GuideSnapshotHeader.channelcountcheck) 
    {
        printf("Guide Snapshot might be damaged. (%u/%u)\n",strGuideHeader.GuideSnapshotHeader.channelcount,strGuideHeader.GuideSnapshotHeader.channelcountcheck);
    }
        
    if (fmodeList)
    {
        printf("You have %u ReplayChannels.\n",strGuideHeader.GuideSnapshotHeader.channelcount);
    }

    if (strGuideHeader.GuideSnapshotHeader.channelcount > MAXCHANNELS)
    {
        // if there are more relaychannels than the maximum array size,
        // turn the option off.

        fmodeDetailShowListings = false;
    }
    
    maxshow = strGuideHeader.GuideSnapshotHeader.channelcount;		
    
    // Copy categories into a buffer
    
    for( unsigned int cc2 = 0; cc2 < strGuideHeader.GroupData.categories; ++cc2 )
    {
        strncpy(strReplayCategory[cc2].szName, strGuideHeader.GroupData.catbuffer+strGuideHeader.GroupData.categoryoffset[cc2],sizeof(strReplayCategory[cc2].szName)-1);
        strReplayCategory[cc2].index = 1 << strGuideHeader.GroupData.category[cc2];
    }
    
    
    // jump to the first channel record
    
    // fptr = (long)strGuideHeader.GuideSnapshotHeader.channeloffset + junkptr; 
    fptr = (long)strGuideHeader.GuideSnapshotHeader.channeloffset + junkptr; 
    
    if (!fmodeSTDIN)
    {
        fseek( guidefile, (long)fptr, SEEK_SET );
    }
    
    //-------------------------------------------------------------------------
    // ReplayChannels
    //-------------------------------------------------------------------------
    
    if ((fmodeListChannels) || (fmodeDetailShowListings))
    {
        printf("\n\nReading ReplayChannels\n\n");
        
        do
        {
            if (fread( &strReplayChannel, sizeof(tagReplayChannel), 1, guidefile ) != 1 )
                // if (fread( &strReplayChannel, REPLAYCHANNEL, 1, guidefile ) != 1 )
            {
                printf("Error reading channel %u\n",curshow);
                fclose( guidefile );
                return 0;
            }
            
            curshow++;
            
            if ((fmodeList) && (fmodeListChannels))
            {
                printf("Channel #%u:\n",curshow);
            }
            
            memset(szCategory,0,sizeof(szCategory));
            memset(szBuffer,0,sizeof(szBuffer));
            memset(szSuzuki,0,sizeof(szSuzuki));
            memset(szActors,0,sizeof(szActors));
            memset(szProducers,0,sizeof(szProducers));
            memset(szDirectors,0,sizeof(szDirectors));
            memset(szGuests,0,sizeof(szGuests));
            memset(szShowDescription,0,sizeof(szShowDescription));
            memset(szEpisodeTitle,0,sizeof(szEpisodeTitle));
            memset(szShowTitle,0,sizeof(szShowTitle));
            memset(szMPAARating,0,sizeof(szMPAARating));
            memset(szTVRating,0,sizeof(szTVRating));
            memset(szOriginalDescriptionBuffer,0,sizeof(szOriginalDescriptionBuffer));
            memset(szIVSStatus,0,sizeof(szIVSStatus));
            strcpy(szStarRating,"*****");
            
            searchrecording = false;
            isguaranteed = false;
            isnorepeats = false;
            
            // Do Endian Conversion
            
            ConvertReplayChannelEndian(&strReplayChannel);
            ConvertReplayShowEndian(&strReplayChannel.ReplayShow);
            ConvertProgramInfoEndian(&strReplayChannel.ReplayShow.ProgramInfo);
            ConvertChannelInfoEndian(&strReplayChannel.ReplayShow.ChannelInfo);
            
            // Process ProgramInfo
            
            MoveMemory(szOriginalDescriptionBuffer,strReplayChannel.ReplayShow.ProgramInfo.szDescription,sizeof(szOriginalDescriptionBuffer));
            
            
            // Process Flags 
            
            iscc = false;
            issap = false;
            isstereo = false;
            ismovie = false;
            isppv = false;
            hasparts = false;
            isletterbox = false;
            isrepeat = false;               
            cc = 0;                         // Offset for Extended Data
            
            if (strReplayChannel.flags & 0x00000020) 
            {
                isnorepeats = true;
            }

            if (strReplayChannel.ReplayShow.guaranteed == 0xFFFFFFFF ) 
            {
                isguaranteed = true;
            }
            
            
            if (strReplayChannel.ReplayShow.guideid != 0 )
            {
                receivedshow = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000001) 
            {
                iscc = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000002) 
            {
                isstereo = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000004) 
            {
                isrepeat = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000008) 
            {
                issap = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000010) 
            {
                isletterbox = true;
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000020) 
            {
                // Movie, load extra 8 bytes into MovieInfo structure.
                
                ismovie = true;
                MoveMemory(&strMovieInfo,szOriginalDescriptionBuffer + cc,sizeof(strMovieInfo));
                ConvertMovieInfoEndian(&strMovieInfo);
                
                memset(szBuffer,0,sizeof(szBuffer));
                cc =  cc + sizeof(strMovieInfo);
                MoveMemory(szBuffer,szOriginalDescriptionBuffer + cc,sizeof(szOriginalDescriptionBuffer) - cc);
                
                // this won't do the half stars
                
                i = strMovieInfo.stars / 10;
                szStarRating[i] = 0;
                
                // MPAA Rating (not all mappings are known)
                
                DisplayMPAARating(strMovieInfo.mpaa,szMPAARating);
            }
            
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000040) 
            {
                
                // Multiple Parts
                
                hasparts = true;
                MoveMemory(&strPartsInfo,szOriginalDescriptionBuffer + cc,sizeof(strPartsInfo));
                ConvertPartsInfoEndian(&strPartsInfo);
                
                memset(szBuffer,0,sizeof(szBuffer));
                
                cc = cc + sizeof(strPartsInfo);
                MoveMemory(szBuffer,szOriginalDescriptionBuffer + cc,sizeof(szOriginalDescriptionBuffer) - cc);
            }
            
            if (strReplayChannel.ReplayShow.ProgramInfo.flags & 0x00000080) 
            {
                isppv = true;
            }
            
            DisplayTVRating(strReplayChannel.ReplayShow.ProgramInfo.flags,szTVRating);
            
            i = cc;             // set offset (needed if it's been managled by HasParts or IsMovie)           
            
            strncpy(szShowTitle,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.titleLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.titleLen;
            
            strncpy(szEpisodeTitle,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.episodeLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.episodeLen;
            
            strncpy(szShowDescription,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.descriptionLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.descriptionLen;
            
            strncpy(szActors,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.actorLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.actorLen;
            
            strncpy(szGuests,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.guestLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.guestLen;
            
            strncpy(szSuzuki,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.suzukiLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.suzukiLen;
            
            strncpy(szProducers,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.producerLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.producerLen;
            
            strncpy(szDirectors,strReplayChannel.ReplayShow.ProgramInfo.szDescription + i,strReplayChannel.ReplayShow.ProgramInfo.directorLen);
            i = i + strReplayChannel.ReplayShow.ProgramInfo.directorLen;
            
            // Convert Codepages
            
            ConvertCodepage(strReplayChannel.szShowLabel);
            ConvertCodepage(strReplayChannel.ReplayShow.ProgramInfo.szDescription);
            ConvertCodepage(szEpisodeTitle);
            ConvertCodepage(szProducers);
            ConvertCodepage(szGuests);
            ConvertCodepage(szActors);
            ConvertCodepage(szShowDescription);
            
            
            bool bBreak = false;
            
            // Reverse Engineering Fun
            
            if (strReplayChannel.unknown1 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown2 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown3 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown4 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown5 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown6 != 0)
            {
                bBreak = true;
            }
            
            if (strReplayChannel.unknown7 != 0)
            {
                bBreak = true;
            }
            
            if (bBreak)
            {
                printf("ALERT! Unknown Value Non-Zero!\nRC.U1-7: 0x%08x,0x%08x0x%08x0x%08x0x%08x0x%08x0x%08x0x%08x)\n",strReplayChannel.unknown1,strReplayChannel.unknown2,strReplayChannel.unknown3,strReplayChannel.unknown4,strReplayChannel.unknown5,strReplayChannel.unknown6,strReplayChannel.unknown7);
            }
            
            
            // calculate the actual start/end recording times
            
            beginrecord = strReplayChannel.ReplayShow.ProgramInfo.eventtime - (strReplayChannel.beforepadding * 60);
            endrecord = strReplayChannel.ReplayShow.ProgramInfo.eventtime + ((strReplayChannel.ReplayShow.ProgramInfo.minutes + strReplayChannel.afterpadding) * 60);
            
            // lookup category
            
            for( unsigned cc2 = 0; cc2 < strGuideHeader.GroupData.categories; ++cc2 )
            {
                if (strReplayCategory[cc2].index == strReplayChannel.category) 
                {
                    strncpy(szCategory,strReplayCategory[cc2].szName,sizeof(szCategory)-1);
                }
            }
            
            
            // minutes is stored in another location for themes
            
            if (strReplayChannel.channeltype == 2) 
            {
                searchrecording = true;
            }
            
            if (strReplayChannel.channeltype == 4) 
            {
                searchrecording = true;
            }
            
            // build strings
            
            DisplayQuality(strReplayChannel.quality,szQuality);
            DisplayType(strReplayChannel.channeltype,szChannelType);
            DisplayDaysOfWeek(strReplayChannel.daysofweek,szRecordDays);
            DisplayIVSStatus(strReplayChannel.ReplayShow.ivsstatus,szIVSStatus);
            DisplayInputSource(strReplayChannel.ReplayShow.inputsource,szInputSource);
            
            
            // Process Sub Structures
            
            if (strReplayChannel.channeltype == 2)
            {
                // Theme
                
                MoveMemory(&strThemeInfo,&strReplayChannel.ThemeInfo,sizeof(strThemeInfo));
                ConvertThemeInfoEndian(&strThemeInfo);
                
            }
            
            // Display Record
            
            if ((fmodeList) && (fmodeListChannels))
            {
                
                strcpy(szHeading,"      Show: ");
                WrapDisplay(szHeading,strReplayChannel.szShowLabel,fmodeWrapDisplay);
                
                if (strReplayChannel.ReplayShow.ProgramInfo.isvalid)
                {
                    if (strReplayChannel.ReplayShow.ProgramInfo.autorecord == 0)
                    {
                        strcpy(szHeading,"            ");
                        strcpy(szOutputBuffer,"Manual Record");
                        WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    }
                }
                
                if (szCategory[0] != 0) 
                {
                    strcpy(szHeading,"  Category: ");
                    WrapDisplay(szHeading,szCategory,fmodeWrapDisplay);
                    
                }
                
                if (!searchrecording) 
                {
                    sprintf(szBuffer,"%s",UnixTimeToString(beginrecord));
                    
                    if (isnorepeats)
                    {
                        if (isguaranteed)
                        {
                            sprintf(szOutputBuffer,"%s to %s (%s, Guaranteed, No Repeats)",szBuffer,UnixTimeToString(endrecord),szChannelType);
                        }
                        else
                        {
                            sprintf(szOutputBuffer,"%s to %s (%s, No Repeats)",szBuffer,UnixTimeToString(endrecord),szChannelType);
                        }
                    }
                    else
                    {
                        if (isguaranteed)
                        {
                            sprintf(szOutputBuffer,"%s to %s (%s, Guaranteed)",szBuffer,UnixTimeToString(endrecord),szChannelType);
                        }
                        else
                        {
                            sprintf(szOutputBuffer,"%s to %s (%s)",szBuffer,UnixTimeToString(endrecord),szChannelType);
                        }
                    }
                    
                    
                    strcpy(szHeading,"      From: ");
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    
                    
                    if (strReplayChannel.channeltype == 1)
                    {
                        strcpy(szHeading,"      Days: ");
                        WrapDisplay(szHeading,szRecordDays,fmodeWrapDisplay);
                    }
                }
                else
                {
                    if (strReplayChannel.channeltype == 2) 
                    {
                        strcpy(szHeading,"    Search: ");
                        WrapDisplay(szHeading,strThemeInfo.szSearchString,fmodeWrapDisplay);
                    }
                    strcpy(szHeading,"      Type: ");
                    WrapDisplay(szHeading,szChannelType,fmodeWrapDisplay);
                }
                
                if (strReplayChannel.ReplayShow.ChannelInfo.isvalid)
                {
                    if (strReplayChannel.ReplayShow.ChannelInfo.szChannelLabel[0] == 0)
                    {
                        strcpy(szOutputBuffer,strReplayChannel.ReplayShow.ChannelInfo.szChannelName);
                    }
                    else
                    {
                        sprintf(szOutputBuffer,"%s (%s)",strReplayChannel.ReplayShow.ChannelInfo.szChannelName,strReplayChannel.ReplayShow.ChannelInfo.szChannelLabel);
                    }
                    
                    strcpy(szHeading,"On Channel: ");
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    
                }
                
                strcpy(szHeading,"   Quality: ");
                WrapDisplay(szHeading,szQuality,fmodeWrapDisplay);
                
                switch( strReplayChannel.channeltype )
                {
                case 1:
                    if (strReplayChannel.keep > 0) 
                    {
                        strcpy(szHeading,"  Episodes: ");
                        
                        if (strReplayChannel.stored > 0 )
                        {
                            sprintf(szOutputBuffer,"%u (%u recorded so far)",strReplayChannel.keep,strReplayChannel.stored);
                        }
                        else
                        {
                            sprintf(szOutputBuffer,"%u",strReplayChannel.keep);
                        }
                        
                        WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    }
                    break;
                case 2:
                    strcpy(szHeading,"      Keep: ");
                    sprintf(szOutputBuffer,"%u minutes",strThemeInfo.thememinutes);
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    break;
                case 4:
                    break;
                default:
                    break;
                }
                
                
                if (strReplayChannel.ReplayShow.ProgramInfo.isvalid)
                {
                    strcpy(szHeading,"  Duration: ");
                    
                    if (strReplayChannel.beforepadding + strReplayChannel.afterpadding != 0 )
                    {
                        sprintf(szOutputBuffer,"%d minutes (-%d,+%d)",strReplayChannel.ReplayShow.ProgramInfo.minutes,strReplayChannel.beforepadding,strReplayChannel.afterpadding);
                        
                    }
                    else
                    {
                        sprintf(szOutputBuffer,"%d minutes",strReplayChannel.ReplayShow.ProgramInfo.minutes);
                    }
                    
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    
                }
                
                
                if (fmodeExtra)
                {
                    
                    if (strReplayChannel.ReplayShow.ChannelInfo.isvalid)
                    {
                        
                        if (strReplayChannel.ReplayShow.ChannelInfo.cablesystem[0] > 0)
                        {
                            strcpy(szHeading," Cable Sys: ");
                            WrapDisplay(szHeading,strReplayChannel.ReplayShow.ChannelInfo.cablesystem,fmodeWrapDisplay);
                        }
                        
                        if (strReplayChannel.ReplayShow.ChannelInfo.tmsID > 0)
                        {
                            strcpy(szHeading,"  TMS ID 1: ");
                            sprintf(szOutputBuffer,"%u",strReplayChannel.ReplayShow.ChannelInfo.tmsID);
                            WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                        }
                    }
                    
                    
                    if (strReplayChannel.ReplayShow.ProgramInfo.isvalid)
                    {
                        if (strReplayChannel.ReplayShow.ProgramInfo.tmsID > 0)
                        {
                            strcpy(szHeading,"  TMS ID 2: ");
                            sprintf(szOutputBuffer,"%u",strReplayChannel.ReplayShow.ProgramInfo.tmsID);
                            WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                        }
                    }
                    
                    if (strReplayChannel.ReplayShow.recorded > 0)
                    {
                        strcpy(szHeading,"  Recorded: ");
                        WrapDisplay(szHeading,UnixTimeToString(strReplayChannel.ReplayShow.recorded),fmodeWrapDisplay);
                        
                    }
                    
                    strcpy(szHeading,"IVS Status: ");
                    sprintf(szOutputBuffer,"%s (0x%08x)",szIVSStatus,strReplayChannel.ReplayShow.ivsstatus);
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    
                    if (receivedshow)
                    {
                        strcpy(szHeading,"  Guide ID: ");
                        sprintf(szOutputBuffer,"0x%08x",strReplayChannel.ReplayShow.guideid);
                        WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                    }
                    
                    if (!searchrecording)
                    {
                        strcpy(szHeading," In Source: ");
                        WrapDisplay(szHeading,szInputSource,fmodeWrapDisplay);
                    }

                    strcpy(szHeading," Ch. Flags: ");
                    sprintf(szOutputBuffer,"%d (0x%04x)",strReplayChannel.flags,strReplayChannel.flags);
                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                }
                
                
                if (fmodeDebug)
                {
                    printf("\n=Debug=\n");
                    printf("                        strReplayChannel.unknown1: %u (0x%08x)\n",strReplayChannel.unknown1,strReplayChannel.unknown1);
                    printf("                        strReplayChannel.unknown2: %u (0x%08x)\n",strReplayChannel.unknown2,strReplayChannel.unknown2);
                    printf("                        strReplayChannel.unknown3: %u (0x%08x)\n",strReplayChannel.unknown3,strReplayChannel.unknown3);
                    printf("                        strReplayChannel.unknown4: %u (0x%08x)\n",strReplayChannel.unknown4,strReplayChannel.unknown4);
                    printf("                        strReplayChannel.unknown5: %u (0x%08x)\n",strReplayChannel.unknown5,strReplayChannel.unknown5);
                    printf("                        strReplayChannel.unknown6: %u (0x%08x)\n",strReplayChannel.unknown6,strReplayChannel.unknown6);
                    printf("                        strReplayChannel.unknown7: %u (0x%08x)\n",strReplayChannel.unknown7,strReplayChannel.unknown7);
                    
                    if (searchrecording)
                    {
                        printf("                               strThemeInfo.flags: %u (0x%08x)\n",strThemeInfo.flags,strThemeInfo.flags);
                        printf("                           strThemeInfo.suzuki_id: %u (0x%08x)\n",strThemeInfo.suzuki_id,strThemeInfo.suzuki_id);
                    }
                    
                    printf("\n");
                }
                
                printf("\n");
                }
                if (fmodeDetailShowListings)
                {
                    strChannel.channel_id = strReplayChannel.created;
                    strChannel.channeltype = strReplayChannel.channeltype;
                    strChannel.keep = strReplayChannel.keep;
                    strChannel.stored = strReplayChannel.stored;
                    strcpy( strChannel.szCategory, szCategory );
                    strcpy( strChannel.szShowLabel, strReplayChannel.szShowLabel );
                    strcpy( strChannel.szChannelType, szChannelType );
                    Channels[ccount] = strChannel;
                    ccount++;
                }
                
                // Get file pointer (this is just for a handy breakpoint to
                // be completely honest.)
                
                fptr = ftell( guidefile );
                
            } 
            while ( curshow < maxshow );
        }
        else
        {
            // jump to the first show record
            
            // fptr = (long)strGuideHeader.GuideSnapshotHeader.showoffset + junkptr; 
            fptr = (long)strGuideHeader.GuideSnapshotHeader.showoffset + junkptr;

            if (!fmodeSTDIN)
            {
                fseek( guidefile, (long)fptr, SEEK_SET );
            }
            
        }
        
        
        //-------------------------------------------------------------------------
        // ReplayShows
        //-------------------------------------------------------------------------
        
        
        fptr = ftell( guidefile );

        if (fmodeListShows) 
        {
            
            printf("\n\nReading ReplayShows\n\n");
            
            moreshows = true;
            curshow = 0;
            
            do
            {
                if (feof( guidefile ) )
                {
                    // wow, a correctly formatted file :)
                    
                    moreshows = false;
                }
                else
                {
                    if (fread( &strReplayShow, sizeof(strReplayShow), 1, guidefile ) != 1 )
                    // if (fread( &strReplayShow, REPLAYSHOW, 1, guidefile ) != 1 )
                    {
                        // we're at the end, bail
                        
                        moreshows = false;
                    }
                    else
                    {
                        
                        curshow++;
                        
                        printf("Show #%u:\n",curshow);
                        
                        memset(szTVRating,0,sizeof(szTVRating));
                        memset(szBuffer,0,sizeof(szBuffer));
                        memset(szSuzuki,0,sizeof(szSuzuki));
                        memset(szActors,0,sizeof(szActors));
                        memset(szProducers,0,sizeof(szProducers));
                        memset(szDirectors,0,sizeof(szDirectors));
                        memset(szGuests,0,sizeof(szGuests));
                        memset(szShowDescription,0,sizeof(szShowDescription));
                        memset(szEpisodeTitle,0,sizeof(szEpisodeTitle));
                        memset(szShowTitle,0,sizeof(szShowTitle));
                        memset(szCategory,0,sizeof(szCategory));
                        memset(szMPAARating,0,sizeof(szMPAARating));
                        memset(szOriginalDescriptionBuffer,0,sizeof(szOriginalDescriptionBuffer));
                        strcpy(szStarRating,"*****");
                        memset(szServiceTier,0,sizeof(szServiceTier));
                        memset(szDeviceDescription,0,sizeof(szDeviceDescription));
                        memset(szExtTV,0,sizeof(szExtTV));
                        memset(szExtMPAA,0,sizeof(szExtMPAA));
                        memset(szGenre,0,sizeof(szGenre));
                        memset(szIVSStatus,0,sizeof(szIVSStatus));

                        // Convert Endian

                        ConvertReplayShowEndian(&strReplayShow);
                        ConvertProgramInfoEndian(&strReplayShow.ProgramInfo);
                        ConvertChannelInfoEndian(&strReplayShow.ChannelInfo);

                        /*
                        // this is for this hack, this seems to be no longer used so copy it from eventtime

                        if (strReplayShow.recorded == 0) {
                              strReplayShow.recorded = strReplayShow.ProgramInfo.eventtime;
                        }
                        */


                        // If requested, look up the ReplayChannel for extended information (Category etc)

                        if (fmodeDetailShowListings)
                        {
                            for ( j = 0; j < ccount; ++j)
                            {
                                if (Channels[j].channel_id == strReplayShow.created)
                                {
                                    strChannel = Channels[j];
                                    break;
                                }
                            }
                        }
                        
                        // Process ProgramInfo

                        MoveMemory(szOriginalDescriptionBuffer,strReplayShow.ProgramInfo.szDescription,sizeof(szOriginalDescriptionBuffer));

                        // Process Flags 
                        
                        iscc = false;
                        issap = false;
                        isstereo = false;
                        ismovie = false;
                        isppv = false;
                        hasparts = false;
                        isletterbox = false;
                        isrepeat = false;   
                        receivedshow = false;
                        isguaranteed = false;

                        cc = 0;                         // Offset for Extended Data

                        if (strReplayShow.guaranteed == 0xFFFFFFFF) 
                        {
                            isguaranteed = true;
                        }

                        if (strReplayShow.guideid != 0)
                        {
                            receivedshow = true;
                        }
                        

                        if (strReplayShow.ProgramInfo.flags & 0x00000001) 
                        {
                            iscc = true;
                        }

                        if (strReplayShow.ProgramInfo.flags & 0x00000002) 
                        {
                            isstereo = true;
                        }

                        if (strReplayShow.ProgramInfo.flags & 0x00000004) 
                        {
                            isrepeat = true;
                        }

                        if (strReplayShow.ProgramInfo.flags & 0x00000008) 
                        {
                            issap = true;
                        }
                        
                        if (strReplayShow.ProgramInfo.flags & 0x00000010) 
                        {
                            isletterbox = true;
                        }
                        
                        if (strReplayShow.ProgramInfo.flags & 0x00000020) 
                        {
                            // Movie, load extra 8 bytes into MovieInfo structure.
                            
                            ismovie = true;
                            MoveMemory(&strMovieInfo,szOriginalDescriptionBuffer + cc,sizeof(strMovieInfo));
                            ConvertMovieInfoEndian(&strMovieInfo);
                            
                            memset(szBuffer,0,sizeof(szBuffer));
                            cc =  cc + sizeof(strMovieInfo);
                            MoveMemory(szBuffer,szOriginalDescriptionBuffer + cc,sizeof(szOriginalDescriptionBuffer) - cc);
                            
                            // this won't do the half stars, it's too cheesy for that!
                            
                            i = strMovieInfo.stars / 10;
                            szStarRating[i] = 0;
                            
                            // MPAA Rating 
                            
                            DisplayMPAARating(strMovieInfo.mpaa,szMPAARating);
                            DisplayExtendedMPAARating(strMovieInfo.mpaa,szExtMPAA,false);
                            
                            if (strlen(szMPAARating) > 0) 
                            {
                                if (strlen(szExtMPAA) > 0)
                                {
                                    strcat( szMPAARating, " (" );
                                    strcat( szMPAARating, szExtMPAA );
                                    strcat( szMPAARating, ")" );
                                }
                            }
                        }
                        
                        
                        if (strReplayShow.ProgramInfo.flags & 0x00000040) 
                        {
                            
                            // Multiple Parts
                            
                            hasparts = true;
                            MoveMemory(&strPartsInfo,szOriginalDescriptionBuffer + cc,sizeof(strPartsInfo));
                            ConvertPartsInfoEndian(&strPartsInfo);
                            
                            memset(szBuffer,0,sizeof(szBuffer));
                            
                            cc = cc + sizeof(strPartsInfo);
                            MoveMemory(szBuffer,szOriginalDescriptionBuffer + cc,sizeof(szOriginalDescriptionBuffer) - cc);
                        }

                        if (strReplayShow.ProgramInfo.flags & 0x00000080) 
                        {
                            isppv = true;
                        }
                        
                        if (!ismovie)
                        {
                            /*
                            **
                            ** NOTE: 
                            ** This is what the Replay does.  However, the TMS data does support a mix
                            ** so, if you want, you can remove "if (!ismovie)".
                            */

                            DisplayTVRating(strReplayShow.ProgramInfo.flags,szTVRating);
                            DisplayExtendedTVRating(strReplayShow.ProgramInfo.flags,szExtTV,false);
                        }

                        if (strlen(szTVRating) > 0) 
                        {
                            if (strlen(szExtTV) > 0)
                            {
                                strcat( szTVRating, " (" );
                                strcat( szTVRating, szExtTV );
                                strcat( szTVRating, ")" );
                            }
                        }


                        
                        i = cc;             // set offset (needed if it's been mangled by HasParts or IsMovie)           
                        
                        strncpy(szShowTitle,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.titleLen);
                        i = i + strReplayShow.ProgramInfo.titleLen;
                        
                        strncpy(szEpisodeTitle,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.episodeLen);
                        i = i + strReplayShow.ProgramInfo.episodeLen;
                        
                        strncpy(szShowDescription,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.descriptionLen);
                        i = i + strReplayShow.ProgramInfo.descriptionLen;
                        
                        strncpy(szActors,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.actorLen);
                        i = i + strReplayShow.ProgramInfo.actorLen;
                        
                        strncpy(szGuests,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.guestLen);
                        i = i + strReplayShow.ProgramInfo.guestLen;
                        
                        strncpy(szSuzuki,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.suzukiLen);
                        i = i + strReplayShow.ProgramInfo.suzukiLen;
                        
                        strncpy(szProducers,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.producerLen);
                        i = i + strReplayShow.ProgramInfo.producerLen;
                        
                        strncpy(szDirectors,strReplayShow.ProgramInfo.szDescription + i,strReplayShow.ProgramInfo.directorLen);
                        i = i + strReplayShow.ProgramInfo.directorLen;
                        
                        
                        // Convert Codepages
                        
                        ConvertCodepage(strReplayShow.ProgramInfo.szDescription);
                        ConvertCodepage(szShowTitle);
                        ConvertCodepage(szEpisodeTitle);
                        ConvertCodepage(szProducers);
                        ConvertCodepage(szGuests);
                        ConvertCodepage(szActors);
                        ConvertCodepage(szShowDescription);
                         
                        // Build Strings

                        DisplayQuality(strReplayShow.quality,szQuality);
                        DisplayInputSource(strReplayShow.inputsource,szInputSource);
                        DisplayDevice(strReplayShow.ChannelInfo.device,szDeviceDescription);
                        DisplayServiceTier(strReplayShow.ChannelInfo.tier,szServiceTier);
                        DisplayIVSStatus(strReplayShow.ivsstatus,szIVSStatus);
                        DisplayGenre(strReplayShow.ProgramInfo.genre1,szGenre,false);
                        DisplayGenre(strReplayShow.ProgramInfo.genre2,szGenre,true);
                        DisplayGenre(strReplayShow.ProgramInfo.genre3,szGenre,true);
                        DisplayGenre(strReplayShow.ProgramInfo.genre4,szGenre,true);

                        // Display Record
                        
                        if (fmodeList)
                        {
                            if (fmodeDetailShowListings)
                            {
                                strcpy(szHeading,"    Replay: ");
                                sprintf(szOutputBuffer,"%s",strChannel.szShowLabel);
                                
                                if (strcmp( szShowTitle, strChannel.szShowLabel) != 0) 
                                {
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                }

                                strcpy( szCategory, strChannel.szCategory );

                            }

                            
                            if (szEpisodeTitle[0] > 31) 
                            {
                                strcpy(szHeading,"      Show: ");
                                sprintf(szOutputBuffer,"%s \"%s\"",szShowTitle,szEpisodeTitle);
                            }
                            else
                            {
                                if (ismovie) 
                                {
                                    strcpy(szHeading,"     Movie: ");

                                    if (szStarRating[0] != 0)
                                    {
                                        if (strMovieInfo.stars % 10)
                                        {
                                            sprintf(szOutputBuffer,"%s (%s1/2, %s, %d",szShowTitle,szStarRating,szMPAARating,strMovieInfo.year);
                                        }
                                        else
                                        {
                                            sprintf(szOutputBuffer,"%s (%s, %s, %d",szShowTitle,szStarRating,szMPAARating,strMovieInfo.year);
                                        }
                                    }
                                    else
                                    {
                                        sprintf(szOutputBuffer,"%s (%s, %d",szShowTitle,szMPAARating,strMovieInfo.year);
                                    }
                                }
                                else
                                {
                                    strcpy(szHeading,"      Show: ");
                                    sprintf(szOutputBuffer,"%s",szShowTitle);
                                }
                                
                            }

                            // display flags

                            cc = 0;
                            i = 0;

                            if (iscc) 
                            {
                                cc++;
                            }
                            if (isstereo)
                            {
                                cc++;
                            }
                            if (isrepeat)
                            {
                                cc++;
                            }
                            if (issap)
                            {
                                cc++;
                            }
                            if (isppv)
                            {
                                cc++;
                            }

                            if (szTVRating[0] > 31)
                            {
                                cc++;
                            }

                            if (isletterbox)
                            {
                                cc++;
                            }

                            if (cc > 0)
                            {
                                if (ismovie)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                                else
                                {   
                                    strcat(szOutputBuffer," (");
                                }
                                i = 1;
                            }
                            else
                            {
                                if (ismovie)
                                {
                                    strcat(szOutputBuffer,")");
                                }
                            }

                            if (szTVRating[0] > 31)
                            {
                                strcat(szOutputBuffer,szTVRating);
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (iscc)
                            {
                                strcat(szOutputBuffer,"CC");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (isstereo)
                            {
                                strcat(szOutputBuffer,"Stereo");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (isrepeat)
                            {
                                strcat(szOutputBuffer,"Repeat");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (issap)
                            {
                                strcat(szOutputBuffer,"SAP");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (isppv)
                            {
                                strcat(szOutputBuffer,"PPV");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (isletterbox)
                            {
                                strcat(szOutputBuffer,"Letterboxed");
                                cc--;
                                if (cc)
                                {
                                    strcat(szOutputBuffer,", ");
                                }
                            }

                            if (i > 0)
                            {
                                strcat(szOutputBuffer,")");
                            }
                            
                            WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);


                            
                            if (hasparts)
                            {
                                strcpy(szHeading,"            ");
                                sprintf(szOutputBuffer,"Part %d of %d",strPartsInfo.partnumber,strPartsInfo.maxparts);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                            }
                            

                            if (strReplayShow.ProgramInfo.isvalid)
                            {
                                
                                if (strReplayShow.ProgramInfo.autorecord == 0)
                                {
                                    strcpy(szHeading,"            ");
                                    strcpy(szOutputBuffer,"Manual Recording");
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                }
                            }

                            
                            if (szShowDescription[0] > 31) 
                            {
                                memset(szHeading,0,sizeof(szHeading));
                                memset(szHeading,' ',12);
                                WrapDisplay(szHeading,szShowDescription,fmodeWrapDisplay);
                            }

                            if (szGenre[0] > 31)
                            {
                                strcpy(szHeading,"            ");
                                WrapDisplay(szHeading,szGenre,fmodeWrapDisplay);                                
                            }

                            if (szCategory[0] != 0) 
                            {
                                strcpy(szHeading,"  Category: ");
                                WrapDisplay(szHeading,szCategory,fmodeWrapDisplay);
                            }

                            if (fmodeDetailShowListings)
                            {
                                strcpy(szHeading,"      Type: ");
                                WrapDisplay(szHeading,strChannel.szChannelType,fmodeWrapDisplay);
                            }

                            strcpy(szHeading,"   Quality: ");
                            WrapDisplay(szHeading,szQuality,fmodeWrapDisplay);

                            strcpy(szHeading,"  Recorded: ");
                            WrapDisplay(szHeading,UnixTimeToString(strReplayShow.recorded),fmodeWrapDisplay);
                            
                            strcpy(szHeading,"  Filename: ");
                            sprintf(szOutputBuffer,"%u.mpg",strReplayShow.recorded);
                            WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                            if (strReplayShow.ChannelInfo.isvalid)
                            {
                                strcpy(szHeading,"      From: ");
                                if (strReplayShow.ChannelInfo.channel > 0) 
                                {
                                    sprintf(szOutputBuffer,"%s (%u)",strReplayShow.ChannelInfo.szChannelName,strReplayShow.ChannelInfo.channel);			
                                }
                                else
                                {
                                    sprintf(szOutputBuffer,"%s",strReplayShow.ChannelInfo.szChannelName);
                                }
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                            }
                            
                            strcpy(szHeading,"  Duration: ");
                            i = strReplayShow.ProgramInfo.minutes + strReplayShow.afterpadding + strReplayShow.beforepadding;	
                            cc = CalculateMinutes(strReplayShow.seconds);       
                            if (cc != i)
                            {
                                // This can happen if the cable goes out during recording or if you hit STOP etc.
                                sprintf(szOutputBuffer,"%d minutes ( %d scheduled )",cc,i);			
                            }
                            else
                            {
                                sprintf(szOutputBuffer,"%d minutes",cc);			
                            }
                            WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                            

                            if (szActors[0] > 31) 
                            {
                                strcpy(szHeading,"    Actors: ");
                                WrapDisplay(szHeading,szActors,fmodeWrapDisplay);
                            }
                            
                            if (szGuests[0] > 31) 
                            {
                                strcpy(szHeading,"    Guests: ");
                                WrapDisplay(szHeading,szGuests,fmodeWrapDisplay);
                            }
                            
                            if (szProducers[0] > 31) 
                            {
                                strcpy(szHeading," Producers: ");
                                WrapDisplay(szHeading,szProducers,fmodeWrapDisplay);
                            }
                            
                            if (szDirectors[0] > 31) 
                            {
                                strcpy(szHeading," Directors: ");
                                WrapDisplay(szHeading,szDirectors,fmodeWrapDisplay);
                            }


                            if (fmodeExtra)
                            {
                                // Display Extended Info

                                memset(szBuffer,0,sizeof(szBuffer));
                                MoveMemory(szBuffer,&strReplayShow.ChannelInfo.device,1);

                                strcpy(szHeading,"   Seconds: ");
                                sprintf(szOutputBuffer,"%d",strReplayShow.seconds);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"Suzuki Str: ");
                                WrapDisplay(szHeading,szSuzuki,fmodeWrapDisplay);

                                strcpy(szHeading,"Genre Code: ");
                                sprintf(szOutputBuffer,"%d,%d,%d,%d",strReplayShow.ProgramInfo.genre1,strReplayShow.ProgramInfo.genre2,strReplayShow.ProgramInfo.genre3,strReplayShow.ProgramInfo.genre4);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"    Device: ");
                                WrapDisplay(szHeading,szDeviceDescription,fmodeWrapDisplay);

                                strcpy(szHeading,"      Tier: ");
                                WrapDisplay(szHeading,szServiceTier,fmodeWrapDisplay);

                                strcpy(szHeading," In Source: ");
                                WrapDisplay(szHeading,szInputSource,fmodeWrapDisplay);

                                strcpy(szHeading,"     Flags: ");
                                sprintf(szOutputBuffer,"0x%08x",strReplayShow.ProgramInfo.flags);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"Play Flags: ");
                                sprintf(szOutputBuffer,"0x%08x",strReplayShow.playbackflags);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"IVS Status: ");
                                sprintf(szOutputBuffer,"%s (0x%08x)",szIVSStatus,strReplayShow.ivsstatus);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"Times Sent: ");
                                sprintf(szOutputBuffer,"%d",strReplayShow.timessent);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                if (receivedshow)
                                {
                                    strcpy(szHeading,"  Guide ID: ");
                                    sprintf(szOutputBuffer,"0x%08x",strReplayShow.guideid);
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                    
                                    strcpy(szHeading,"  NDX Size: ");
                                    sprintf(szOutputBuffer,"%u",strReplayShow.indexsize);
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                    
                                    strcpy(szHeading,"  MPG Size: ");
                                    sprintf(szOutputBuffer,"%u",strReplayShow.mpegsize);
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                }

                                
                                strcpy(szHeading,"  MPEG GOP: ");
                                sprintf(szOutputBuffer,"%u",strReplayShow.GOP_count);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"  High GOP: ");
                                sprintf(szOutputBuffer,"%u",strReplayShow.GOP_highest);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                strcpy(szHeading,"  Last GOP: ");
                                sprintf(szOutputBuffer,"%u",strReplayShow.GOP_last);
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);

                                if (strReplayShow.ChannelInfo.cablesystem[0] > 0)
                                {
                                    strcpy(szHeading," Cable Sys: ");
                                    WrapDisplay(szHeading,strReplayShow.ChannelInfo.cablesystem,fmodeWrapDisplay);
                                }
                                if (strReplayShow.ChannelInfo.tmsID > 0)
                                {
                                    strcpy(szHeading,"  TMS ID 1: ");
                                    sprintf(szOutputBuffer,"%u",strReplayShow.ChannelInfo.tmsID);
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                }
                                if (strReplayShow.ProgramInfo.tmsID > 0)
                                {
                                    strcpy(szHeading,"  TMS ID 2: ");
                                    sprintf(szOutputBuffer,"%u",strReplayShow.ProgramInfo.tmsID);
                                    WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                                }

                                strcpy(szHeading,"  Instance: ");

                                if (fmodeDetailShowListings)
                                {
                                    if (strChannel.keep)
                                    {
                                        if (strChannel.stored < strChannel.keep)
                                        {
                                            sprintf(szOutputBuffer,"%u / %u of %u",strReplayShow.instance + 1,strChannel.stored,strChannel.keep);
                                        }
                                        else
                                        {
                                            sprintf(szOutputBuffer,"%u / %u",strReplayShow.instance + 1,strChannel.stored);
                                        }
                                    }
                                    else
                                    {
                                        sprintf(szOutputBuffer,"%u",strReplayShow.instance);
                                    }
                                }
                                else
                                {
                                    sprintf(szOutputBuffer,"%u",strReplayShow.instance);
                                }                                   
                                WrapDisplay(szHeading,szOutputBuffer,fmodeWrapDisplay);
                            }
                            
                            if (fmodeDebug)
                            {
                                printf("\n=Debug=\n");
                                printf("                             strReplayShow.unused: %u (0x%08x)\n",strReplayShow.unused,strReplayShow.unused);
                                printf("\n");

                            }
                            

                            // debug

                            if (strReplayShow.GOP_last > 0) {
                                    printf("\n");
                            }


                            if (strReplayShow.GOP_highest > 0) {
                                    printf("\n");
                            }

                            //

                            printf("\n");
                            
                            // Get file pointer (this is just for a handy breakpoint to
                            // be completely honest.)
                    }
                    
                    fptr = ftell( guidefile );
                  }
               }
               
            }
            while ( moreshows == true );
        }
        
        // Done, shut it down
        
        printf("Done!\n");
        fclose( guidefile );
             
        return 0;
        
}


