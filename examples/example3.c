#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio_io.h"

#define TSF_IMPLEMENTATION
#include "../tsf.h"

#define TML_IMPLEMENTATION
#include "../tml.h"

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// Holds global MIDI playback state
static double g_Msec;               //current playback time
static tml_message* g_MidiMessage;  //next message to be played

// Callback function called by the audio thread
static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	float* stream = (float*)pOutput;
	for (ma_uint32 SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; frameCount; frameCount -= SampleBlock, stream += SampleBlock * 2) // 2 channel output
	{
		//We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
		if (SampleBlock > frameCount) SampleBlock = frameCount;

		//Loop through all MIDI messages which need to be played up until the current playback time
		for (g_Msec += SampleBlock * (1000.0 / 44100.0); g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next)
		{
			switch (g_MidiMessage->type)
			{
				case TML_PROGRAM_CHANGE: //channel program (preset) change (special handling for 10th MIDI channel with drums)
					tsf_channel_set_presetnumber(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->program, (g_MidiMessage->channel == 9));
					break;
				case TML_NOTE_ON: //play a note
					tsf_channel_note_on(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key, g_MidiMessage->velocity / 127.0f);
					break;
				case TML_NOTE_OFF: //stop a note
					tsf_channel_note_off(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->key);
					break;
				case TML_PITCH_BEND: //pitch wheel modification
					tsf_channel_set_pitchwheel(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->pitch_bend);
					break;
				case TML_CONTROL_CHANGE: //MIDI controller messages
					tsf_channel_midi_control(g_TinySoundFont, g_MidiMessage->channel, g_MidiMessage->control, g_MidiMessage->control_value);
					break;
			}
		}

		// Render the block of audio samples in float format
		tsf_render_float(g_TinySoundFont, stream, (int)SampleBlock, 0);
	}
}

int main(int argc, char *argv[])
{
	// This implements a small program that you can launch without
	// parameters for a default file & soundfont, or with these arguments:
	//
	// ./example3-... <yourfile>.mid <yoursoundfont>.sf2

	tml_message* TinyMidiLoader = NULL;

	// Define the desired audio output format we request
	ma_device device;
	ma_device_config deviceConfig;
	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_f32;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.dataCallback = AudioCallback;

	// Initialize the audio system
	if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
	{
		fprintf(stderr, "Could not initialize audio hardware or driver\n");
		return 1;
	}

	//Venture (Original WIP) by Ximon
	//https://musescore.com/user/2391686/scores/841451
	//License: Creative Commons copyright waiver (CC0)
	TinyMidiLoader = tml_load_filename((argc >= 2 ? argv[1] : "venture.mid"));
	if (!TinyMidiLoader)
	{
		fprintf(stderr, "Could not load MIDI file\n");
		return 1;
	}

	//Set up the global MidiMessage pointer to the first MIDI message
	g_MidiMessage = TinyMidiLoader;

	// Load the SoundFont from a file
	g_TinySoundFont = tsf_load_filename(
		(argc >= 3 ? argv[2] : "florestan-subset.sf2")
	);
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont\n");
		return 1;
	}

	//Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
	tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);

	// Set the SoundFont rendering output mode
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, (int)deviceConfig.sampleRate, -10.0f);

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	if (ma_device_start(&device) != MA_SUCCESS)
	{
		fprintf(stderr, "Failed to start playback device.\n");
		ma_device_uninit(&device);
		return 1;
	}

	// Wait until the entire MIDI file has been played back (until the end of the linked message list is reached)
	while (g_MidiMessage != NULL) ma_sleep(100);

	ma_device_uninit(&device);

	// We could call tsf_close(g_TinySoundFont) and tml_free(TinyMidiLoader)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
