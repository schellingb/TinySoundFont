//--------------------------------------------//
// SFOTool                                    //
// License: Public Domain (www.unlicense.org) //
//--------------------------------------------//

#include <stdio.h>
#include <string.h>

typedef char sfo_fourcc[4];
#define SFO_FourCCEquals(a, b) (a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3])
struct sfo_riffchunk { sfo_fourcc id; unsigned int size; };
struct sfo_wavheader
{
	char RIFF[4]; unsigned int ChunkSize; char WAVE[4], fmt[4]; unsigned int Subchunk1Size;
	unsigned short AudioFormat,NumOfChan; unsigned int SamplesPerSec, bytesPerSec;
	unsigned short blockAlign, bitsPerSample; char Subchunk2ID[4]; unsigned int Subchunk2Size;
};

static void sfo_copy(FILE* src, FILE* trg, unsigned int size)
{
	unsigned int block;
	unsigned char buf[512];
	for (; size; size -= block)
	{
		block = (size > sizeof(buf) ? sizeof(buf) : size);
		fread(buf, 1, block, src);
		fwrite(buf, 1, block, trg);
	}
}

static int sfo_riffchunk_read(struct sfo_riffchunk* parent, struct sfo_riffchunk* chunk, FILE* f)
{
	int is_riff, is_list;
	if (parent && sizeof(sfo_fourcc) + sizeof(unsigned int) > parent->size) return 0;
	if (!fread(&chunk->id, sizeof(sfo_fourcc), 1, f) || *chunk->id <= ' ' || *chunk->id >= 'z') return 0;
	if (!fread(&chunk->size, sizeof(unsigned int), 1, f)) return 0;
	if (parent && sizeof(sfo_fourcc) + sizeof(unsigned int) + chunk->size > parent->size) return 0;
	if (parent) parent->size -= sizeof(sfo_fourcc) + sizeof(unsigned int) + chunk->size;
	is_riff = SFO_FourCCEquals(chunk->id, "RIFF"), is_list = SFO_FourCCEquals(chunk->id, "LIST");
	if (is_riff && parent) return 0; /* not allowed */
	if (!is_riff && !is_list) return 1; /* custom type without sub type */
	if (!fread(&chunk->id, sizeof(sfo_fourcc), 1, f) || *chunk->id <= ' ' || *chunk->id >= 'z') return 0;
	chunk->size -= sizeof(sfo_fourcc);
	return 1;
}

