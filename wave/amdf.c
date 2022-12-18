#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef int8_t fourcc[4];
typedef int16_t sample_t;

struct riff_hdr {
	fourcc id;
	uint32_t size;
	fourcc type;
};

struct fmt_ck {
	fourcc id;
	uint32_t size;
	uint16_t fmt_tag;
	uint16_t channels;
	uint32_t samples_per_sec;
	uint32_t bytes_per_sec;
	uint16_t block_align;
	uint16_t bits_per_sample;
};

struct data_hdr {
	fourcc id;
	uint32_t size;
};

struct wav_hdr {
	struct riff_hdr riff;
	struct fmt_ck fmt;
	struct data_hdr data;
};

int main()
{

	struct wav_hdr header = {0};
	FILE *fp = fopen(".\\output.wav", "rb");
	if (fp == NULL) {
		printf("Could not open file\n");
		return 1;
	}
	fseek(fp, 0, SEEK_SET);
	size_t ret_code = fread(&(header.riff.id), sizeof(header.riff.id), 1, fp);
	printf("RETURN CODE: %zu \n", ret_code);
	printf("RIFF HDR ID: %s\n", header.riff.id);

	fseek(fp, 0, SEEK_CUR);
	fread(&(header.riff.size), sizeof(header.riff.size), 1, fp);
	printf("RIFF HDR SIZE: %u\n", header.riff.size);

	fseek(fp, 0, SEEK_CUR);
	fread(&(header.riff.type), sizeof(header.riff.type), 1, fp);
	printf("RIFF HDR type: %s\n", header.riff.type);
	
	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.id, sizeof(header.fmt.id), 1, fp);
	printf("FMT chunk ID: %s\n", header.fmt.id);
	
	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.size, sizeof(header.fmt.size), 1, fp);
	printf("FMT chunk size: %u\n", header.fmt.size);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.fmt_tag, sizeof(header.fmt.fmt_tag), 1, fp);
	printf("FMT TAG: %u\n", header.fmt.fmt_tag);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.channels, sizeof(header.fmt.channels), 1, fp);
	printf("FMT CHANNELS: %u\n", header.fmt.channels);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.samples_per_sec, sizeof(header.fmt.samples_per_sec), 1, fp);
	printf("FMT SAMPLES PER SEC: %u\n", header.fmt.samples_per_sec);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.bytes_per_sec, sizeof(header.fmt.bytes_per_sec), 1, fp);
	printf("FMT BYTES PER SEC: %u\n", header.fmt.bytes_per_sec);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.block_align, sizeof(header.fmt.block_align), 1, fp);
	printf("FMT BLOCK ALIGN: %u\n", header.fmt.block_align);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.fmt.bits_per_sample, sizeof(header.fmt.bits_per_sample), 1, fp);
	printf("FMT BITS PER SAMPLE: %u\n", header.fmt.bits_per_sample);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.data.id, sizeof(header.data.id), 1, fp);
	printf("DATA ID: %s\n", header.data.id);

	fseek(fp, 0, SEEK_CUR);
	fread(&header.data.size, sizeof(header.data.size), 1, fp);
	printf("DATA SIZE: %u\n", header.data.size);

	sample_t *data = malloc(header.data.size);
	fseek(fp, 0, SEEK_CUR);
	fread(data, sizeof(data), 1, fp);
	printf("Read data successfully\n");
	

	fclose(fp);
    return 0;
}