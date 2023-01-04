#include <stdio.h>
#include <math.h>
#define PI_F 3.14159265f

#include <jack/jack.h>
typedef jack_default_audio_sample_t sample_t;

static jack_client_t* client = NULL;
static jack_port_t* port_phs = NULL, * port_freq = NULL;
static jack_port_t* port_out = NULL;
static jack_nframes_t sr;

/*---- tbl ----*/
#define TBL_SIZE 4096
// fundamental frequency of first table
#define MIN_FREQ 20.f

typedef struct wave_tbl {
    float max_freq;
    sample_t buf[TBL_SIZE];
} wave_tbl;
static wave_tbl* tbl;
static size_t tbl_len;

// fourier coefficient for sin(k*t), k>=1
static float coeff(size_t k)
{
    if (k % 2 == 0)
        return -1.f / k;
    else
        return 1.f / k;
}

// generate band limited wavetable buffer
static void tbl_genbuf(sample_t* buf, float freq)
{
    float nyquist = sr / 2.f;
    float step = 2 * PI_F / TBL_SIZE;
    size_t i;
    sample_t max = 0;

    for (i = 0; i < TBL_SIZE; ++i) {
        int k;
        float phi = i * step;

        // add up harmonics
        buf[i] = 0;
        for (k = 1; k <= 16; ++k)
            buf[i] += coeff(k) * sinf(k * phi);

        // update max amplitude for normalization
        if (fabs(buf[i]) > max)
            max = fabs(buf[i]);

    }

    if (max > 0) {
        for (i = 0; i < TBL_SIZE; ++i)
            buf[i] /= max;
    }

}

// init wavetable, this should be called before acitvating the client
static void tbl_init()
{
    float nyquist = sr / 2.f;
    float freq = MIN_FREQ;
    size_t n;

    tbl_len = log2f(nyquist / MIN_FREQ) + 1;
    tbl = calloc(tbl_len, sizeof(wave_tbl));

    for (n = 0; n < tbl_len; ++n) {
        tbl[n].max_freq = freq;
        tbl_genbuf(tbl[n].buf, freq);

        freq *= 2;
    }
}

static void tbl_finish()
{
    free(tbl);
}

static size_t tbl_index(float freq)
{
    size_t i;
    for (i = 0; i < tbl_len; ++i)
        if (freq < tbl[i].max_freq)
            return i;

    return tbl_len - 1;
}

static sample_t tbl_eval(const sample_t* buf, float phs)
{
    sample_t x0, x1;
    size_t i;
    float fr;

    if (phs < 0 || phs >= 1)
        return 0;

    fr = phs * TBL_SIZE;
    i = (size_t)fr; // take floor
    fr = fr - i;

    x0 = buf[i];
    x1 = buf[(i + 1) % TBL_SIZE];

    return (1 - fr) * x0 + fr * x1;
}

/*---- /tbl ----*/

static int on_process(jack_nframes_t nframes, void* arg)
{
    const sample_t* phs, * freq;
    sample_t* out;
    jack_nframes_t i;

    phs = jack_port_get_buffer(port_phs, nframes);
    freq = jack_port_get_buffer(port_freq, nframes);
    out = jack_port_get_buffer(port_out, nframes);

    for (i = 0; i < nframes; ++i) {
        size_t idx = tbl_index(freq[i]);

        if (idx >= tbl_len - 1) {
            out[i] = tbl_eval(tbl[idx].buf, phs[i]);
        }
        else {
            float f1 = tbl[idx].max_freq;
            float f0 = idx == 0 ? 0 : tbl[idx - 1].max_freq;

            float w = (freq[i] - f0) / (f1 - f0);
            out[i] = tbl_eval(tbl[idx].buf, phs[i]) * (1 - w) + tbl_eval(tbl[idx + 1].buf, phs[i]) * w;
        }
    }

    return 0;
}

static void jack_init()
{
    client = jack_client_open("terrain", JackNoStartServer, NULL);

    sr = jack_get_sample_rate(client);
    jack_set_process_callback(client, on_process, NULL);

    port_phs = jack_port_register(client, "phs", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    port_freq = jack_port_register(client, "freq", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    port_out = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

}

static void jack_finish()
{
    jack_deactivate(client);
    jack_client_close(client);
}

int main() {
    jack_init();
    tbl_init();
    jack_activate(client);
    getchar();

    jack_finish();

    return 0;
}
