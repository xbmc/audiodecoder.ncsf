#define MYVERSION "1.9"

/*
	changelog

2014-10-24 11:43 UTC - CyberBotX
- Sync from in_ncsf v1.10.3
- Version is now 1.9

2014-03-27 20:32 UTC - kode54
- Fixed seeking when silence test buffer is not empty
- Version is now 1.8

2013-04-26 08:48 UTC - kode54
- Fixed a bug in the Lanczos resampler
- Version is now 1.7

2013-04-26 01:56 UTC - kode54
- Replaced windowed sinc interpolation with a different implementation
- Version is now 1.6

2013-04-23 06:17 UTC - kode54
- Implemented windowed sinc interpolation
- Version is now 1.5

2013-04-19 00:33 UTC - kode54
- Another interpolation fix
- Version is now 1.4

2013-04-16 03:46 UTC - kode54
- Interpolation fix
- Version is now 1.3

2013-04-16 02:49 UTC - kode54
- Crash fix related to previous change set
- Version is now 1.2

2013-04-16 02:29 UTC - kode54
- Fixed interpolation by implementing a sample history buffer
- Version is now 1.1

2013-04-16 00:33 UTC - kode54
- Initial release completed
- Version is now 1.0

2013-04-15 22:33 UTC - kode54
- Copied from foo_input_gsf code base

*/

#define _WIN32_WINNT 0x0501

#include "../SDK/foobar2000.h"
#include "../helpers/window_placement_helper.h"
#include "../ATLHelpers/ATLHelpers.h"

#include "resource.h"

#include <stdio.h>
#include <time.h>

#include "../../../SSEQPlayer/SDAT.h"
#include "../../../SSEQPlayer/Player.h"

#include <psflib.h>

#include "circular_buffer.h"

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>

#include <memory>

//#define DBG(a) OutputDebugString(a)
#define DBG(a)

typedef unsigned long u_long;

// {3B68C445-9A2C-43A3-AB55-A3243C0C2EF6}
static const GUID guid_cfg_sample_rate =
{ 0x3b68c445, 0x9a2c, 0x43a3, { 0xab, 0x55, 0xa3, 0x24, 0x3c, 0xc, 0x2e, 0xf6 } };
// {8FC7D2C7-BD9F-4551-B514-C4895EF69E82}
static const GUID guid_cfg_history_rate =
{ 0x8fc7d2c7, 0xbd9f, 0x4551, { 0xb5, 0x14, 0xc4, 0x89, 0x5e, 0xf6, 0x9e, 0x82 } };
// {8E4F962E-AFAD-416F-968E-CAC4A7A1ADC0}
static const GUID guid_cfg_infinite =
{ 0x8e4f962e, 0xafad, 0x416f, { 0x96, 0x8e, 0xca, 0xc4, 0xa7, 0xa1, 0xad, 0xc0 } };
// {327522F2-BDFA-4982-B593-5D34AF3E59FC}
static const GUID guid_cfg_deflength =
{ 0x327522f2, 0xbdfa, 0x4982, { 0xb5, 0x93, 0x5d, 0x34, 0xaf, 0x3e, 0x59, 0xfc } };
// {810437ED-647D-40AD-8257-A79BE51FE118}
static const GUID guid_cfg_deffade =
{ 0x810437ed, 0x647d, 0x40ad, { 0x82, 0x57, 0xa7, 0x9b, 0xe5, 0x1f, 0xe1, 0x18 } };
// {12C840BC-544B-45D4-B43E-7C955EC0A7E6}
static const GUID guid_cfg_suppressopeningsilence =
{ 0x12c840bc, 0x544b, 0x45d4, { 0xb4, 0x3e, 0x7c, 0x95, 0x5e, 0xc0, 0xa7, 0xe6 } };
// {D9AC5614-4B0D-4615-A560-2CA5F0773B33}
static const GUID guid_cfg_suppressendsilence =
{ 0xd9ac5614, 0x4b0d, 0x4615, { 0xa5, 0x60, 0x2c, 0xa5, 0xf0, 0x77, 0x3b, 0x33 } };
// {F61E9204-4C45-4E77-BD9B-B6018933E33F}
static const GUID guid_cfg_endsilenceseconds =
{ 0xf61e9204, 0x4c45, 0x4e77, { 0xbd, 0x9b, 0xb6, 0x1, 0x89, 0x33, 0xe3, 0x3f } };
// {D5A26FCB-DDFE-45CB-8F5A-BA0099639678}
static const GUID guid_cfg_interpolation =
{ 0xd5a26fcb, 0xddfe, 0x45cb, { 0x8f, 0x5a, 0xba, 0x0, 0x99, 0x63, 0x96, 0x78 } };
// {8CA9918F-8F72-413A-A57D-9D455FA5DC38}
static const GUID guid_cfg_placement =
{ 0x8ca9918f, 0x8f72, 0x413a, { 0xa5, 0x7d, 0x9d, 0x45, 0x5f, 0xa5, 0xdc, 0x38 } };

enum
{
	default_cfg_sample_rate = 44100,
	default_cfg_infinite = 0,
	default_cfg_deflength = 170000,
	default_cfg_deffade = 10000,
	default_cfg_suppressopeningsilence = 1,
	default_cfg_suppressendsilence = 1,
	default_cfg_endsilenceseconds = 5,
	default_cfg_interpolation = INTERPOLATION_SINC
};

static cfg_int cfg_sample_rate(guid_cfg_sample_rate,default_cfg_sample_rate);
static cfg_int cfg_infinite(guid_cfg_infinite,default_cfg_infinite);
static cfg_int cfg_deflength(guid_cfg_deflength,default_cfg_deflength);
static cfg_int cfg_deffade(guid_cfg_deffade,default_cfg_deffade);
static cfg_int cfg_suppressopeningsilence(guid_cfg_suppressopeningsilence,default_cfg_suppressopeningsilence);
static cfg_int cfg_suppressendsilence(guid_cfg_suppressendsilence,default_cfg_suppressendsilence);
static cfg_int cfg_endsilenceseconds(guid_cfg_endsilenceseconds,default_cfg_endsilenceseconds);
static cfg_int cfg_interpolation(guid_cfg_interpolation,default_cfg_interpolation);
static cfg_window_placement cfg_placement(guid_cfg_placement);

