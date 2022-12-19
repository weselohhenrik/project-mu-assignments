#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

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

void write_wav_file(const struct wav_hdr header, const sample_t *data, const char *file_name)
{
	int err;
	FILE *wav_file;
	if ((err = fopen_s(&wav_file, file_name, "w")) != 0) {
		printf("Error opening wav file\n");
	} else {
		fwrite(&header, sizeof(struct wav_hdr), 1, wav_file);
		fwrite(data, header.data.size, 1, wav_file);
	}
}

void write_samples_to_csv(const sample_t *samples, const size_t buffer_size ,const char *file_name) 
{
	int err;
	FILE *csv_file;
	if ((err = fopen_s(&csv_file, file_name, "w")) != 0) {
		printf("Error opening file for writing csv file\n");
		 
	} else {
		printf("Hello\n");
		char comma_new_line[] = ",\n";
		for (int i = 0; i < buffer_size/sizeof(sample_t); i++) {
			uint32_t data_point = samples[i];
			//fwrite(&data_point, sizeof(samples[i]), 1, csv_file);
			//fwrite(comma_new_line, sizeof(comma_new_line), 1, csv_file);
			fprintf(csv_file, "%u,\n", samples[i]);
		}
	}
	fclose(csv_file);
}

int main(int argc, char *argv[])
{
	char *file_name = argv[1];

	struct wav_hdr header = {0};
	FILE *fp = fopen(file_name, "rb");
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
	fread(data, header.data.size, 1, fp);
	write_wav_file(header, data, "test.wav");
	printf("Read data successfully\n");

	size_t analyze_size = (44100 /1000) * 50;
	sample_t *analyze_sample = malloc(analyze_size * sizeof(sample_t));
	// load first 50 ms
	for (size_t i = 0; i < analyze_size; i++) {
		analyze_sample[i] = data[i];
	}
	write_samples_to_csv(analyze_sample, analyze_size * sizeof(sample_t), "analyze.csv");

	// calculate AMDF
	sample_t *amdf = malloc(analyze_size * sizeof(sample_t));
	for (int k = 0; k < analyze_size; k++) {
		sample_t g = 0;
		
		for (int n = 0; n < (analyze_size - k - 1); n++) {
			sample_t x = analyze_sample[n];
			int index = n + k;
			sample_t x_off = analyze_sample[index];
			sample_t result = abs(x - x_off);
			g += result;
		}
		amdf[k] = g/(analyze_size-k);
	}

	write_samples_to_csv(amdf, analyze_size * sizeof(sample_t), "amdf.csv");

	fclose(fp);
	free(amdf);
	free(analyze_sample);
    return 0;
}