/*
 *      Copyright (C) 2014 Arne Morten Kvarving
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "kodi/libXBMC_addon.h"
#include "RingBuffer.h"

#include <iostream>
#include "SDAT.h"
#include "Player.h"

extern "C" {
#include <stdio.h>
#include <stdint.h>
#include "psflib.h"

#include "kodi/kodi_audiodec_dll.h"

ADDON::CHelper_libXBMC_addon *XBMC           = NULL;

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!XBMC)
    XBMC = new ADDON::CHelper_libXBMC_addon;

  if (!XBMC->RegisterMe(hdl))
  {
    delete XBMC, XBMC=NULL;
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  return ADDON_STATUS_OK;
}

//-- Stop ---------------------------------------------------------------------
// This dll must cease all runtime activities
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Stop()
{
}

//-- Destroy ------------------------------------------------------------------
// Do everything before unload of this add-on
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Destroy()
{
  XBMC=NULL;
}

//-- HasSettings --------------------------------------------------------------
// Returns true if this add-on use settings
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
bool ADDON_HasSettings()
{
  return false;
}

//-- GetStatus ---------------------------------------------------------------
// Returns the current Status of this visualisation
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_GetStatus()
{
  return ADDON_STATUS_OK;
}

//-- GetSettings --------------------------------------------------------------
// Return the settings for XBMC to display
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
unsigned int ADDON_GetSettings(ADDON_StructSetting ***sSet)
{
  return 0;
}

//-- FreeSettings --------------------------------------------------------------
// Free the settings struct passed from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------

void ADDON_FreeSettings()
{
}

//-- SetSetting ---------------------------------------------------------------
// Set a specific Setting value (called from XBMC)
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
ADDON_STATUS ADDON_SetSetting(const char *strSetting, const void* value)
{
  return ADDON_STATUS_OK;
}

//-- Announce -----------------------------------------------------------------
// Receive announcements from XBMC
// !!! Add-on master function !!!
//-----------------------------------------------------------------------------
void ADDON_Announce(const char *flag, const char *sender, const char *message, const void *data)
{
}

static void * psf_file_fopen( const char * uri )
{
  return  XBMC->OpenFile(uri, 0);
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
  return XBMC->ReadFile(handle, buffer, size*count);
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
  return XBMC->SeekFile(handle, offset, whence) > -1?0:-1;
}

static int psf_file_fclose( void * handle )
{
  XBMC->CloseFile(handle);

  return 0;
}

static long psf_file_ftell( void * handle )
{
  return XBMC->GetFilePosition(handle);
}

const psf_file_callbacks psf_file_system =
{
  "\\/",
  psf_file_fopen,
  psf_file_fread,
  psf_file_fseek,
  psf_file_fclose,
  psf_file_ftell
};

struct ncsf_loader_state
{
  uint32_t sseq;
  std::vector<uint8_t> sdatData;
  std::unique_ptr<SDAT> sdat;

  ncsf_loader_state() : sseq( 0 ) { }
};

struct NCSFContext
{
  ncsf_loader_state sseq;
  Player player;
  int64_t length;
  int sample_rate;
  int64_t pos;
  int year;
  std::string file;
  std::vector<uint8_t> qsound_state;
  CRingBuffer sample_buffer;
  std::string title;
  std::string album;
};


#define BORK_TIME 0xC0CAC01A
static unsigned long parse_time_crap(const char *input)
{
  if (!input) return BORK_TIME;
  int len = strlen(input);
  if (!len) return BORK_TIME;
  int value = 0;
  {
    int i;
    for (i = len - 1; i >= 0; i--)
    {
      if ((input[i] < '0' || input[i] > '9') && input[i] != ':' && input[i] != ',' && input[i] != '.')
      {
        return BORK_TIME;
      }
    }
  }
  std::string foo = input;
  char *bar = (char *) &foo[0];
  char *strs = bar + foo.size() - 1;
  while (strs > bar && (*strs >= '0' && *strs <= '9'))
  {
    strs--;
  }
  if (*strs == '.' || *strs == ',')
  {
    // fraction of a second
    strs++;
    if (strlen(strs) > 3) strs[3] = 0;
    value = atoi(strs);
    switch (strlen(strs))
    {
      case 1:
        value *= 100;
        break;
      case 2:
        value *= 10;
        break;
    }
    strs--;
    *strs = 0;
    strs--;
  }
  while (strs > bar && (*strs >= '0' && *strs <= '9'))
  {
    strs--;
  }
  // seconds
  if (*strs < '0' || *strs > '9') strs++;
  value += atoi(strs) * 1000;
  if (strs > bar)
  {
    strs--;
    *strs = 0;
    strs--;
    while (strs > bar && (*strs >= '0' && *strs <= '9'))
    {
      strs--;
    }
    if (*strs < '0' || *strs > '9') strs++;
    value += atoi(strs) * 60000;
    if (strs > bar)
    {
      strs--;
      *strs = 0;
      strs--;
      while (strs > bar && (*strs >= '0' && *strs <= '9'))
      {
        strs--;
      }
      value += atoi(strs) * 3600000;
    }
  }
  return value;
}

static int psf_info_meta(void * context, const char * name, const char * value)
{
  NCSFContext* ncsf = (NCSFContext*)context;

  if (!strcasecmp(name, "game"))
    ncsf->album = value;
  else if (!strcasecmp(name, "year"))
    ncsf->year = atoi(value);
  else if (!strcasecmp(name, "length"))
  {
    int temp = parse_time_crap(value);
    if (temp != BORK_TIME)
      ncsf->length = temp;
  }

  return 0;
}

inline unsigned get_le32( void const* p )
{
    return  (unsigned) ((unsigned char const*) p) [3] << 24 |
            (unsigned) ((unsigned char const*) p) [2] << 16 |
            (unsigned) ((unsigned char const*) p) [1] <<  8 |
            (unsigned) ((unsigned char const*) p) [0];
}

int ncsf_loader(void * context, const uint8_t * exe, size_t exe_size,
                const uint8_t * reserved, size_t reserved_size)
{
  struct ncsf_loader_state * state = ( struct ncsf_loader_state * ) context;

  if ( reserved_size >= 4 )
  {
    state->sseq = get_le32( reserved );
  }

  if ( exe_size >= 12 )
  {
    uint32_t sdat_size = get_le32( exe + 8 );
    if ( sdat_size > exe_size ) return -1;

    if ( state->sdatData.empty() )
      state->sdatData.resize( sdat_size, 0 );
    else if ( state->sdatData.size() < sdat_size )
      state->sdatData.resize( sdat_size );
    memcpy( &state->sdatData[0], exe, sdat_size );
  }

  return 0;
}

static bool Load(NCSFContext* r)
{
  if (psf_load(r->file.c_str(), &psf_file_system, 0x25,
               0, 0, psf_info_meta, r, 0) <= 0)
  {
    delete r;
    return false;
  }

  if (psf_load(r->file.c_str(), &psf_file_system, 0x25,
               ncsf_loader, &r->sseq, 0, 0, 0) < 0)
  {
    delete r;
    return false;
  }

  r->player.Stop(true);
  PseudoFile file;
  file.data = &r->sseq.sdatData;
  r->sseq.sdat.reset(new SDAT(file, r->sseq.sseq));
  auto* sseqToPlay = r->sseq.sdat->sseq.get();
  r->player.sseqVol = Cnv_Scale(sseqToPlay->info.vol);
  r->player.sampleRate = 48000;
  r->player.interpolation = INTERPOLATION_SINC;
  r->player.Setup(sseqToPlay);
  r->player.Timer();

  r->pos = 0;
  return true;
}

void* Init(const char* strFile, unsigned int filecache, int* channels,
           int* samplerate, int* bitspersample, int64_t* totaltime,
           int* bitrate, AEDataFormat* format, const AEChannel** channelinfo)
{
  NCSFContext* result = new NCSFContext;
  result->sample_buffer.Create(16384);
  result->file = strFile;
  if (!Load(result))
    return NULL;

  *totaltime = result->length;
  static enum AEChannel map[3] = {
    AE_CH_FL, AE_CH_FR, AE_CH_NULL
  };
  *format = AE_FMT_S16NE;
  *channelinfo = map;
  *channels = 2;
  *bitspersample = 16;
  *bitrate = 0.0;
  *samplerate = 48000;

  return result;
}

int ReadPCM(void* context, uint8_t* pBuffer, int size, int *actualsize)
{
  NCSFContext* ncsf = (NCSFContext*)context;
  if (ncsf->pos >= ncsf->length*48000*4/1000)
    return 1;

  if (ncsf->sample_buffer.getMaxReadSize() == 0) {
    std::vector<unsigned char> temp(2048*2*2);
    unsigned written=2048;
    ncsf->player.GenerateSamples(temp, 0, written);
    ncsf->sample_buffer.WriteData((const char*)&temp[0], written*4);
  }

  int tocopy = std::min(size, (int)ncsf->sample_buffer.getMaxReadSize());
  ncsf->sample_buffer.ReadData((char*)pBuffer, tocopy);
  ncsf->pos += tocopy;
  *actualsize = tocopy;
  return 0;
}

int64_t Seek(void* context, int64_t time)
{
  NCSFContext* ncsf = (NCSFContext*)context;

  int64_t pos = time*48000*4/1000;
  if (pos < ncsf->pos)
    Load(ncsf);

  std::vector<unsigned char> temp(2048*2*2);
  while (ncsf->pos < pos)
  {
    unsigned written=2048;
    ncsf->player.GenerateSamples(temp, 0, written);
    ncsf->pos += written*4;
  }

  int64_t overshoot = ncsf->pos-pos;

  ncsf->sample_buffer.Clear();
  ncsf->sample_buffer.WriteData((const char*)&temp[0], overshoot);
  ncsf->pos = pos;

  return time;
}

bool DeInit(void* context)
{
  delete (NCSFContext*)context;
  return true;
}

bool ReadTag(const char* strFile, char* title, char* artist, int* length)
{
  NCSFContext* result = new NCSFContext;
  if (psf_load(strFile, &psf_file_system, 0x25, 0, 0, psf_info_meta, result, 0) <= 0)
  {
    delete result;
    return false;
  }
  const char* rslash = strrchr(strFile,'/');
  if (!rslash)
    rslash = strrchr(strFile,'\\');
  strcpy(title,rslash+1);
  strcpy(artist,result->album.c_str());
  *length = result->length/1000;
  return true;
}

int TrackCount(const char* strFile)
{
  return 1;
}
}
