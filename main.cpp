//--------------------------------------------//
// WebMIDIPlayer                              //
// License: Public Domain (www.unlicense.org) //
//--------------------------------------------//

#include <ZL_Application.h>
#include <ZL_Display.h>
#include <ZL_Audio.h>
#include <ZL_Font.h>

// Avoid name conflicts when statically linking with ZillaLib
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_INTEGER_CONVERSION
#define stb_vorbis_get_info                      wmstb_vorbis_get_info
#define stb_vorbis_get_comment                   wmstb_vorbis_get_comment
#define stb_vorbis_get_error                     wmstb_vorbis_get_error
#define stb_vorbis_close                         wmstb_vorbis_close
#define stb_vorbis_get_sample_offset             wmstb_vorbis_get_sample_offset
#define stb_vorbis_get_file_offset               wmstb_vorbis_get_file_offset
#define stb_vorbis_decode_memory                 wmstb_vorbis_decode_memory
#define stb_vorbis_open_memory                   wmstb_vorbis_open_memory
#define stb_vorbis_seek_frame                    wmstb_vorbis_seek_frame
#define stb_vorbis_seek                          wmstb_vorbis_seek
#define stb_vorbis_seek_start                    wmstb_vorbis_seek_start
#define stb_vorbis_stream_length_in_samples      wmstb_vorbis_stream_length_in_samples
#define stb_vorbis_stream_length_in_seconds      wmstb_vorbis_stream_length_in_seconds
#define stb_vorbis_get_frame_float               wmstb_vorbis_get_frame_float
#define stb_vorbis_get_samples_float_interleaved wmstb_vorbis_get_samples_float_interleaved
#define stb_vorbis_get_samples_float             wmstb_vorbis_get_samples_float
#include "stb_vorbis.inl"

#define TSF_IMPLEMENTATION
#include "tsf.h"

#define TML_IMPLEMENTATION
#include "tml.h"

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// Holds global MIDI playback state
static double g_Msec; //current playback time
static tml_message *g_TinyMidiLoader, *g_MidiMessage;  //next message to be played
static ZL_String g_SongName, g_MidiFileName, g_SoundFontFileName;
static ZL_TextBuffer g_SongNameBuf;
static ZL_Font g_Font;
static unsigned int g_MidiLength, g_SeekMsec = 0xFFFFFFFF;

extern "C" void TML_AddTrackName(const char* str) 
{
	if (g_SongName.length()) g_SongName += "\n";
	g_SongName += str;
}

static void FeedMidiEvents(bool PlayNotes)
{
	for (; g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next)
	{
		switch (g_MidiMessage->type)
		{
			case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
				tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
				break;
			case TML_NOTE_ON: //play a note
				if (PlayNotes) tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
				break;
			case TML_NOTE_OFF: //stop a note
				if (PlayNotes) tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
				break;
			case TML_PITCH_BEND: //pitch wheel modification
				tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
				break;
			case TML_CONTROL_CHANGE: //MIDI controller messages
				tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
				break;
		}
	}
}

// Callback function called by the audio thread
static bool AudioCallback(short* buffer, unsigned int samples, bool need_mix)
{
	if (!g_TinySoundFont) return false;

	if (g_SeekMsec != 0xFFFFFFFF && g_TinyMidiLoader)
	{
		tsf_reset(g_TinySoundFont);
		tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0); //Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
		g_MidiMessage = g_TinyMidiLoader;
		g_Msec = g_SeekMsec - 0.001; //seek a tiny bit before the wanted point to actually play the NOTE ON on the requested time afterwards
		FeedMidiEvents(false);
		g_SeekMsec = 0xFFFFFFFF;
	}

	//Number of samples to process
	int SampleBlock, SampleCount = samples; //2 output channels
	for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, buffer += (SampleBlock * 2))
	{
		//We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
		if (SampleBlock > SampleCount) SampleBlock = SampleCount;

		//Loop through all MIDI messages which need to be played up until the current playback time
		g_Msec += SampleBlock * (1000.0 / 44100.0);
		FeedMidiEvents(true);

		// Render the block of audio samples in short format
		tsf_render_short(g_TinySoundFont, (short*)buffer, SampleBlock, need_mix);
	}
	return true;
}

static void SeekMidi(unsigned int Msec)
{
	if (!g_TinySoundFont || !g_TinyMidiLoader) return;
	g_SeekMsec = Msec;
}

extern "C" bool WMP_LoadSoundFont(const char* name, const void* data, int len)
{
	if (g_TinySoundFont) tsf_close(g_TinySoundFont);
	g_TinySoundFont = tsf_load_memory(data, len);
	if (!g_TinySoundFont) { g_SoundFontFileName.clear(); return false; }
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, 44100, 0.0);
	tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0); //Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
	g_SoundFontFileName = name;
	SeekMidi((unsigned int)g_Msec); //reset MIDI
	return true;
}

