#include <stdio.h>
#include <math.h>
#include <Windows.h>
#include <stdbool.h>
#define PI_F 3.14159265f

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <jack/jack.h>
typedef jack_default_audio_sample_t sample_t;

static volatile int done = 0;

#define MAX_DELAY (1<<24)
static sample_t buf[MAX_DELAY];

// delay time in samples
static jack_nframes_t delay = 2000;
// delay gain
static float gain = 0.8f;

static float frequency = 220.0f;

static jack_client_t* client = NULL;
static jack_port_t* out_port = NULL;
static jack_port_t* in_port = NULL;
static jack_nframes_t sr;
static sample_t status = 0;
static bool a_pressed = false;

static float delay_tick()
{
    static float mem = 0;
    mem = 0.0001 * delay + 0.9999 * mem;
    return mem;
}

static float gain_tick()
{
    static float mem = 0;
    mem = 0.001 * gain + 0.999 * mem;
    return mem;

}

static sample_t buf_eval(float pos)
{
    sample_t x0, x1;
    size_t i;
    float fr;

    i = (size_t)pos;
    fr = pos - i;
    x0 = buf[i];
    x1 = buf[(i + 1) % MAX_DELAY];

    return (1 - fr) * x0 + fr * x1;
}

static int on_process(jack_nframes_t nframes, void* arg)
{
    sample_t* out;
    sample_t* in;
    jack_nframes_t i;

    static jack_nframes_t wp = 0;

    out = static_cast<sample_t*>(jack_port_get_buffer(out_port, nframes));
    in = static_cast<sample_t*>(jack_port_get_buffer(in_port, nframes));

    // calculate delay 
    delay = (sr / frequency);

    for (i = 0; i < nframes; ++i) {
        // calculate read pointer
        int rp = wp - delay_tick();
        if (rp < 0)
            rp += MAX_DELAY;


        // old y[n] = x[n] + g * x[n-D] 
        // new: y[n] = x[n] + g * (0.5* y[n-N] + 0.5 * y[n-N-1])
        out[i] = in[i] + gain_tick() * (0.5*buf_eval(rp) + 0.5*buf_eval(rp-1));
        buf[wp] = out[i];

        // advance write pointer to next position
        wp++;
        if (wp >= MAX_DELAY)
            wp -= MAX_DELAY;
    }

    return 0;
}

static void jack_init()
{
    client = jack_client_open("delay", JackNoStartServer, NULL);

    sr = jack_get_sample_rate(client);
    jack_set_process_callback(client, on_process, NULL);

    out_port = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    in_port = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

    jack_activate(client);
}

static void jack_finish()
{
    jack_deactivate(client);
    jack_client_close(client);
}

static void glfw_error_callback(int error, const char* message)
{
    fprintf(stderr, "GLFW Error: %d: %s\n", error, message);
}

int main() {
    jack_init();

    glfwSetErrorCallback(glfw_error_callback);
    const char* glsl_version = "#version 130";

    if (!glfwInit())
        return 1;

    GLFWwindow* window = glfwCreateWindow(600, 600, "KPS", NULL, NULL);
    if (window == NULL) {
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool show_demo_window = true;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Delay");
        ImGui::SliderFloat("feedback", &gain, 0.7f, 1.0f);
        //ImGui::SliderInt("delay", (int*)&delay, 500, 44000);
        ImGui::SliderFloat("frequency", &frequency, 150.0f, 600.0f);
        ImGui::End();

        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }


    glfwDestroyWindow(window);
    glfwTerminate();


    jack_finish();

    return 0;
}
