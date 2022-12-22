#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "csv.h"

typedef uint8_t fourcc[4];
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
	if ((err = fopen_s(&wav_file, file_name, "wb")) != 0) {
		printf("Error opening wav file\n");
	} else {
		fwrite(&header, sizeof(struct wav_hdr), 1, wav_file);
		fwrite(data, header.data.size, 1, wav_file);
	}
}

double AMDF(sample_t *samples, size_t buf_size, int tau)
{
	double sum = 0;
	size_t sample_len = buf_size/sizeof(sample_t);
	for (int i = 0; i < (sample_len - tau); i++) {
		int32_t sample_a = samples[i];
		int32_t sample_b = samples[i+tau];
		int32_t diff = sample_a -sample_b;
		sum += abs(diff);
	}

	return sum/(sample_len-tau); 
}

int16_t* local_minimum(double *samples, size_t buf_size, int bounds)
{
	int16_t *local_minima = malloc(buf_size);
	int minima_index = 0;
	for (int i = bounds+1 ; i < buf_size; i++) {
		if (samples[i] < samples[i-1] && samples[i] < samples[i+1]) {
			local_minima[minima_index] = i;
			minima_index += 1;
		}
	}
	return local_minima;
}

double detect_pitches_amdf(sample_t *samples, size_t buf_size, uint32_t sample_rate, int bounds)
{
	size_t sample_num = buf_size/sizeof(sample_t);
	double amdf_vals[sample_num * sizeof(double)];
	for (int i = 1; i < sample_num; i++) {
		amdf_vals[i] = AMDF(samples, buf_size, i);
	}
	write_samples_to_csv_d(amdf_vals, buf_size, "amdf.csv");
	sample_t *sample = local_minimum(amdf_vals, buf_size, bounds);
	sample_t period = sample[1]-sample[0];
	return (double)sample_rate/ period;
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

	sample_t *data = malloc(header.data.size * sizeof(sample_t));
	fseek(fp, 0, SEEK_CUR);
	fread(data, sizeof(sample_t), header.data.size, fp);
	
	
	write_wav_file(header, data, "test.wav");
	printf("Read data successfully\n");

	size_t analyze_size = (44100 /1000) * 50;
	sample_t *analyze_sample = malloc(analyze_size * sizeof(sample_t));
	// load first 50 ms
	for (size_t i = 0; i < analyze_size; i++) {
		analyze_sample[i] = data[i+1024];
	}
	write_samples_to_csv_sample(analyze_sample, analyze_size * sizeof(sample_t), "analyze.csv");
	
	double frequency = detect_pitches_amdf(analyze_sample, analyze_size * sizeof(sample_t), header.fmt.samples_per_sec, 0);

	printf("Frequency: %f\n", frequency);

	fclose(fp);
	free(analyze_sample); 
    return 0;
}