static cfg_dropdown_history cfg_history_rate(guid_cfg_history_rate,16);

static const char field_length[]="ncsf_length";
static const char field_fade[]="ncsf_fade";

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
	pfc::string8 foo = input;
	char *bar = (char *) foo.get_ptr();
	char *strs = bar + foo.length() - 1;
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

static void print_time_crap(int ms, char *out)
{
	char frac[8];
	int i,h,m,s;
	if (ms % 1000)
	{
		sprintf(frac, ".%3.3d", ms % 1000);
		for (i = 3; i > 0; i--)
			if (frac[i] == '0') frac[i] = 0;
		if (!frac[1]) frac[0] = 0;
	}
	else
		frac[0] = 0;
	h = ms / (60*60*1000);
	m = (ms % (60*60*1000)) / (60*1000);
	s = (ms % (60*1000)) / 1000;
	if (h) sprintf(out, "%d:%2.2d:%2.2d%s",h,m,s,frac);
	else if (m) sprintf(out, "%d:%2.2d%s",m,s,frac);
	else sprintf(out, "%d%s",s,frac);
}

static void info_meta_add(file_info & info, const char * tag, pfc::ptr_list_t< const char > const& values)
{
	t_size count = info.meta_get_count_by_name( tag );
	if ( count )
	{
		// append as another line
		pfc::string8 final = info.meta_get(tag, count - 1);
		final += "\r\n";
		final += values[0];
		info.meta_modify_value( info.meta_find( tag ), count - 1, final );
	}
	else
	{
		info.meta_add(tag, values[0]);
	}
	for ( count = 1; count < values.get_count(); count++ )
	{
		info.meta_add( tag, values[count] );
	}
}

static void info_meta_ansi( file_info & info )
{
	for ( unsigned i = 0, j = info.meta_get_count(); i < j; i++ )
	{
		for ( unsigned k = 0, l = info.meta_enum_value_count( i ); k < l; k++ )
		{
			const char * value = info.meta_enum_value( i, k );
			info.meta_modify_value( i, k, pfc::stringcvt::string_utf8_from_ansi( value ) );
		}
	}
	for ( unsigned i = 0, j = info.info_get_count(); i < j; i++ )
	{
		const char * name = info.info_enum_name( i );
		if ( name[ 0 ] == '_' )
			info.info_set( pfc::string8( name ), pfc::stringcvt::string_utf8_from_ansi( info.info_enum_value( i ) ) );
	}
}

static int find_crlf(pfc::string8 & blah)
{
	int pos = blah.find_first('\r');
	if (pos >= 0 && *(blah.get_ptr()+pos+1) == '\n') return pos;
	return -1;
}

static const char * fields_to_split[] = {"ARTIST", "ALBUM ARTIST", "PRODUCER", "COMPOSER", "PERFORMER", "GENRE"};

static bool meta_split_value( const char * tag )
{
	for ( unsigned i = 0; i < _countof( fields_to_split ); i++ )
	{
		if ( !stricmp_utf8( tag, fields_to_split[ i ] ) ) return true;
	}
	return false;
}

static void info_meta_write(pfc::string_base & tag, const file_info & info, const char * name, int idx, int & first)
{
	pfc::string8 v = info.meta_enum_value(idx, 0);
	if (meta_split_value(name))
	{
		t_size count = info.meta_enum_value_count(idx);
		for (t_size i = 1; i < count; i++)
		{
			v += "; ";
			v += info.meta_enum_value(idx, i);
		}
	}

	int pos = find_crlf(v);

	if (pos == -1)
	{
		if (first) first = 0;
		else tag.add_byte('\n');
		tag += name;
		tag.add_byte('=');
		// r->write(v.c_str(), v.length());
		tag += v;
		return;
	}
	while (pos != -1)
	{
		pfc::string8 foo;
		foo = v;
		foo.truncate(pos);
		if (first) first = 0;
		else tag.add_byte('\n');
		tag += name;
		tag.add_byte('=');
		tag += foo;
		v = v.get_ptr() + pos + 2;
		pos = find_crlf(v);
	}
	if (v.length())
	{
		tag.add_byte('\n');
		tag += name;
		tag.add_byte('=');
		tag += v;
	}
}

struct psf_info_meta_state
{
	file_info * info;

	pfc::string8_fast name;

	bool utf8;

	int tag_song_ms;
	int tag_fade_ms;

	psf_info_meta_state()
		: info( 0 ), utf8( false ), tag_song_ms( 0 ), tag_fade_ms( 0 )
	{
	}
};

static int psf_info_meta(void * context, const char * name, const char * value)
{
	psf_info_meta_state * state = ( psf_info_meta_state * ) context;

	pfc::string8_fast & tag = state->name;

	tag = name;

	if (!stricmp_utf8(tag, "game"))
	{
		DBG("reading game as album");
		tag = "album";
	}
	else if (!stricmp_utf8(tag, "year"))
	{
		DBG("reading year as date");
		tag = "date";
	}

	if (!stricmp_utf8_partial(tag, "replaygain_"))
	{
		DBG("reading RG info");
		//info.info_set(tag, value);
		state->info->info_set_replaygain(tag, value);
	}
	else if (!stricmp_utf8(tag, "length"))
	{
		DBG("reading length");
		int temp = parse_time_crap(value);
		if (temp != BORK_TIME)
		{
			state->tag_song_ms = temp;
			state->info->info_set_int(field_length, state->tag_song_ms);
		}
	}
	else if (!stricmp_utf8(tag, "fade"))
	{
		DBG("reading fade");
		int temp = parse_time_crap(value);
		if (temp != BORK_TIME)
		{
			state->tag_fade_ms = temp;
			state->info->info_set_int(field_fade, state->tag_fade_ms);
		}
	}
	else if (!stricmp_utf8(tag, "utf8"))
	{
		state->utf8 = true;
	}
	else if (!stricmp_utf8_partial(tag, "_lib"))
	{
		DBG("found _lib");
		state->info->info_set(tag, value);
	}
	else if (!stricmp_utf8(tag, "_refresh"))
	{
		DBG("found _refresh");
		state->info->info_set(tag, value);
	}
	else if (tag[0] == '_')
	{
		DBG("found unknown required tag, failing");
		console::formatter() << "Unsupported tag found: " << tag << ", required to play file.";
		return -1;
	}
	else
	{
		state->info->meta_add( tag, value );
	}

	return 0;
}

