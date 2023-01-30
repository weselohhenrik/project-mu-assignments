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
static jack_port_t* port_in = NULL;

static void buf_init()
{

}


static int on_process(jack_nframes_t nframes, void* arg)
{
    sample_t* out;
    sample_t* in;
    jack_nframes_t i;

    out = static_cast<sample_t*>(jack_port_get_buffer(port_out, nframes));
    in = static_cast<sample_t*>(jack_port_get_buffer(port_in, nframes));

    static float phs_f = 0.0f;
    for (i = 0; i < nframes; ++i) {

    }
    return 0;
}

static void glfw_error_callback(int error, const char* message)
{
    fprintf(stderr, "GLFW Error: %d: %s\n", error, message);
}

static void jack_init(void)
{
    client = jack_client_open("terrain", JackNoStartServer, NULL);

    jack_set_process_callback(client, on_process, NULL);

    port_out = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    port_in = jack_port_register(client, "in", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
}

static void jack_finish(void)
{
    jack_deactivate(client);
    jack_client_close(client);
}

int main(void)
{
    jack_init();
    buf_init();    /* <- must be called before activating client */
    jack_activate(client);

    glfwSetErrorCallback(glfw_error_callback);
    const char* glsl_version = "#version 130";

    if (!glfwInit())
        return 1;

    GLFWwindow* window = glfwCreateWindow(400, 400, "KPS", NULL, NULL);
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

    bool down = false;
    bool show_demo_window = true;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        ImGui::Begin("KPS");
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
