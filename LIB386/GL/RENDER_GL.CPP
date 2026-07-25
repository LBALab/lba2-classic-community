#include <GL/RENDER_GL.H>

#include <SYSTEM/LOG.H>
#include <SYSTEM/WINDOW.H>

#ifdef USE_GPURENDERER

/* ── GL headers ─────────────────────────────────────────────────────────── */
#if defined(__ANDROID__)
#include <GLES2/gl2.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>
#endif

#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>

/* ── Shader sources (GL 2.1 / GLES 2.0 compatible) ─────────────────────── */

#if defined(__ANDROID__)
#define GLSL_VERT_HEADER "#version 100\n"
#define GLSL_FRAG_HEADER "#version 100\nprecision mediump float;\n"
#else
#define GLSL_VERT_HEADER "#version 110\n"
#define GLSL_FRAG_HEADER "#version 110\n"
#endif

static const char *s_vertSrc =
    GLSL_VERT_HEADER
    "attribute vec2 aPos;\n"
    "attribute vec2 aTexCoord;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
    "    vTexCoord = aTexCoord;\n"
    "}\n";

static const char *s_fragSrc =
    GLSL_FRAG_HEADER
    "uniform sampler2D uTex;\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(uTex, vTexCoord);\n"
    "}\n";

/* ── GL state ───────────────────────────────────────────────────────────── */

static GLuint s_program = 0;
static GLuint s_vbo = 0;
static GLuint s_texture = 0;
static GLint s_uTexLoc = -1;
static GLint s_aPosLoc = -1;
static GLint s_aTexCoordLoc = -1;
static U32 s_texWidth = 0;
static U32 s_texHeight = 0;
static S32 s_initialized = 0;

/* Fullscreen quad: position (x,y) + texcoord (u,v) */
static const GLfloat s_quadVerts[] = {
    /* pos       texcoord */
    -1.0f, -1.0f,  0.0f, 1.0f,  /* bottom-left  (flip Y: texcoord 1.0) */
     1.0f, -1.0f,  1.0f, 1.0f,  /* bottom-right */
    -1.0f,  1.0f,  0.0f, 0.0f,  /* top-left      (flip Y: texcoord 0.0) */
     1.0f,  1.0f,  1.0f, 0.0f,  /* top-right */
};

/* ── Shader helpers ─────────────────────────────────────────────────────── */

