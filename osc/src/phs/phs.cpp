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

static jack_client_t* client = NULL;
static jack_port_t* port_phs = NULL, * port_freq = NULL;
static jack_nframes_t sr;

static float freq = 200;

static int on_process(jack_nframes_t nframes, void* arg)
{
    static float phs = 0;
    sample_t* phs_out, * freq_out;
    jack_nframes_t i;

    phs_out = static_cast<sample_t*>(jack_port_get_buffer(port_phs, nframes));
    freq_out = static_cast<sample_t*>(jack_port_get_buffer(port_freq, nframes));

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

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Wave Terrain", NULL, NULL);
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


        ImGui::Begin("Hello, Imgui!");
        ImGui::Text("frequency: %f", freq);
        /*if (ImGui::Button("increase Radius"))
            radius += 0.05;
        if (ImGui::Button("decrease Radius"))
            radius -= 0.05f;
        */
        ImGui::SliderFloat("freq", &freq, 50.0f, 400.0f);
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