struct ncsf_loader_state
{
	uint32_t sseq;
	std::vector<uint8_t> sdatData;
	std::unique_ptr<SDAT> sdat;

	ncsf_loader_state() : sseq( 0 ) { }
};

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

static class psf_file_container
{
	critical_section lock;

	struct psf_file_opened
	{
		pfc::string_simple path;
		file::ptr f;

		psf_file_opened() { }

		psf_file_opened( const char * _p )
			: path( _p ) { }

		psf_file_opened( const char * _p, file::ptr _f )
			: path( _p ), f( _f ) { }

		bool operator== ( psf_file_opened const& in ) const
		{
			return !strcmp( path, in.path );
		}
	};

	pfc::list_t<psf_file_opened> hints;

public:
	void add_hint( const char * path, file::ptr f )
	{
		insync( lock );
		hints.add_item( psf_file_opened( path, f ) );
	}

	void remove_hint( const char * path )
	{
		insync( lock );
		hints.remove_item( psf_file_opened( path ) );
	}

	bool try_hint( const char * path, file::ptr & out )
	{
		insync( lock );
		t_size index = hints.find_item( psf_file_opened( path ) );
		if ( index == ~0 ) return false;
		out = hints[ index ].f;
		out->reopen( abort_callback_dummy() );
		return true;
	}
} g_hint_list;

struct psf_file_state
{
	file::ptr f;
};

static void * psf_file_fopen( const char * uri )
{
	try
	{
		psf_file_state * state = new psf_file_state;
		if ( !g_hint_list.try_hint( uri, state->f ) )
			filesystem::g_open( state->f, uri, filesystem::open_mode_read, abort_callback_dummy() );
		return state;
	}
	catch (...)
	{
		return NULL;
	}
}

static size_t psf_file_fread( void * buffer, size_t size, size_t count, void * handle )
{
	try
	{
		psf_file_state * state = ( psf_file_state * ) handle;
		size_t bytes_read = state->f->read( buffer, size * count, abort_callback_dummy() );
		return bytes_read / size;
	}
	catch (...)
	{
		return 0;
	}
}

static int psf_file_fseek( void * handle, int64_t offset, int whence )
{
	try
	{
		psf_file_state * state = ( psf_file_state * ) handle;
		state->f->seek_ex( offset, (foobar2000_io::file::t_seek_mode) whence, abort_callback_dummy() );
		return 0;
	}
	catch (...)
	{
		return -1;
	}
}

static int psf_file_fclose( void * handle )
{
	try
	{
		psf_file_state * state = ( psf_file_state * ) handle;
		delete state;
		return 0;
	}
	catch (...)
	{
		return -1;
	}
}

static long psf_file_ftell( void * handle )
{
	try
	{
		psf_file_state * state = ( psf_file_state * ) handle;
		return state->f->get_position( abort_callback_dummy() );
	}
	catch (...)
	{
		return -1;
	}
}

const psf_file_callbacks psf_file_system =
{
	"\\/|:",
	psf_file_fopen,
	psf_file_fread,
	psf_file_fseek,
	psf_file_fclose,
	psf_file_ftell
};

class input_ncsf
{
	bool no_loop, eof;

	circular_buffer<t_int16> silence_test_buffer;
	std::vector<uint8_t> sample_buffer;

	ncsf_loader_state m_sseq;

	Player m_player;

	service_ptr_t<file> m_file;

	pfc::string8 m_path;

	int err;

	int data_written,remainder,pos_delta,startsilence,silence;

	double ncsfemu_pos;

	int song_len,fade_len;
	int tag_song_ms,tag_fade_ms;

	file_info_impl m_info;

	bool first_block, do_filter, do_suppressendsilence;

public:
	input_ncsf() : silence_test_buffer( 0 )
	{
	}

	~input_ncsf()
	{
		g_hint_list.remove_hint( m_path );
	}

	void open( service_ptr_t<file> p_file, const char * p_path, t_input_open_reason p_reason, abort_callback & p_abort )
	{
		input_open_file_helper( p_file, p_path, p_reason, p_abort );

		m_path = p_path;
		g_hint_list.add_hint( p_path, p_file );

		psf_info_meta_state info_state;
		info_state.info = &m_info;

		if ( psf_load( p_path, &psf_file_system, 0x25, 0, 0, psf_info_meta, &info_state, 0 ) <= 0 )
			throw exception_io_data( "Not an NCSF file" );

		if ( !info_state.utf8 )
			info_meta_ansi( m_info );

		tag_song_ms = info_state.tag_song_ms;
		tag_fade_ms = info_state.tag_fade_ms;

		if (!tag_song_ms)
		{
			tag_song_ms = cfg_deflength;
			tag_fade_ms = cfg_deffade;
		}

		m_info.set_length( (double)( tag_song_ms + tag_fade_ms ) * .001 );
		m_info.info_set_int( "channels", 2 );

		m_file = p_file;
	}

	void get_info( file_info & p_info, abort_callback & p_abort )
	{
		p_info.copy( m_info );
	}

	t_filestats get_file_stats( abort_callback & p_abort )
	{
		return m_file->get_stats( p_abort );
	}

