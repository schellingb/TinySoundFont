#define TSF_IMPLEMENTATION
#include "../tsf.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio_io.h"

//This is a minimal SoundFont with a single loopin saw-wave sample/instrument/preset (484 bytes)
const static unsigned char MinimalSoundFont[] =
{
	#define TEN0 0,0,0,0,0,0,0,0,0,0
	'R','I','F','F',220,1,0,0,'s','f','b','k',
	'L','I','S','T',88,1,0,0,'p','d','t','a',
	'p','h','d','r',76,TEN0,TEN0,TEN0,TEN0,0,0,0,0,TEN0,0,0,0,0,0,0,0,255,0,255,0,1,TEN0,0,0,0,
	'p','b','a','g',8,0,0,0,0,0,0,0,1,0,0,0,'p','m','o','d',10,TEN0,0,0,0,'p','g','e','n',8,0,0,0,41,0,0,0,0,0,0,0,
	'i','n','s','t',44,TEN0,TEN0,0,0,0,0,0,0,0,0,TEN0,0,0,0,0,0,0,0,1,0,
	'i','b','a','g',8,0,0,0,0,0,0,0,2,0,0,0,'i','m','o','d',10,TEN0,0,0,0,
	'i','g','e','n',12,0,0,0,54,0,1,0,53,0,0,0,0,0,0,0,
	's','h','d','r',92,TEN0,TEN0,0,0,0,0,0,0,0,50,0,0,0,0,0,0,0,49,0,0,0,34,86,0,0,60,0,0,0,1,TEN0,TEN0,TEN0,TEN0,0,0,0,0,0,0,0,
	'L','I','S','T',112,0,0,0,'s','d','t','a','s','m','p','l',100,0,0,0,86,0,119,3,31,7,147,10,43,14,169,17,58,21,189,24,73,28,204,31,73,35,249,38,46,42,71,46,250,48,150,53,242,55,126,60,151,63,108,66,126,72,207,
		70,86,83,100,72,74,100,163,39,241,163,59,175,59,179,9,179,134,187,6,186,2,194,5,194,15,200,6,202,96,206,159,209,35,213,213,216,45,220,221,223,76,227,221,230,91,234,242,237,105,241,8,245,118,248,32,252
};

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// Callback function called by the audio thread
static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	// Note we don't do any thread concurrency control here because in this
	// example all notes are started before the audio playback begins.
	// If you do play notes while the audio thread renders output you
	// will need a mutex of some sort.
	tsf_render_short(g_TinySoundFont, (short*)pOutput, (int)frameCount, 0);
}

int main(int argc, char *argv[])
{
	// Define the desired audio output format we request
	ma_device device;
	ma_device_config deviceConfig;
	deviceConfig = ma_device_config_init(ma_device_type_playback);
	deviceConfig.playback.format = ma_format_s16;
	deviceConfig.playback.channels = 2;
	deviceConfig.sampleRate = 44100;
	deviceConfig.dataCallback = AudioCallback;

	// Initialize the audio system
	if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS)
	{
		fprintf(stderr, "Could not initialize audio hardware or driver\n");
		return 1;
	}

	// Load the SoundFont from the memory block
	g_TinySoundFont = tsf_load_memory(MinimalSoundFont, sizeof(MinimalSoundFont));
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont\n");
		return 1;
	}

	// Set the rendering output mode to 44.1khz and -10 decibel gain
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, (int)deviceConfig.sampleRate, -10);

	// Start two notes before starting the audio playback
	tsf_note_on(g_TinySoundFont, 0, 48, 1.0f); //C2
	tsf_note_on(g_TinySoundFont, 0, 52, 1.0f); //E2

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	if (ma_device_start(&device) != MA_SUCCESS)
	{
		fprintf(stderr, "Failed to start playback device.\n");
		ma_device_uninit(&device);
		return 1;
	}

	// Let the audio callback play some sound for 3 seconds
	ma_sleep(3000);

	ma_device_uninit(&device);

	// We could call tsf_close(g_TinySoundFont)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
