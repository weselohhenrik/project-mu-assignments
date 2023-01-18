#include <stdio.h>
#include <math.h>
#include <Windows.h>
#include <stdbool.h>
#define PI_F 3.14159265f

#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <jack/jack.h>
#include <vector>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
typedef jack_default_audio_sample_t sample_t;

static float radius = 0.5f;
static float center_x = 0.0f;
static float center_y = 0.0f;
static volatile int done = 0;

static jack_client_t* client = NULL;
static jack_port_t* port_freq = NULL, *port_phs = NULL;
static jack_port_t* port_out = NULL;

std::vector<sample_t> g_samples = {};

float map(float val, float x0, float x1, float y0, float y1)
{
    return y0 + (y1 - y0) * (val - x0) / (x1 - x0);
}

float map_i32(int val, int x0, int x1, int y0, int y1)
{
    return y0 + (y1 - y0) * (val - x0) / (x1 - x0);
}

/* ---------------- tbl ---------------- */

#define TBL_SIZE 512
static sample_t tbl[TBL_SIZE][TBL_SIZE];

void print_samples_to_file()
{
    FILE* fp;
    fopen_s(&fp, "samples.txt", "wt");
    for (int y = 0; y < 512; ++y) {
        for (int x = 0; x < 512; ++x) {
            fprintf(fp, " %f ", tbl[y][x]);
        }
        fprintf(fp, "\n");
    }
}
/* init wavetable. this should be called before
 * activating client */
static void tbl_init(void)
{
    /* divide [0,2PI) into TBL_SIZE evenly spaced samples */
    float step = 2 * PI_F / TBL_SIZE;
    size_t i;
    size_t j;
    for (i = 0; i < TBL_SIZE; ++i) {
        float y = map(i ,-TBL_SIZE, TBL_SIZE, -1.0f, 1.0f);
        for (j = 0; j < TBL_SIZE; ++j) {
            float x = map(j ,-TBL_SIZE, TBL_SIZE, -1.0f, 1.0f);
            tbl[i][j] = sinf(x)*sinf(y)*(x-1)*(y-1)*(x+1)*(y+1);
        }
    }
    //print_samples_to_file(tbl);
}

static sample_t tbl_eval(float phs)
{
    sample_t x0, x1;
    size_t i;  /* integer position */
    float fr;  /* fractional position */

    /* prevent crashing when phs is invalid.
     * remove this if phs is generated within this program */
    if (phs < 0 || phs >= 1)
        return 0;

    fr = phs * TBL_SIZE;
    i = (size_t)fr; /* take floor */
    fr = fr - i;

    x0 = tbl[i][i];
    x1 = tbl[(i + 1) % TBL_SIZE][i];

    /* linear interpolate between sample */
    return (1 - fr) * x0 + fr * x1;
}

sample_t interpolate(sample_t v00, sample_t v10, sample_t v01, sample_t v11, float i_fr, float j_fr)
{
    sample_t out = 0;
    sample_t u0 = (1 - i_fr) * v00 + i_fr * v10;
    sample_t u1 = (1 - i_fr) * v01 + i_fr * v11;

    out = (1 - j_fr) * u0 + j_fr * u1;
    return out;
}
#if 1 
static sample_t _tbl_eval(float x, float y)
{
    sample_t v00, v10, v01, v11;
    size_t i, j;
    float i_fr, j_fr;

    /* 1. wrap x,y to be in the range [-1,1) */
    while (x > 1.0f) x -= 1.0f;
    while (x < -1.0f) x+=1.0f;

    while (y > 1.0f) y -= 1.0f;
    while (y < -1.0f) y += 1.0f;

    /* 2. map x,y to float index i, j in the range [0, N)
    *    (you might need a new variable here) */
    i = map(y, -1.0f, 1.0f, 0.0f, TBL_SIZE);
    size_t i_dx = i;
    j = map(x, -1.0f, 1.0f, 0.0f, TBL_SIZE);
    size_t j_dx = j;

    /* 3. compute integer (i) and fractional part (fr) of float
    *    index i, j */
    i_fr = TBL_SIZE * y;
    i = (size_t)i_fr;
    i_fr = i_fr - i;

    j_fr = TBL_SIZE * x;
    j = (size_t)j_fr;
    j_fr = j_fr - i;

    /* 4. find neighboring values */
    v00 = tbl[i_dx][j_dx];
    v10 = tbl[(i_dx + 1) % TBL_SIZE][j_dx];
    v01 = tbl[i_dx][(j_dx + 1) % TBL_SIZE];
    v11 = tbl[(i_dx + 1) % TBL_SIZE][(j_dx + 1) % TBL_SIZE];

    /* 5. bilinear interpolation with i_fr and j_fr as weights */
    return interpolate(v00, v10, v01, v11, i_fr, j_fr);
}
#endif

/* ---------------- /tbl ---------------- */

static int on_process(jack_nframes_t nframes, void* arg)
{
    sample_t* freq, *phs;
    sample_t* out;
    jack_nframes_t i;

    freq = static_cast<sample_t*>(jack_port_get_buffer(port_freq, nframes));
    phs = static_cast<sample_t*>(jack_port_get_buffer(port_phs, nframes));
    out = static_cast<sample_t*>(jack_port_get_buffer(port_out, nframes));

    static float phs_f = 0.0f;
    for (i = 0; i < nframes; ++i) {
        phs_f = (float)phs[i];
        sample_t x = radius * cosf(2 * PI_F * phs_f) + center_x;
        sample_t y = radius * sinf(2 * PI_F * phs_f) + center_y;
        out[i] = _tbl_eval(x, y);
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

    port_phs = jack_port_register(client, "phs", JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsInput, 0);
    port_freq = jack_port_register(client, "freq", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    port_out = jack_port_register(client, "out", JACK_DEFAULT_AUDIO_TYPE,
        JackPortIsOutput, 0);
}

static void jack_finish(void)
{
    jack_deactivate(client);
    jack_client_close(client);
}

int main(void)
{
    jack_init();
    tbl_init();    /* <- must be called before activating client */
    jack_activate(client);

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

    bool down = false;
    bool show_demo_window = true;

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        ImGui::Begin("Hello, Imgui!");
        ImGui::Text("radius: %f", radius);
        ImGui::SliderFloat("radius", &radius, 0.0f, 1.0f);
        ImGui::SliderFloat("Offset X", &center_x, -1.0f, 1.0f);
        ImGui::SliderFloat("Offset Y", &center_y, -1.0f, 1.0f);
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