	void decode_initialize( unsigned p_flags, abort_callback & p_abort )
	{
		if ( m_sseq.sdatData.empty() )
		{
			if ( psf_load( m_path, &psf_file_system, 0x25, ncsf_loader, &m_sseq, 0, 0, 0 ) <= 0 )
				throw exception_io_data( "Invalid NCSF file" );
		}
		else
		{
			m_player.Stop( true );
		}

		m_player.interpolation = (Interpolation) (int) cfg_interpolation;

		srand(static_cast<unsigned>(time(nullptr)));

		PseudoFile file;
		file.data = &m_sseq.sdatData;

		m_sseq.sdat.reset(new SDAT(file, m_sseq.sseq));

		auto * sseqToPlay = m_sseq.sdat->sseq.get();

		m_player.sseqVol = Cnv_Scale(sseqToPlay->info.vol);
		m_player.sampleRate = cfg_sample_rate;
		m_player.Setup( sseqToPlay );
		m_player.Timer();

		ncsfemu_pos = 0.;

		startsilence = silence = 0;

		eof = 0;
		err = 0;
		data_written = 0;
		remainder = 0;
		pos_delta = 0;
		ncsfemu_pos = 0;
		no_loop = ( p_flags & input_flag_no_looping ) || !cfg_infinite;
		first_block = true;

		calcfade();

		sample_buffer.resize( 1024 * 2 * sizeof( int16_t ), 0 );

		do_suppressendsilence = !! cfg_suppressendsilence;

		unsigned skip_max = cfg_endsilenceseconds * m_player.sampleRate;

		if ( cfg_suppressopeningsilence ) // ohcrap
		{
			for (;;)
			{
				p_abort.check();

				unsigned skip_howmany = skip_max - silence;
				unsigned unskippable = 0;
				if ( skip_howmany > 1024 )
					skip_howmany = 1024;
				m_player.GenerateSamples( sample_buffer, 0, skip_howmany );
				short * foo = ( short * ) &sample_buffer[0];
				unsigned i;
				for ( i = 0; i < skip_howmany; ++i )
				{
					if ( foo[ 0 ] || foo[ 1 ] ) break;
					foo += 2;
				}
				silence += i;
				if ( i < skip_howmany )
				{
					remainder = skip_howmany - i + unskippable;
					memmove( &sample_buffer[0], foo, remainder * sizeof( int16_t ) * 2 );
					break;
				}
				if ( silence >= skip_max )
				{
					eof = true;
					break;
				}
			}

			startsilence += silence;
			silence = 0;
		}

		if ( do_suppressendsilence ) silence_test_buffer.resize( skip_max * 2 );
	}

	bool decode_run( audio_chunk & p_chunk, abort_callback & p_abort )
	{
		p_abort.check();

		if ( ( eof || err < 0 ) && !silence_test_buffer.data_available() ) return false;

		if ( no_loop && tag_song_ms && ( pos_delta + MulDiv( data_written, 1000, m_player.sampleRate ) ) >= tag_song_ms + tag_fade_ms )
			return false;

		UINT written = 0;

		int samples;

		if ( no_loop )
		{
			samples = ( song_len + fade_len ) - data_written;
			if ( samples > 1024 ) samples = 1024;
		}
		else
		{
			samples = 1024;
		}

		short * ptr;

		if ( do_suppressendsilence )
		{
			if ( !eof )
			{
				unsigned free_space = silence_test_buffer.free_space() / 2;
				while ( free_space )
				{
					p_abort.check();

					unsigned samples_to_render;
					if ( remainder )
					{
						samples_to_render = remainder;
						remainder = 0;
					}
					else
					{
						samples_to_render = free_space;
						if ( samples_to_render > 1024 )
							samples_to_render = 1024;
						m_player.GenerateSamples( sample_buffer, 0, samples_to_render );
					}
					silence_test_buffer.write( (short *) &sample_buffer[0], samples_to_render * 2 );
					free_space -= samples_to_render;
					if ( remainder )
					{
						memmove( &sample_buffer[0], ((short *) &sample_buffer[0]) + samples_to_render * 2, remainder * 4 );
					}
				}
			}

			if ( silence_test_buffer.test_silence() )
			{
				eof = true;
				return false;
			}

			written = silence_test_buffer.data_available() / 2;
			if ( written > 1024 ) written = 1024;
			silence_test_buffer.read( (short *) &sample_buffer[0], written * 2 );
			ptr = (short *) &sample_buffer[0];
		}
		else
		{
			if ( remainder )
			{
				written = remainder;
				remainder = 0;
			}
			else
			{
				written = 1024;
				m_player.GenerateSamples( sample_buffer, 0, written );
			}

			ptr = (short *) &sample_buffer[0];
		}

		ncsfemu_pos += double( written ) / double( m_player.sampleRate );

		int d_start, d_end;
		d_start = data_written;
		data_written += written;
		d_end = data_written;

		if ( tag_song_ms && d_end > song_len && no_loop )
		{
			short * foo = ptr;
			int n;
			for( n = d_start; n < d_end; ++n )
			{
				if ( n > song_len )
				{
					if ( n > song_len + fade_len )
					{
						* ( DWORD * ) foo = 0;
					}
					else
					{
						int bleh = song_len + fade_len - n;
						foo[ 0 ] = MulDiv( foo[ 0 ], bleh, fade_len );
						foo[ 1 ] = MulDiv( foo[ 1 ], bleh, fade_len );
					}
				}
				foo += 2;
			}
		}

		p_chunk.set_data_fixedpoint( ptr, written * 4, m_player.sampleRate, 2, 16, audio_chunk::channel_config_stereo );

		return true;
	}

	void decode_seek( double p_seconds, abort_callback & p_abort )
	{
		eof = false;
		first_block = true;

		double buffered_time = (double)(silence_test_buffer.data_available() / 2) / 44100.0;

		ncsfemu_pos += buffered_time;

		silence_test_buffer.reset();

		if ( p_seconds < ncsfemu_pos )
		{
			decode_initialize( no_loop ? input_flag_no_looping : 0, p_abort );
		}
		unsigned int howmany = ( int )( audio_math::time_to_samples( p_seconds - ncsfemu_pos, m_player.sampleRate ) );

		// more abortable, and emu doesn't like doing huge numbers of samples per call anyway
		while ( howmany )
		{
			p_abort.check();

			unsigned samples = 1024;
			m_player.GenerateSamples( sample_buffer, 0, samples );
			if ( samples > howmany )
			{
				memmove( &sample_buffer[0], ((short *) &sample_buffer[0]) + howmany * 2, ( samples - howmany ) * 4 );
				remainder = samples - howmany;
				samples = howmany;
			}
			howmany -= samples;
		}

		data_written = 0;
		pos_delta = ( int )( p_seconds * 1000. );
		ncsfemu_pos = p_seconds;

		calcfade();
	}

