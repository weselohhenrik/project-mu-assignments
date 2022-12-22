#include <stdint.h>
#include <stdio.h>
#include "csv.h"

void write_samples_to_csv_sample(const sample_t *samples, const size_t buffer_size ,const char *file_name) 
{
	int err;
	FILE *csv_file;
	if ((err = fopen_s(&csv_file, file_name, "w")) != 0) {
		printf("Error opening file for writing csv file\n");
		 
	} else {
		for (int i = 0; i < buffer_size/sizeof(sample_t); i++) {
			fprintf(csv_file, "%d\n", samples[i]);
		}
	}
	fclose(csv_file);
}

void write_samples_to_csv_i32(const int32_t *samples, const size_t buffer_size ,const char *file_name) 
{
	int err;
	FILE *csv_file;
	if ((err = fopen_s(&csv_file, file_name, "w")) != 0) {
		printf("Error opening file for writing csv file\n");
		 
	} else {
		for (int i = 0; i < buffer_size/sizeof(sample_t); i++) {
			fprintf(csv_file, "%d\n", samples[i]);
		}
	}
	fclose(csv_file);
}

void write_samples_to_csv_u32(const uint32_t *samples, const size_t buffer_size ,const char *file_name) 
{
	int err;
	FILE *csv_file;
	if ((err = fopen_s(&csv_file, file_name, "w")) != 0) {
		printf("Error opening file for writing csv file\n");
		 
	} else {
		for (int i = 0; i < buffer_size/sizeof(sample_t); i++) {
			fprintf(csv_file, "%ul\n", samples[i]);
		}
	}
	fclose(csv_file);
}

void write_samples_to_csv_d(const double *samples, const size_t buffer_size ,const char *file_name) 
{
	int err;
	FILE *csv_file;
	if ((err = fopen_s(&csv_file, file_name, "w")) != 0) {
		printf("Error opening file for writing csv file\n");
		 
	} else {
		for (int i = 0; i < buffer_size/sizeof(sample_t); i++) {
			fprintf(csv_file, "%f\n", samples[i]);
		}
	}
	fclose(csv_file);
}