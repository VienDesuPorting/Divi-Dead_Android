/* GPU rendering via SDL2 GL + OpenGL ES 2.0 */

#ifdef __ANDROID__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

static SDL_GLContext gl_context = NULL;
static GLuint gl_program = 0;
static GLuint gl_texture = 0;
static GLint gl_initialized = 0;
static int gl_tex_width = 0;
static int gl_tex_height = 0;

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
    "    gl_FragColor = texture2D(u_texture, v_texcoord).bgra;\n"
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

int android_gl_init(SDL_Window *window) {
    if (gl_initialized) return 1;

    printf("GL: init starting, window=%p\n", (void*)window);
    if (!window) { printf("GL: window is NULL!\n"); return 0; }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("GL: context failed: %s\n", SDL_GetError());
        return 0;
    }
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
    if (!status) { printf("GL: program link failed\n"); return 0; }

    /* Create texture with fixed 640x480 size */
    gl_tex_width = 640;
    gl_tex_height = 480;
    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, gl_tex_width, gl_tex_height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glUseProgram(gl_program);
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    gl_initialized = 1;
    printf("GL: initialized OK (viewport %dx%d, texture %dx%d)\n", w, h, gl_tex_width, gl_tex_height);
    return 1;
}

void android_gl_render(SDL_Window *window, SDL_Surface *surface) {
    if (!gl_initialized) {
        if (!android_gl_init(window)) return;
    }

    /* Verify the GL context is still valid before doing anything.
     * On Adreno 800-series GPUs (Nothing Phone 3A, Snapdragon 7s Gen 3),
     * the GL context can become invalid after video playback —
     * SDL_GL_MakeCurrent returns failure but the old code ignored it,
     * leading to SIGSEGV on the first GL call. */
    int mc_result = SDL_GL_MakeCurrent(window, gl_context);
    if (mc_result != 0) {
        printf("GL: SDL_GL_MakeCurrent failed: %s — attempting reinit\n",
               SDL_GetError());
        /* Try to recover by reinitializing the GL context */
        if (gl_context) {
            SDL_GL_DeleteContext(gl_context);
            gl_context = NULL;
        }
        gl_initialized = 0;
        if (!android_gl_init(window)) {
            printf("GL: reinit failed — skipping render\n");
            return;
        }
        /* Retry MakeCurrent after reinit */
        if (SDL_GL_MakeCurrent(window, gl_context) != 0) {
            printf("GL: MakeCurrent still failing after reinit — skipping\n");
            return;
        }
    }

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    
    SDL_LockSurface(surface);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surface->w, surface->h,
                    GL_RGBA, GL_UNSIGNED_BYTE, surface->pixels);
    SDL_UnlockSurface(surface);

    
    double scale_x = (double)win_w / 640.0;
    double scale_y = (double)win_h / 480.0;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    double qw = (640.0 * scale) / win_w;
    double qh = (480.0 * scale) / win_h;

    glViewport(0, 0, win_w, win_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl_program);

    GLfloat vertices[] = {
        -qw, -qh,  0.0f, 1.0f,
         qw, -qh,  1.0f, 1.0f,
        -qw,  qh,  0.0f, 0.0f,
         qw,  qh,  1.0f, 0.0f,
    };

    GLint pos_loc = glGetAttribLocation(gl_program, "a_position");
    GLint tex_loc = glGetAttribLocation(gl_program, "a_texcoord");
    GLint tex_uni = glGetUniformLocation(gl_program, "u_texture");

    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), vertices);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), vertices+2);
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_loc);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glUniform1i(tex_uni, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SDL_GL_SwapWindow(window);
}

void android_gl_render_rect(SDL_Window *window, SDL_Surface *surface, SDL_Rect *rect) {
    if (!gl_initialized) {
        if (!android_gl_init(window)) return;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    
    SDL_LockSurface(surface);

    int bytes_per_pixel = surface->format->BytesPerPixel;
    Uint8 *src = (Uint8 *)surface->pixels;
    src += rect->y * surface->pitch + rect->x * bytes_per_pixel;

    /* Allocate temp buffer for contiguous row data */
    int buf_size = rect->w * rect->h * bytes_per_pixel;
    Uint8 *buf = (Uint8 *)malloc(buf_size);
    if (buf) {
        /* Copy rows from surface to temp buffer (removing pitch padding) */
        for (int row = 0; row < rect->h; row++) {
            memcpy(buf + row * rect->w * bytes_per_pixel,
                   src + row * surface->pitch,
                   rect->w * bytes_per_pixel);
        }
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h,
                        GL_RGBA, GL_UNSIGNED_BYTE, buf);
        free(buf);
    }
    SDL_UnlockSurface(surface);

    
    double scale_x = (double)win_w / 640.0;
    double scale_y = (double)win_h / 480.0;
    double scale = scale_x < scale_y ? scale_x : scale_y;
    double qw = (640.0 * scale) / win_w;
    double qh = (480.0 * scale) / win_h;

    glViewport(0, 0, win_w, win_h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl_program);

    GLfloat vertices[] = {
        -qw, -qh,  0.0f, 1.0f,
         qw, -qh,  1.0f, 1.0f,
        -qw,  qh,  0.0f, 0.0f,
         qw,  qh,  1.0f, 0.0f,
    };

    GLint pos_loc = glGetAttribLocation(gl_program, "a_position");
    GLint tex_loc = glGetAttribLocation(gl_program, "a_texcoord");
    GLint tex_uni = glGetUniformLocation(gl_program, "u_texture");

    glVertexAttribPointer(pos_loc, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), vertices);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), vertices+2);
    glEnableVertexAttribArray(pos_loc);
    glEnableVertexAttribArray(tex_loc);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_texture);
    glUniform1i(tex_uni, 0);
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