int main(int argc, const char** argv)
{
	const char* arg_sf_in  = (argc > 1 ? argv[1] : NULL);
	const char* arg_smpl   = (argc > 2 ? argv[2] : NULL);
	const char* arg_sf_out = (argc > 3 ? argv[3] : NULL);
	char ext_sf_in  = (arg_sf_in  ? arg_sf_in [strlen(arg_sf_in)-1]  | 0x20 : '\0');
	char ext_smpl   = (arg_smpl   ? arg_smpl  [strlen(arg_smpl)-1]   | 0x20 : '\0');
	char ext_sf_out = (arg_sf_out ? arg_sf_out[strlen(arg_sf_out)-1] | 0x20 : '\0');
	struct sfo_riffchunk chunkHead, chunkList, chunk;
	FILE* f_sf_in = NULL, *f_smpl = NULL, *f_sf_out = NULL;

	if (argc < 2 || argc > 4)
	{
		print_usage:
		fprintf(stderr, "Usage Help:\n");
		fprintf(stderr, "%s <SF2/SFO>: Show type of sample stream contained (PCM or OGG)\n", argv[0]);
		fprintf(stderr, "%s <SF2> <WAV>: Dump PCM sample stream to .WAV file\n", argv[0]);
		fprintf(stderr, "%s <SFO> <OGG>: Dump OGG sample stream to .OGG file\n", argv[0]);
		fprintf(stderr, "%s <SF2/SFO> <WAV> <SF2>: Write new .SF2 soundfont file using PCM sample stream from .WAV file\n", argv[0]);
		fprintf(stderr, "%s <SF2/SFO> <OGG> <SFO>: Write new .SFO soundfont file using OGG sample stream from .OGG file\n", argv[0]);
		if (f_sf_in)  fclose(f_sf_in);
		if (f_smpl)   fclose(f_smpl);
		if (f_sf_out) fclose(f_sf_out);
		return 1;
	}

	f_sf_in = fopen(arg_sf_in, "rb");
	if (!f_sf_in) { fprintf(stderr, "Error: Passed input file '%s' does not exist\n\n", arg_sf_in); goto print_usage; }

	if (!sfo_riffchunk_read(NULL, &chunkHead, f_sf_in) || !SFO_FourCCEquals(chunkHead.id, "sfbk"))
	{
		fprintf(stderr, "Error: Passed input file '%s' is not a valid soundfont file\n\n", arg_sf_in);
		goto print_usage;
	}
	while (sfo_riffchunk_read(&chunkHead, &chunkList, f_sf_in))
	{
		unsigned int pos_listsize = (unsigned int)ftell(f_sf_in) - 8;
		if (!SFO_FourCCEquals(chunkList.id, "sdta"))
		{
			fseek(f_sf_in, chunkList.size, SEEK_CUR);
			continue;
		}
		for (; sfo_riffchunk_read(&chunkList, &chunk, f_sf_in); fseek(f_sf_in, chunkList.size, SEEK_CUR))
		{
			int is_pcm = SFO_FourCCEquals(chunk.id, "smpl");
			if (!is_pcm && !SFO_FourCCEquals(chunk.id, "smpo"))
				continue;

			printf("Soundfont file '%s' contains a %s sample stream\n", arg_sf_in, (is_pcm ? "PCM" : "OGG"));
			if (ext_sf_in != '2' && ext_sf_in != 'o') printf("    Warning: Soundfont file has unknown file extension (should be .SF2 or .SFO)\n");
			if (ext_sf_in == '2' && !is_pcm)          printf("    Warning: Soundfont file has .SF%c extension but sample stream is %s\n", '2', "OGG (should be .SFO)");
			if (ext_sf_in == 'o' && is_pcm)           printf("    Warning: Soundfont file has .SF%c extension but sample stream is %s\n", 'O', "PCM (should be .SF2)");
			if (arg_sf_out)
			{
				unsigned int pos_smpchunk, end_smpchunk, len_smpl, end_sf, len_list_in, len_list_out;

				printf("Writing file '%s' with samples from '%s'\n", arg_sf_out, arg_smpl);
				if (ext_sf_out != '2' && ext_sf_out != 'o') printf("    Warning: Soundfont file has unknown file extension (should be .SF2 or .SFO)\n");
				if (ext_smpl != 'v' && ext_smpl != 'g')     printf("    Warning: Sample file has unknown file extension (should be .WAV or .OGG)\n");
				if (ext_sf_out == '2' && ext_smpl != 'v')   printf("    Warning: Soundfont file has .SF%c extension but sample file is .%s\n", '2', "OGG");
				if (ext_sf_out == 'o' && ext_smpl == 'v')   printf("    Warning: Soundfont file has .SF%c extension but sample file is .%s\n", 'O', "WAV");

				f_smpl = fopen(arg_smpl, "rb");
				if (!f_smpl) { fprintf(stderr, "Error: Unable to open input file '%s'\n\n", arg_smpl); goto print_usage; }

				if (ext_smpl == 'v')
				{
					struct sfo_wavheader wav_hdr;
					fread(&wav_hdr, sizeof(wav_hdr), 1, f_smpl);
					if (!SFO_FourCCEquals(wav_hdr.Subchunk2ID, "data") || !SFO_FourCCEquals(wav_hdr.RIFF, "RIFF")
					 || !SFO_FourCCEquals(wav_hdr.WAVE, "WAVE") || !SFO_FourCCEquals(wav_hdr.fmt, "fmt ")
					 || wav_hdr.Subchunk1Size != 16 || wav_hdr.AudioFormat != 1 || wav_hdr.NumOfChan != 1
					 || wav_hdr.bytesPerSec != wav_hdr.SamplesPerSec * sizeof(short) || wav_hdr.bitsPerSample != sizeof(short) * 8)
						{ fprintf(stderr, "Input .WAV file is not a valid raw PCM encoded wave file\n\n"); goto print_usage; }

					len_smpl = wav_hdr.Subchunk2Size;
				}
				else
				{
					fseek(f_smpl, 0, SEEK_END);
					len_smpl = (unsigned int)ftell(f_smpl);
					fseek(f_smpl, 0, SEEK_SET);
				}

				f_sf_out = fopen(arg_sf_out, "wb");
				if (!f_sf_out) { fprintf(stderr, "Error: Unable to open output file '%s'\n\n", arg_sf_out); goto print_usage; }

				pos_smpchunk = (unsigned int)(ftell(f_sf_in) - sizeof(struct sfo_riffchunk));
				end_smpchunk = pos_smpchunk + (unsigned int)sizeof(struct sfo_riffchunk) + chunk.size;
				fseek(f_sf_in, 0, SEEK_END);
				end_sf = (unsigned int)ftell(f_sf_in);

				/* Write data before list chunk size */
				fseek(f_sf_in, 0, SEEK_SET);
				sfo_copy(f_sf_in, f_sf_out, pos_listsize);

				/* Write new list chunk size */
				fread(&len_list_in, 4, 1, f_sf_in);
				len_list_out = len_list_in - chunk.size + len_smpl;
				fwrite(&len_list_out, 4, 1, f_sf_out);

				/* Write data until sample chunk */
				sfo_copy(f_sf_in, f_sf_out, pos_smpchunk - pos_listsize - 4);

				/* Write sample chunk */
				fwrite((ext_smpl == 'v' ? "smpl" : "smpo"), 4, 1, f_sf_out);
				fwrite(&len_smpl, 4, 1, f_sf_out);
				sfo_copy(f_smpl, f_sf_out, len_smpl);
				fclose(f_smpl);

				/* Write data after sample chunk */
				fseek(f_sf_in, end_smpchunk, SEEK_SET);
				sfo_copy(f_sf_in, f_sf_out, end_sf - end_smpchunk);
				fclose(f_sf_out);
			}
			else if (arg_smpl)
			{
				f_smpl = fopen(arg_smpl, "wb");
				printf("Writing file '%s' with %s sample stream\n", arg_smpl, (is_pcm ? "PCM" : "OGG"));
				if (ext_smpl != 'v' && ext_smpl != 'g') printf("    Warning: Sample file has unknown file extension (should be .WAV or .OGG)\n");
				if (ext_smpl == 'v' && !is_pcm)         printf("    Warning: Sample file has .%s extension but sample stream is %s\n", "WAV", "OGG (should be .OGG)");
				if (ext_smpl == 'g' && is_pcm)          printf("    Warning: Sample file has .%s extension but sample stream is %s\n", "OGG", "PCM (should be .WAV)");
				if (!f_smpl) { fprintf(stderr, "Unable to open output file '%s'\n\n", arg_smpl); goto print_usage; }

				if (is_pcm)
				{
					struct sfo_wavheader wav_hdr;
					memcpy(wav_hdr.Subchunk2ID, "data", 4);
					memcpy(wav_hdr.RIFF, "RIFF", 4);
					memcpy(wav_hdr.WAVE, "WAVE", 4);
					memcpy(wav_hdr.fmt, "fmt ", 4);
					wav_hdr.Subchunk1Size = 16;
					wav_hdr.AudioFormat = 1;
					wav_hdr.NumOfChan = 1;
					wav_hdr.Subchunk2Size = (unsigned int)chunk.size;
					wav_hdr.ChunkSize = sizeof(wav_hdr) - 4 - 4 + wav_hdr.Subchunk2Size;
					wav_hdr.SamplesPerSec = 22050;
					wav_hdr.bytesPerSec = 22050 * sizeof(short);
					wav_hdr.blockAlign = 1 * sizeof(short);
					wav_hdr.bitsPerSample = sizeof(short) * 8;
					fwrite(&wav_hdr, sizeof(wav_hdr), 1, f_smpl);
				}
				sfo_copy(f_sf_in, f_smpl, chunk.size);
				fclose(f_smpl);
				printf("DONE\n");
			}
			fclose(f_sf_in);
			return 0;
		}
	}

	fprintf(stderr, "Passed input file is not a valid soundfont file\n\n");
	goto print_usage;
}
