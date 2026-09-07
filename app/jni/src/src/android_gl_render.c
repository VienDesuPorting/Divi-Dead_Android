/*
 * android_gl_render.c - GPU-accelerated rendering using SDL2's GL API
 * and OpenGL ES 2.0 directly (not SDL_Renderer).
 *
 * SDL_GL_CreateContext works on Android with SDL_WINDOW_OPENGL flag.
 * This gives us an EGL context for direct OpenGL ES rendering.
 *
 * Approach:
 * 1. Create EGL context via SDL_GL_CreateContext
 * 2. Create a GL texture from the 640x480 surface
 * 3. Render a fullscreen quad with the texture
 * 4. SDL_GL_SwapWindow to display
 *
 * This avoids SDL_Renderer which doesn't work with SDLActivity's SurfaceView.
 */

#ifdef __ANDROID__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

static SDL_GLContext gl_context = NULL;
static GLuint gl_program = 0;
static GLuint gl_texture = 0;
static GLint gl_initialized = 0;

/* Shader sources */
static const char *vertex_shader_src =
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

static const char *fragment_shader_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    gl_FragColor = texture2D(u_texture, v_texcoord);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        printf("GL shader error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

/* Initialize GL context and shaders. Call after SDL_CreateWindow with SDL_WINDOW_OPENGL. */
int android_gl_init(SDL_Window *window) {
    if (gl_initialized) return 1;

    /* Create EGL context */
    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("GL: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 0;
    }

    /* Make context current */
    if (SDL_GL_MakeCurrent(window, gl_context) < 0) {
        printf("GL: SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());
        return 0;
    }

    /* Compile shaders */
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src);
    if (!vs || !fs) return 0;

    gl_program = glCreateProgram();
    glAttachShader(gl_program, vs);
    glAttachShader(gl_program, fs);
    glLinkProgram(gl_program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint status;
    glGetProgramiv(gl_program, GL_LINK_STATUS, &status);
    if (!status) {
        printf("GL: program link failed\n");
        return 0;
    }

    /* Create texture */
    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Set up vertex data (fullscreen quad with 4:3 letterbox) */
    glUseProgram(gl_program);

    /* Set viewport to full window */
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, w, h);

    /* Clear to black */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gl_initialized = 1;
    printf("GL: initialized OK (viewport %dx%d)\n", w, h);
    return 1;
}

/* Render a 640x480 BGRA surface to the screen using GPU.
 * Handles 4:3 letterbox automatically via vertex coordinates. */
void android_gl_render(SDL_Window *window, SDL_Surface *surface) {
    if (!gl_initialized) {
        if (!android_gl_init(window)) return;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    /* Calculate letterbox: fit 640x480 (4:3) into window */
    double scale_x = (double)win_w / 640.0;
    double scale_y = (double)win_h / 480.0;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    double quad_w = (640.0 * scale) / win_w;   /* normalized -1..1 */
    double quad_h = (480.0 * scale) / win_h;

    /* Upload surface to texture */
    SDL_LockSurface(surface);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surface->w, surface->h, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_UnlockSurface(surface);

    /* Render fullscreen quad with letterbox */
    glViewport(0, 0, win_w, win_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl_program);

    /* Vertex data: position (x,y) + texcoord (u,v) */
    GLfloat vertices[] = {
        -quad_w, -quad_h,   0.0f, 1.0f,   /* bottom-left */
         quad_w, -quad_h,   1.0f, 1.0f,   /* bottom-right */
        -quad_w,  quad_h,   0.0f, 0.0f,   /* top-left */
         quad_w,  quad_h,   1.0f, 0.0f,   /* top-right */
    };

    GLint pos_loc = glGetAttribLocation(gl_program, "a_position");
    GLint tex_loc = glGetAttribLocation(gl_program, "a_texcoord");
    GLint tex_uniform = glGetUniformLocation(gl_program, "u_texture");

    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), vertices + 2);
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_loc);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glUniform1i(tex_uniform, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    SDL_GL_SwapWindow(window);
}

void android_gl_cleanup(void) {
    if (gl_texture) glDeleteTextures(1, &gl_texture);
    if (gl_program) glDeleteProgram(gl_program);
    if (gl_context) SDL_GL_DeleteContext(gl_context);
    gl_initialized = 0;
}

#endif /* __ANDROID__ */
