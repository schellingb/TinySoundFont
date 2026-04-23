#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio_io.h"

#define TSF_IMPLEMENTATION
#include "../tsf.h"

// Holds the global instance pointer
static tsf* g_TinySoundFont;

// A Mutex so we don't call note_on/note_off while rendering audio samples
static ma_mutex g_Mutex;

// Callback function called by the audio thread
static void AudioCallback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
	// Render the audio samples in float format
	ma_mutex_lock(&g_Mutex); //get exclusive lock
	tsf_render_float(g_TinySoundFont, (float*)pOutput, (int)frameCount, 0);
	ma_mutex_unlock(&g_Mutex);
}

int main(int argc, char *argv[])
{
	int i, Notes[7] = { 48, 50, 52, 53, 55, 57, 59 };

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

	// Load the SoundFont from a file
	g_TinySoundFont = tsf_load_filename("florestan-subset.sf2");
	if (!g_TinySoundFont)
	{
		fprintf(stderr, "Could not load SoundFont\n");
		return 1;
	}

	// Set the SoundFont rendering output mode
	tsf_set_output(g_TinySoundFont, TSF_STEREO_INTERLEAVED, (int)deviceConfig.sampleRate, 0);

	// Create the mutex
	ma_mutex_init(&g_Mutex);

	// Start the actual audio playback here
	// The audio thread will begin to call our AudioCallback function
	if (ma_device_start(&device) != MA_SUCCESS)
	{
		fprintf(stderr, "Failed to start playback device.\n");
		ma_device_uninit(&device);
		return 1;
	}

	// Loop through all the presets in the loaded SoundFont
	for (i = 0; i < tsf_get_presetcount(g_TinySoundFont); i++)
	{
		//Get exclusive mutex lock, end the previous note and play a new note
		printf("Play note %d with preset #%d '%s'\n", Notes[i % 7], i, tsf_get_presetname(g_TinySoundFont, i));
		ma_mutex_lock(&g_Mutex);
		tsf_note_off(g_TinySoundFont, i - 1, Notes[(i - 1) % 7]);
		tsf_note_on(g_TinySoundFont, i, Notes[i % 7], 1.0f);
		ma_mutex_unlock(&g_Mutex);
		ma_sleep(1000);
	}

	ma_device_uninit(&device);

	// We could call tsf_close(g_TinySoundFont) and ma_mutex_uninit(&g_Mutex)
	// here to free the memory and resources but we just let the OS clean up
	// because the process ends here.
	return 0;
}
