/* OpenGL throughput of the paths that a WoW64 Wine thunks differently for
 * 32-bit applications: drawing from client memory, texture upload, pixel
 * read-back, buffer mapping and buffer upload, and presentation.
 * Usage: gl-perf-test.exe [seconds-per-case]; prints one line per case. */
#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_WRITE_ONLY 0x88B9
typedef ptrdiff_t GLsizeiptr;
typedef void (APIENTRY *gen_buffers_t)(GLsizei, GLuint *);
typedef void (APIENTRY *bind_buffer_t)(GLenum, GLuint);
typedef void (APIENTRY *buffer_data_t)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void *(APIENTRY *map_buffer_t)(GLenum, GLenum);
typedef GLboolean (APIENTRY *unmap_buffer_t)(GLenum);

enum { EDGE = 512, VERTICES = 30000, BUFFER = 4 << 20 };
static HDC dc;
static double frequency;
static float vertices[VERTICES * 2];
static unsigned char pixels[EDGE * EDGE * 4], data[BUFFER];

static double now(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart / frequency;
}

/* Run CASE until SECONDS have passed; report the rate per second. */
static void measure(const char *name, double seconds, double units, const char *unit, void (*step)(void))
{
    double start, elapsed;
    unsigned int count = 0;

    step();
    glFinish();
    start = now();
    do
    {
        step();
        glFinish();
        count++;
    } while ((elapsed = now() - start) < seconds);
    printf("%-14s %10.1f %s/s  (%u in %.2fs, error %#x)\n", name, count * units / elapsed, unit,
           count, elapsed, glGetError());
    fflush(stdout);
}

static void draw_client(void)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLES, 0, VERTICES);
    glDisableClientState(GL_VERTEX_ARRAY);
}

static void upload(void)
{
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, EDGE, EDGE, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

static void read_back(void)
{
    glReadPixels(0, 0, EDGE, EDGE, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}

static gen_buffers_t gen_buffers;
static bind_buffer_t bind_buffer;
static buffer_data_t buffer_data;
static map_buffer_t map_buffer;
static unmap_buffer_t unmap_buffer;

static void map_write(void)
{
    void *map;

    buffer_data(GL_ARRAY_BUFFER, BUFFER, NULL, GL_STREAM_DRAW);
    if ((map = map_buffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY))) memcpy(map, data, BUFFER);
    unmap_buffer(GL_ARRAY_BUFFER);
}

static void buffer_upload(void)
{
    buffer_data(GL_ARRAY_BUFFER, BUFFER, data, GL_STREAM_DRAW);
}

static void present(void)
{
    glClearColor((GetTickCount() & 255) / 255.0f, 0.2f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    SwapBuffers(dc);
}

int main(int argc, char **argv)
{
    PIXELFORMATDESCRIPTOR pfd = {0};
    double seconds = argc > 1 ? atof(argv[1]) : 3.0;
    LARGE_INTEGER counter;
    HGLRC context;
    GLuint texture, buffer;
    HWND hwnd;
    int format, i;

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    QueryPerformanceFrequency(&counter);
    frequency = (double)counter.QuadPart;
    hwnd = CreateWindowA("STATIC", "gl-perf", WS_POPUP | WS_VISIBLE, 0, 0, EDGE, EDGE, NULL, NULL, NULL, NULL);
    dc = GetDC(hwnd);
    if (!(format = ChoosePixelFormat(dc, &pfd)) || !SetPixelFormat(dc, format, &pfd)
        || !(context = wglCreateContext(dc)) || !wglMakeCurrent(dc, context))
    {
        printf("FAIL no OpenGL context (%lu)\n", GetLastError());
        return 1;
    }
    printf("pointer %u-bit, %s | %s | %s\n", (unsigned int)sizeof(void *) * 8, glGetString(GL_VENDOR),
           glGetString(GL_RENDERER), glGetString(GL_VERSION));
    for (i = 0; i < VERTICES * 2; i++) vertices[i] = (float)((i * 7919) % 2000) / 1000.0f - 1.0f;
    for (i = 0; i < (int)sizeof(data); i++) data[i] = (unsigned char)i;
    glViewport(0, 0, EDGE, EDGE);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, EDGE, EDGE, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    measure("client-draw", seconds, VERTICES / 3 / 1e6, "Mtri", draw_client);
    measure("tex-upload", seconds, EDGE * EDGE * 4 / 1048576.0, "MiB", upload);
    measure("read-pixels", seconds, EDGE * EDGE * 4 / 1048576.0, "MiB", read_back);
    gen_buffers = (gen_buffers_t)(void *)wglGetProcAddress("glGenBuffers");
    bind_buffer = (bind_buffer_t)(void *)wglGetProcAddress("glBindBuffer");
    buffer_data = (buffer_data_t)(void *)wglGetProcAddress("glBufferData");
    map_buffer = (map_buffer_t)(void *)wglGetProcAddress("glMapBuffer");
    unmap_buffer = (unmap_buffer_t)(void *)wglGetProcAddress("glUnmapBuffer");
    if (gen_buffers && bind_buffer && buffer_data && map_buffer && unmap_buffer)
    {
        gen_buffers(1, &buffer);
        bind_buffer(GL_ARRAY_BUFFER, buffer);
        measure("map-write", seconds, BUFFER / 1048576.0, "MiB", map_write);
        measure("buffer-upload", seconds, BUFFER / 1048576.0, "MiB", buffer_upload);
        bind_buffer(GL_ARRAY_BUFFER, 0);
    }
    else printf("SKIP buffer objects unavailable\n");
    measure("present", seconds, 1, "frame", present);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(context);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    return 0;
}
