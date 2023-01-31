#include <stdio.h>
#include <math.h>
#include <Windows.h>
#include <stdbool.h>
#define PI_F 3.14159265f

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <jack/jack.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
typedef jack_default_audio_sample_t sample_t;


static jack_client_t* client = NULL;
static jack_port_t* port_out = NULL;

static float AMP = 0.1f;

static jack_nframes_t sr;
static size_t five_ms_sample_count;

static float burst_factor = 0.0f;

/* ------------------ ar ------------------ */

#define eps 0.001f

static float atk = 0.002f;
static float rel = 0.002f;

enum State {
    IDLE,
    ATTACK,
    RELEASE
};

static float
ratio2pole(float t, float ratio)
{
    return powf(ratio, 1 / (t * sr));
}

static sample_t
ar_tick(sample_t gate)
{
    static enum State state = IDLE;
    static sample_t gate_old = 0;
    static sample_t out = 0;

    static float pole;
    static sample_t target;

    if (gate > gate_old) {        /* 0 -> 1 */
        state = ATTACK;
        target = 1 + eps;
        pole = ratio2pole(atk, eps / target);
    }
    else if (gate < gate_old) { /* 1 -> 0 */
        state = RELEASE;
        target = -eps;
        pole = ratio2pole(rel, eps / (1 + eps));
    }

    gate_old = gate;
    out = (1 - pole) * target + pole * out;

    switch (state) {
    case IDLE:
        return 0;
    case ATTACK:
        if (out >= 1) {
            out = 1;
            state = RELEASE;
            target = -eps;
            pole = ratio2pole(rel, eps / (1 + eps));
        }
        break;
    case RELEASE:
        if (out <= 0) {
            out = 0;
            state = IDLE;
        }
        break;
    }
    return out;
}

/* ------------------ /ar ------------------ */

static float random(float min, float max)
{
    return (rand() / (float)RAND_MAX)* (max - min) + min;
}


static int on_process(jack_nframes_t nframes, void* arg)
{
    sample_t* out;
    jack_nframes_t i;

    out = static_cast<sample_t*>(jack_port_get_buffer(port_out, nframes));

    for (i = 0; i < nframes; ++i) {
        out[i] = ar_tick(burst_factor) * random(-1.0f, 1.0f);
    }
    return 0;
}

static void glfw_error_callback(int error, const char* message)
{
    fprintf(stderr, "GLFW Error: %d: %s\n", error, message);
}

static void jack_init(void)
{
    client = jack_client_open("excite", JackNoStartServer, NULL);

    jack_set_process_callback(client, on_process, NULL);

    sr = jack_get_sample_rate(client);

    port_out = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput, 0);
}

static void jack_finish(void)
{
    jack_deactivate(client);
    jack_client_close(client);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_A && action == GLFW_PRESS) {
        burst_factor = 1.0f;
        five_ms_sample_count = 0;
    }
    if (key == GLFW_KEY_A && action == GLFW_RELEASE) {
        burst_factor = 0.0f;
    }
}

int main(void)
{
    jack_init();
    jack_activate(client);

    glfwSetErrorCallback(glfw_error_callback);
    const char* glsl_version = "#version 130";

    if (!glfwInit())
        return 1;

    GLFWwindow* window = glfwCreateWindow(400, 400, "Excite", NULL, NULL);
    if (window == NULL) {
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetKeyCallback(window, key_callback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool down = false;
    bool show_demo_window = true;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        ImGui::Begin("Excite");
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