	bool decode_can_seek()
	{
		return true;
	}

	bool decode_get_dynamic_info( file_info & p_out, double & p_timestamp_delta )
	{
		if ( first_block )
		{
			p_out.info_set_int( "samplerate", m_player.sampleRate );
			p_timestamp_delta = 0.;
			first_block = false;
			return true;
		}
		return false;
	}

	bool decode_get_dynamic_info_track( file_info & p_out, double & p_timestamp_delta )
	{
		return false;
	}

	void decode_on_idle( abort_callback & p_abort )
	{
	}

	void retag( const file_info & p_info, abort_callback & p_abort )
	{
		m_info.copy( p_info );

		pfc::array_t<t_uint8> buffer;
		buffer.set_size( 16 );

		m_file->seek( 0, p_abort );

		BYTE *ptr = buffer.get_ptr();
		m_file->read_object( ptr, 16, p_abort );
		if (ptr[0] != 'P' || ptr[1] != 'S' || ptr[2] != 'F' ||
			ptr[3] != 0x25) throw exception_io_data();
		int reserved_size = pfc::byteswap_if_be_t( ((unsigned long*)ptr)[1] );
		int exe_size = pfc::byteswap_if_be_t( ((unsigned long*)ptr)[2] );
		m_file->seek(16 + reserved_size + exe_size, p_abort);
		m_file->set_eof(p_abort);

		pfc::string8 tag = "[TAG]utf8=1\n";

		int first = 1;
		// _lib and _refresh tags first
		int n, p = p_info.info_get_count();
		for (n = 0; n < p; n++)
		{
			const char *t = p_info.info_enum_name(n);
			if (*t == '_')
			{
				if (first) first = 0;
				else tag.add_byte('\n');
				tag += t;
				tag.add_byte('=');
				tag += p_info.info_enum_value(n);
			}
		}
		// Then info
		p = p_info.meta_get_count();
		for (n = 0; n < p; n++)
		{
			const char * t = p_info.meta_enum_name(n);
			if (*t == '_' ||
				!stricmp(t, "length") ||
				!stricmp(t, "fade")) continue; // dummy protection
			if (!stricmp(t, "album")) info_meta_write(tag, p_info, "game", n, first);
			else if (!stricmp(t, "date"))
			{
				const char * val = p_info.meta_enum_value(n, 0);
				char * end;
				strtoul(p_info.meta_enum_value(n, 0), &end, 10);
				if (size_t(end - val) < strlen(val))
					info_meta_write(tag, p_info, t, n, first);
				else
					info_meta_write(tag, p_info, "year", n, first);
			}
			else info_meta_write(tag, p_info, t, n, first);
		}
		// Then time and fade
		{
			int tag_song_ms = 0, tag_fade_ms = 0;
			const char *t = p_info.info_get(field_length);
			if (t)
			{
				char temp[16];
				tag_song_ms = atoi(t);
				if (first) first = 0;
				else tag.add_byte('\n');
				tag += "length=";
				print_time_crap(tag_song_ms, temp);
				tag += temp;
				t = p_info.info_get(field_fade);
				if (t)
				{
					tag_fade_ms = atoi(t);
					tag.add_byte('\n');
					tag += "fade=";
					print_time_crap(tag_fade_ms, (char *)&temp);
					tag += temp;
				}
			}
		}

		// Then ReplayGain
		/*
		p = p_info.info_get_count();
		for (n = 0; n < p; n++)
		{
			const char *t = p_info.info_enum_name(n);
			if (!strnicmp(t, "replaygain_",11))
			{
				if (first) first = 0;
				else tag.add_byte('\n');
				tag += t;
				else tag.add_byte('=');
				tag += p_info.info_enum_value(n);
			}
		}
		*/
		replaygain_info rg = p_info.get_replaygain();
		char rgbuf[replaygain_info::text_buffer_size];
		if (rg.is_track_gain_present())
		{
			rg.format_track_gain(rgbuf);
			if (first) first = 0;
			else tag.add_byte('\n');
			tag += "replaygain_track_gain";
			tag.add_byte('=');
			tag += rgbuf;
		}
		if (rg.is_track_peak_present())
		{
			rg.format_track_peak(rgbuf);
			if (first) first = 0;
			else tag.add_byte('\n');
			tag += "replaygain_track_peak";
			tag.add_byte('=');
			tag += rgbuf;
		}
		if (rg.is_album_gain_present())
		{
			rg.format_album_gain(rgbuf);
			if (first) first = 0;
			else tag.add_byte('\n');
			tag += "replaygain_album_gain";
			tag.add_byte('=');
			tag += rgbuf;
		}
		if (rg.is_album_peak_present())
		{
			rg.format_album_peak(rgbuf);
			if (first) first = 0;
			else tag.add_byte('\n');
			tag += "replaygain_album_peak";
			tag.add_byte('=');
			tag += rgbuf;
		}

		m_file->write_object( tag.get_ptr(), tag.length(), p_abort );
	}

	static bool g_is_our_content_type( const char * p_content_type )
	{
		return false;
	}

	static bool g_is_our_path( const char * p_full_path, const char * p_extension )
	{
		return (!stricmp(p_extension,"ncsf") || !stricmp(p_extension,"minincsf"));
	}

private:
	void calcfade()
	{
		song_len=MulDiv(tag_song_ms-pos_delta,m_player.sampleRate,1000);
		fade_len=MulDiv(tag_fade_ms,m_player.sampleRate,1000);
	}
};

static const int srate_tab[]={8000,11025,16000,22050,24000,32000,44100,48000,64000,88200,96000};

