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

#include <kodi/addon-instance/AudioDecoder.h>
#include <kodi/Filesystem.h>
#include "RingBuffer.h"

#include <algorithm>
#include <iostream>
#include "SDAT.h"
#include "Player.h"

extern "C" {
#include <stdio.h>
#include <stdint.h>
#include "psflib.h"

static void * psf_file_fopen( const char * uri )
{
  kodi::vfs::CFile* file = new kodi::vfs::CFile;
  if (!file->OpenFile(uri, 0))
  {
    delete file;
    return nullptr;
  }

  return file;
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->Read(buffer, size*count);
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->Seek(offset, whence) > -1 ? 0 : -1;
}

static int psf_file_fclose( void * handle )
{
  delete static_cast<kodi::vfs::CFile*>(handle);

  return 0;
}

static long psf_file_ftell( void * handle )
{
  kodi::vfs::CFile* file = static_cast<kodi::vfs::CFile*>(handle);
  return file->GetPosition();
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

}

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

class CNCSFCodec : public kodi::addon::CInstanceAudioDecoder,
                   public kodi::addon::CAddonBase
{
public:
  CNCSFCodec(KODI_HANDLE instance) :
    CInstanceAudioDecoder(instance) {}

  virtual ~CNCSFCodec()
  {
  }

  virtual bool Init(const std::string& filename, unsigned int filecache,
                    int& channels, int& samplerate,
                    int& bitspersample, int64_t& totaltime,
                    int& bitrate, AEDataFormat& format,
                    std::vector<AEChannel>& channellist) override
  {
    ctx.sample_buffer.Create(16384);
    ctx.file = filename;
    if (!Load(&ctx))
      return false;

    totaltime = ctx.length;
    format = AE_FMT_S16NE;
    channellist = { AE_CH_FL, AE_CH_FR };
    channels = 2;
    bitspersample = 16;
    bitrate = 0.0;
    samplerate = 48000;

    return true;
  }

  virtual int ReadPCM(uint8_t* buffer, int size, int& actualsize) override
  {
    if (ctx.pos >= ctx.length*48000*4/1000)
      return 1;

    if (ctx.sample_buffer.getMaxReadSize() == 0) {
      std::vector<unsigned char> temp(2048*2*2);
      unsigned written=2048;
      ctx.player.GenerateSamples(temp, 0, written);
      ctx.sample_buffer.WriteData((const char*)&temp[0], written*4);
    }

    int tocopy = std::min(size, (int)ctx.sample_buffer.getMaxReadSize());
    ctx.sample_buffer.ReadData((char*)buffer, tocopy);
    ctx.pos += tocopy;
    actualsize = tocopy;
    return 0;
  }

  virtual int64_t Seek(int64_t time) override
  {
    int64_t pos = time*48000*4/1000;
    if (pos < ctx.pos)
      Load(&ctx);

    std::vector<unsigned char> temp(2048*2*2);
    while (ctx.pos < pos)
    {
      unsigned written=2048;
      ctx.player.GenerateSamples(temp, 0, written);
      ctx.pos += written*4;
    }

    int64_t overshoot = ctx.pos-pos;

    ctx.sample_buffer.Clear();
    ctx.sample_buffer.WriteData((const char*)&temp[0], overshoot);
    ctx.pos = pos;

    return time;
  }

  virtual bool ReadTag(const std::string& file, std::string& title,
                       std::string& artist, int& length) override
  {
    NCSFContext result;
    if (psf_load(file.c_str(), &psf_file_system, 0x25, 0, 0, psf_info_meta, &result, 0) <= 0)
      return false;

    const char* rslash = strrchr(file.c_str(),'/');
    if (!rslash)
      rslash = strrchr(file.c_str(),'\\');
    title = rslash+1;
    artist = result.album;
    length = result.length/1000;
    return true;
  }

private:
  NCSFContext ctx;
};


class ATTRIBUTE_HIDDEN CMyAddon : public kodi::addon::CAddonBase
{
public:
  CMyAddon() { }
  virtual ADDON_STATUS CreateInstance(int instanceType, std::string instanceID, KODI_HANDLE instance, KODI_HANDLE& addonInstance) override
  {
    addonInstance = new CNCSFCodec(instance);
    return ADDON_STATUS_OK;
  }
  virtual ~CMyAddon()
  {
  }
};


ADDONCREATOR(CMyAddon);
