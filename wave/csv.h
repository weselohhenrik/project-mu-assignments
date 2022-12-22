#include <stdio.h>
#include <stdint.h>

typedef int16_t sample_t;

void write_samples_to_csv_sample(const sample_t *samples, const size_t buffer_size ,const char *file_name);
void write_samples_to_csv_i32(const int32_t *samples, const size_t buffer_size ,const char *file_name);
void write_samples_to_csv_u32(const uint32_t *samples, const size_t buffer_size ,const char *file_name);
void write_samples_to_csv_d(const double *samples, const size_t buffer_size ,const char *file_name);