class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance {
public:
	//Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
	CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

	//Note that we don't bother doing anything regarding destruction of our class.
	//The host ensures that our dialog is destroyed first, then the last reference to our preferences_page_instance object is released, causing our object to be deleted.


	//dialog resource ID
	enum {IDD = IDD_PSF_CONFIG};
	// preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
	t_uint32 get_state();
	void apply();
	void reset();

	//WTL message map
	BEGIN_MSG_MAP(CMyPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_INDEFINITE, BN_CLICKED, OnButtonClick)
		COMMAND_HANDLER_EX(IDC_SOS, BN_CLICKED, OnButtonClick)
		COMMAND_HANDLER_EX(IDC_SES, BN_CLICKED, OnButtonClick)
		COMMAND_HANDLER_EX(IDC_SILENCE, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_DLENGTH, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_DFADE, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_SAMPLERATE, CBN_EDITCHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_SAMPLERATE, CBN_SELCHANGE, OnSelChange)
		DROPDOWN_HISTORY_HANDLER(IDC_SAMPLERATE, cfg_history_rate)
		COMMAND_HANDLER_EX(IDC_INTERPOLATION, CBN_SELCHANGE, OnSelChange)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM);
	void OnEditChange(UINT, int, CWindow);
	void OnButtonClick(UINT, int, CWindow);
	void OnSelChange(UINT, int, CWindow);
	bool HasChanged();
	void OnChanged();

	const preferences_page_callback::ptr m_callback;

	CComboBox m_interpolation;

	CHyperLink m_link_ncsf;
	CHyperLink m_link_kode54;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM) {
	SendDlgItemMessage( IDC_INDEFINITE, BM_SETCHECK, cfg_infinite );
	SendDlgItemMessage( IDC_SOS, BM_SETCHECK, cfg_suppressopeningsilence );
	SendDlgItemMessage( IDC_SES, BM_SETCHECK, cfg_suppressendsilence );

	SetDlgItemInt( IDC_SILENCE, cfg_endsilenceseconds, FALSE );

	{
		char temp[16];
		// wsprintf((char *)&temp, "= %d Hz", 33868800 / cfg_divider);
		// SetDlgItemText(wnd, IDC_HZ, (char *)&temp);

		print_time_crap( cfg_deflength, (char *)&temp );
		uSetDlgItemText( m_hWnd, IDC_DLENGTH, (char *)&temp );

		print_time_crap( cfg_deffade, (char *)&temp );
		uSetDlgItemText( m_hWnd, IDC_DFADE, (char *)&temp );
	}

	m_interpolation = GetDlgItem( IDC_INTERPOLATION );
	m_interpolation.AddString( _T( "None" ) );
	m_interpolation.AddString( _T( "Linear" ) );
	m_interpolation.AddString( _T( "4 Point Legrange" ) );
	m_interpolation.AddString( _T( "6 Point Legrange" ) );
	m_interpolation.AddString( _T( "16 Point Nuttall 3-Term Sinc" ) );
	m_interpolation.SetCurSel( cfg_interpolation );

	char temp[16];

	for( unsigned n = _countof(srate_tab); n--; )
	{
		if ( srate_tab[n] != cfg_sample_rate )
		{
			itoa(srate_tab[n], temp, 10);
			cfg_history_rate.add_item(temp);
		}
	}
	itoa( cfg_sample_rate, temp, 10 );
	cfg_history_rate.add_item(temp);
	CWindow w = GetDlgItem( IDC_SAMPLERATE );
	cfg_history_rate.setup_dropdown( w );
	w.SendMessage( CB_SETCURSEL, 0 );

	m_link_ncsf.SetLabel( _T( "NCSF Home Page" ) );
	m_link_ncsf.SetHyperLink( _T( "http://www.cyberbotx.com/NCSF/" ) );
	m_link_ncsf.SubclassWindow( GetDlgItem( IDC_URL ) );

	m_link_kode54.SetLabel( _T( "kode's foobar2000 plug-ins" ) );
	m_link_kode54.SetHyperLink( _T( "http://kode54.foobar2000.org/" ) );
	m_link_kode54.SubclassWindow( GetDlgItem( IDC_K54 ) );

	{
		/*OSVERSIONINFO ovi = { 0 };
		ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		BOOL bRet = ::GetVersionEx(&ovi);
		if ( bRet && ( ovi.dwMajorVersion >= 5 ) )*/
		{
			DWORD color = GetSysColor( 26 /* COLOR_HOTLIGHT */ );
			m_link_ncsf.m_clrLink = color;
			m_link_ncsf.m_clrVisited = color;
			m_link_kode54.m_clrLink = color;
			m_link_kode54.m_clrVisited = color;
		}
	}

	return FALSE;
}

void CMyPreferences::OnEditChange(UINT, int, CWindow) {
	OnChanged();
}

void CMyPreferences::OnButtonClick(UINT, int, CWindow) {
	OnChanged();
}

void CMyPreferences::OnSelChange(UINT, int, CWindow) {
	OnChanged();
}

t_uint32 CMyPreferences::get_state() {
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}

void CMyPreferences::reset() {
	char temp[16];
	SendDlgItemMessage( IDC_INDEFINITE, BM_SETCHECK, default_cfg_infinite );
	SendDlgItemMessage( IDC_SOS, BM_SETCHECK, default_cfg_suppressopeningsilence );
	SendDlgItemMessage( IDC_SES, BM_SETCHECK, default_cfg_suppressendsilence );
	SetDlgItemInt( IDC_SAMPLERATE, default_cfg_sample_rate, FALSE );
	m_interpolation.SetCurSel( default_cfg_interpolation );
	SetDlgItemInt( IDC_SILENCE, default_cfg_endsilenceseconds, FALSE );
	print_time_crap( default_cfg_deflength, (char *)&temp );
	uSetDlgItemText( m_hWnd, IDC_DLENGTH, (char *)&temp );
	print_time_crap( default_cfg_deffade, (char *)&temp );
	uSetDlgItemText( m_hWnd, IDC_DFADE, (char *)&temp );

	OnChanged();
}

