#include "minisdl_audio.h"

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
static void AudioCallback(void* data, Uint8 *stream, int len)
{
	//Number of samples to process
	int SampleBlock, SampleCount = (len / (2 * sizeof(float))); //2 output channels
	for (SampleBlock = TSF_RENDER_EFFECTSAMPLEBLOCK; SampleCount; SampleCount -= SampleBlock, stream += (SampleBlock * (2 * sizeof(float))))
	{
		//We progress the MIDI playback and then process TSF_RENDER_EFFECTSAMPLEBLOCK samples at once
		if (SampleBlock > SampleCount) SampleBlock = SampleCount;

		//Loop through all MIDI messages which need to be played up until the current playback time
		for (g_Msec += SampleBlock * (1000.0 / 44100.0); g_MidiMessage && g_Msec >= g_MidiMessage->time; g_MidiMessage = g_MidiMessage->next)
		{
			int Preset;
			switch (g_MidiMessage->type)
			{
				case TML_PROGRAM_CHANGE: //channel program (preset) change
					if (g_MidiMessage->channel == 9)
					{
						//10th MIDI channel uses percussion sound bank (128)
						Preset = tsf_get_presetindex(g_TinySoundFont, 128, g_MidiMessage->program);
						if (Preset < 0) Preset = tsf_get_presetindex(g_TinySoundFont, 128, 0);
						if (Preset < 0) Preset = tsf_get_presetindex(g_TinySoundFont, 0, g_MidiMessage->program);
					}
					else Preset = tsf_get_presetindex(g_TinySoundFont, 0, g_MidiMessage->program);
					tsf_channel_set_preset(g_TinySoundFont, g_MidiMessage->channel, (Preset < 0 ? 0 : Preset));
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
					switch (g_MidiMessage->control)
					{
						#define FLOAT_APPLY_MSB(val, msb) (((((int)(val*16383.5f)) &  0x7f) | (msb << 7)) / 16383.0f)
						#define FLOAT_APPLY_LSB(val, lsb) (((((int)(val*16383.5f)) & ~0x7f) |  lsb      ) / 16383.0f)
						case TML_VOLUME_MSB: case TML_EXPRESSION_MSB:
							tsf_channel_set_volume(g_TinySoundFont, g_MidiMessage->channel, FLOAT_APPLY_MSB(tsf_channel_get_volume(g_TinySoundFont, g_MidiMessage->channel), g_MidiMessage->control_value));
							break;
						case TML_VOLUME_LSB: case TML_EXPRESSION_LSB:
							tsf_channel_set_volume(g_TinySoundFont, g_MidiMessage->channel, FLOAT_APPLY_LSB(tsf_channel_get_volume(g_TinySoundFont, g_MidiMessage->channel), g_MidiMessage->control_value));
							break;
						case TML_BALANCE_MSB: case TML_PAN_MSB: 
							tsf_channel_set_pan(g_TinySoundFont, g_MidiMessage->channel, FLOAT_APPLY_MSB(tsf_channel_get_pan(g_TinySoundFont, g_MidiMessage->channel), g_MidiMessage->control_value));
							break;
						case TML_BALANCE_LSB: case TML_PAN_LSB:
							tsf_channel_set_pan(g_TinySoundFont, g_MidiMessage->channel, FLOAT_APPLY_LSB(tsf_channel_get_pan(g_TinySoundFont, g_MidiMessage->channel), g_MidiMessage->control_value));
							break;
						case TML_ALL_SOUND_OFF:
							tsf_channel_sounds_off_all(g_TinySoundFont, g_MidiMessage->channel);
							break;
						case TML_ALL_CTRL_OFF:
							tsf_channel_set_volume(g_TinySoundFont, g_MidiMessage->channel, 1.0f);
							tsf_channel_set_pan(g_TinySoundFont, g_MidiMessage->channel, 0.5f);
							break;
						case TML_ALL_NOTES_OFF:
							tsf_channel_note_off_all(g_TinySoundFont, g_MidiMessage->channel);
							break;
					}
					break;
			}
		}

		// Render the block of audio samples in float format
		tsf_render_float(g_TinySoundFont, (float*)stream, SampleBlock, 0);
	}
}

int main(int argc, char *argv[])
{
	tml_message* TinyMidiLoader = NULL;

	// Define the desired audio output format we request
	SDL_AudioSpec OutputAudioSpec;
	OutputAudioSpec.freq = 44100;
	OutputAudioSpec.format = AUDIO_F32;
	OutputAudioSpec.channels = 2;
	OutputAudioSpec.samples = 4096;
	OutputAudioSpec.callback = AudioCallback;

	// Initialize the audio system
	if (SDL_AudioInit(TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not initialize audio hardware or driver\n");
		return 1;
	}

	//Venture (Original WIP) by Ximon
	//https://musescore.com/user/2391686/scores/841451
	//License: Creative Commons copyright waiver (CC0)
	TinyMidiLoader = tml_load_filename("venture.mid");
	if (!TinyMidiLoader)
	{
		fprintf(stderr, "Could not load MIDI file\n");
		return 1;
	}

	//Set up the global MidiMessage pointer to the first MIDI message
	g_MidiMessage = TinyMidiLoader;

	// Load the SoundFont from a file
	g_TinySoundFont = tsf_load_filename("florestan-subset.sf2");
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont\n");
		return 1;
	}

	//Initialize preset on special 10th MIDI channel to use percussion sound bank (128) if available
	tsf_channel_set_bank_preset(g_TinySoundFont, 9, 128, 0);

	// Set the SoundFont rendering output mode with -18 db volume gain
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, OutputAudioSpec.freq, -18.0f);

	// Request the desired audio output format
	if (SDL_OpenAudio(&OutputAudioSpec, TSF_NULL) < 0)
	{
		fprintf(stderr, "Could not open the audio hardware or the desired audio output format\n");
		return 1;
	}

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	SDL_PauseAudio(0);

	//Wait until the entire MIDI file has been played back (until the end of the linked message list is reached)
	while (g_MidiMessage != NULL) SDL_Delay(100);

	// We could call tsf_close(g_TinySoundFont) and tml_free(TinyMidiLoader)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
