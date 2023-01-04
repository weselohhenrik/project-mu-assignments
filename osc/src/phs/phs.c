#include <stdio.h>
#include <math.h>
#include <Windows.h>
#include <stdbool.h>
#define PI_F 3.14159265f

#include <jack/jack.h>
typedef jack_default_audio_sample_t sample_t;

static volatile int done = 0;

static jack_client_t* client = NULL;
static jack_port_t* port_phs = NULL, * port_freq = NULL;
static jack_nframes_t sr;

static float freq = 200;

static int on_process(jack_nframes_t nframes, void* arg)
{
    static float phs = 0;
    sample_t* phs_out, * freq_out;
    jack_nframes_t i;

    phs_out = jack_port_get_buffer(port_phs, nframes);
    freq_out = jack_port_get_buffer(port_freq, nframes);

    for (i = 0; i < nframes; ++i) {
        freq_out[i] = freq;
        phs_out[i] = phs;

        phs += freq_out[i] / sr;

        while (phs >= 1) phs--;
        while (phs < 0) phs++;
    }

    return 0;
}

static void jack_init()
{
    client = jack_client_open("phs", JackNoStartServer, NULL);

    sr = jack_get_sample_rate(client);
    jack_set_process_callback(client, on_process, NULL);

    port_phs = jack_port_register(client, "phs", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    port_freq = jack_port_register(client, "freq", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    jack_activate(client);
}

static void jack_finish()
{
    jack_deactivate(client);
    jack_client_close(client);
}

int main() {
    jack_init();

    bool down = false;

    while (!done)
    {
        if (GetKeyState('W') & 0x8000)
        {
            if (!down) {
                down = true;
            }
        }
        else {
            if (down) {
                down = false;
                freq += 20.0f;
            }
        }
        if (GetKeyState('Q') & 0x8000)
        {
            done = 1;
        }
    }

    jack_finish();

    return 0;
}