void CMyPreferences::apply() {
	int t;
	char temp[16];
	cfg_infinite = SendDlgItemMessage( IDC_INDEFINITE, BM_GETCHECK );
	cfg_suppressopeningsilence = SendDlgItemMessage( IDC_SOS, BM_GETCHECK );
	cfg_suppressendsilence = SendDlgItemMessage( IDC_SES, BM_GETCHECK );
	t = GetDlgItemInt( IDC_SAMPLERATE, NULL, FALSE );
	if ( t < 6000 ) t = 6000;
	else if ( t > 96000 ) t = 96000;
	SetDlgItemInt( IDC_SAMPLERATE, t, FALSE );
	itoa( t, temp, 10 );
	cfg_history_rate.add_item(temp);
	cfg_sample_rate = t;
	cfg_interpolation = m_interpolation.GetCurSel();
	t = GetDlgItemInt( IDC_SILENCE, NULL, FALSE );
	if ( t > 0 ) cfg_endsilenceseconds = t;
	SetDlgItemInt( IDC_SILENCE, cfg_endsilenceseconds, FALSE );
	t = parse_time_crap( string_utf8_from_window( GetDlgItem( IDC_DLENGTH ) ) );
	if ( t != BORK_TIME ) cfg_deflength = t;
	else
	{
		print_time_crap( cfg_deflength, (char *)&temp );
		uSetDlgItemText( m_hWnd, IDC_DLENGTH, (char *)&temp );
	}
	t = parse_time_crap( string_utf8_from_window( GetDlgItem( IDC_DFADE ) ) );
	if ( t != BORK_TIME ) cfg_deffade = t;
	else
	{
		print_time_crap( cfg_deffade, (char *)&temp );
		uSetDlgItemText( m_hWnd, IDC_DFADE, (char *)&temp );
	}

	OnChanged(); //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

bool CMyPreferences::HasChanged() {
	//returns whether our dialog content is different from the current configuration (whether the apply button should be enabled or not)
	bool changed = false;
	if ( !changed && SendDlgItemMessage( IDC_INDEFINITE, BM_GETCHECK ) != cfg_infinite ) changed = true;
	if ( !changed && SendDlgItemMessage( IDC_SOS, BM_GETCHECK ) != cfg_suppressopeningsilence ) changed = true;
	if ( !changed && SendDlgItemMessage( IDC_SES, BM_GETCHECK ) != cfg_suppressendsilence ) changed = true;
	if ( !changed && GetDlgItemInt( IDC_SAMPLERATE, NULL, FALSE ) != cfg_sample_rate ) changed = true;
	if ( !changed && m_interpolation.GetCurSel() != cfg_interpolation ) changed = true;
	if ( !changed && GetDlgItemInt( IDC_SILENCE, NULL, FALSE ) != cfg_endsilenceseconds ) changed = true;
	if ( !changed )
	{
		int t = parse_time_crap( string_utf8_from_window( GetDlgItem( IDC_DLENGTH ) ) );
		if ( t != BORK_TIME && t != cfg_deflength ) changed = true;
	}
	if ( !changed )
	{
		int t = parse_time_crap( string_utf8_from_window( GetDlgItem( IDC_DFADE ) ) );
		if ( t != BORK_TIME && t != cfg_deffade ) changed = true;
	}
	return changed;
}
void CMyPreferences::OnChanged() {
	//tell the host that our state has changed to enable/disable the apply button appropriately.
	m_callback->on_state_changed();
}

class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
	// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
	const char * get_name() {return "NCSF Decoder";}
	GUID get_guid() {
		// {B0F9D2D7-805E-4DA4-A7F5-454AE772EBB8}
		static const GUID guid = { 0xb0f9d2d7, 0x805e, 0x4da4, { 0xa7, 0xf5, 0x45, 0x4a, 0xe7, 0x72, 0xeb, 0xb8 } };
		return guid;
	}
	GUID get_parent_guid() {return guid_input;}
};

typedef struct
{
	unsigned song, fade;
} INFOSTRUCT;

static BOOL CALLBACK TimeProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		uSetWindowLong(wnd,DWL_USER,lp);
		{
			INFOSTRUCT * i=(INFOSTRUCT*)lp;
			char temp[16];
			if (!i->song && !i->fade) uSetWindowText(wnd, "Set length");
			else uSetWindowText(wnd, "Edit length");
			if ( i->song != ~0 )
			{
				print_time_crap(i->song, (char*)&temp);
				uSetDlgItemText(wnd, IDC_LENGTH, (char*)&temp);
			}
			if ( i->fade != ~0 )
			{
				print_time_crap(i->fade, (char*)&temp);
				uSetDlgItemText(wnd, IDC_FADE, (char*)&temp);
			}
		}
		cfg_placement.on_window_creation(wnd);
		return 1;
	case WM_COMMAND:
		switch(wp)
		{
		case IDOK:
			{
				INFOSTRUCT * i=(INFOSTRUCT*)uGetWindowLong(wnd,DWL_USER);
				int foo;
				foo = parse_time_crap(string_utf8_from_window(wnd, IDC_LENGTH));
				if (foo != BORK_TIME) i->song = foo;
				else i->song = ~0;
				foo = parse_time_crap(string_utf8_from_window(wnd, IDC_FADE));
				if (foo != BORK_TIME) i->fade = foo;
				else i->fade = ~0;
			}
			EndDialog(wnd,1);
			break;
		case IDCANCEL:
			EndDialog(wnd,0);
			break;
		}
		break;
	case WM_DESTROY:
		cfg_placement.on_window_destruction(wnd);
		break;
	}
	return 0;
}

static bool context_time_dialog(unsigned * song_ms, unsigned * fade_ms)
{
	bool ret;
	INFOSTRUCT * i = new INFOSTRUCT;
	if (!i) return 0;
	i->song = *song_ms;
	i->fade = *fade_ms;
	HWND hwnd = core_api::get_main_window();
	ret = uDialogBox(IDD_TIME, hwnd, TimeProc, (long)i) > 0;
	if (ret)
	{
		*song_ms = i->song;
		*fade_ms = i->fade;
	}
	delete i;
	return ret;
}