static GLuint CompileShader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        Log_Error("[GL] glCreateShader failed.");
        return 0;
    }
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        Log_Error("[GL] Shader compile error:\n\t%s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint LinkProgram(GLuint vert, GLuint frag) {
    GLuint prog = glCreateProgram();
    if (prog == 0) {
        Log_Error("[GL] glCreateProgram failed.");
        return 0;
    }
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        Log_Error("[GL] Program link error:\n\t%s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

/* paletteLUT builds 0xAARRGGBB which in little-endian memory is
   [B, G, R, A] — use GL_BGRA to match the byte layout. */
#ifdef GL_BGRA
#define FMT_BGRA GL_BGRA
#else
#define FMT_BGRA 0x80E1 /* GL_BGRA_EXT */
#endif

S32 GL_Renderer_Init(U32 resX, U32 resY) {
    if (s_initialized) {
        return TRUE;
    }

    if (!Window_CreateGLContext()) {
        return FALSE;
    }
    if (!Window_GLMakeCurrent()) {
        return FALSE;
    }

    Log_Info("[GL] Initializing renderer (%ux%u)...", resX, resY);

    /* Compile and link shader program */
    GLuint vert = CompileShader(GL_VERTEX_SHADER, s_vertSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, s_fragSrc);
    if (vert == 0 || frag == 0) {
        Log_Error("[GL] Failed to compile shaders.");
        return FALSE;
    }

    s_program = LinkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (s_program == 0) {
        return FALSE;
    }

    s_uTexLoc = glGetUniformLocation(s_program, "uTex");
    s_aPosLoc = glGetAttribLocation(s_program, "aPos");
    s_aTexCoordLoc = glGetAttribLocation(s_program, "aTexCoord");

    /* Create VBO for fullscreen quad */
    glGenBuffers(1, &s_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadVerts), s_quadVerts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Create texture for the framebuffer.
       paletteLUT builds 0xAARRGGBB which in little-endian memory is
       [B, G, R, A] — so we use FMT_BGRA to match the byte layout. */
    glGenTextures(1, &s_texture);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0,
                 FMT_BGRA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    s_texWidth = resX;
    s_texHeight = resY;

    /* GL state */
    glViewport(0, 0, resX, resY);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    /* Clear to black so the window is never transparent/garbage before
       the first PresentFrame call. */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    Window_GL_SwapWindow();

    s_initialized = 1;
    Log_Info("[GL] Renderer initialized OK.");
    return TRUE;
}

void GL_Renderer_Shutdown(void) {
    if (!s_initialized) {
        return;
    }

    if (s_texture) {
        glDeleteTextures(1, &s_texture);
        s_texture = 0;
    }
    if (s_vbo) {
        glDeleteBuffers(1, &s_vbo);
        s_vbo = 0;
    }
    if (s_program) {
        glDeleteProgram(s_program);
        s_program = 0;
    }

    Window_DestroyGLContext();
    s_initialized = 0;
    Log_Info("[GL] Renderer shut down.");
}

void GL_Renderer_Resize(U32 resX, U32 resY) {
    if (!s_initialized) {
        return;
    }

    glViewport(0, 0, resX, resY);

    /* Recreate the texture at the new size */
    if (s_texture) {
        glBindTexture(GL_TEXTURE_2D, s_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, resX, resY, 0,
                     FMT_BGRA, GL_UNSIGNED_BYTE, NULL);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    s_texWidth = resX;
    s_texHeight = resY;
}

void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    if (!s_initialized || argbPixels == NULL) {
        return;
    }

    if (!Window_GLMakeCurrent()) {
        return;
    }

    /* Query the actual window size and set the viewport to match.
       The game renders at a fixed resolution (e.g. 848x480) into the
       texture, but the window may be larger (fullscreen, resize). The
       fullscreen quad in clip space maps to the viewport, so the
       viewport must cover the entire window for the texture to fill it. */
    SDL_Window *win = Window_GetSDLWindow();
    int winW = (int)width, winH = (int)height;
    if (win) {
        SDL_GetWindowSize(win, &winW, &winH);
    }
    glViewport(0, 0, winW, winH);

    /* Upload the ARGB framebuffer to the GL texture */
    glBindTexture(GL_TEXTURE_2D, s_texture);

    /* Set filter mode (bilinear vs nearest) */
    GLenum filter = filterLinear ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                    FMT_BGRA, GL_UNSIGNED_BYTE, argbPixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    /* Draw fullscreen quad */
    glUseProgram(s_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glUniform1i(s_uTexLoc, 0);

    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glEnableVertexAttribArray(s_aPosLoc);
    glVertexAttribPointer(s_aPosLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat), (const void *)0);
    glEnableVertexAttribArray(s_aTexCoordLoc);
    glVertexAttribPointer(s_aTexCoordLoc, 2, GL_FLOAT, GL_FALSE,
                          4 * sizeof(GLfloat),
                          (const void *)(2 * sizeof(GLfloat)));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(s_aPosLoc);
    glDisableVertexAttribArray(s_aTexCoordLoc);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    Window_GL_SwapWindow();
}

S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    /* Phase 2 stub: 3D geometry not yet routed through GL.
       Will be implemented in Phase 6. */
    (void)typePoly;
    (void)colorPoly;
    (void)nbPoints;
    (void)ptrPoints;
    return 0;
}

#else /* !USE_GPURENDERER */

/* Stubs when GL renderer is compiled out */
S32 GL_Renderer_Init(U32 resX, U32 resY) {
    (void)resX; (void)resY;
    return FALSE;
}
void GL_Renderer_Shutdown(void) {}
void GL_Renderer_Resize(U32 resX, U32 resY) {
    (void)resX; (void)resY;
}
void GL_Present(const void *argbPixels, U32 pitch, U32 width, U32 height,
                S32 filterLinear) {
    (void)argbPixels; (void)pitch; (void)width; (void)height;
    (void)filterLinear;
}
S32 GL_RenderTriangleList(S32 typePoly, S32 colorPoly, S32 nbPoints,
                          Struc_Point *ptrPoints) {
    (void)typePoly; (void)colorPoly; (void)nbPoints; (void)ptrPoints;
    return 0;
}

#endif /* USE_GPURENDERER */