extern "C" bool WMP_LoadMidi(const char* name, const void* data, int len)
{
	if (g_TinySoundFont) for (int i = 0; i < 16; i++) tsf_channel_note_off_all(g_TinySoundFont, i);
	if (g_TinyMidiLoader) tml_free(g_TinyMidiLoader);
	g_MidiFileName = name;
	g_SongName = "";
	g_MidiMessage = g_TinyMidiLoader = tml_load_memory(data, len);
	g_SongNameBuf = g_Font.CreateBuffer(.5f, g_SongName.c_str());
	if (!g_TinyMidiLoader) { g_MidiFileName.clear(); return false; }
	tml_get_info(g_TinyMidiLoader, NULL, NULL, NULL, NULL, &g_MidiLength);
	SeekMidi(0);
	return true;
}

static struct sWebMIDIPlayer : public ZL_Application
{
	sWebMIDIPlayer() : ZL_Application(60) { }

	virtual void Load(int argc, char *argv[])
	{
		if (!ZL_Application::LoadReleaseDesktopDataBundle()) return;
		if (!ZL_Display::Init("Web MIDI Player", 640, 360, ZL_DISPLAY_ALLOWRESIZEHORIZONTAL)) return;
		ZL_Display::ClearFill(ZL_Color::White);
		ZL_Display::SetAA(true);
		ZL_Audio::Init();

		g_Font = ZL_Font("Data/fntBig.png");

		size_t sfLength;
		char* sfData = ZL_File("Data/SYNTHGMS.SFO").GetContentsMalloc(&sfLength);
		g_TinySoundFont = tsf_load_memory(sfData, (int)sfLength);
		free(sfData);
		if (!g_TinySoundFont)
		{
			ZL_ASSERTMSG(false, "Could not load SoundFont");
			return;
		}
		tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, 44100, 0.0);
		g_SongNameBuf = g_Font.CreateBuffer(.5f, "Load a MIDI file to play it");

		#if 0 //Load local midi file for testing non-web version
		sfData = ZL_File("venture.mid").GetContentsMalloc(&sfLength);
		WMP_LoadMidi("venture.mid", sfData, (int)sfLength);
		free(sfData);
		#endif

		ZL_Audio::HookAudioMix(&AudioCallback);

		ZL_Display::sigKeyDown.connect(this, &sWebMIDIPlayer::OnKeyDown);
		ZL_Display::sigPointerDown.connect(this, &sWebMIDIPlayer::OnClick);
		ZL_Display::sigPointerMove.connect(this, &sWebMIDIPlayer::OnMove);
	}

	void OnKeyDown(ZL_KeyboardEvent& e)
	{
		if (e.key == ZLK_ESCAPE) Quit();
	}

	void OnMove(ZL_PointerMoveEvent& e) { if (e.state & ZL_BUTTON_LEFT) SeekMidi((unsigned int)(e.x / ZLWIDTH * g_MidiLength)); }
	void OnClick(ZL_PointerPressEvent& e) { SeekMidi((unsigned int)(e.x / ZLWIDTH * g_MidiLength)); }

	virtual void AfterFrame()
	{
		ZL_Display::FillGradient(0, 0, ZLWIDTH, ZLHEIGHT, ZLRGB(0,0,.3), ZLRGB(0,0,.3), ZLRGB(.4,.4,.4), ZLRGB(.4,.4,.4));

		if (g_TinyMidiLoader && g_MidiLength)
		{
			unsigned int Msec = (g_Msec > g_MidiLength ? g_MidiLength : (unsigned int)g_Msec);
			float x = s(ZLWIDTH / g_MidiLength * Msec);
			ZL_Display::FillGradient(0, 0, x, ZLHEIGHT, ZLRGB(.3,0,0), ZLRGB(.3,0,0), ZLRGB(.4,.4,.4), ZLRGB(.4,.4,.4));
			g_Font.Draw(5, 5, ZL_String::format("[%02d:%02d / %02d:%02d]", Msec/60000, (Msec/1000)%60, g_MidiLength/60000, (g_MidiLength/1000)%60).c_str(), ZL_Origin::BottomLeft);
		}
		g_Font.Draw(ZLHALFW, ZLFROMH(5), g_MidiFileName.c_str(), ZL_Origin::TopCenter);
		g_SongNameBuf.Draw(ZLHALFW, ZLFROMH(35), ZL_Origin::TopCenter);
		g_Font.Draw(ZLFROMW(5), 5, g_SoundFontFileName.c_str(), ZL_Origin::BottomRight);
	}
} WebMIDIPlayer;