class length_info_filter : public file_info_filter
{
	bool set_length, set_fade;
	unsigned m_length, m_fade;

	metadb_handle_list m_handles;

public:
	length_info_filter( const pfc::list_base_const_t<metadb_handle_ptr> & p_list )
	{
		set_length = false;
		set_fade = false;

		pfc::array_t<t_size> order;
		order.set_size(p_list.get_count());
		order_helper::g_fill(order.get_ptr(),order.get_size());
		p_list.sort_get_permutation_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,order.get_ptr());
		m_handles.set_count(order.get_size());
		for(t_size n = 0; n < order.get_size(); n++) {
			m_handles[n] = p_list[order[n]];
		}

	}

	void length( unsigned p_length )
	{
		set_length = true;
		m_length = p_length;
	}

	void fade( unsigned p_fade )
	{
		set_fade = true;
		m_fade = p_fade;
	}

	virtual bool apply_filter(metadb_handle_ptr p_location,t_filestats p_stats,file_info & p_info)
	{
		t_size index;
		if (m_handles.bsearch_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,p_location,index))
		{
			if ( set_length )
			{
				if ( m_length ) p_info.info_set_int( field_length, m_length );
				else p_info.info_remove( field_length );
			}
			if ( set_fade )
			{
				if ( m_fade ) p_info.info_set_int( field_fade, m_fade );
				else p_info.info_remove( field_fade );
			}
			return set_length | set_fade;
		}
		else
		{
			return false;
		}
	}
};

class context_ncsf : public contextmenu_item_simple
{
public:
	virtual unsigned get_num_items() { return 1; }

	virtual void get_item_name(unsigned n, pfc::string_base & out)
	{
		if (n) uBugCheck();
		out = "Edit length";
	}

	/*virtual void get_item_default_path(unsigned n, pfc::string_base & out)
	{
		out.reset();
	}*/
	GUID get_parent() {return contextmenu_groups::tagging;}

	virtual bool get_item_description(unsigned n, pfc::string_base & out)
	{
		if (n) uBugCheck();
		out = "Edits the length of the selected NCSF file, or sets the length of all selected NCSF files.";
		return true;
	}

	virtual GUID get_item_guid(unsigned p_index)
	{
		if (p_index) uBugCheck();
		// {EFB10FFC-D4EA-434A-A840-C47F1F3ABEF4}
		static const GUID guid = { 0xefb10ffc, 0xd4ea, 0x434a, { 0xa8, 0x40, 0xc4, 0x7f, 0x1f, 0x3a, 0xbe, 0xf4 } };
		return guid;
	}

	virtual bool context_get_display(unsigned n,const pfc::list_base_const_t<metadb_handle_ptr> & data,pfc::string_base & out,unsigned & displayflags,const GUID &)
	{
		if (n) uBugCheck();
		unsigned i, j;
		i = data.get_count();
		for (j = 0; j < i; j++)
		{
			pfc::string_extension ext(data.get_item(j)->get_path());
			if (stricmp_utf8(ext, "NCSF") && stricmp_utf8(ext, "MININCSF")) return false;
		}
		if (i == 1) out = "Edit length";
		else out = "Set length";
		return true;
	}

	virtual void context_command(unsigned n,const pfc::list_base_const_t<metadb_handle_ptr> & data,const GUID& caller)
	{
		if (n) uBugCheck();
		unsigned tag_song_ms = 0, tag_fade_ms = 0;
		unsigned i = data.get_count();
		file_info_impl info;
		abort_callback_impl m_abort;
		if (i == 1)
		{
			// fetch info from single file
			metadb_handle_ptr handle = data.get_item(0);
			handle->metadb_lock();
			const file_info * p_info;
			if (handle->get_info_locked(p_info) && p_info)
			{
				const char *t = p_info->info_get(field_length);
				if (t) tag_song_ms = atoi(t);
				t = p_info->info_get(field_fade);
				if (t) tag_fade_ms = atoi(t);
			}
			handle->metadb_unlock();
		}
		if (!context_time_dialog(&tag_song_ms, &tag_fade_ms)) return;
		static_api_ptr_t<metadb_io_v2> p_imgr;

		service_ptr_t<length_info_filter> p_filter = new service_impl_t< length_info_filter >( data );
		if ( tag_song_ms != ~0 ) p_filter->length( tag_song_ms );
		if ( tag_fade_ms != ~0 ) p_filter->fade( tag_fade_ms );

		p_imgr->update_info_async( data, p_filter, core_api::get_main_window(), 0, 0 );
	}
};

class version_ncsf : public componentversion
{
public:
	virtual void get_file_name(pfc::string_base & out) { out = core_api::get_my_file_name(); }
	virtual void get_component_name(pfc::string_base & out) { out = "NCSF Decoder"; }
	virtual void get_component_version(pfc::string_base & out) { out = MYVERSION; }
	virtual void get_about_message(pfc::string_base & out)
	{
		out = "Foobar2000 version by kode54\nOriginal library by Naram Qashat (CyberBotX) <cyberbotx@cyberbotx.com>" /*"\n\nCore: ";
		out += psx_getversion();
		out +=*/ "\n\nhttp://www.cyberbotx.com/NCSF/\nhttps://github.com/kode54/SSEQPlayer\nhttps://github.com/kode54/foo_input_ncsf\nhttp://kode54.foobar2000.org/";
	}
};

DECLARE_FILE_TYPE( "NCSF files", "*.NCSF;*.MININCSF" );

static input_singletrack_factory_t<input_ncsf>              g_input_ncsf_factory;
static preferences_page_factory_t <preferences_page_myimpl> g_config_ncsf_factory;
static contextmenu_item_factory_t <context_ncsf>            g_contextmenu_item_ncsf_factory;
static service_factory_single_t   <version_ncsf>            g_componentversion_ncsf_factory;

VALIDATE_COMPONENT_FILENAME("foo_input_ncsf.dll");
