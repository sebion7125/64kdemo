// 64kDemo - prozedurale Landschaft (Raymarching), 64k-Demoscene-Stil.
//
// Kein Link gegen X11/GL: alles zur Laufzeit via dlopen. Header
// (X11, GL) nur zur Kompilierzeit. Alle Funktionszeiger mit "p_"-Praefix,
// GL-Zeiger mit expliziten Signaturen.
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include "scene.h"

// ================= X11-Funktionszeiger (dlsym aus libX11.so.6) =================
static Display *(*p_XOpenDisplay)(const char *);
static Window   (*p_XDefaultRootWindow)(Display *);
static Window   (*p_XCreateWindow)(Display *, Window, int, int, unsigned int, unsigned int,
                                   unsigned int, int, int, Visual *, unsigned long, XSetWindowAttributes *);
static Colormap (*p_XCreateColormap)(Display *, Window, Visual *, int);
static void     (*p_XMapWindow)(Display *, Window);
static Status   (*p_XSync)(Display *, int);
static int      (*p_XPending)(Display *);
static int      (*p_XNextEvent)(Display *, XEvent *);
static void     (*p_XCloseDisplay)(Display *);
static void     (*p_XSetWMProtocols)(Display *, Window, Atom *, int);
static Atom     (*p_XInternAtom)(Display *, const char *, int);
static void     (*p_XSetWMProtocols)(Display *, Window, Atom *, int);
static KeySym   (*p_XLookupKeysym)(XKeyEvent *, int);
static int      (*p_XLockPointer)(Window);
static int      (*p_XUnlockPointer)(Window);
static int      (*p_XQueryPointer)(Display *, Window, Window *, Window *, int *, int *, int *, int *, unsigned int *);
static int      (*p_XWarpPointer)(Display *, Window, Window, int, int, unsigned int, unsigned int, int, int);

// ================= GLX-Funktionszeiger (via glXGetProcAddressARB) =================
static void            *(*p_glXGetProcAddressARB)(const char *);
static GLXFBConfig     *(*p_glXChooseFBConfig)(Display *, int, const int *, int *);
static XVisualInfo     *(*p_glXGetVisualFromFBConfig)(Display *, GLXFBConfig);
static GLXContext      (*p_glXCreateContextAttribsARB)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
static GLXContext      (*p_glXCreateContext)(Display *, XVisualInfo *, GLXContext, Bool);
static XVisualInfo     *(*p_glXChooseVisual)(Display *, int, const int *);
static int             (*p_glXMakeCurrent)(Display *, GLXDrawable, GLXContext);
static void            (*p_glXSwapBuffers)(Display *, GLXDrawable);
static void            (*p_glXDestroyContext)(Display *, GLXContext);

// ================= GL-Funktionszeiger (explizite Signaturen, p_-Praefix) =================
static const GLubyte *(*p_glGetString)(GLenum name);
static GLuint (*p_glCreateShader)(GLenum type);
static void (*p_glShaderSource)(GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
static void (*p_glCompileShader)(GLuint shader);
static void (*p_glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
static void (*p_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
static GLuint (*p_glCreateProgram)(void);
static void (*p_glAttachShader)(GLuint program, GLuint shader);
static void (*p_glLinkProgram)(GLuint program);
static void (*p_glGetProgramiv)(GLuint program, GLenum pname, GLint *params);
static void (*p_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
static void (*p_glDeleteShader)(GLuint shader);
static void (*p_glGenVertexArrays)(GLsizei n, GLuint *arrays);
static void (*p_glBindVertexArray)(GLuint array);
static void (*p_glGenBuffers)(GLsizei n, GLuint *buffers);
static void (*p_glBindBuffer)(GLenum target, GLuint buffer);
static void (*p_glBufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
static void (*p_glBufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
static void (*p_glEnableVertexAttribArray)(GLuint index);
static void (*p_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
static void (*p_glUseProgram)(GLuint program);
static void (*p_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
static void (*p_glDrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei primcount);
static void (*p_glDrawArraysInstancedBaseInstance)(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount, GLuint baseInstance);
static void (*p_glVertexAttribDivisor)(GLuint index, GLuint divisor);
static void (*p_glDepthMask)(GLboolean flag);
static void (*p_glGenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
static void (*p_glBindRenderbuffer)(GLenum target, GLuint renderbuffer);
static void (*p_glRenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
static void (*p_glRenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
static void (*p_glFramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
static void (*p_glDeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
// glBlitNamedFramebuffer: eindeutige 12-Parameter-Signatur (GL 4.2+ / ARB_direct_state_access).
// Wird statt glBlitFramebuffer verwendet, weil Mesa-GLX (llvmpipe-VM) fuer glBlitFramebuffer
// den 10-Parameter-ARB-Entrypoint liefert und 12-Parameter-Core-Aufrufe dadurch still verschiebt.
static void (*p_glBlitNamedFramebuffer)(GLuint readFB, GLuint drawFB,
                                        GLint readX0, GLint readY0, GLint readX1, GLint readY1,
                                        GLint drawX0, GLint drawY0, GLint drawX1, GLint drawY1,
                                        GLbitfield mask, GLenum filter);
static void (*p_glDepthFunc)(GLenum func);
static void (*p_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
static void (*p_glClear)(GLbitfield mask);
static void (*p_glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
static void (*p_glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *data);
static void (*p_glFinish)(void);
static void (*p_glGenTextures)(GLsizei n, GLuint *textures);
static void (*p_glGenerateMipmap)(GLenum target);
static void (*p_glBindTexture)(GLenum target, GLuint texture);
static void (*p_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
static void (*p_glGetTexImage)(GLenum target, GLint level, GLenum format, GLenum type, void *pixels);
static void (*p_glScissor)(GLint x, GLint y, GLsizei width, GLsizei height);
static void (*p_glColorMask)(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
static void (*p_glPolygonOffset)(GLfloat factor, GLfloat units);
static void (*p_glTexParameteri)(GLenum target, GLenum pname, GLint param);
static void (*p_glActiveTexture)(GLenum texture);
static void (*p_glGenFramebuffers)(GLsizei n, GLuint *framebuffers);
static void (*p_glBindFramebuffer)(GLenum target, GLuint framebuffer);
static void (*p_glFramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
static GLenum (*p_glCheckFramebufferStatus)(GLenum target);
static void (*p_glDeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
static void (*p_glDeleteTextures)(GLsizei n, const GLuint *textures);
static void (*p_glBlendFunc)(GLenum sfactor, GLenum dfactor);
static void (*p_glEnable)(GLenum cap);
static void (*p_glDisable)(GLenum cap);
static void (*p_glUniform1f)(GLint location, GLfloat v0);
static void (*p_glUniform2fv)(GLint location, GLsizei count, const GLfloat *value);
static void (*p_glUniform2f)(GLint location, GLfloat v0, GLfloat v1);
static void (*p_glUniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
static void (*p_glUniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
static void (*p_glUniform1i)(GLint location, GLint v0);
static GLint (*p_glGetUniformLocation)(GLuint program, const GLchar *name);
static void (*p_glUniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
static void (*p_glLineWidth)(GLfloat width);
static void (*p_glReadBuffer)(GLenum mode);
static GLenum (*p_glGetError)(void);

// ================= Bibliotheken laden =================
static void gl_fail(const char *n) { fprintf(stderr, "GL: %s fehlt\n", n); exit(1); }
static void load_libs(void) {
  // --- X11 ---
  void *xlib = dlopen("libX11.so.6", RTLD_NOW | RTLD_GLOBAL);
  if (!xlib) { fprintf(stderr, "libX11.so.6 nicht gefunden: %s\n", dlerror()); exit(1); }
#define LOADX(name) do { p_##name = (void *)dlsym(xlib, #name); \
  if (!p_##name) { fprintf(stderr, "X11: %s fehlt\n", #name); exit(1); } } while (0)
  LOADX(XOpenDisplay); LOADX(XDefaultRootWindow); LOADX(XCreateWindow);
  LOADX(XCreateColormap); LOADX(XMapWindow); LOADX(XSync); LOADX(XPending);
  LOADX(XNextEvent); LOADX(XCloseDisplay); LOADX(XInternAtom);
  LOADX(XSetWMProtocols); LOADX(XLookupKeysym);
  // Pointer-Lock (optional; fehlt es, greift der Held-Maus-Modus)
  p_XLockPointer   = (void *)dlsym(xlib, "XLockPointer");
  p_XUnlockPointer = (void *)dlsym(xlib, "XUnlockPointer");
  p_XQueryPointer  = (void *)dlsym(xlib, "XQueryPointer");
  p_XWarpPointer   = (void *)dlsym(xlib, "XWarpPointer");
#undef LOADX

  // --- GL/GLX: nur libGL.so.1 (libglvnd-Dispatcher). Alle GL- und GLX-1.3+-
  //     Funktionen ueber dessen glXGetProcAddressARB holen. ---
  void *glxlib = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
  if (!glxlib) { fprintf(stderr, "libGL.so.1 nicht gefunden: %s\n", dlerror()); exit(1); }
  p_glXGetProcAddressARB = (void *(*)(const char *))dlsym(glxlib, "glXGetProcAddressARB");
  if (!p_glXGetProcAddressARB)
    p_glXGetProcAddressARB = (void *(*)(const char *))dlsym(glxlib, "glXGetProcAddress");
  if (!p_glXGetProcAddressARB) { fprintf(stderr, "glXGetProcAddressARB fehlt\n"); exit(1); }

#define LOADGL(name, type) do { if (!(p_##name = (type)p_glXGetProcAddressARB(#name))) gl_fail(#name); } while (0)
  // GLX 1.3/1.4
  LOADGL(glXChooseFBConfig,           GLXFBConfig *(*)(Display *, int, const int *, int *));
  LOADGL(glXGetVisualFromFBConfig,    XVisualInfo *(*)(Display *, GLXFBConfig));
  LOADGL(glXCreateContextAttribsARB,  GLXContext (*)(Display *, GLXFBConfig, GLXContext, Bool, const int *));
  LOADGL(glXCreateContext,            GLXContext (*)(Display *, XVisualInfo *, GLXContext, Bool));
  LOADGL(glXChooseVisual,             XVisualInfo *(*)(Display *, int, const int *));
  LOADGL(glXMakeCurrent,              int (*)(Display *, GLXDrawable, GLXContext));
  LOADGL(glXSwapBuffers,              void (*)(Display *, GLXDrawable));
  LOADGL(glXDestroyContext,           void (*)(Display *, GLXContext));
  // GL (Zeiger haben explizite Signaturen, void*-Cast reicht)
#define LOADGL2(name) do { if (!(p_##name = (void *)p_glXGetProcAddressARB(#name))) gl_fail(#name); } while (0)
  LOADGL2(glGetString);
  LOADGL2(glCreateShader);
  LOADGL2(glShaderSource);
  LOADGL2(glCompileShader);
  LOADGL2(glGetShaderiv);
  LOADGL2(glGetShaderInfoLog);
  LOADGL2(glCreateProgram);
  LOADGL2(glAttachShader);
  LOADGL2(glLinkProgram);
  LOADGL2(glGetProgramiv);
  LOADGL2(glGetProgramInfoLog);
  LOADGL2(glDeleteShader);
  LOADGL2(glGenVertexArrays);
  LOADGL2(glBindVertexArray);
  LOADGL2(glGenBuffers);
  LOADGL2(glBindBuffer);
  LOADGL2(glBufferData);
  LOADGL2(glBufferSubData);
  LOADGL2(glEnableVertexAttribArray);
  LOADGL2(glVertexAttribPointer);
  LOADGL2(glUseProgram);
  LOADGL2(glDrawArrays);
  LOADGL2(glDrawArraysInstanced);
  LOADGL2(glDrawArraysInstancedBaseInstance);
  LOADGL2(glVertexAttribDivisor);
  LOADGL2(glDepthMask);
  LOADGL2(glGenRenderbuffers);
  LOADGL2(glBindRenderbuffer);
  LOADGL2(glRenderbufferStorageMultisample);
  LOADGL2(glRenderbufferStorage);
  LOADGL2(glFramebufferRenderbuffer);
  LOADGL2(glDeleteRenderbuffers);
  LOADGL2(glBlitNamedFramebuffer);
  LOADGL2(glDepthFunc);
  LOADGL2(glViewport);
  LOADGL2(glClear);
  LOADGL2(glClearColor);
  LOADGL2(glReadPixels);
  LOADGL2(glFinish);
  LOADGL2(glGenTextures);
  LOADGL2(glGenerateMipmap);
  LOADGL2(glBindTexture);
  LOADGL2(glTexImage2D);
  LOADGL2(glGetTexImage);
  LOADGL2(glScissor);
  LOADGL2(glColorMask);
  LOADGL2(glPolygonOffset);
  LOADGL2(glTexParameteri);
  LOADGL2(glActiveTexture);
  LOADGL2(glGenFramebuffers);
  LOADGL2(glBindFramebuffer);
  LOADGL2(glFramebufferTexture2D);
  LOADGL2(glCheckFramebufferStatus);
  LOADGL2(glDeleteFramebuffers);
  LOADGL2(glDeleteTextures);
  LOADGL2(glBlendFunc);
  LOADGL2(glEnable);
  LOADGL2(glDisable);
  LOADGL2(glUniform1f);
  LOADGL2(glUniform2fv);
  LOADGL2(glUniform2f);
  LOADGL2(glUniform3f);
  LOADGL2(glUniform4f);
  LOADGL2(glUniform1i);
  LOADGL2(glGetUniformLocation);
  LOADGL2(glUniformMatrix4fv);
  LOADGL2(glLineWidth);
  LOADGL2(glReadBuffer);
  LOADGL2(glGetError);
#undef LOADGL2
#undef LOADGL
}

// ================= Shader-Quelltexte (generiert) =================
extern const char VS_SRC[];
extern const char FS_SRC[];
extern const char BLIT_FS_SRC[];
extern const char HUD_VS_SRC[];
extern const char HUD_FS_SRC[];
extern const char TREELINES_VS_SRC[];
extern const char TREELINES_FS_SRC[];
extern const char TREE_VS_SRC[];
extern const char TREE_FS_SRC[];
extern const char GRASS_VS_SRC[];
extern const char GRASS_FS_SRC[];
extern const char CROWN_VS_SRC[];
extern const char CROWN_FS_SRC[];
extern const char LEAF_VS_SRC[];
extern const char LEAF_FS_SRC[];

static GLuint compile_shader(GLenum type, const char *src, const char *name) {
  GLuint s = p_glCreateShader(type);
  p_glShaderSource(s, 1, &src, 0);
  p_glCompileShader(s);
  GLint ok = 0;
  p_glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    p_glGetShaderInfoLog(s, sizeof log, 0, log);
    fprintf(stderr, "Shaderfehler (%s):\n%s\n", name, log);
    exit(1);
  }
  return s;
}

static GLuint make_program(const char *vs, const char *fs, const char *name) {
  GLuint p = p_glCreateProgram();
  GLuint v = compile_shader(GL_VERTEX_SHADER, vs, name);
  GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs, name);
  p_glAttachShader(p, v);
  p_glAttachShader(p, f);
  p_glLinkProgram(p);
  GLint ok = 0;
  p_glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    p_glGetProgramInfoLog(p, sizeof log, 0, log);
    fprintf(stderr, "Linkfehler (%s):\n%s\n", name, log);
    exit(1);
  }
  p_glDeleteShader(v);
  p_glDeleteShader(f);
  return p;
}

// ================= Font (8x8, prozedural) =================
#define FONT_COLS 41
#define ATLAS_W (FONT_COLS * 8)   // 328
#define ATLAS_H 8
static const char *FONT_STR = "0123456789.ABCDEFGHIJKLMNOPQRSTUVWXYZ/ x-";
static const uint8_t FONT_GLYPHS[FONT_COLS][8] = {
  {0x3C,0x42,0x46,0x4A,0x52,0x62,0x3C,0x00}, // 0
  {0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00}, // 1
  {0x3C,0x42,0x02,0x04,0x08,0x10,0x7E,0x00}, // 2
  {0x3E,0x41,0x01,0x0E,0x01,0x01,0x7E,0x00}, // 3
  {0x04,0x0C,0x14,0x24,0x7F,0x04,0x04,0x00}, // 4
  {0x7F,0x40,0x7E,0x01,0x01,0x41,0x3E,0x00}, // 5
  {0x3C,0x40,0x40,0x7E,0x41,0x41,0x3E,0x00}, // 6
  {0x7F,0x01,0x02,0x04,0x08,0x08,0x08,0x00}, // 7
  {0x3E,0x41,0x41,0x3E,0x41,0x41,0x3E,0x00}, // 8
  {0x3E,0x41,0x41,0x3F,0x01,0x02,0x3C,0x00}, // 9
  {0x00,0x00,0x00,0x00,0x00,0x00,0x30,0x00}, // .
  {0x18,0x24,0x42,0x42,0x7E,0x42,0x42,0x00}, // A
  {0x7E,0x41,0x41,0x7E,0x41,0x41,0x7E,0x00}, // B
  {0x3C,0x46,0x40,0x40,0x40,0x46,0x3C,0x00}, // C
  {0x78,0x44,0x42,0x42,0x42,0x44,0x78,0x00}, // D
  {0x7F,0x40,0x40,0x7E,0x40,0x40,0x7F,0x00}, // E
  {0x7F,0x40,0x40,0x7E,0x40,0x40,0x40,0x00}, // F
  {0x3C,0x46,0x40,0x5E,0x43,0x46,0x3C,0x00}, // G
  {0x41,0x41,0x41,0x7F,0x41,0x41,0x41,0x00}, // H
  {0x7E,0x10,0x10,0x10,0x10,0x10,0x7E,0x00}, // I
  {0x07,0x02,0x02,0x02,0x02,0x42,0x3C,0x00}, // J
  {0x41,0x42,0x44,0x78,0x44,0x42,0x41,0x00}, // K
  {0x40,0x40,0x40,0x40,0x40,0x40,0x7F,0x00}, // L
  {0x81,0xC3,0xA5,0x99,0x81,0x81,0x81,0x00}, // M
  {0x41,0x61,0x51,0x49,0x45,0x43,0x41,0x00}, // N
  {0x3E,0x41,0x41,0x41,0x41,0x41,0x3E,0x00}, // O
  {0x7E,0x41,0x41,0x7E,0x40,0x40,0x40,0x00}, // P
  {0x3C,0x42,0x42,0x42,0x4A,0x44,0x3B,0x00}, // Q
  {0x7E,0x41,0x41,0x7E,0x50,0x48,0x44,0x00}, // R
  {0x3F,0x40,0x40,0x3E,0x01,0x01,0x7E,0x00}, // S
  {0x7F,0x08,0x08,0x08,0x08,0x08,0x08,0x00}, // T
  {0x41,0x41,0x41,0x41,0x41,0x41,0x3E,0x00}, // U
  {0x41,0x41,0x41,0x41,0x22,0x14,0x08,0x00}, // V
  {0x81,0x81,0x81,0x99,0xA5,0xC3,0x81,0x00}, // W (Spiegelbild von M)
  {0x42,0x42,0x24,0x18,0x24,0x42,0x42,0x00}, // X
  {0x41,0x41,0x22,0x14,0x08,0x08,0x08,0x00}, // Y
  {0x7F,0x02,0x04,0x08,0x10,0x20,0x7F,0x00}, // Z
  {0x01,0x02,0x04,0x08,0x10,0x20,0x20,0x00}, // /
  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // ' '
  {0x00,0x00,0x41,0x12,0x0C,0x12,0x41,0x00}, // x
  {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
};

static GLuint texFont = 0;
static void build_font(void) {
  static uint8_t atlas[ATLAS_W * ATLAS_H * 4];
  memset(atlas, 0, sizeof atlas);
  for (int g = 0; g < FONT_COLS; g++)
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 8; c++)
        if (FONT_GLYPHS[g][r] & (0x80 >> c))
          atlas[(r * ATLAS_W + g * 8 + c) * 4 + 3] = 255;
  // fester Pixel fuer Hintergrund-Quad (recht unten, im Leerraum)
  atlas[(7 * ATLAS_W + ATLAS_W - 1) * 4 + 3] = 255;
  p_glGenTextures(1, &texFont);
  p_glBindTexture(GL_TEXTURE_2D, texFont);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ATLAS_W, ATLAS_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// ================= Zustand =================
#define HUD_MAX_CHARS 160   // pro Zeile; insgesamt ~140 Zeichen (3 Zeilen + Labels)
static float camYaw = 0.85f, camPitch = 0.32f, camRadius = 48.0f;
static float camTgt[3] = { 0.0f, 5.0f, 0.0f };
static int mouseHeld = 0, lastMX = 0, lastMY = 0;
static int orbit = 1;
static int scaleIdx = 1; // 0: 1x, 1: 1/2, 2: 1/4
static int keyW = 0, keyA = 0, keyS = 0, keyD = 0, keyE = 0, keyF = 0;
static int keyUp = 0, keyDown = 0, keyRun = 0;
static int showTreeWire = 0;
static int flyMode = 1;              // 1 = frei schweben (Maus-Look), 0 = Orbit
static float flyPos[3] = { 30.0f, 20.0f, 34.0f };   // Startposition
static float flyYaw = -2.42f, flyPitch = -0.32f;     // Blickrichtung (Richtung Ziel 0,5,0)

static double now_sec(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ================= FBO (Resolutionsskalierung + Mesh-Depth) =================
// Farbtextur RGBA16F (HDR, Mesh + Raymarch), Depth-Texture DEPTH_COMPONENT24
// (Mesh-Pass schreibt Tiefe, Raymarch-Pass compositet dagegen).
static GLuint fbo = 0, texScene = 0, texDepth = 0;      // Single-Sample (Compositing + Sampling)
static GLuint fboMS = 0, rbColorMS = 0, rbDepthMS = 0;   // Multi-Sample (Mesh-Render, MSAA)
static int fboW = 0, fboH = 0;
static int useMSAA = 1;   // 1 = MSAA an (M-Taste zum Umschalten: 4x -> 8x -> off)
static int msaaSamples = 4;  // 4 oder 8 (M-Taste zyklisch)
static int grassFrozen = 0;   // 1 = Gras-Rebuild eingefroren (G-Taste): gleiche Halme, egal wo die Kamera hin
// DEBUG-Zweig-Schalter:
//   J = Depth-Write im Gras-Pass an/aus (Hypothese: Fehlordnung + Depth-Write
//       verdeckt Halme, die am Swap schlagartig sichtbar werden)
//   K = Dichte-Kante an/aus (CUT 0.01 vs. 0 = Feld endet weich bei grassMaxDist)
static int dbgGrassDepth = 1, dbgGrassEdge = 1;
//   C = Kategorie-Farbe als Albedo (nur waehrend ein Fade laeuft), normale
//       Gras-Grafik bleibt sonst voll erhalten
static int dbgGrassCat = 0;    // DEBUG C: 0 aus, 1 Fade-Farben, 2 Arten-Farben
static int dbgTreeMask = 0;    // DEBUG (; bzw. DEMO_TREEMASK): Baum-Mesh als Maske
// DEBUG-Umgebungsvariablen — deterministische Screenshots OHNE X-Tasten (die
// Zustellung synthetischer Tasten ist in der VM unzuverlaessig):
//   DEMO_TREE=1       Kamera beim Start direkt vor den naechsten Baum
//   DEMO_TREEMASK=1   Baum-Mesh in reinem Rot (Maske fuer Rinden-Messungen)
//   DEMO_SHOT=12.5    Screenshot automatisch nach 12,5 s Laufzeit
static int envTreeJump = 0;
static float envShotAt = -1.0f;
//   DEMO_FREEZE=3.5  Shader-Zeit (Wind) auf 3,5 s einfrieren -> Screenshots sind
//                      zwischen Laeufen Pixel fuer Pixel vergleichbar (noetig fuer
//                      Maschen-Abgleich: der Baum-Sway verschiebt die Meshes sonst
//                      um zehn Pixel)
static float envFreeze = -1.0f;
//   DEMO_NOLEAF=1    Einzelblaetter nicht zeichnen (Vorher/Nachher-Messung)
static int envNoLeaf = 0;
// DEMO_NOTERRAIN=1: Terrain, Himmel und Gras nicht zeichnen -> schwarzer
// Hintergrund. Messbank fuer LOD-Farben: ohne Landschaft mittelt kein
// Hintergrund in die Baumfarbe hinein, und die Bilddifferenz ist exakt der Baum.
static int envNoTerrain = 0;
//   DEMO_LEAFMASK=1    Einzelblaetter in reinem Rot (Maske fuer Blatt-Messungen)
static int envLeafMask = 0;
static int envCrownMask = 0;
static int envGlDbg = 0;
static int envShadowDump = 0;
//   Y = Alpha-to-Coverage im Gras-Pass. Alpha wird dann in eine MSAA-Sample-
//       Maske uebersetzt statt gemischt: ein halbtransparenter Halm schreibt
//       Tiefe nur noch fuer die Samples, die er abdeckt. Loesst den Reveal-Pop
//       (sterbender Halm verdeckt einen eingespielten dahinter) und macht die
//       Rasterung nahezu unabhaengig von der Zeichenreihenfolge.
static int dbgAlpha2Cov = 1;
//   I = Zellen beim Build fern->nah entlang der Blickrichtung anordnen, also
//       echte Hinten-nach-Vorn-Zeichenreihenfolge (statt Diamond-Order um das
//       Build-Zentrum). Zum Vergleich, ob sich Sortieren aufs AA-Rauschen lohnt.
static int dbgSortOrder = 0;
//   U = Blickwinkel-Duennung: Halme in flachem Blickwinkel werden pro Halm
//       wegduenniert (alpha), senkrechter Blick bleibt voll. Begruendung und
//       Messwerte in grass.vert.
static int dbgAngThin = 1;
static float grassAngS0 = 0.35f, grassAngQ = 0.7f;
static float gCX = 0, gCZ = 0;   // letztes Gras-Build-Zentrum (auch fuer R-Reset)
static int grassOnly = 0;     // 1 = TEST: nur Gras zeichnen (V-Taste), kein Terrain/Baeume/Blattdach
static int hudMode = 2;       // H-Taste: 2 = Vollstats (2 Zeilen), 1 = reduziert (1 Zeile), 0 = aus
float grassDensityScale = 1.0f;  // 0.2 - 8.0 (Slider unten im Fenster, extern in scene.h)
float grassHeightScale = 1.0f;   // 0.3 - 3.0 (Halmhoehe-Slider)
float grassMaxDist = 170.0f;     // 40 - 240m (max. Gras-Sichtweite, Slider)
float gWorldTime = 0.0f;         // aktuelle Weltzeit (fuer Birth-Time der Halme)
static int sliderDragging = 0;          // 1 = Slider wird per Maus gezogen
// Slider-Fade: Slider+Labels blenden ein, wenn die Maus sich ueber sie
// bewegt (ohne Taste), und aus, wenn die Maus 3s nicht im Bereich war.
// Einblenden 0.5s, Ausblenden 1.5s (wie beim Gras, nur anders getaktet).
static float sliderFade = 1.0f;         // 0..1 (1 = sichtbar)
static float lastMouseInAreaT = -1e9f;  // letzte Zeit, Maus im Slider-Bereich
// Slider-Rebuild-Zustand (Debounce + 2-Phasen-Transition, s. Rebuild-Trigger)
static float lastDensity = 1.0f, lastMaxDist = 80.0f;
static float lastSliderT = -1e9f;       // letzter Slider-Wertwechsel
static int   sliderDirty = 0;           // Slider-Wert geaendert, noch nicht gebaut
static int   sliderPhase = 0;           // 0=idle 1=OUT laeuft 2=wartet 2s 3=IN laeuft
static float sliderPhaseT = 0.0f;
// MSAA_SAMPLES: dynamisch (msaaSamples, 4 oder 8, M-Taste)
static void ensure_fbo(int w, int h) {
  int sw = w >> scaleIdx, sh = h >> scaleIdx;
  if (sw < 2) sw = 2;
  if (sh < 2) sh = 2;
  if (sw == fboW && sh == fboH) return;
  if (fbo)   { p_glDeleteFramebuffers(1, &fbo);   p_glDeleteTextures(1, &texScene); p_glDeleteTextures(1, &texDepth); }
  if (fboMS) { p_glDeleteFramebuffers(1, &fboMS); p_glDeleteRenderbuffers(1, &rbColorMS); p_glDeleteRenderbuffers(1, &rbDepthMS); }
  // Single-Sample-Farbe (Compositing + Sampling)
  p_glGenTextures(1, &texScene);
  p_glBindTexture(GL_TEXTURE_2D, texScene);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sw, sh, 0, GL_RGBA, GL_HALF_FLOAT, 0);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // Single-Sample-Depth-Textur (Sampling im Raymarch-Pass)
  p_glGenTextures(1, &texDepth);
  p_glBindTexture(GL_TEXTURE_2D, texDepth);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, sw, sh, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 0);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  p_glGenFramebuffers(1, &fbo);
  p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texScene, 0);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texDepth, 0);
  // Multi-Sample-Farbe + Depth (Renderbuffer, Mesh-Render mit MSAA)
  p_glGenRenderbuffers(1, &rbColorMS);
  p_glBindRenderbuffer(GL_RENDERBUFFER, rbColorMS);
  p_glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_RGBA16F, sw, sh);
  p_glGenRenderbuffers(1, &rbDepthMS);
  p_glBindRenderbuffer(GL_RENDERBUFFER, rbDepthMS);
  p_glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaSamples, GL_DEPTH_COMPONENT24, sw, sh);
  p_glGenFramebuffers(1, &fboMS);
  p_glBindFramebuffer(GL_FRAMEBUFFER, fboMS);
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbColorMS);
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbDepthMS);
  if (p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    fprintf(stderr, "FBO-MS unvollstaendig!\n");
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  fboW = sw; fboH = sh;
}

// ================= CPU-Terrain: siehe terrain_cpu.c (ueber scene.h) =================

// ================= Mesh-Resources (Baeume/Gras, Geometrie aus trees_cpu/grass_cpu) =================
// Baum-Geometrie in ZWEI Saetzen: der Build-Thread fuellt den einen, gezeichnet
// wird aus dem anderen (drawSet). Nur so laesst sich der 350-MB-Upload in
// Scheiben aufteilen, ohne dass die GPU halbhochgeladene Daten sieht.
static GLuint vboTree[2] = {0,0}, vaoTree[2] = {0,0};
static GLuint vboCrown[2] = {0,0}, vaoCrown[2] = {0,0};
static GLuint vboLeaf[2] = {0,0}, vaoLeaf[2] = {0,0};
static GLuint vboLeafBase = 0, vboCrownQuad = 0;
static int drawSet = 0;
static GLuint vboGrass = 0, vaoGrass = 0, vboGrassBase = 0;
static GLuint vboWire = 0, vaoWire = 0;

// Grundformen (per-Vertex-Attribute der Instanz-Shader)
// Gras-Grundform: 5 Vertices als GL_TRIANGLE_STRIP -> genau dieselben drei
// Dreiecke wie frueher mit 9 duplizierten Vertices (GL_TRIANGLES):
//   Strip (v0,v1,v2,v3,v4) = (v0,v1,v2), (v1,v2,v3), (v2,v3,v4)
//   = unterer Keil, oberer Keil, Spitze   (Menge identisch zu 0,1,2 / 2,1,3 / 2,3,4)
// Das sind 44 % weniger Vertex-Aufrufe pro Halm — bei 550k+ Halmen der
// guenstigste Hebel, und noetig geworden durch die Arten-Rechnung im Vertex-Shader.
// Culling ist fuer Gras aus, die alternierende Windung im Strip ist also egal
// (die Normale kommt aus yaw, nicht aus der Windung).
static const float grassBase[5][2] = {
  {0.0f, -0.5f}, {0.0f, 0.5f}, {0.55f, -0.34f}, {0.55f, 0.34f}, {1.0f, 0.0f}
};
// Grundblatt (leaf.vert): aBase = (t laengs, w quer), zwei Dreiecke
// Statiches Quad (2 Dreiecke) fuer die instanzierten Kronen-Karten: die Ecke ist
// identisch fuer alle Karten, deshalb liegt sie einmal im VBO statt sechsmal pro
// Karte in der Instanz (Runde 28).
static const float crownQuad[6][2] = {
  {-0.5f,-0.5f},{0.5f,-0.5f},{-0.5f,0.5f},
  { 0.5f,-0.5f},{ 0.5f, 0.5f},{-0.5f,0.5f}
};
static const float leafBase[6][2] = {
  {-0.5f,-0.5f},{0.5f,-0.5f},{-0.5f,0.5f},{0.5f,-0.5f},{0.5f,0.5f},{-0.5f,0.5f}
};

// ================= Blatthorizont-Atlas (ferne Kronen) =================
// Ferne Baeume zeigen kein echtes Blattwerk (zu viele Blaeue), sondern Quads, die
// eine Textur sampeln. Die Textur wird NICHT gemalt: sie ist die Aufloesung
// echter Einzelblaetter einer Referenz-Krone, aus 16 Richtungen orthografisch
// aufgenommen (Albedo + Abdeckung). Die Kronen-Quads waehlen abhaengig von der
// Blickrichtung die passende Kachel -> die Krone parallaxiert, wenn man sie
// umlaeuft, statt wie ein gemalter Fleck zu stehen.
#define FOL_ATLAS 2048
#define FOL_TILES 8                 // 8x8 Kacheln = 4 Varianten x 16 Richtungen
#define FOL_TILEPX (FOL_ATLAS / FOL_TILES)
#define FOL_AZ    16
#define FOL_VAR   4
static GLuint folTex = 0, folFbo = 0, folDepth = 0, vboFol = 0, vaoFol = 0;

// ---------- Ganzbaum-Kronen-Kacheln (TODO 23, Runde 29) ----------
// Die groebste Detailstufe (tier 0, ab 20x Wuchshoehe ~ 135 m) zeichnet statt
// hunderter Einzelkarten EIN Quad pro Baum, beklebt mit einer Kronen-Kachel.
// Gemessen (Runde 28): tier 0 hielt 246 535 von 545 871 Karten — der groesste
// Full-Rate-Posten. Eine Kachel pro Baum = 364 Quads.
// Die Kachel ist prozedural: Silhouette einer Laubkrone (breiter als hoch,
// ausgefranster Rand aus Buscheln), innen Buschel-Sprenkel statt flacher
// Scheibe. Farbneutral — die Blattfarbe des Baumes kommt als Tint.
#define CRN_N   2                 // 2x2 Kacheln
#define CRN_PX  128               // Pixel pro Kachel
static GLuint crnTex = 0;
static float crnHash2(float x, float y){
  float r = sinf(x * 12.9898f + y * 78.233f) * 43758.5453f;
  return r - floorf(r);
}
static float crnNoise(float x, float y){
  float ix = floorf(x), iy = floorf(y), fx = x - ix, fy = y - iy;
  float a = crnHash2(ix, iy), b = crnHash2(ix + 1, iy);
  float c = crnHash2(ix, iy + 1), d = crnHash2(ix + 1, iy + 1);
  float ux = fx * fx * (3.0f - 2.0f * fx), uy = fy * fy * (3.0f - 2.0f * fy);
  return a + (b - a) * ux + (c - a) * uy + (a - b - c + d) * ux * uy;
}
static float crnSstep(float e0, float e1, float x){
  float t = (x - e0) / (e1 - e0); if(t < 0.0f) t = 0.0f; if(t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}
static void build_crown_tiles(void){
  static unsigned char img[CRN_N * CRN_PX * CRN_N * CRN_PX * 4];
  for(int ty = 0; ty < CRN_N; ty++)
  for(int tx = 0; tx < CRN_N; tx++)
  for(int py = 0; py < CRN_PX; py++)
  for(int px = 0; px < CRN_PX; px++){
    float u = (px + 0.5f) / CRN_PX * 2.0f - 1.0f;
    float v = (py + 0.5f) / CRN_PX * 2.0f - 1.0f;
    float ang = atan2f(v, u);
    float r0 = 0.80f + 0.07f * sinf(3.0f * ang + tx * 1.7f + ty * 2.3f)
                    + 0.05f * cosf(5.0f * ang - tx * 0.9f + ty * 1.1f);
    if(v < 0.0f) r0 *= 1.0f - 0.18f * (-v);          // Unterseite flacher
    float rr = sqrtf(u * u * 0.92f + v * v * 1.12f);
    float edge = crnNoise(px * 0.11f + tx * 7.0f, py * 0.11f + ty * 5.0f);
    float a = r0 + (edge - 0.5f) * 0.30f;            // Buschel ragen ueber die Silhouette
    float cov = 1.0f - crnSstep(a - 0.14f, a + 0.06f, rr);
    float n2 = crnNoise(px * 0.22f + tx * 3.0f, py * 0.22f + ty * 9.0f);
    float n3 = crnNoise(px * 0.55f + tx * 11.0f, py * 0.55f + ty * 13.0f);
    cov *= 0.88f + 0.12f * n2;                       // innen fast dicht, leicht gefleckt
    float lum = 0.76f + 0.24f * (0.6f * n2 + 0.4f * n3);
    lum *= 0.86f + 0.14f * crnSstep(-0.9f, 0.9f, v); // unten dunkler
    int o = ((ty * CRN_PX + py) * (CRN_N * CRN_PX) + tx * CRN_PX + px) * 4;
    unsigned char cc = (unsigned char)(lum * 255.0f);
    img[o] = cc; img[o+1] = cc; img[o+2] = cc;
    img[o+3] = (unsigned char)(cov * 255.0f);
  }
  p_glGenTextures(1, &crnTex);
  p_glBindTexture(GL_TEXTURE_2D, crnTex);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CRN_N * CRN_PX, CRN_N * CRN_PX, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, img);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  printf("Kronen-Kacheln: %dx%d Kacheln a %d px, prozedural (%.2f s)\n",
         CRN_N, CRN_N, CRN_PX, now_sec() - 0.0);
}
// Shadow-Map: eine orthogliche Licht-Kaskade um die Kamera. Geworfen werden
// Baum-Mesh, Kronen-Quads und Einzelblaetter — die Kronen sind der
// Schattenwerfer, den man auf dem Boden sieht.
// Aufloesung: 1024^2 ueber 140 m waren 13,7 cm pro Texel -> blockiges Flecken-
// muster auf Staemmen und weichen Kanten (Nutzerbefund "schlecht aufgeloest").
// 2048^2 ueber 90 m = 4,4 cm/Texel; die Nahe der Kamera ist auch der Bereich, in
// dem Schatten ueberhaupt aufgelöst genug sind (dunst ab 60 m).
#define SHADOW_SZ 2048
#define SHADOW_HALF 45.0f
static GLuint shadowTex = 0, shadowFbo = 0;
static float  lightMVP[16];
static int    shadowOn = 1, shadowLeafOn = 1, shadowSampleOn = 1, shadowForce = 0, shadowVis = 0;
static float folPatch[FOLIAGE_CAP * LEAF_FLOATS];

// Orthografische MVP (quadratischer Viewport), fuer die Atlas-Aufnahme
static void ortho_mvp(const float cp[3], const float tg[3], float half, float out[16]){
  float fx = tg[0] - cp[0], fy = tg[1] - cp[1], fz = tg[2] - cp[2];
  float fl = sqrtf(fx * fx + fy * fy + fz * fz); if(fl < 1e-6f) fl = 1e-6f;
  fx /= fl; fy /= fl; fz /= fl;
  float rx = -fz, ry = 0.0f, rz = fx;
  float rl = sqrtf(rx * rx + rz * rz); if(rl < 1e-6f){ rx = 1.0f; rz = 0.0f; rl = 1.0f; }
  rx /= rl; rz /= rl;
  float ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;
  float dr = rx * cp[0] + ry * cp[1] + rz * cp[2];
  float du = ux * cp[0] + uy * cp[1] + uz * cp[2];
  float df = fx * cp[0] + fy * cp[1] + fz * cp[2];
  const float dz = 2.0f * half * 1.4f;      // Tiefenbereich um die Krone
  out[0] = rx / half; out[4] = ry / half; out[8]  = rz / half; out[12] = -dr / half;
  out[1] = ux / half; out[5] = uy / half; out[9]  = uz / half; out[13] = -du / half;
  out[2] = 2.0f * fx / dz; out[6] = 2.0f * fy / dz; out[10] = 2.0f * fz / dz;
  out[14] = -2.0f * df / dz - 1.0f;
  out[3] = 0.0f; out[7] = 0.0f; out[11] = 0.0f; out[15] = 1.0f;
}

typedef struct { GLint tex, mvp, on, px, bias; } ShadowLoc;
static void fillShadowLoc(GLuint prog, ShadowLoc *sl){
  sl->tex = p_glGetUniformLocation(prog, "uShadow");
  sl->mvp = p_glGetUniformLocation(prog, "uLightMVP");
  sl->on  = p_glGetUniformLocation(prog, "uShadowOn");
  sl->px  = p_glGetUniformLocation(prog, "uShadowPx");
  sl->bias = p_glGetUniformLocation(prog, "uShadowBias");
}
// Depth-Bias: GPU Gems 1 Ch. 12 verlangt polygon offset im Wurf (haben wir) und
// einen kleinen Bias im Vergleich. Der alte Wert 0.0025 war in DEPTH-Einheiten
// und entsprach bei 196 m Tiefenbereich 0,49 m — ein halber Meter, damit waren
// alle flachen Schatten (Blatt ueber Halm) weg. Hier: 1,5 Texel, aus der
// Texelgroesse in Metern in Depth-Einheiten umgerechnet.
static float shadowBiasUnits(void){
  float texelM = 2.0f * SHADOW_HALF / (float)SHADOW_SZ;   // m pro Texel
  float dz     = 2.0f * SHADOW_HALF * 1.4f;               // Tiefenbereich der Ortho-Kiste
  return 2.0f * (1.5f * texelM) / dz;
}
static void applyShadow(const ShadowLoc *sl, int on){
  if(sl->tex >= 0){ p_glActiveTexture(GL_TEXTURE3); p_glBindTexture(GL_TEXTURE_2D, shadowTex);
                    p_glUniform1i(sl->tex, 3); }
  if(sl->mvp >= 0) p_glUniformMatrix4fv(sl->mvp, 1, GL_FALSE, lightMVP);
  if(sl->on  >= 0) p_glUniform1f(sl->on, (float)on);
  if(sl->px  >= 0) p_glUniform1f(sl->px, 1.0f / (float)SHADOW_SZ);
  if(sl->bias >= 0) p_glUniform1f(sl->bias, shadowBiasUnits());
  p_glActiveTexture(GL_TEXTURE0);
}
static void init_shadow(void){
  p_glGenTextures(1, &shadowTex);
  p_glBindTexture(GL_TEXTURE_2D, shadowTex);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, SHADOW_SZ, SHADOW_SZ, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  p_glGenFramebuffers(1, &shadowFbo);
  p_glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
  printf("Shadow-Map %dx%d: FBO 0x%x\n", SHADOW_SZ, SHADOW_SZ,
         p_glCheckFramebufferStatus(GL_FRAMEBUFFER));
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void build_foliage_atlas(GLuint progLeaf){
  GLint lMVP  = p_glGetUniformLocation(progLeaf, "uMVP");
  GLint lCam  = p_glGetUniformLocation(progLeaf, "uCamPos");
  GLint lLod  = p_glGetUniformLocation(progLeaf, "uLodCam");
  GLint lTime = p_glGetUniformLocation(progLeaf, "uTime");
  GLint lAlb  = p_glGetUniformLocation(progLeaf, "uAlbedoOnly");
  GLint lMask = p_glGetUniformLocation(progLeaf, "uLeafMask");
  p_glGenTextures(1, &folTex);
  p_glBindTexture(GL_TEXTURE_2D, folTex);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FOL_ATLAS, FOL_ATLAS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  // Kein Mipmap: die Kacheln liegen nebeneinander, ein Mip-Wechsel wuerde sie
  // auflosen (Kronen-frag begrenzt die Stufe selbst und rueckt die UV an den
  // Kachelrand heran).
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  p_glGenFramebuffers(1, &folFbo);
  p_glBindFramebuffer(GL_FRAMEBUFFER, folFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, folTex, 0);
  p_glGenRenderbuffers(1, &folDepth);
  p_glBindRenderbuffer(GL_RENDERBUFFER, folDepth);
  p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, FOL_ATLAS, FOL_ATLAS);
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, folDepth);
  p_glGenBuffers(1, &vboFol);
  p_glGenVertexArrays(1, &vaoFol);
  p_glBindVertexArray(vaoFol);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboLeafBase);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboFol);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)0);
  p_glVertexAttribDivisor(1, 1);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)12);
  p_glVertexAttribDivisor(2, 1);
  p_glEnableVertexAttribArray(3);
  p_glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)28);
  p_glVertexAttribDivisor(3, 1);
  p_glEnableVertexAttribArray(4);
  p_glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)40);  // iLodR
  p_glVertexAttribDivisor(4, 1);

  p_glDisable(GL_BLEND);
  p_glEnable(GL_DEPTH_TEST);
  p_glDepthFunc(GL_LESS);
  // Aufnahme-Rahmen: der Buschel fillt die Kachel zu ~85 %. half bestimmt auch
  // den Tiefenbereich (dz = 2,8*half), camR = dz/2 -> der Buschel liegt genau in
  // der Mitte des orthografischen Bereichs (sonst wird die hintere Haelfte weg-
  // geclippt, gemessen an der halbleeren Kachel).
  const float s = 2.2f, cy = 0.0f, camR = 0.42f * s, half = 0.30f * s;
  double tCap0 = now_sec();
  int nLast = 0;
  for(int v = 0; v < FOL_VAR; v++){
    int n = foliage_patch(folPatch, FOLIAGE_CAP, 12.7f + v * 37.31f, s);
    nLast = n;
    p_glBindBuffer(GL_ARRAY_BUFFER, vboFol);
    p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)n * LEAF_FLOATS * sizeof(float), folPatch, GL_DYNAMIC_DRAW);
    for(int a = 0; a < FOL_AZ; a++){
      int t = v * FOL_AZ + a;
      float az = a * 6.2832f / (float)FOL_AZ;
      float cp[3] = { sinf(az) * camR, cy + 0.10f * s, cosf(az) * camR };
      float tg[3] = { 0.0f, cy, 0.0f };
      float m[16]; ortho_mvp(cp, tg, half, m);
      int txi = t % FOL_TILES, tyi = t / FOL_TILES;
      p_glViewport(txi * FOL_TILEPX, tyi * FOL_TILEPX, FOL_TILEPX, FOL_TILEPX);
      // glClear wirkt auf den GANZEN Framebuffer, nicht nur den Viewport — ohne
      // Scissor wuerde jede Kachel alle vorherigen loeschen (gemessen: am Ende
      // war genau eine Kachel im Atlas).
      p_glEnable(GL_SCISSOR_TEST);
      p_glScissor(txi * FOL_TILEPX, tyi * FOL_TILEPX, FOL_TILEPX, FOL_TILEPX);
      p_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
      p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      p_glUseProgram(progLeaf);
      p_glUniformMatrix4fv(lMVP, 1, GL_FALSE, m);
      p_glUniform3f(lCam, cp[0], cp[1], cp[2]);
      if(lLod >= 0) p_glUniform3f(lLod, cp[0], cp[1], cp[2]);
      p_glUniform1f(lTime, 0.0f);
      p_glUniform1f(lAlb, 1.0f);
      p_glUniform1f(lMask, 0.0f);
      p_glBindVertexArray(vaoFol);
      p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)n);
    }
  }
  // DEBUG (DEMO_FOLDUMP=1): Atlas als PPM — verifiziert, dass die Kacheln
  // wirklich Blattwerk enthalten und nicht leer sind.
  if(getenv("DEMO_FOLDUMP")){
    unsigned char *buf = (unsigned char*)malloc((size_t)FOL_ATLAS * FOL_ATLAS * 4);
    if(buf){
      p_glBindTexture(GL_TEXTURE_2D, folTex);
      p_glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buf);
      FILE *f = fopen("/tmp/64kdemo_folatlas.ppm", "wb");
      if(f){ fprintf(f, "P6\n%d %d\n255\n", FOL_ATLAS, FOL_ATLAS);
             unsigned char *row = (unsigned char*)malloc((size_t)FOL_ATLAS * 3);
             for(int y = FOL_ATLAS - 1; y >= 0; y--) {
               for(int x = 0; x < FOL_ATLAS; x++){
                 unsigned char *q = buf + ((size_t)y * FOL_ATLAS + x) * 4;
                 row[x*3+0] = q[0]; row[x*3+1] = q[1]; row[x*3+2] = q[2];
               }
               fwrite(row, 3, FOL_ATLAS, f);
             }
             free(row);
             fclose(f); printf("DEBUG Atlas-Dump geschrieben\n"); }
      free(buf);
    }
  }
  p_glDisable(GL_SCISSOR_TEST);
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  p_glDisable(GL_DEPTH_TEST);
  printf("Blatt-Atlas: %dx%d Kacheln (%d px), %d Varianten x %d Richtungen, %d Blaetter je Krone, %.2f s\n",
         FOL_TILES, FOL_TILES, FOL_TILEPX, FOL_VAR, FOL_AZ, nLast, now_sec() - tCap0);
}

// Upload des fertigen Builds in Scheiben: pro Frame hoechstens UP_BYTES, damit
// ein 350-MB-Build keinen 140-ms-Ruckler erzeugt (gemessener Anteil am frueheren
// Rebuild von 490 ms). Erst wenn ALLES hochgeladen ist, wechselt drawSet — die
// GPU sieht zu keinem Zeitpunkt halbfertige Geometrie.
#define UP_BYTES (6u * 1024u * 1024u)   // feine Slices: nur so greift das Zeitbudget
static struct { const float *src; GLuint vbo; size_t bytes, pos; } upBuf[3];
static int upActive = 0, upNextSet = 1;

// Ueberblendung beim Satz-Wechsel (Runde 27, Nutzer: "die Baeume springen wild
// zwischen den LODs", "auch die Baeume ganz in der Ferne sollten sich langsam
// einblenden"). Der bisher gezeichnete Satz bleibt sichtbar und loest sich aus,
// waehrend der neue sich einloest — beides per Wuerfelentscheid im Vertex-/Frag-
// ment, also ohne Alpha-Blend und ohne Sortierabhaengigkeit.
static double treeSwapT = -1e30;
static double treeFadeDur = 0.9;
static int treeOldSet = -1;
double treeTierSwitchS = 0.0;   // DEMO_TIERSWITCH (Messbank)
int treeDbg = 0;              // DEMO_TREEDEBUG: Satz-Wechsel + Ueberblendung protokollieren
// Zaehle zum jeweils gezeichneten Satz (die D-Zaehler in trees_cpu.c zeigen schon
// auf den neuen Build, bevor der Upload fertig ist — die gezeichneten Zaehler
// duerfen nicht mitlaufen).
static int nTreeVertsCur = 0, nCrownVertsCur = 0, nLeafInstCur = 0;
static int nTreeVertsOld = 0, nCrownVertsOld = 0, nLeafInstOld = 0;

static void upload_begin(const float *tv, const float *cv, const float *lv,
                         int ntv, int ncv, int nli, int nextSet){
  upBuf[0].src = tv; upBuf[0].vbo = vboTree[nextSet];
  upBuf[0].bytes = (size_t)ntv * TREE_FLOATS * sizeof(float);
  upBuf[1].src = cv; upBuf[1].vbo = vboCrown[nextSet];
  upBuf[1].bytes = (size_t)ncv * CROWN_FLOATS * sizeof(float);
  upBuf[2].src = lv; upBuf[2].vbo = vboLeaf[nextSet];
  upBuf[2].bytes = (size_t)nli * LEAF_FLOATS * sizeof(float);
  for(int i = 0; i < 3; i++){
    upBuf[i].pos = 0;
    if(upBuf[i].bytes == 0) upBuf[i].bytes = 0;   // leeres Puffer ist fertig (pos 0)
    p_glBindBuffer(GL_ARRAY_BUFFER, upBuf[i].vbo);
    p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(upBuf[i].bytes ? upBuf[i].bytes : 16),
                   NULL, GL_DYNAMIC_DRAW);
  }
  p_glBindBuffer(GL_ARRAY_BUFFER, vboWire);
  p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nWireVerts * WIRE_FLOATS * sizeof(float),
                 wireVerts, GL_DYNAMIC_DRAW);
  upNextSet = nextSet;
  upActive = 1;
}

// wird pro Frame aufgerufen; gibt zurueck, ob gerade ein Upload laeuft.
// Zeitbudget statt Byte-Budget: ein festes 24-MB-Kontingent brauchte bei der
// VM (1,4 s/Frame) 15 Frames = 20 s, bis das neue Set zeichnet — die naechsten
// Baeume fehlten sichtbar. Mit Zeitbudget passt sich die Menge an die Framezeit an.
#define UP_MS_MAX 20.0
#define UP_MS_MIN 3.0
static int upload_step(double frameMs){
  if(!upActive) return 0;
  // Budget aus der letzten Framezeit: 25% des Frames, geklemmt auf 3..20 ms.
  // So saugt eine schnelle GPU mehr pro Frame als die Software-VM, ohne dass
  // ein Frame dominiert wird.
  double budgetMs = frameMs * 0.25;
  if(budgetMs < UP_MS_MIN) budgetMs = UP_MS_MIN;
  if(budgetMs > UP_MS_MAX) budgetMs = UP_MS_MAX;
  double t0 = now_sec();
  for(int i = 0; i < 3; i++){
    if(upBuf[i].pos >= upBuf[i].bytes) continue;
    for(;;){
      size_t n = upBuf[i].bytes - upBuf[i].pos;
      if(n > UP_BYTES) n = UP_BYTES;
      p_glBindBuffer(GL_ARRAY_BUFFER, upBuf[i].vbo);
      p_glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)upBuf[i].pos, (GLsizeiptr)n,
                        upBuf[i].src + upBuf[i].pos / sizeof(float));
      upBuf[i].pos += n;
      if(upBuf[i].pos >= upBuf[i].bytes) break;
      if((now_sec() - t0) * 1000.0 >= budgetMs) break;   // Framezeit einhalten
    }
    if((now_sec() - t0) * 1000.0 >= budgetMs) break;
  }
  for(int i = 0; i < 3; i++) if(upBuf[i].pos < upBuf[i].bytes) return 1;
  upActive = 0;
  // Wechsel: der alte Satz bleibt zum Ausblenden erhalten.
  treeOldSet = drawSet;
  nTreeVertsOld = nTreeVertsCur; nCrownVertsOld = nCrownVertsCur; nLeafInstOld = nLeafInstCur;
  drawSet = upNextSet;
  nTreeVertsCur = nTreeVertsD; nCrownVertsCur = nCrownVertsD; nLeafInstCur = nLeafInstD;
  treeSwapT = now_sec();
  if(treeDbg) printf("  Satz-Wechsel: alt=Satz %d (%d V / %d K / %d B), neu=Satz %d "
                     "(%d V / %d K / %d B), Ueberblendung %.2f s\n",
                     treeOldSet, nTreeVertsOld, nCrownVertsOld, nLeafInstOld,
                     drawSet, nTreeVertsCur, nCrownVertsCur, nLeafInstCur, treeFadeDur);
  return 0;
}

static void build_tree_lines(void){   // VAOs + Grundformen (Geometrie selbst: trees_cpu.c)
  for(int k = 0; k < 2; k++){
    p_glGenBuffers(1, &vboTree[k]);
    p_glGenBuffers(1, &vboCrown[k]);
    p_glGenBuffers(1, &vboLeaf[k]);
  }
  p_glGenBuffers(1, &vboLeafBase);
  p_glGenBuffers(1, &vboCrownQuad);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboCrownQuad);
  p_glBufferData(GL_ARRAY_BUFFER, sizeof crownQuad, crownQuad, GL_STATIC_DRAW);
  p_glGenBuffers(1, &vboGrass);
  p_glGenBuffers(1, &vboWire);
  p_glGenBuffers(1, &vboGrassBase);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboGrassBase);
  p_glBufferData(GL_ARRAY_BUFFER, sizeof grassBase, grassBase, GL_STATIC_DRAW);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboLeafBase);
  p_glBufferData(GL_ARRAY_BUFFER, sizeof leafBase, leafBase, GL_STATIC_DRAW);

  for(int k = 0; k < 2; k++){
  // Baum-Mesh: aPos(3) aNrm(3) aCol(4)
  p_glGenVertexArrays(1, &vaoTree[k]);
  p_glBindVertexArray(vaoTree[k]);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboTree[k]);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)0);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)12);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)24);
  p_glEnableVertexAttribArray(3);
  p_glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)36);  // sway,u,v

  // Kronen-Billboards: aCenter(3) aOff(2) aData(4)
  // Kronen-Karten: 6 Eckpunkte aus vboCrownQuad (divisor 0) + 1 Instanz pro
  // Karte: aCenter(3) aData(4)=size,rot,seed,variant  iLodR(1) iCol(3)
  p_glGenVertexArrays(1, &vaoCrown[k]);
  p_glBindVertexArray(vaoCrown[k]);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboCrownQuad);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);
  p_glVertexAttribDivisor(1, 0);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboCrown[k]);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)0);
  p_glVertexAttribDivisor(0, 1);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)12);
  p_glVertexAttribDivisor(2, 1);
  p_glEnableVertexAttribArray(3);
  p_glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)28); // lodR
  p_glVertexAttribDivisor(3, 1);
  p_glEnableVertexAttribArray(4);
  p_glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)32); // Blattfarbe
  p_glVertexAttribDivisor(4, 1);
  p_glEnableVertexAttribArray(5);
  p_glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)44); // lodR2
  p_glVertexAttribDivisor(5, 1);

  // Einzelblaetter naher Baeume: aBase(2) statisch + iPos(3) iData(4) iCol(3) [instanzen]
  p_glGenVertexArrays(1, &vaoLeaf[k]);
  p_glBindVertexArray(vaoLeaf[k]);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboLeafBase);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboLeaf[k]);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)0);
  p_glVertexAttribDivisor(1, 1);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)12);
  p_glVertexAttribDivisor(2, 1);
  p_glEnableVertexAttribArray(3);
  p_glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)28);
  p_glVertexAttribDivisor(3, 1);
  p_glEnableVertexAttribArray(4);
  p_glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, LEAF_FLOATS * 4, (const void *)40);  // iLodR
  p_glVertexAttribDivisor(4, 1);
  }

  // Gras: aBase(2) + iPos(3) iData(4) [instanzen]
  p_glGenVertexArrays(1, &vaoGrass);
  p_glBindVertexArray(vaoGrass);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboGrassBase);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboGrass);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, GRASS_FLOATS * 4, (const void *)0);
  p_glVertexAttribDivisor(1, 1);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, GRASS_FLOATS * 4, (const void *)12);
  p_glVertexAttribDivisor(2, 1);
  p_glEnableVertexAttribArray(3);
  p_glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, GRASS_FLOATS * 4, (const void *)28);
  p_glVertexAttribDivisor(3, 1);
  p_glEnableVertexAttribArray(4);
  p_glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, GRASS_FLOATS * 4, (const void *)36);
  p_glVertexAttribDivisor(4, 1);
  // Attribut 5 (death-Time) ist weg: Fade-Zeiten stecken in der Gruppen-
  // tabelle uFadeGrp, der Halm traegt nur noch den Gruppen-Index (Attribut 4).

  // Wireframe: aPos(3)
  p_glGenVertexArrays(1, &vaoWire);
  p_glBindVertexArray(vaoWire);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboWire);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, WIRE_FLOATS * 4, (const void *)0);

  p_glBindVertexArray(0);
  printf("Mesh-Pipeline bereit\n");
}

// ================= MVP-Matrix (exakt wie die Raymarcher-Kamera) =================
// Raymarcher: rd = normalize(fwd*1.7 + right*uv.x + up*uv.y),
// right = normalize(cross(fwd,(0,1,0))), up = cross(right,fwd).
// Raymarcher: rd = fwd*1.7 + right*uv.x + up*uv.y, uv = (frag-0.5*res)/res.y.
// NDC normalisiert x durch W/2 und y durch H/2, uv dagegen beide durch H.
// -> NDC-Fokallaenge fy = 1.7 * (H / (H/2)) = 3.4, und fx = fy * H / W.
static void build_mvp(const float camPos[3], const float camTgt[3], int w, int h, float out[16]){
  float fx = camTgt[0] - camPos[0], fy = camTgt[1] - camPos[1], fz = camTgt[2] - camPos[2];
  float fl = sqrtf(fx * fx + fy * fy + fz * fz);
  fx /= fl; fy /= fl; fz /= fl;
  float rx = -fz, ry = 0.0f, rz = fx;           // cross(fwd, (0,1,0))
  float rl = sqrtf(rx * rx + ry * ry + rz * rz);
  rx /= rl; ry /= rl; rz /= rl;
  float ux = ry * fz - rz * fy, uy = rz * fx - rx * fz, uz = rx * fy - ry * fx;
  // View-Matrix: view = R@(p-ro), R-Reihen = right, up, -fwd.
  // Translation = -R@ro = (-dot(ro,right), -dot(ro,up), +dot(ro,fwd)).
  float V[16] = {
    rx, ux, -fx, 0.0f,
    ry, uy, -fy, 0.0f,
    rz, uz, -fz, 0.0f,
    -(rx * camPos[0] + ry * camPos[1] + rz * camPos[2]),
    -(ux * camPos[0] + uy * camPos[1] + uz * camPos[2]),
     (fx * camPos[0] + fy * camPos[1] + fz * camPos[2]),
    1.0f
  };
  // Projektion: clip = (fx*vx, fy*vy, A*vz + B, -vz) mit vx=dot(v,right) etc.
  // fy = 3.4 (NDC-Fokallaenge = 2 * Raymarcher-1.7), fx = fy * H / W.
  // Z-Mapping: NDC.z = -A + B/t  mit A=(n+f)/(n-f)... siehe linearT() in fragment.glsl.
  const float pfy = 3.4f;
  const float pfx = pfy * (float)h / (float)w;
  const float zn = CAM_NEAR, zf = CAM_FAR;
  const float za = (zn + zf) / (zn - zf);
  const float zb = 2.0f * zn * zf / (zn - zf);
  // P (Projektion) ist fast diagonal: [pfx,0,0,0 | 0,pfy,0,0 | 0,0,za,-1 | 0,0,zb,0]
  // P ist fast diagonal -> Multiplikation spezialisiert (kein 4x4x4-Loop):
  //   out[c,0] = pfx*V[c,0], out[c,1] = pfy*V[c,1],
  //   out[c,2] = za*V[c,2] + zb*V[c,3], out[c,3] = -V[c,2]
  for(int c = 0; c < 4; c++){
    out[c*4+0] = pfx * V[c*4+0];
    out[c*4+1] = pfy * V[c*4+1];
    out[c*4+2] = za * V[c*4+2] + zb * V[c*4+3];
    out[c*4+3] = -V[c*4+2];
  }
}

// ---------------- Ferne Grasflaeche: Top-Down-Einfang der echten Halme ----------------
// Ab ~25m ist ein Halm nur noch 1-2 px breit (w_px 2,3 bei 20m, 1,67 bei 40m,
// 1,04 bei 110m). Seine MSAA-Abdeckung flackert beim Kameramarsch -> das
// AA-Rauschen. Ab diesem Band uebernimmt eine Textur, die von der Engine selbst
// eingefangen wird: Ortho von oben auf ein echtes Grasstueck, als ALBEDO (ohne
// Beleuchtung/Fog/Tonemap, damit das Terrain sie normal beleuchtet). Kostet
// Laufzeit, aber keine Datei-Bytes.
#define CAP_TEX   1024      // Kantenlaenge der Einfangtextur [px]
#define CAP_M     16.0f     // Kantenlaenge des eingefangenen Feldes [m] -> 64 px/m
#define REGION_M  48.0f     // neuer Einfang pro Region
// Aufnahmewinkel. Ein Top-Down-Foto der senkrecht stehenden Halme ist als
// Ferntextur unbrauchbar: gemessen nur 7% Bildabdeckung bei 145 Halmen/m2,
// weil von oben fast nur die Spitzen zu sehen sind. Die Ferne sieht aber die
// SEITEN, also wird schraeg aufgenommen. Der echte Blickwinkel auf fernen Grund
// ist winzig (Kamera 2m ueber Grund, 40m entfernt = 3 Grad); eine stark
// foerzierte Aufnahme waere aber ein 16 x 1,4m-Schmalband und kachelt sichtbar.
// 22 Grad ist der Kompromiss aus seitigem Anteil und kachelbarer Form.
static float capElev = 22.0f * 0.01745329f;
static GLuint grassTex = 0, capFbo = 0, capDepth = 0;
static float  capBox[2] = {0.0f, 0.0f};   // Welt-XZ der Unterencke
static float  capAmt = 0.0f;
static int    capRegion = -2147483647;
static int    grassTexOn = 1;
static float  grassTexFar0 = 70.0f, grassTexFar1 = 115.0f;
static float  reedDens = 1.0f, reedWide = 1.0f;   // DEMO_REED="<dens> <wide>"
static float  reedH = 1.0f;                        // DEMO_REEDH=<f>: Schilf-Hoehe 
// Uniform-Orte des Gras-Programms (auch fuer den Einfang-Pass noetig)
typedef struct { GLuint prog; GLint mvp, cam, time, height, maxdist, fadedur,
                 grp, cat, angS0, angQ, cap, farfade, albOnly,
                 reedDens, reedWide; } GrassProg;
static GrassProg gpGrass;
// Schraege Ortho-Kamera: Blick in +X mit Erhoehungswinkel e ueber dem Grund.
// Bild-x <- Welt-Z, Bild-y <- Grundrichtung +X (um sin e foerziert), Tiefe
// entlang der Blickrichtung. Das Grundstueck CAP_M x CAP_M m fuellt genau das
// Bild, weil die y-Ausdehnung CAP_M*sin e betraegt.
static void ortho_oblique(float px, float pz, float h0, float e, float out[16]){
  float sE = sinf(e), cE = cosf(e);
  float s  = 2.0f / CAP_M;
  float sv = 2.0f / (CAP_M * sE);
  float d  = 1.0f / (CAP_M * 1.5f);
  out[0]=0.0f;  out[4]=0.0f;  out[8]=-s;        out[12]= pz * s;
  out[1]=sE*sv; out[5]=cE*sv; out[9]=0.0f;      out[13]=-(sE*px + cE*h0)*sv;
  out[2]=cE*d;  out[6]=-sE*d; out[10]=0.0f;     out[14]=(-cE*px + sE*h0)*d;
  out[3]=0.0f;  out[7]=0.0f;  out[11]=0.0f;     out[15]=1.0f;
}
static void grass_capture(float cx, float cz){
  if(!capFbo || !gpGrass.prog || nGrass < 1) return;
  // Nur auf echter Grasflaeche einfangen (kein Wasser, kein Steilhang).
  float px = cx, pz = cz; int found = 0;
  for(int ring = 0; ring < 8 && !found; ring++)
    for(int k = 0; k < 10 && !found; k++){
      float a = k * 0.6283185f + ring * 0.37f, r = ring * 5.0f;
      float tx = cx + cosf(a) * r, tz = cz + sinf(a) * r;
      float h = c_terrainH(tx, tz);
      if(h < WATER_Y + 0.20f || h > 7.0f) continue;
      if(fabsf(c_terrainH(tx + 0.6f, tz) - h) > 0.3f ||
         fabsf(c_terrainH(tx, tz + 0.6f) - h) > 0.3f) continue;
      px = tx; pz = tz; found = 1;
    }
  float x0 = px - CAP_M * 0.5f, z0 = pz - CAP_M * 0.5f;
  float h0 = c_terrainH(px, pz);
  // UV-Anker: u=0 bei Welt-Z = pz + CAP_M/2 (Bild-x ist -Z), v=0 bei Welt-X = x0
  capBox[0] = pz + CAP_M * 0.5f; capBox[1] = x0;
  // Hintergrund = Terrain-Grasfarbe genau dort: die Lücken zwischen den Halmen
  // sollen spaeter genauso aussehen wie der Untergrund.
  float g  = c_fbm(px * 0.9f, pz * 0.9f);
  float cr = (0.18f + 0.19f * g) * 0.87f;
  float cg = (0.46f + 0.20f * g) * 0.93f;
  float cb = (0.09f + 0.03f * g) * 0.89f;
  float mvp[16]; ortho_oblique(px, pz, h0, capElev, mvp);
  p_glBindFramebuffer(GL_FRAMEBUFFER, capFbo);
  p_glViewport(0, 0, CAP_TEX, CAP_TEX);
  p_glClearColor(cr, cg, cb, 1.0f);
  p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  p_glEnable(GL_DEPTH_TEST); p_glDepthFunc(GL_LEQUAL); p_glDepthMask(GL_TRUE);
  p_glDisable(GL_BLEND); p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  p_glUseProgram(gpGrass.prog);
  p_glUniformMatrix4fv(gpGrass.mvp, 1, GL_FALSE, mvp);
  p_glUniform3f(gpGrass.cam, px, 30.0f, pz);
  p_glUniform1f(gpGrass.time, 0.0f);
  p_glUniform1f(gpGrass.height, grassHeightScale);
  p_glUniform1f(gpGrass.maxdist, 1e9f);
  p_glUniform1f(gpGrass.fadedur, grassFadeDur);
  p_glUniform2fv(gpGrass.grp, GRASS_NGRP, grassGrp);
  p_glUniform1f(gpGrass.cat, 0.0f);
  p_glUniform1f(gpGrass.angS0, 0.0f);
  p_glUniform1f(gpGrass.angQ, 0.0f);
  p_glUniform1f(gpGrass.cap, 1.0f);
  p_glUniform1f(gpGrass.farfade, 1e9f);
  p_glUniform1f(gpGrass.albOnly, 1.0f);
  p_glBindVertexArray(vaoGrass);
  p_glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 5, (GLuint)nGrass, 0);
  p_glBindVertexArray(0);
  // Mips erzeugen: zwingend mit grassTex GEBUNDEN und ohne gebundenes FBO.
  // Der Aufruf wirkt auf die aktuell gebundene Textur - vorher war das die
  // Depth-Textur aus ensure_fbo(), grassTex bekam NIE Mipmaps und war damit
  // mipmap-incomplett (MIN_FILTER ist LINEAR_MIPMAP_LINEAR). Spec 8.13.1:
  // incomplette Textur -> Sample liefert (0,0,0,1), also SCHWARZ. Mesa ist in
  // dem Pfad nachsichtig, NVIDIA nicht -> auf dem Host war die Ferntextur schwarz.
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  p_glBindTexture(GL_TEXTURE_2D, grassTex);
  p_glGenerateMipmap(GL_TEXTURE_2D);
  capAmt = 1.0f;
  {
    int inPatch = 0;
    for (int i = 0; i < nGrass; i++){
      float bx = grassInst[i * GRASS_FLOATS + 0], bz = grassInst[i * GRASS_FLOATS + 2];
      if (bx >= x0 && bx < x0 + CAP_M && bz >= z0 && bz < z0 + CAP_M) inPatch++;
    }
    printf("Gras-Einfang: Zentrum (%.1f,%.1f) Terrain %.2f m, gefunden=%d, "
           "Halme im Feld %d von %d (%.0f Halme/m2)\n",
           px, pz, c_terrainH(px, pz), found, inPatch, nGrass,
           (float)inPatch / (CAP_M * CAP_M));
  }
}


// ================= HUD-Text =================
// HUD-Groesse skaliert mit der Bildhoehe (4K = 3x, Minimum 720p)
static float hud_scale(int h) {
  float s = (float)h / 720.0f;
  if (s < 1.0f) s = 1.0f;
  return s;
}
static int build_text_verts(float *v, const char *s, int w, int h, float px0, float py0) {
  int n = 0;
  float sc = hud_scale(h);
  float px = px0, py = py0, cs = 16.0f * sc;
  static const float pts[6][2] = { {0,0},{1,0},{0,1},{1,0},{1,1},{0,1} };
  for (const char *c = s; *c; c++) {
    char ch = *c;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Font hat nur Grossbuchstaben
    const char *f = strchr(FONT_STR, ch);
    if (f) {
      int gi = (int)(f - FONT_STR);
      if (n >= HUD_MAX_CHARS * 6) break;   // Bounds-Check: kein Buffer-Overflow
      for (int i = 0; i < 6; i++) {
        float fx = pts[i][0], fy = pts[i][1];
        v[n*4+0] = (px + fx*cs) / w * 2.0f - 1.0f;
        v[n*4+1] = 1.0f - (py + fy*cs) / h * 2.0f;
        v[n*4+2] = (gi*8 + fx*8 + 0.5f) / ATLAS_W;
        v[n*4+3] = (fy*8 + 0.5f) / ATLAS_H;
        n++;
      }
    }
    px += cs;
  }
  return n;
}

// Screenshot via glReadPixels -> PPM (P6). glReadPixels liest von unten links,
// daher vertikaler Flip.
static void save_screenshot_ppm(const char *path, int w, int h) {
  if (w < 1 || h < 1) return;   // ungueltige Groesse (Fenster minimiert?)
  // GL_RGBA statt GL_RGB: Mesa llvmpipe 26.0.8 schreibt mit GL_RGB bei
  // bestimmten Groessen (w mod 4)*(h-1) EXTRA Bytes ueber das Buffer-Ende
  // (Heap-Overflow, Crash "malloc(): invalid size"). RGBA ist praezise.
  unsigned char *buf = malloc((size_t)w * h * 4);
  if (!buf) return;
  // WICHTIG: ReadBuffer VOR ReadPixels setzen (war vorher nachher — Bug!)
  p_glReadBuffer(GL_BACK);
  // Race-Condition-Fix: glFinish() warten bis alle GPU-Threads fertig sind,
  //  sonst crasht glReadPixels (Mesa-Threads rendern noch)
  p_glFinish();
  p_glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
  if (p_glGetError() != GL_NO_ERROR) {
    free(buf);
    return;
  }
  // Alpha entfernen (RGBA -> RGB, in-place von vorne: 3i+2 < 4i ist sicher)
  for (int i = 0; i < w * h; i++) {
    buf[i*3+0] = buf[i*4+0];
    buf[i*3+1] = buf[i*4+1];
    buf[i*3+2] = buf[i*4+2];
  }
  for (int y = 0; y < h / 2; y++) {
    int y2 = h - 1 - y;
    for (int x = 0; x < w; x++) {
      size_t i = (size_t)(y * w + x) * 3, j = (size_t)(y2 * w + x) * 3;
      for (int c = 0; c < 3; c++) { unsigned char t = buf[i+c]; buf[i+c] = buf[j+c]; buf[j+c] = t; }
    }
  }
  FILE *f = fopen(path, "wb");
  if (f) {
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(buf, 1, (size_t)w * h * 3, f);
    fclose(f);
    printf("Screenshot: %s\n", path);
  }
  free(buf);
}
// Debug: den eingefangenen Gras-Ausschnitt als PPM sichern (verifiziert, dass
// der Top-Down-Pass wirklich Halme enthaelt und nicht nur den Klarfarbton).
static void save_grass_tex_ppm(const char *path){
  if(!capFbo) return;
  unsigned char *buf = malloc((size_t)CAP_TEX * CAP_TEX * 4);
  if(!buf) return;
  p_glBindFramebuffer(GL_FRAMEBUFFER, capFbo);
  p_glReadBuffer(GL_COLOR_ATTACHMENT0);
  p_glFinish();
  p_glReadPixels(0, 0, CAP_TEX, CAP_TEX, GL_RGBA, GL_UNSIGNED_BYTE, buf);
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if(p_glGetError() != GL_NO_ERROR){ free(buf); return; }
  for (int i = 0; i < CAP_TEX * CAP_TEX; i++){
    buf[i*3+0] = buf[i*4+0]; buf[i*3+1] = buf[i*4+1]; buf[i*3+2] = buf[i*4+2];
  }
  FILE *f = fopen(path, "wb");
  if(f){
    fprintf(f, "P6\n%d %d\n255\n", CAP_TEX, CAP_TEX);
    fwrite(buf, 1, (size_t)CAP_TEX * CAP_TEX * 3, f);
    fclose(f);
    printf("Gras-Einfang: %s\n", path);
  }
  free(buf);
}

// Slider-Setzen (1=Dichte, 2=Hoehe, 3=Sichtweite), pos = 0..1
static void set_slider(int which, float pos){
  if (pos < 0.0f) pos = 0.0f; if (pos > 1.0f) pos = 1.0f;
  if (which == 1) grassDensityScale = 0.2f + pos * 5.4f;   // max 5.6x (staubt bei MD=120 am 5M-CAP)
  else if (which == 2) grassHeightScale = 0.3f + pos * 2.7f;
  else grassMaxDist = 40.0f + pos * 200.0f;
}

// Slider-Rendering (3 Slider: Dichte/Hoehe/Sichtweite). VBO muss gebunden
// sein. Hintergrund- und Regler-Quads werden je in EINEM Draw gezeichnet
// (gleiche Tints), der Fuellstand pro Farbe (3 Draws).
static void draw_sliders(float posD, float posH, float posM, int w, int h, int locTint, float fade){
  if (fade <= 0.004f) return;   // vollstendig ausgeblendet: gar nicht zeichnen
  const float hsc = hud_scale(h);
  const float slW = 300.0f * hsc, slH = 18.0f * hsc;
  const float x0 = w/2.0f - slW/2.0f, x1 = w/2.0f + slW/2.0f;
  float buv[2] = { (ATLAS_W - 0.5f) / ATLAS_W, (ATLAS_H - 0.5f) / ATLAS_H };
  float y0[3] = { h - 44.0f * hsc, h - 72.0f * hsc, h - 100.0f * hsc };
  float pos[3] = { posD, posH, posM };
  float fill[3][3] = { {0.3f,0.8f,0.3f}, {0.5f,0.8f,0.3f}, {0.3f,0.6f,0.8f} };
  float bg[48], hd[48];
  for (int i = 0; i < 3; i++) {
    if (pos[i] < 0.0f) pos[i] = 0.0f; if (pos[i] > 1.0f) pos[i] = 1.0f;
    float hx = x0 + pos[i] * slW, y1 = y0[i] + slH;
    bg[i*16+0]=x0/w*2-1;   bg[i*16+1]=1-y0[i]/h*2; bg[i*16+2]=buv[0]; bg[i*16+3]=buv[1];
    bg[i*16+4]=x1/w*2-1;   bg[i*16+5]=1-y0[i]/h*2; bg[i*16+6]=buv[0]; bg[i*16+7]=buv[1];
    bg[i*16+8]=x0/w*2-1;   bg[i*16+9]=1-y1/h*2;   bg[i*16+10]=buv[0]; bg[i*16+11]=buv[1];
    bg[i*16+12]=x1/w*2-1;  bg[i*16+13]=1-y1/h*2;  bg[i*16+14]=buv[0]; bg[i*16+15]=buv[1];
    hd[i*16+0]=(hx-2*hsc)/w*2-1; hd[i*16+1]=1-y0[i]/h*2; hd[i*16+2]=buv[0]; hd[i*16+3]=buv[1];
    hd[i*16+4]=(hx+2*hsc)/w*2-1; hd[i*16+5]=1-y0[i]/h*2; hd[i*16+6]=buv[0]; hd[i*16+7]=buv[1];
    hd[i*16+8]=(hx-2*hsc)/w*2-1; hd[i*16+9]=1-y1/h*2;   hd[i*16+10]=buv[0]; hd[i*16+11]=buv[1];
    hd[i*16+12]=(hx+2*hsc)/w*2-1;hd[i*16+13]=1-y1/h*2;  hd[i*16+14]=buv[0]; hd[i*16+15]=buv[1];
  }
  // Tracks und Regler: JE Quad als eigener 4-Vertex-Strip. (Ein gemeinsamer
  // 12-Vertex-Strip wuerde die Quads mit Verbindungsdreiecken verbinden:
  // dunn graue/weiße Linien zwischen den Slidern.)
  p_glUniform4f(locTint, 0.75f, 0.75f, 0.75f, 0.85f * fade);   // hellgrauer Untergrund
  for (int i = 0; i < 3; i++) {
    p_glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), &bg[i * 16], GL_DYNAMIC_DRAW);
    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  for (int i = 0; i < 3; i++) {
    float hx = x0 + pos[i] * slW, y1 = y0[i] + slH;
    float f[16] = {
      x0/w*2-1, 1-y0[i]/h*2, buv[0], buv[1],
      hx/w*2-1, 1-y0[i]/h*2, buv[0], buv[1],
      x0/w*2-1, 1-y1/h*2,   buv[0], buv[1],
      hx/w*2-1, 1-y1/h*2,   buv[0], buv[1],
    };
    p_glBufferData(GL_ARRAY_BUFFER, sizeof f, f, GL_DYNAMIC_DRAW);
    p_glUniform4f(locTint, fill[i][0], fill[i][1], fill[i][2], 0.9f * fade);
    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  p_glUniform4f(locTint, 1.0f, 1.0f, 1.0f, fade);
  for (int i = 0; i < 3; i++) {
    p_glBufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), &hd[i * 16], GL_DYNAMIC_DRAW);
    p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
}

// Kamera direkt vor den naechsten Baum (Debug: Rinde / Blattwerk pruefen)
static void tree_jump(void){
  float tp[3], ts = 2.0f, hf = 1.5f;
  if (!tree_find_nearest(flyPos[0], flyPos[2], tp, &ts, &hf)){
    printf("DEBUG: kein Baum in der Nahe\n"); return;
  }
  float dx = flyPos[0] - tp[0], dz = flyPos[2] - tp[2];
  float l = sqrtf(dx * dx + dz * dz);
  if(l < 0.1f){ dx = 1.0f; dz = 0.0f; l = 1.0f; }
  // Rahmung auf die KRONE: sie sitzt bei by + s*1.5*hf*1.15 (hf = individueller
  // Groessenfaktor des Baums), also je nach Baum 3..12 m ueber der Basis.
  float crownY = tp[1] + ts * 1.5f * hf * 1.15f;
  float dist = 1.6f + ts * hf * 0.75f;
  // DEMO_TREEDIST=<m>: Abstand erzwungen (Ganzbaum-Ansicht fuer Messungen)
  const char *edist = getenv("DEMO_TREEDIST");
  if (edist) dist = (float)atof(edist);
  float camY = crownY - ts * hf * 0.35f;
  if (edist){
    // Messbank: Richtung waehlen, in der kein Ruecken die Sicht auf die Krone
    // verdeckt. Beobachtung: bei 80 m lag der Baum hinter einem Huegel, die
    // Messung zaehlte daraufhin NULL Geruest-Pixel — eine Messung, die nichts
    // ueber das LOD aussagte.
    float best = 1e9f; int bi = 0;
    for(int a = 0; a < 16; a++){
      float ang = (float)a * 6.2832f / 16.0f;
      float ex = sinf(ang), ez = cosf(ang), worst = -1e9f;
      for(int k = 2; k < 24; k++){
        float t = dist * (float)k / 24.0f;
        float sight = crownY + (camY - crownY) * (t / dist);
        float g = c_terrainH(tp[0] + ex * t, tp[2] + ez * t) - sight;
        if(g > worst) worst = g;
      }
      if(worst < best){ best = worst; bi = a; }
    }
    float ang = (float)bi * 6.2832f / 16.0f;
    dx = sinf(ang); dz = cosf(ang); l = 1.0f;
    printf("DEBUG Messbank: Richtung %.0f grad, hoechster Hindernis-Ueberschuss %.2f m\n",
           ang * 57.29578f, best);
  }
  flyPos[0] = tp[0] + dx / l * dist;
  flyPos[1] = camY;
  flyPos[2] = tp[2] + dz / l * dist;
  flyYaw = atan2f(tp[0] - flyPos[0], tp[2] - flyPos[2]);
  float dy = crownY - flyPos[1];
  flyPitch = atan2f(dy, sqrtf(dist * dist - 0.0f));
  // gemerkter Fokusbaum: DEMO_ONETREE baut genau diesen (nicht den naechsten am
  // Build-Zentrum), damit Abstandsmessungen denselben Baum zeigen.
  treeFocus[0] = tp[0]; treeFocus[1] = tp[1]; treeFocus[2] = tp[2]; treeFocusValid = 1;
  printf("DEBUG Baum-Sprung: (%.1f, %.1f, %.1f) Groesse %.2f hf %.2f Krone %.1f m, Abstand %.2f m\n",
         tp[0], tp[1], tp[2], ts, hf, crownY, dist);
}

int main(int argc, char **argv) {
  setvbuf(stdout, 0, _IONBF, 0);
  int winW = 1280, winH = 720;
  if (argc >= 3) { winW = atoi(argv[1]); winH = atoi(argv[2]); }
  if (argc >= 4 && (strcmp(argv[3], "wire") == 0 || strcmp(argv[3], "-wire") == 0))
    showTreeWire = 1;
  if (winW < 320) winW = 320;
  if (winH < 240) winH = 240;

  load_libs();

  Display *disp = p_XOpenDisplay(0);
  if (!disp) { fprintf(stderr, "Kein X-Display (DISPLAY=%s)\n", getenv("DISPLAY") ? getenv("DISPLAY") : ""); return 1; }
  Window root = p_XDefaultRootWindow(disp);

  // --- GLX: FBConfig + glXCreateContextAttribsARB (GLX 1.4, normgerecht) ---
  GLXFBConfig fbconfig = 0;
  XVisualInfo *vi = 0;
  {
    static const int fbattrs[] = {
      GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
      GLX_DOUBLEBUFFER, True,
      GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
      GLX_DEPTH_SIZE, 24,
      None
    };
    int nfb = 0;
    GLXFBConfig *fbs = p_glXChooseFBConfig(disp, 0, fbattrs, &nfb);
    fprintf(stderr, "[glx] glXChooseFBConfig: %d FBConfigs\n", nfb);
    if (fbs && nfb > 0) {
      fbconfig = fbs[0];
      vi = p_glXGetVisualFromFBConfig(disp, fbconfig);
      // fbs bleibt allocated (einmaliges kleines Leak, 64k-Budget)
    }
  }
  // Fallback: glXChooseVisual (klassische API, nur GL 2.1-Kontext)
  if (!vi) {
    fprintf(stderr, "[glx] Fallback glXChooseVisual...\n");
    static const int cvattrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_DEPTH_SIZE, 24, None };
    vi = p_glXChooseVisual(disp, 0, cvattrs);
    fbconfig = 0;
  }
  if (!vi) { fprintf(stderr, "Kein passendes GLX-Visual\n"); return 1; }
  fprintf(stderr, "[glx] Visual: id=0x%lx depth=%d  FBConfig=%s\n",
          (unsigned long)vi->visualid, vi->depth, fbconfig ? "ja (GLX 1.4 Kontext)" : "nein (GLX 1.3 Fallback)");

  unsigned long evmask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask | StructureNotifyMask;
  // X-Fenster MIT dem GLX-Visual. CWColormap ist Pflicht (sonst CopyFromParent
  // -> BadMatch, wenn das Visual vom Root-Visual abweicht).
  Colormap cmap = p_XCreateColormap(disp, root, vi->visual, AllocNone);
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof swa);
  swa.colormap = cmap;
  swa.border_pixel = 0;
  swa.event_mask = evmask;
  Window win = p_XCreateWindow(disp, root, 0, 0, winW, winH, 0,
                               vi->depth, InputOutput,
                               vi->visual,
                               CWColormap | CWBorderPixel | CWEventMask,
                               &swa);
  p_XSync(disp, 0);
  static Atom protos[1];
  protos[0] = p_XInternAtom(disp, "WM_DELETE_WINDOW", 0);
  p_XSetWMProtocols(disp, win, protos, 1);
  Atom wmDelete = protos[0];

  p_XMapWindow(disp, win);
  p_XSync(disp, 0);

  // Kontext: GLX 1.4 (glXCreateContextAttribsARB) fuer GL 4.3 (glBlitNamedFramebuffer),
  // Fallback auf 3.3, dann GLX 1.3.
  GLXContext ctx = 0;
  if (fbconfig) {
    static const int ctx43[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB, 4, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
      None
    };
    static const int ctx33[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 3,
      None
    };
    ctx = p_glXCreateContextAttribsARB(disp, fbconfig, 0, True, ctx43);
    if (!ctx) ctx = p_glXCreateContextAttribsARB(disp, fbconfig, 0, True, ctx33);
    if (!ctx) ctx = p_glXCreateContextAttribsARB(disp, fbconfig, 0, True, (int[]){ None });
  }
  if (!ctx) ctx = p_glXCreateContext(disp, vi, 0, True); // GLX 1.3 Fallback
  if (!ctx) { fprintf(stderr, "GLX-Kontext fehlgeschlagen\n"); return 1; }
  if (!p_glXMakeCurrent(disp, win, ctx)) { fprintf(stderr, "glXMakeCurrent fehlgeschlagen\n"); return 1; }
  p_XSync(disp, 0);

  const char *glver = (const char *)p_glGetString(GL_VERSION);
  printf("64kDemo - OpenGL: %s\n", glver ? glver : "?");
  // Pointer-Lock aktivieren (Maus-Look im Fly-Modus)
  if (p_XLockPointer) {
    p_XLockPointer(win);
    if (p_XWarpPointer) p_XWarpPointer(disp, None, win, 0, 0, 0, 0, winW / 2, winH / 2);
  }
  printf("FLY: Maus=Schauen WASD=Fliegen Space/X=Hoch/Runter Shift=Turbo O=Orbit R=Reset 1/2/3=Skala T=Wire P=Shot G=GrasFrost V=NurGras M=MSAA H=HUD Q/Esc=Ende\n");
  printf("DEBUG: C=Farbcodierung(gelb aus/cyan ein) J=DepthWrite K=DichteKante L=Fade-Dauer strecken\n");

  // ---------- GL-Setup ----------
  GLuint progScene = make_program(VS_SRC, FS_SRC, "scene");
  GLuint progBlit  = make_program(VS_SRC, BLIT_FS_SRC, "blit");
  GLuint progHud   = make_program(HUD_VS_SRC, HUD_FS_SRC, "hud");
  GLuint progWire  = make_program(TREELINES_VS_SRC, TREELINES_FS_SRC, "wire");
  GLuint progTMesh = make_program(TREE_VS_SRC, TREE_FS_SRC, "tmesh");
  GLuint progGrass = make_program(GRASS_VS_SRC, GRASS_FS_SRC, "grass");
  GLuint progCrown = make_program(CROWN_VS_SRC, CROWN_FS_SRC, "crown");
  GLuint progLeaf  = make_program(LEAF_VS_SRC, LEAF_FS_SRC, "leaf");

  GLint locRes    = p_glGetUniformLocation(progScene, "uRes");
  GLint locTime   = p_glGetUniformLocation(progScene, "uTime");
  GLint locCamPos = p_glGetUniformLocation(progScene, "uCamPos");
  GLint locCamTgt = p_glGetUniformLocation(progScene, "uCamTgt");
  GLint locBlitTex = p_glGetUniformLocation(progBlit, "uTex");
  GLint locHudFont = p_glGetUniformLocation(progHud, "uFont");
  GLint locTint    = p_glGetUniformLocation(progHud, "uTint");
  GLint locWireMVP = p_glGetUniformLocation(progWire, "uMVP");
  GLint locWireCam = p_glGetUniformLocation(progWire, "uCamPos");
  GLint locWireCol = p_glGetUniformLocation(progWire, "uColor");
  GLint locTMVP    = p_glGetUniformLocation(progTMesh, "uMVP");
  GLint locTCam    = p_glGetUniformLocation(progTMesh, "uCamPos");
  GLint locTTime   = p_glGetUniformLocation(progTMesh, "uTime");
  GLint locTMask   = p_glGetUniformLocation(progTMesh, "uTreeMask");
  GLint locTFade   = p_glGetUniformLocation(progTMesh, "uTreeFade");
  GLint locGMVP    = p_glGetUniformLocation(progGrass, "uMVP");
  GLint locGCam    = p_glGetUniformLocation(progGrass, "uCamPos");
  GLint locGTime   = p_glGetUniformLocation(progGrass, "uTime");
  GLint locGHeight = p_glGetUniformLocation(progGrass, "uHeightScale");
  GLint locGMaxDist = p_glGetUniformLocation(progGrass, "uMaxDist");
  GLint locGFade = p_glGetUniformLocation(progGrass, "uFadeDur");
  GLint locGrp = p_glGetUniformLocation(progGrass, "uFadeGrp");
  GLint locGCat = p_glGetUniformLocation(progGrass, "uDbgCat");
  GLint locGAngS0 = p_glGetUniformLocation(progGrass, "uAngS0");
  GLint locGAngQ  = p_glGetUniformLocation(progGrass, "uAngQ");
  GLint locGCap   = p_glGetUniformLocation(progGrass, "uCapture");
  GLint locGReedD = p_glGetUniformLocation(progGrass, "uReedDens");
  GLint locGReedW = p_glGetUniformLocation(progGrass, "uReedWide");
  GLint locGReedH = p_glGetUniformLocation(progGrass, "uReedH");
  GLint locGFarFade = p_glGetUniformLocation(progGrass, "uFarFade");
  GLint locGAlbOnly = p_glGetUniformLocation(progGrass, "uAlbedoOnly");
  gpGrass.prog = progGrass; gpGrass.mvp = locGMVP; gpGrass.cam = locGCam;
  gpGrass.time = locGTime; gpGrass.height = locGHeight; gpGrass.maxdist = locGMaxDist;
  gpGrass.fadedur = locGFade; gpGrass.grp = locGrp; gpGrass.cat = locGCat;
  gpGrass.angS0 = locGAngS0; gpGrass.angQ = locGAngQ; gpGrass.cap = locGCap;
  gpGrass.farfade = locGFarFade; gpGrass.albOnly = locGAlbOnly;
  gpGrass.reedDens = locGReedD; gpGrass.reedWide = locGReedW;
  // Einfang-Textur + FBO (Laufzeit, keine Datei-Bytes)
  p_glGenTextures(1, &grassTex);
  p_glBindTexture(GL_TEXTURE_2D, grassTex);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, CAP_TEX, CAP_TEX, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  p_glGenFramebuffers(1, &capFbo);
  p_glBindFramebuffer(GL_FRAMEBUFFER, capFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grassTex, 0);
  p_glGenRenderbuffers(1, &capDepth);
  p_glBindRenderbuffer(GL_RENDERBUFFER, capDepth);
  p_glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, CAP_TEX, CAP_TEX);
  p_glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, capDepth);
  if(p_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    printf("WARN: Gras-Einfang-FBO unvollstaendig: status=0x%x GL-Err=0x%x "
           "tex=%u fbo=%u depth=%u\n",
           p_glCheckFramebufferStatus(GL_FRAMEBUFFER), p_glGetError(),
           grassTex, capFbo, capDepth);
  p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
  p_glBindRenderbuffer(GL_RENDERBUFFER, 0);
  GLint locGTex    = p_glGetUniformLocation(progScene, "uGrassTex");
  GLint locGBox    = p_glGetUniformLocation(progScene, "uGrassBox");
  GLint locGSide   = p_glGetUniformLocation(progScene, "uGrassSide");
  GLint locGTexAmt = p_glGetUniformLocation(progScene, "uGrassTexAmt");
  GLint locGFar0   = p_glGetUniformLocation(progScene, "uGrassFar0");
  GLint locGFar1   = p_glGetUniformLocation(progScene, "uGrassFar1");
  GLint locCMVP    = p_glGetUniformLocation(progCrown, "uMVP");
  GLint locCCam    = p_glGetUniformLocation(progCrown, "uCamPos");
  GLint locCTgt    = p_glGetUniformLocation(progCrown, "uCamTgt");
  GLint locCFol    = p_glGetUniformLocation(progCrown, "uFoliage");
  GLint locCFol2   = p_glGetUniformLocation(progCrown, "uCrownTex");
  GLint locCTime   = p_glGetUniformLocation(progCrown, "uTime");
  GLint locCMask   = p_glGetUniformLocation(progCrown, "uCrownMask");
  GLint locCFade   = p_glGetUniformLocation(progCrown, "uTreeFade");
  GLint locCOld    = p_glGetUniformLocation(progCrown, "uTreeOld");
  GLint locCLFace  = p_glGetUniformLocation(progCrown, "uLightFace");
  GLint locLMVP    = p_glGetUniformLocation(progLeaf, "uMVP");
  GLint locLCam    = p_glGetUniformLocation(progLeaf, "uCamPos");
  GLint locLLod    = p_glGetUniformLocation(progLeaf, "uLodCam");
  GLint locLTime   = p_glGetUniformLocation(progLeaf, "uTime");
  GLint locLMask   = p_glGetUniformLocation(progLeaf, "uLeafMask");
  GLint locLFade   = p_glGetUniformLocation(progLeaf, "uTreeFade");
  GLint locLOld    = p_glGetUniformLocation(progLeaf, "uTreeOld");
  // uAlbedoOnly steht in der Atlas-Kapture auf 1 und wird pro Programm NICHT
  // zurueckgesetzt — ohne dieses 0 hier lief die komplette Blatt-Rendrierung mit
  // roher Albedo (unbeleuchtet), waehrend die Kronen-Karten beleuchtet wurden.
  GLint locLAlb    = p_glGetUniformLocation(progLeaf, "uAlbedoOnly");

  // Fullscreen-Dreieck
  static const float tri[6] = { -1,-1, 3,-1, -1,3 };
  GLuint vboTri = 0, vaoTri = 0;
  p_glGenBuffers(1, &vboTri);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboTri);
  p_glBufferData(GL_ARRAY_BUFFER, sizeof tri, tri, GL_STATIC_DRAW);
  p_glGenVertexArrays(1, &vaoTri);
  p_glBindVertexArray(vaoTri);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8, (const void *)0);

  // HUD-VBO
  GLuint vboHud = 0, vaoHud = 0;
  p_glGenBuffers(1, &vboHud);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboHud);
  p_glBufferData(GL_ARRAY_BUFFER, HUD_MAX_CHARS * 6 * 4 * sizeof(float), 0, GL_DYNAMIC_DRAW);
  p_glGenVertexArrays(1, &vaoHud);
  p_glBindVertexArray(vaoHud);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (const void *)0);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (const void *)8);

  build_font();
  build_tree_lines();   // VAOs + Grundformen
  envTreeJump = getenv("DEMO_TREE") ? 1 : 0;
  if (getenv("DEMO_TREEMASK")) dbgTreeMask = 1;
  { const char *es = getenv("DEMO_SHOT"); if (es) envShotAt = (float)atof(es); }
  { const char *ef = getenv("DEMO_FREEZE"); if (ef) envFreeze = (float)atof(ef); }
  // DEMO_TREEFADE=<sek>: Ueberblendzeit beim LOD-Satzwechsel (0 = hart schneiden)
  { const char *tf = getenv("DEMO_TREEFADE"); if (tf) treeFadeDur = (double)atof(tf); }
  { const char *ts = getenv("DEMO_TIERSWITCH"); if (ts) treeTierSwitchS = (double)atof(ts); }
  // DEMO_TREEBUILDRANGE: Zellen-Radius des Wald-Baus (9 m pro Zelle) — kleiner
  // Wald = schneller Build, damit die Ueberblendung messbar wird.
  { const char *br = getenv("DEMO_TREEBUILDRANGE"); if (br) treeBuildRange = atoi(br); }
  { const char *td = getenv("DEMO_TREEDEBUG"); if (td) treeDbg = atoi(td); }
  { const char *nb = getenv("DEMO_NOBB0"); if (nb) treeBillboard0 = atoi(nb); }
  { const char *t2 = getenv("DEMO_T2");
    if(t2){ int a = 3, b = 3, c = 4;
      if(sscanf(t2, "%d %d %d", &a, &b, &c) == 3 && a > 0 && c > 0){
        treeT2Stride = a; treeT2Gnum = b; treeT2Gden = c; } } }
  { const char *cp = getenv("DEMO_CARDT0");
    if(cp) { int d = 3; float sc = 1.30f;
      if(sscanf(cp, "%d %f", &d, &sc) == 2){ treeCardDiv0 = d < 1 ? 1 : d; treeCardScale0 = sc; } } }
  { const char *ea = getenv("DEMO_ARTEN"); if (ea) dbgGrassCat = atoi(ea); }
  // DEMO_GRASSMD=<m>  : max. Gras-Sichtweite (statt Slider)
  // DEMO_GRASSTEX="<far0> <far1>" : Uebergabe an die Einfangtextur, "0" = Textur aus
  { const char *eg = getenv("DEMO_GRASSMD"); if (eg) grassMaxDist = (float)atof(eg); }
  // DEMO_ONETREE: ein einzelner Baum vor der Kamera, kein Gras — damit Baum-
  // Arbeit (Feinast, Blattbuschel, LOD-Stufen) schnell und vergleichbar testbar
  // ist. DEMO_TIER=0|2|3 erzwingt die Detailstufe dieses Baums.
  { const char *eo = getenv("DEMO_ONETREE");
    if (eo && atoi(eo)) { treeOneShot = 1; envTreeJump = 1; grassMaxDist = 0.5f; } }
  { const char *et = getenv("DEMO_TIER"); if (et) treeForceTier = atoi(et); }
  { const char *el = getenv("DEMO_TREELOG"); if (el) treeTierLog = atoi(el); }
  { const char *ek = getenv("DEMO_KIDPCT"); if (ek) ezKidPct = atoi(ek); }
  { const char *er = getenv("DEMO_LODRSCALE"); if (er) treeLodScale = (float)atof(er); }
  { const char *eg = getenv("DEMO_TREEGEN"); if (eg) treeGenMode = (strstr(eg,"alt")||strstr(eg,"0")) ? 0 : 1; }
  { const char *es = getenv("DEMO_EZSPREAD"); if (es) treeEzSpread = (float)atof(es); }
  { const char *ep = getenv("DEMO_EZPRESET"); if (ep) treeEzPreset = atoi(ep); }
  // Blatt-Palette (HSV-Grenzen pro Baum): benannte Voreinstellung oder sechs
  // Zahlen. Standard (kompiliert) = "saftig": hue 93..133 Grad, sat 0,60..0,85,
  // val 0,34..0,55. "aktuell" = die Palette vor der Umstellung (hue 72..144 Grad,
  // enthaelt olivige Randwerte — Nutzer: "herbstlich, blass").
  { const char *pal = getenv("DEMO_LEAFPAL");
    if(pal){
      if(strstr(pal, "aktuell")){ ezHueMin = 0.20f; ezHueMax = 0.40f;
        ezSatMin = 0.62f; ezSatMax = 0.95f; ezValMin = 0.24f; ezValMax = 0.46f; }
      else if(strstr(pal, "frisch")){ ezHueMin = 0.24f; ezHueMax = 0.32f;
        ezSatMin = 0.55f; ezSatMax = 0.75f; ezValMin = 0.45f; ezValMax = 0.65f; }
      else if(strstr(pal, "tief")){ ezHueMin = 0.30f; ezHueMax = 0.42f;
        ezSatMin = 0.70f; ezSatMax = 0.92f; ezValMin = 0.26f; ezValMax = 0.40f; }
      else if(strstr(pal, "wald")){ ezHueMin = 0.28f; ezHueMax = 0.36f;
        ezSatMin = 0.65f; ezSatMax = 0.88f; ezValMin = 0.38f; ezValMax = 0.58f; }
    } }
  { const char *hsv = getenv("DEMO_LEAFHSV");
    if(hsv){ float v[6];
      if(sscanf(hsv, "%f %f %f %f %f %f", v, v+1, v+2, v+3, v+4, v+5) == 6){
        ezHueMin = v[0]; ezHueMax = v[1]; ezSatMin = v[2];
        ezSatMax = v[3]; ezValMin = v[4]; ezValMax = v[5];
      } else fprintf(stderr, "DEMO_LEAFHSV erwartet 6 Zahlen: hmin hmax smin smax vmin vmax\n"); } }
  { const char *eh = getenv("DEMO_REEDH"); if (eh) reedH = (float)atof(eh); }
  { const char *er = getenv("DEMO_REED");
    if (er) { reedDens = 1.0f; reedWide = 1.0f;
              sscanf(er, "%f %f", &reedDens, &reedWide); } }
  { const char *et = getenv("DEMO_GRASSTEX");
    if (et) { if (!atoi(et)) grassTexOn = 0;
              else { sscanf(et, "%f %f", &grassTexFar0, &grassTexFar1); } } }
  { const char *em = getenv("DEMO_CROWNMASK"); if (em) envCrownMask = atoi(em); }
  { const char *es = getenv("DEMO_SHADOW"); if (es) shadowOn = atoi(es); }
  { const char *el = getenv("DEMO_SHADOWLEAF"); if (el) shadowLeafOn = atoi(el); }
  { const char *ez = getenv("DEMO_SHADOWSAMPLE"); if (ez) shadowSampleOn = atoi(ez); }
  { const char *ef = getenv("DEMO_SHADOWFORCE"); if (ef) shadowForce = atoi(ef); }
  { const char *ev = getenv("DEMO_SHADOWVIS"); if (ev) shadowVis = atoi(ev) + 1; }
  { const char *ed = getenv("DEMO_SHADOWDUMP"); if (ed) envShadowDump = atoi(ed); }
  { const char *eg = getenv("DEMO_GLDBG"); if (eg) envGlDbg = atoi(eg); }
  { const char *ec = getenv("DEMO_CAM");
    if (ec){ float cx=0, cz=0, cp=-0.30f, cy2=1e30f; if(sscanf(ec, "%f %f %f %f", &cx, &cz, &cp, &cy2) >= 2){
      flyPos[0] = cx; flyPos[2] = cz; flyPos[1] = c_terrainH(cx, cz) + 1.10f;
      flyPitch = cp; if(cy2 < 1e29f) flyYaw = cy2;
      printf("DEBUG Kamera gesetzt: (%.1f, %.2f, %.1f) Pitch %.2f Yaw %.2f\n", cx, flyPos[1], cz, cp, flyYaw); } } }
  envNoLeaf = getenv("DEMO_NOLEAF") ? 1 : 0;
  envNoTerrain = getenv("DEMO_NOTERRAIN") ? 1 : 0;
  envLeafMask = getenv("DEMO_LEAFMASK") ? atoi(getenv("DEMO_LEAFMASK")) : 0;  // 1=rot, 2=nur Albedo
  if (envTreeJump) tree_jump();
  build_foliage_atlas(progLeaf);   // Atlas braucht vboLeafBase (build_tree_lines) + progLeaf
  build_crown_tiles();             // Ganzbaum-Kacheln fuer die groebste Detailstufe
  init_shadow();
  ShadowLoc slScene, slTree, slGrass, slLeaf, slCrown;
  fillShadowLoc(progScene, &slScene); fillShadowLoc(progTMesh, &slTree);
  fillShadowLoc(progGrass, &slGrass); fillShadowLoc(progLeaf,  &slLeaf);
  fillShadowLoc(progCrown, &slCrown);
  printf("DBG Shadow-Loc scene tex=%d mvp=%d on=%d px=%d | tree tex=%d mvp=%d on=%d px=%d\n",
         slScene.tex, slScene.mvp, slScene.on, slScene.px,
         slTree.tex, slTree.mvp, slTree.on, slTree.px);

  // ---------- Hauptloop ----------
  double tPrev = now_sec();
  double loopT0 = tPrev;
  double fpsEMA = 60.0;
  int running = 1;
  int shotDone = 0;
  static float hudVerts[HUD_MAX_CHARS * 6 * 4];

  while (running) {
    double tNow = now_sec();
    // Mit DEMO_FREEZE liegt die Birth-Zeit sonst in der echten Laufzeit, uTime
    // ist aber eingefroren -> vFade bleibt 0 und das Gras verschwindet in jedem
    // Freeze-Test. Ein Freeze soll die Fade-Animation abschalten, deshalb hier
    // eine Birth-Zeit weit in der Vergangenheit (Fade-in fertig, Fade-out fertig).
    gWorldTime = envFreeze >= 0.0f ? -1e6f : (float)tNow;
    double rawDt = tNow - tPrev;
    tPrev = tNow;
    // Erster Frame: rawDt ist nicht echt (tPrev wurde vor der Loop gesetzt).
    //  Ersten ECHTEN Frame-Abstand (2. Frame) als FPS-Basis nehmen.
    static int frameCount = 0;
    frameCount++;
    if (frameCount == 1) {
      // Erster Frame: FPS noch nicht bekannt, fpsEMA bleibt 60 (Initialwert)
    } else if (frameCount == 2) {
      // Zweiter Frame: erster echter Frame-Abstand -> FPS-Basis
      if (rawDt > 0) fpsEMA = 1.0 / rawDt;
    } else {
      // Ab 3. Frame: EMA
      if (rawDt > 0) fpsEMA = fpsEMA * 0.95 + (1.0 / rawDt) * 0.05;
    }
    if (frameCount % 10 == 0) printf("DBG fps %.2f t %.2f\n", fpsEMA, tNow);
    // Frame-Zeit-Statistik (Messbank fuer das Stottern): Roh-Abstaende sammeln
    // und periodisch Median/p95/Max ausgeben. Der fps-EMA allein versteckt einen
    // 500-ms-Einbruch, weil er gluettet.
    {
      static float smp[2048]; static int nsmp = 0; static double lastPrintT = 0;
      if(nsmp < 2048) smp[nsmp++] = (float)(rawDt * 1000.0);
      if(tNow - lastPrintT >= 2.0 && nsmp > 0){
        lastPrintT = tNow;
        static float buf[2048];
        for(int i = 0; i < nsmp; i++) buf[i] = smp[i];
        for(int i = 1; i < nsmp; i++){ float v = buf[i]; int j = i - 1;
          while(j >= 0 && buf[j] > v){ buf[j+1] = buf[j]; j--; } buf[j+1] = v; }
        printf("Frame-Zeiten %d Frames: median %.0f ms  p95 %.0f ms  max %.0f ms\n",
               nsmp, buf[nsmp/2], buf[(int)(nsmp*0.95)], buf[nsmp-1]);
        fflush(stdout);
        nsmp = 0;
      }
    }
    float dt = rawDt > 0.1f ? 0.1f : (float)rawDt;
    // DEMO_WALK="vx vz": Kameraweg mit fester Geschwindigkeit (m/s) — macht
    // Rebuild-Stottern reproduzierbar messbar, ohne Tastatur. Nur camTgt verschieben,
    // camPos wird pro Frame daraus abgeleitet (Orbit-Modus).
    { static float wx = 0, wz = 0; static int wread = 0;
      if(!wread){ wread = 1;
        const char *ew = getenv("DEMO_WALK");
        if(ew) { if(sscanf(ew, "%f %f", &wx, &wz) != 2){ wx = wz = 0; } }
      }
      if(wx != 0.0f || wz != 0.0f){
        camTgt[0] += wx * dt; camTgt[2] += wz * dt;
      } }

    // --- Events (XEvent aus Xlib.h, Tasten via XLookupKeysym) ---
    while (p_XPending(disp)) {
      XEvent e;
      p_XNextEvent(disp, &e);
      switch (e.type) {
        case KeyPress: {
          KeySym ks = p_XLookupKeysym(&e.xkey, 0);
          if (ks == XK_Escape || ks == XK_q) running = 0;
          if (ks == XK_1) scaleIdx = 0;
          if (ks == XK_2) scaleIdx = 1;
          if (ks == XK_3) scaleIdx = 2;
          if (ks == XK_p) { save_screenshot_ppm("/tmp/64kdemo_shot.ppm", winW, winH);
                            save_grass_tex_ppm("/tmp/64kdemo_grasstex.ppm"); }
          if (ks == XK_t) showTreeWire = !showTreeWire;
          if (ks == XK_m) {   // MSAA zyklisch: 4x -> 8x -> off -> 4x
            if (useMSAA && msaaSamples == 4) { msaaSamples = 8; fboW = 0; printf("MSAA: 8x\n"); }
            else if (useMSAA && msaaSamples == 8) { useMSAA = 0; printf("MSAA: OFF\n"); }
            else { useMSAA = 1; msaaSamples = 4; fboW = 0; printf("MSAA: 4x\n"); }
          }
          if (ks == XK_g) { grassFrozen = !grassFrozen; printf("Gras-Rebuild: %s\n", grassFrozen ? "EINGEFROREN (gleiche Halme)" : "aktiv"); }
          if (ks == XK_j) { dbgGrassDepth = !dbgGrassDepth;
            printf("DEBUG Depth-Write Gras-Pass: %s\n", dbgGrassDepth ? "AN" : "AUS"); }
          if (ks == XK_k) { dbgGrassEdge = !dbgGrassEdge;
            grassSetDensCut(dbgGrassEdge ? 0.01f : 0.0f);
            printf("DEBUG Dichte-Kante: %s (neuer Build noetig: G aus/ein oder 1m bewegen)\n",
                   dbgGrassEdge ? "AN (CUT 0.01)" : "AUS (CUT 0)"); }
          if (ks == XK_z) {
            // Uebergabe-Abstand Gras-Geometrie -> Ferntextur
            if (grassTexOn && grassTexFar0 > 20.0f) { grassTexFar0 = 15.0f; grassTexFar1 = 30.0f; }
            else if (grassTexOn) { grassTexOn = 0; }
            else { grassTexOn = 1; grassTexFar0 = 70.0f; grassTexFar1 = 115.0f; capRegion = -2147483647; }
            printf("Ferne Gras-Textur: %s (Uebergabe %.0f..%.0fm)\n",
                   grassTexOn ? "AN" : "AUS", grassTexFar0, grassTexFar1);
          }
          if (ks == XK_u) { dbgAngThin = !dbgAngThin;
            printf("DEBUG Blickwinkel-Duennung: %s (s0=%.2f q=%.2f)\n",
                   dbgAngThin ? "AN" : "AUS", grassAngS0, grassAngQ); }
          if (ks == XK_i) { dbgSortOrder = !dbgSortOrder;
            grassSetSortOrder(dbgSortOrder);
            printf("DEBUG Zeichenreihenfolge: %s (neuer Build noetig: G aus/ein oder 1m bewegen)\n",
                   dbgSortOrder ? "hinten->vorn sortiert" : "Diamond-Order"); }
          if (ks == XK_y) { dbgAlpha2Cov = !dbgAlpha2Cov;
            printf("DEBUG Alpha-to-Coverage: %s (wirkt nur mit MSAA an)\n",
                   dbgAlpha2Cov ? "AN" : "AUS"); }
          if (ks == XK_c) { dbgGrassCat = (dbgGrassCat + 1) % 3;
            printf("DEBUG Gras-Farbmodus: %s\n",
                   dbgGrassCat == 0 ? "AUS"
                   : (dbgGrassCat == 1 ? "FADE (gelb=aus, cyan=ein)"
                     : "ARTEN (gruen Wiese, gelb Hochgras, violett Lavendel, weiss Gaensebl., orange Schilf)")); }
          if (ks == XK_l) {   // Fade-Dauer strecken/stauchen: VM laeuft mit 1-2 FPS,
            // ein 2s-Fade ist dort zeitlich nicht aufloesbar. CPU und Shader
            // nutzen dieselbe Dauer.
            static const float durs[] = { 1.0f, 2.0f, 10.0f, 30.0f };
            static int durIdx = 0;   // Standard = durs[0] = 1s
            durIdx = (durIdx + 1) % (int)(sizeof durs / sizeof durs[0]);
            grassFadeDur = durs[durIdx];
            printf("DEBUG Fade-Dauer: %.0fs\n", grassFadeDur); }
          if (ks == XK_h) {   // HUD zyklisch: Vollstats -> reduziert -> aus
            hudMode = (hudMode + 2) % 3;
            printf("HUD: %s\n", hudMode == 2 ? "Vollstats (2 Zeilen)" : (hudMode == 1 ? "reduziert (1 Zeile)" : "aus"));
          }
          if (ks == XK_v) { grassOnly = !grassOnly; printf("TEST: nur Gras zeichnen: %s\n", grassOnly ? "AN" : "aus"); }
          if (ks == XK_b) grass_debug_dump();
          if (ks == XK_semicolon){ dbgTreeMask = !dbgTreeMask;
            printf("DEBUG Baum-Maske (Rinde): %s\n", dbgTreeMask ? "AN (reines Rot)" : "aus"); }
          if (ks == XK_apostrophe) tree_jump();
          if (ks == XK_n) {   // DEBUG: 3 Punkte: 45m vor, je 20m links/rechts/gerade
            float cnp = cosf(flyPitch);
            float fxn = cnp * sinf(flyYaw), fz2 = cnp * cosf(flyYaw);
            float lx = fz2, lz = -fxn;   // linke Achse
            for (int s = -1; s <= 1; s++) {
              grass_debug_set_point(flyPos[0] + fxn * 45.0f + lx * 20.0f * s,
                                    flyPos[2] + fz2 * 45.0f + lz * 20.0f * s);
              grass_debug_region_dump(0, s + 1);
            }
          }
          if (ks == XK_m) {   // DEBUG: gleiche 3 Punkte erneut (Kamera muss stillstehen)
            float cnp = cosf(flyPitch);
            float fxn = cnp * sinf(flyYaw), fz2 = cnp * cosf(flyYaw);
            float lx = fz2, lz = -fxn;
            for (int s = -1; s <= 1; s++) {
              grass_debug_set_point(flyPos[0] + fxn * 45.0f + lx * 20.0f * s,
                                    flyPos[2] + fz2 * 45.0f + lz * 20.0f * s);
              grass_debug_region_dump(1, s + 1);
            }
          }
          if (ks == XK_o) {
            // Fly <-> Orbit umschalten, ohne Sprung synchronisieren
            if (flyMode) {
              // von Fly nach Orbit: Orbit-Parameter aus Fly-Kamera ableiten
              camTgt[0] = flyPos[0]; camTgt[1] = flyPos[1]; camTgt[2] = flyPos[2];
              camRadius = 20.0f;
              camYaw = flyYaw + 3.14159f;   // Orbit-Kamera steht hinter dem Blick
              camPitch = -flyPitch;
            } else {
              // von Orbit nach Fly: Fly-Kamera an Orbit-Kamera setzen
              float cp = cosf(camPitch), sp2 = sinf(camPitch);
              flyPos[0] = camTgt[0] + camRadius * cp * cosf(camYaw);
              flyPos[1] = camTgt[1] + camRadius * sp2;
              flyPos[2] = camTgt[2] + camRadius * cp * sinf(camYaw);
              flyYaw = camYaw + 3.14159f;
              flyPitch = -camPitch;
            }
            flyMode = !flyMode;
          }
          if (ks == XK_r) {
            flyPos[0]=30; flyPos[1]=20; flyPos[2]=34; flyYaw=-2.42f; flyPitch=-0.32f;
            // Gras mit zuruecksetzen: Build an der Startposition. Altes Gras
            // im Walk-Quadrat wird kopiert/gefadet, der Rest (meist weit
            // ueber dem MD-Cull) ist einfach weg; das Start-Gras (deterministisch)
            // fadet in 2s ein.
            gCX = 30.0f; gCZ = 34.0f;
            grass_build_request(gCX, gCZ, 0);
          }
          if (ks == XK_w) keyW = 1;
          if (ks == XK_a) keyA = 1;
          if (ks == XK_s) keyS = 1;
          if (ks == XK_d) keyD = 1;
          if (ks == XK_e) keyE = 1;
          if (ks == XK_f) keyF = 1;
          if (ks == XK_space) keyUp = 1;
          if (ks == XK_x) keyDown = 1;
          if (ks == XK_Shift_L || ks == XK_Shift_R) keyRun = 1;
          break;
        }
        case KeyRelease: {
          KeySym ks = p_XLookupKeysym(&e.xkey, 0);
          if (ks == XK_w) keyW = 0;
          if (ks == XK_a) keyA = 0;
          if (ks == XK_s) keyS = 0;
          if (ks == XK_d) keyD = 0;
          if (ks == XK_e) keyE = 0;
          if (ks == XK_f) keyF = 0;
          if (ks == XK_space) keyUp = 0;
          if (ks == XK_x) keyDown = 0;
          if (ks == XK_Shift_L || ks == XK_Shift_R) keyRun = 0;
          break;
        }
        case ButtonPress:
          if (e.xbutton.button == 1) {
            mouseHeld = 1;
            // Slider-Treffer? (unten Mitte, 300*hsc breit; y-Offsets tabellarisch)
            float hsc = hud_scale(winH);
            float sw = 300.0f * hsc;
            int sx0 = winW/2 - (int)(sw / 2.0f);
            static const float slY[3][2] = { {-52,-30}, {-80,-58}, {-108,-86} };
            for (int i = 0; i < 3; i++)
              if (e.xbutton.x >= sx0 && e.xbutton.x <= sx0 + sw &&
                  e.xbutton.y >= winH + slY[i][0] * hsc && e.xbutton.y <= winH + slY[i][1] * hsc) {
                sliderDragging = i + 1;
                set_slider(i + 1, (e.xbutton.x - sx0) / sw);
                break;
              }
          }
          else if (e.xbutton.button == 4) camRadius *= 0.9f;
          else if (e.xbutton.button == 5) camRadius *= 1.1f;
          break;
        case ButtonRelease:
          if (e.xbutton.button == 1) { mouseHeld = 0; sliderDragging = 0; }
          break;
        case MotionNotify:
          if (sliderDragging)
            set_slider(sliderDragging,
                       (e.xmotion.x - (winW/2 - (int)(150.0f * hud_scale(winH))))
                       / (300.0f * hud_scale(winH)));
          if (!flyMode && mouseHeld && !sliderDragging) {
            camYaw += (e.xmotion.x - lastMX) * 0.005f;
            camPitch += (e.xmotion.y - lastMY) * 0.005f;
            if (camPitch < 0.05f) camPitch = 0.05f;
            if (camPitch > 1.45f) camPitch = 1.45f;
          }
          lastMX = e.xmotion.x;
          lastMY = e.xmotion.y;
          break;
        case ConfigureNotify:
          if (e.xconfigure.width >= 320 && e.xconfigure.height >= 240) {
            winW = e.xconfigure.width;
            winH = e.xconfigure.height;
          }
          break;
        case ClientMessage:
          if ((Atom)e.xclient.data.l[0] == wmDelete) running = 0;
          break;
      }
    }

    // --- Zeiger-Position (XQueryPointer, jedes Frame) ---
    // Fuer Maus-Look (Fly, relativ) und das Slider-Fade (absolut, Bereich).
    int ptrInWin = 0, ptrX = 0, ptrY = 0, rx = 0, ry = 0;
    if (p_XQueryPointer) {
      // Prototyp aus third_party/include/X11/Xlib.h (9 Parameter — NICHT der
      // Standard-X11-XQueryPointer!): root-/child-Fenster KOMMEN VOR den
      // Koordinaten. ptrX/ptrY = win_x/win_y = FENSTER-Koordinaten.
      Window rootRet, childRet; unsigned int mask;
      ptrInWin = p_XQueryPointer(disp, win, &rootRet, &childRet, &rx, &ry, &ptrX, &ptrY, &mask);
    }

    // --- Slider-Fade: Maus im Bereich -> einblenden (0.5s); 3s ohne
    //     Bereich -> ausblenden (1.5s). Beim Ziehen bleibt sichtbar.
//     Mit gedruckter linker Taste (Kamera-Look) werden sie schneller
//     ausgeblendet (0.5s) — beim Navigieren sind sie im Weg.
    //     (rawDt statt dt: UI-Fade laeuft in Echtzeit, nicht im dt-Clamp)
    {
      float fdt = (float)rawDt;
      float hsc = hud_scale(winH);
      float cs = 16.0f * hsc;
      // Bereich = Tracks + Labels (laengstes Label "Sichtweite" = 10 Zeichen)
      float ax0 = winW / 2.0f - 150.0f * hsc - 10.0f * cs - 8.0f * hsc;
      float ax1 = winW / 2.0f + 150.0f * hsc;
      float ay0 = winH - 100.0f * hsc;
      float ay1 = winH - 26.0f * hsc;
      int inArea = ptrInWin && ptrX >= ax0 && ptrX <= ax1 && ptrY >= ay0 && ay1 >= ptrY;
      // Mit gedruckter linker Taste wird die Kamera bewegt (Look) — dort storen
      // die Slider nur, also sofort ausblenden. Beim Ziehen EINES SLIDERS bleiben
      // sie naturlich sichtbar.
      int navigating = mouseHeld && !sliderDragging;
      if (!navigating && (sliderDragging || inArea)) {
        if (inArea) lastMouseInAreaT = tNow;
        if (sliderFade < 1.0f) {
          sliderFade += fdt * 2.0f;   // 0.5s zum Einblenden
          if (sliderFade > 1.0f) sliderFade = 1.0f;
        }
      } else if (navigating || tNow - lastMouseInAreaT >= 3.0f) {
        sliderFade -= fdt * (navigating ? 2.0f : (1.0f / 1.5f));   // Look: 0.5s, sonst 1.5s
        if (sliderFade < 0.0f) sliderFade = 0.0f;
      }
    }

    // --- Maus-Look (relativ, nur bei gedrückter linker Taste) ---
    // Delta zur letzten Position (kein Drift, wenn die Maus stillsteht).
    {
      static int lastPX = -1, lastPY = -1;
      if (flyMode && ptrInWin) {
        if (lastPX < 0) { lastPX = ptrX; lastPY = ptrY; }
        int dx = ptrX - lastPX, dy = ptrY - lastPY;
        lastPX = ptrX; lastPY = ptrY;
        // Wrap-Artefakte am Fenster-Edge vermeiden
        if (dx >  winW / 2) dx -= winW;
        if (dx < -winW / 2) dx += winW;
        if (dy >  winH / 2) dy -= winH;
        if (dy < -winH / 2) dy += winH;
        if (mouseHeld && !sliderDragging && (dx != 0 || dy != 0)) {
          flyYaw   += dx * 0.0008f;
          flyPitch += dy * 0.0008f;   // invertiert: Maus hoch -> Blick runter (Klick-Nav)
          if (flyPitch >  1.45f) flyPitch =  1.45f;   // +83°
          if (flyPitch < -1.45f) flyPitch = -1.45f;   // -83°
        }
      }
    }

    // --- Kamera ---
    float camPos[3];
    if (flyMode) {
      // frei schweben: WASD + Space/X bewegen, Shift = Turbo
      float cp = cosf(flyPitch), fy = sinf(flyPitch);
      float fx = cp * sinf(flyYaw), fz = cp * cosf(flyYaw);
      float rx = -fz, rz = fx;                 // rechte Achse (wie Orbit-Konvention)
      float sp = (keyRun ? 34.0f : 13.0f) * dt;
      if (keyW) { flyPos[0] += fx * sp; flyPos[1] += fy * sp; flyPos[2] += fz * sp; }
      if (keyS) { flyPos[0] -= fx * sp; flyPos[1] -= fy * sp; flyPos[2] -= fz * sp; }
      if (keyD) { flyPos[0] += rx * sp; flyPos[2] += rz * sp; }
      if (keyA) { flyPos[0] -= rx * sp; flyPos[2] -= rz * sp; }
      if (keyUp)   flyPos[1] += sp;
      if (keyDown) flyPos[1] -= sp;
      if (flyPos[1] < 0.5f)  flyPos[1] = 0.5f;
      if (flyPos[1] > 150.0f) flyPos[1] = 150.0f;
      camPos[0] = flyPos[0]; camPos[1] = flyPos[1]; camPos[2] = flyPos[2];
      camTgt[0] = flyPos[0] + fx * 10.0f;
      camTgt[1] = flyPos[1] + fy * 10.0f;
      camTgt[2] = flyPos[2] + fz * 10.0f;
    } else {
      // Orbit-Modus (wie vorher)
      if (orbit) camYaw += dt * 0.02f;
      float sp = camRadius * 0.6f * dt;
      float mfx = -cosf(camYaw), mfz = -sinf(camYaw);
      float rfx = -mfz, rfz = mfx;
      if (keyW) { camTgt[0] += mfx * sp; camTgt[2] += mfz * sp; }
      if (keyS) { camTgt[0] -= mfx * sp; camTgt[2] -= mfz * sp; }
      if (keyD) { camTgt[0] += rfx * sp; camTgt[2] += rfz * sp; }
      if (keyA) { camTgt[0] -= rfx * sp; camTgt[2] -= rfz * sp; }
      if (keyE) camTgt[1] += sp;
      if (keyF) camTgt[1] -= sp;
      if (camTgt[1] < 0.0f) camTgt[1] = 0.0f;
      if (camTgt[1] > 40.0f) camTgt[1] = 40.0f;
      if (camRadius < 8.0f) camRadius = 8.0f;
      if (camRadius > 160.0f) camRadius = 160.0f;
      float cp = cosf(camPitch), sp2 = sinf(camPitch);
      camPos[0] = camTgt[0] + camRadius * cp * cosf(camYaw);
      camPos[1] = camTgt[1] + camRadius * sp2;
      camPos[2] = camTgt[2] + camRadius * cp * sinf(camYaw);
    }

    // --- Rebuild bei Kamera-Bewegung ---
    // Baeume: Schwellwert 12m (Flaeche +/-144m, kein Pop-in). Der Build laeuft
    // im Worker-Thread (Runde 26): vorher blockierten Generieren (350 ms) und
    // Upload (140 ms) den Frame = der gemeldete Aussetzer alle ~12 m.
    // Gras: inkrementell (Zweipuffer, ~8000 Zellen/Frame), Schwellwert 12m.
    {
      static int builtT = 0, builtG = 0;
      static float tCX = 0, tCZ = 0;
      // DEMO_TIERSWITCH=<sek>: nach n Sekunden hart auf tier 0 umstellen und einen
      // Build ausloesen — Messbank fuer die Ueberblendung (alter Satz tier 3,
      // neuer Satz tier 0, Kameraposition identisch).
      { static double t0 = 0; static int done = 0;
        if(treeTierSwitchS > 0 && !done){
          if(t0 == 0) t0 = tNow;
          if(tNow - t0 >= treeTierSwitchS){ done = 1; treeForceTier = 0; tCX = 1e30f;
            printf("  Messbank: erzwungener Stufenwechsel auf tier 0 (t=%.2f s)\n", tNow - t0);
            fflush(stdout); }
        } }
      if(!builtT || ((!upActive && !trees_build_busy()) &&
                     (fabsf(camTgt[0] - tCX) > 12.0f || fabsf(camTgt[2] - tCZ) > 12.0f))){
        tCX = camTgt[0]; tCZ = camTgt[2];
        if(!builtT){
          // Erster Build synchron (Baeume ab Frame 1 sichtbar), danach Thread.
          treeLodRef[0] = camPos[0]; treeLodRef[1] = camPos[1]; treeLodRef[2] = camPos[2];
          treeLodRefValid = 1;
          double tB0 = now_sec();
          trees_build(tCX, tCZ);
          double tB1 = now_sec();
          upload_begin(treeVertsD, crownVertsD, leafInstD,
                       nTreeVertsD, nCrownVertsD, nLeafInstD, 1 - drawSet);
          while(upload_step(1e9)){ }
          double tB2 = now_sec();
          printf("  Baum-Erstbuild: Generieren %.0f ms + Upload %.0f ms = %.0f ms  (%.0f MB Puffer)\n",
                 (tB1 - tB0) * 1000.0, (tB2 - tB1) * 1000.0, (tB2 - tB0) * 1000.0,
                 ((double)nTreeVertsD * TREE_FLOATS + (double)nCrownVertsD * CROWN_FLOATS
                  + (double)nLeafInstD * LEAF_FLOATS) * 4.0 / 1e6);
          builtT = 1;
          trees_thread_start();
        } else {
          trees_build_request(tCX, tCZ, camPos);   // Worker baut im Hintergrund
        }
        fflush(stdout);
        printf("Baeume um (%.0f,%.0f): %d Baeume, %d Vertices, %d Canopy-Quads, %d Einzelblaetter\n", tCX, tCZ, nTreesD, nTreeVertsD, nCrownVertsD, nLeafInstD);
        printf("  Caps: Geruest %d/%d  Karten %d/%d  Blaetter %d/%d   TREffer: Geruest %d Karten %d Blaetter %d\n",
               nTreeVertsD, TREE_VERT_CAP, nCrownVertsD, CROWN_CAP, nLeafInstD, LEAF_CAP,
               treeCapTree, treeCapCrown, treeCapLeaf);
        if(treeCapSprig) printf("  Laub-Liste pro Baum voll: %d\n", treeCapSprig);
        printf("  uebersprungen (Budget): %d Baeume, heruntergestuft auf groebste Stufe: %d\n",
               treeSkipped, treeDowngraded);
        treeCapTree = treeCapCrown = treeCapLeaf = treeSkipped = treeDowngraded = 0;
        // Messbank: Welt-BBox des Baumgeruests in Metern (Kronendurchmesser,
        // Wuchshoehe, Stammradius unten) — ohne Bildskala-Raten zu muessen.
        if(treeOneShot && nTreeVertsD > 0){
          float mn[3] = {1e9f,1e9f,1e9f}, mx[3] = {-1e9f,-1e9f,-1e9f};
          for(int i = 0; i < nTreeVertsD; i++)
            for(int k = 0; k < 3; k++){
              float v = treeVertsD[i * TREE_FLOATS + k];
              if(v < mn[k]) mn[k] = v;
              if(v > mx[k]) mx[k] = v;
            }
          printf("  BBox: Breite %.2f m  Hoehe %.2f m  Tiefe %.2f m  (Boden y=%.2f, Scheitel y=%.2f)\n",
                 mx[0] - mn[0], mx[1] - mn[1], mx[2] - mn[2], mn[1], mx[1]);
          // Laub-/Karten-Hoehenverteilung relativ zur Wuchshoehe — Vergleich mit
          // der EZ-Tree-Referenz (node /tmp/ezcmp: Laub beginnt dort bei 24% der
          // Baumhoehe, Stammanteil 55%).
          { float lmn = 1e9f, lmx = -1e9f, cmn = 1e9f, cmx = -1e9f;
            for(int i = 0; i < nLeafInstD; i++){
              float v = leafInstD[i * LEAF_FLOATS + 1];
              if(v < lmn) lmn = v;
              if(v > lmx) lmx = v;
            }
            for(int i = 0; i < nCrownVertsD; i++){
              float v = crownVertsD[i * CROWN_FLOATS + 1];
              if(v < cmn) cmn = v;
              if(v > cmx) cmx = v;
            }
            float H = mx[1] - mn[1];
            printf("  Laub  y %.2f..%.2f m  = %.0f%%..%.0f%% der Baumhoehe  (%d Karten y %.2f..%.2f)\n",
                   lmn, lmx, 100*(lmn - mn[1])/H, 100*(lmx - mn[1])/H,
                   nCrownVertsD, cmn, cmx);
          }
          printf("  Preset %s  unit %.4f  Stammradius %.3f m  Spread %.2f  s=%.2f hf=%.2f\n",
                 ezDbgName, ezDbgUnit, ezDbgR0, treeEzSpread, ezDbgS, ezDbgHf);
          { printf("  Stammradien (m):");
            for (int i = 0; i <= ezDbgNSec && i < 13; i++) printf(" %.3f", ezDbgR[i]);
            printf("\n"); }
        }
      }
      // Fertigen Worker-Build abholen und in Scheiben hochladen. Der Wechsel der
      // Zeichenseite (drawSet) passiert erst nach dem letzten Slice — die GPU
      // sieht nie halbfertige Geometrie.
      if(!upActive){
        float *tv, *cv, *lv; int ntv, ncv, nli, ntr;
        trees_build_finished_snapshot(&tv, &cv, &lv, &ntv, &ncv, &nli, &ntr);
        if(tv){
          upload_begin(tv, cv, lv, ntv, ncv, nli, 1 - drawSet);
          printf("  Baum-Build (Thread) uebernommen: %d Baeume, %.0f MB — Upload in Scheiben (%u MB/Frame)\n",
                 ntr,
                 ((double)ntv * TREE_FLOATS + (double)ncv * CROWN_FLOATS
                  + (double)nli * LEAF_FLOATS) * 4.0 / 1e6, UP_BYTES >> 20);
          fflush(stdout);
        }
      }
      { double u0 = now_sec(); upload_step(rawDt * 1000.0); double u1 = now_sec();
        static double upMax = 0.0, upSum = 0.0; static int upN = 0;
        double ms = (u1 - u0) * 1000.0;
        if(ms > 0.5){ upSum += ms; upN++; if(ms > upMax) upMax = ms; }
        if(upN){ static double lastUp = 0;
          if(tNow - lastUp >= 2.0){
            printf("  Baum-Upload: %d Slices, letzter %.0f ms, max %.0f ms, Mittel %.0f ms\n",
                   upN, ms, upMax, upSum / upN);
            fflush(stdout); lastUp = tNow; upSum = 0; upN = 0; upMax = 0;
          } } }
      // Blickrichtung (XZ-projiziert) fuer die sortierte Walk-Ordnung setzen —
      // vor jedem Build, damit der Build die aktuelle Richtung benutzt.
      { float vx = camTgt[0] - camPos[0], vz = camTgt[2] - camPos[2];
        float vl = sqrtf(vx * vx + vz * vz);
        if (vl < 1e-3f) { vx = 1.0f; vz = 0.0f; } else { vx /= vl; vz /= vl; }
        grassSetViewDir(vx, vz); }
      if(!builtG){
        // Erster Build: synchron (Gras ab dem ersten Frame sichtbar).
        // Danach laeuft der Generator im eigenen Thread (grass_build_request).
        // Zentrum = camPos (nicht camTgt): Bei Kameradrehung bleibt das Gras
        //  "da" (kein Neubuild). Build-Radius = grassMaxDist + 20m Margin.
        gCX = camPos[0]; gCZ = camPos[2];
        grass_build_begin(gCX, gCZ, 2);   // Modus 2 (IN): alles neu, blendet beim Start ein
        while(!grass_build_step(16000)) {}
        grass_build_finish();
        p_glBindBuffer(GL_ARRAY_BUFFER, vboGrass);
        p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nGrass * GRASS_FLOATS * sizeof(float), grassInst, GL_DYNAMIC_DRAW);
        builtG = 1;
        grass_thread_start();
      } else if(!grassFrozen){
        // Rebuild-Trigger:
        //  a) Positionswechsel >1m -> POS-Build (Modus 0): Zell-Kopieren,
        //     neue Halme faden 2s ein, ueberzaehlige (nb gesunken) faden 2s aus.
        //  b) Dichte/Sichtweite-Slider: Debounce (Wert muss 1s stabil sein),
        //     dann 2-Phasen-Transition: OUT (Modus 1: altes Gras fadet 2s
        //     aus) -> IN (Modus 2: neues Gras fadet 2s ein). Ein kompletter
        //     Slider-Zug = genau eine Transition (frueher: jeder Drag-Schritt
        //     trieb einen eigenen Rebuild -> Gras verschwand/blendete mehrfach).
        if (grassDensityScale != lastDensity || grassMaxDist != lastMaxDist) {
          lastDensity = grassDensityScale;
          lastMaxDist = grassMaxDist;
          lastSliderT = tNow;
          sliderDirty = 1;
        }
        int sliderPending = sliderDirty && (tNow - lastSliderT < 1.0f);
        if (sliderPhase == 0 && sliderDirty && !sliderPending) {
          // 1s Ruhe -> OUT-Build (altes Gras blendet aus; laeuft ueber die
          //  alte Zell-Tabelle, Zentrum ist egal).
          grass_build_request(gCX, gCZ, 1);
          sliderPhase = 1;
          sliderDirty = 0;
        } else if (sliderPhase == 2 && tNow - sliderPhaseT >= grassFadeDur) {
          // OUT-Fade zu Ende -> IN-Build (neues Gras blendet ein), Zentrum
          //  = aktuelle Kamera.
          gCX = camPos[0]; gCZ = camPos[2];
          grass_build_request(gCX, gCZ, 2);
          sliderPhase = 3;
        }
        if (sliderPhase == 0 && !sliderPending &&
            (fabsf(camPos[0] - gCX) > 1.0f || fabsf(camPos[2] - gCZ) > 1.0f)) {
          gCX = camPos[0]; gCZ = camPos[2];
          grass_build_request(gCX, gCZ, 0);
        }
      }
      // Fertigen Build vom Worker-Thread abholen (VBO-Upload im Main-Thread,
      // GL-Kontext laeuft dort). Race-Fix: glFinish() NACH dem Upload (GPU
      // liest beim Wechsel noch den alten VBO).
      {
        float *gbuf; int gn;
        grass_build_finished_snapshot(&gbuf, &gn);
        if (gbuf) {
          p_glBindBuffer(GL_ARRAY_BUFFER, vboGrass);
          p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)gn * GRASS_FLOATS * sizeof(float), gbuf, GL_DYNAMIC_DRAW);
          p_glFinish();
          // Slider-Transition: OUT-Build fertig -> 2s warten (Fade-out),
          // IN-Build fertig -> fertig.
          if (sliderPhase == 1) { sliderPhase = 2; sliderPhaseT = tNow; }
          else if (sliderPhase == 3) sliderPhase = 0;
        }
      }
    }

    // --- Render ---
    ensure_fbo(winW, winH);
    float mvp[16];
    build_mvp(camPos, camTgt, fboW, fboH, mvp);
    // Regionwechsel -> neues Grasstueck einfangen. Bindet sein eigenes FBO und
    // laesst das Haupt-FBO danach unangetastet (der naechste Pass bindet eh neu).
    if(grassTexOn && nGrass > 0){
      int reg = (int)floorf(camPos[0] / REGION_M) * 46337
              + (int)floorf(camPos[2] / REGION_M);
      if(reg != capRegion){ capRegion = reg; grass_capture(camPos[0], camPos[2]); }
    }

    // 1) Szene-Pass (Raymarch: Himmel/Terrain/Wasser) ERST: Farbe +
    //    Projektionstiefe (gl_FragDepth) ins FBO. Meshes werden danach mit
    //    Depth-Test dagegen gerendert -> MSAA-Kanten mischen Mesh+Landschaft
    //    (nicht mehr Mesh+schwarzer Clear-Hintergrund).
    p_glBindFramebuffer(GL_FRAMEBUFFER, useMSAA ? fboMS : fbo);
    p_glViewport(0, 0, fboW, fboH);
    // TEST: Magenta-Hintergrund bei Gras-nur-Modus (Halme klar unterscheidbar)
    if (grassOnly) p_glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
    else p_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // WICHTIG: Depth-Test AN + LEQUAL. llvmpipe (Mesa 26.0.8) schreibt gl_FragDepth
    // nur, wenn der Depth-Test aktiv ist (Spec: soll immer geschrieben werden).
    // LEQUAL, damit Himmel (FragDepth=1.0) gegen gecleartes 1.0 durchkommt.
    p_glEnable(GL_DEPTH_TEST);
    p_glDepthFunc(GL_LEQUAL);
    // ---------- Schatten-Wurf: alles aus Sicht der Sonne ----------
    // Farbmaske aus -> es landet nur Tiefe in shadowTex. Die echten Programme
    // werden wiederverwendet, weil ihre discard()s (Blattsilhouette, Atlas-Alpha)
    // den Schatten richtig loechern. uShadowOn ist in diesem Durchlauf AUS,
    // sonst wurde die eigene Tiefen-Textur waehrend des Schreibens gelesen.
    int shadowThis = shadowOn && !grassOnly && shadowFbo != 0;
    if (shadowThis){
      float sl2 = sqrtf(0.55f*0.55f + 0.38f*0.38f + 0.62f*0.62f);
      float S[3] = { 0.55f/sl2, 0.38f/sl2, -0.62f/sl2 };   // wie SUN in allen Shadern
      float ctr[3] = { camPos[0], camPos[1], camPos[2] };
      float D = 1.4f * SHADOW_HALF;
      float eye[3] = { ctr[0] - S[0]*D, ctr[1] - S[1]*D, ctr[2] - S[2]*D };
      ortho_mvp(eye, ctr, SHADOW_HALF, lightMVP);
      p_glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
      p_glViewport(0, 0, SHADOW_SZ, SHADOW_SZ);
      p_glClear(GL_DEPTH_BUFFER_BIT);
      p_glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      p_glEnable(GL_DEPTH_TEST); p_glDepthFunc(GL_LESS);
      p_glDepthMask(GL_TRUE);
      p_glDisable(GL_BLEND);
      p_glEnable(GL_POLYGON_OFFSET_FILL); p_glPolygonOffset(1.5f, 2.0f);
      float tSh = envFreeze >= 0.0f ? envFreeze : (float)tNow;
      p_glUseProgram(progTMesh);
      p_glUniformMatrix4fv(locTMVP, 1, GL_FALSE, lightMVP);
      p_glUniform3f(locTCam, eye[0], eye[1], eye[2]);
      p_glUniform1f(locTTime, tSh);
      p_glUniform1f(locTMask, 0.0f);
      applyShadow(&slTree, 0);
      if(locTFade >= 0) p_glUniform1f(locTFade, 1.0f);
      p_glBindVertexArray(vaoTree[drawSet]);
      p_glDrawArrays(GL_TRIANGLES, 0, nTreeVertsCur);
      if(nCrownVertsCur > 0){
        p_glUseProgram(progCrown);
        p_glUniformMatrix4fv(locCMVP, 1, GL_FALSE, lightMVP);
        p_glUniform3f(locCCam, eye[0], eye[1], eye[2]);
        p_glUniform3f(locCTgt, ctr[0], ctr[1], ctr[2]);
        p_glActiveTexture(GL_TEXTURE0); p_glBindTexture(GL_TEXTURE_2D, folTex);
        p_glUniform1i(locCFol, 0);
        p_glActiveTexture(GL_TEXTURE1); p_glBindTexture(GL_TEXTURE_2D, crnTex);
        if(locCFol2 >= 0) p_glUniform1i(locCFol2, 1);
        p_glActiveTexture(GL_TEXTURE0);
        p_glUniform1f(locCTime, tSh);
        p_glUniform1f(locCMask, 0.0f);
        // Im Schatten-Wurf muessen die Kronen-Quads SENKRECHT zur Sonnenrichtung
        // stehen. Kamera-zugewandte Billboards stehen bei einer Sonne mit 24,6°
        // Erhebung fast kantig -> sie werfen nur schmale Streifen statt einer
        // Kronenflaeche.
        if(locCLFace >= 0) p_glUniform1f(locCLFace, 1.0f);
        applyShadow(&slCrown, 0);
        if(locCFade >= 0) p_glUniform1f(locCFade, 1.0f);
        p_glBindVertexArray(vaoCrown[drawSet]);
        p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nCrownVertsCur);
      }
      if(shadowLeafOn && nLeafInstCur > 0 && !envNoLeaf){
        p_glUseProgram(progLeaf);
        p_glUniformMatrix4fv(locLMVP, 1, GL_FALSE, lightMVP);
        p_glUniform3f(locLCam, eye[0], eye[1], eye[2]);
        // LOD-Auscull der Blaetter haengt an der KAMERA ab, nicht am Wurf-
        // betrachter: mit uCamPos = Sonnenauge (63 m weg) waren alle 288800
        // Blaetter weggecullt und haben EXAKT 0 Texel zur Shadow-Map
        // beigetragen (gemessen: Map mit/ohne Blatt-Wurf bitgleich).
        if(locLLod >= 0) p_glUniform3f(locLLod, camPos[0], camPos[1], camPos[2]);
        p_glUniform1f(locLTime, tSh);
        p_glUniform1f(locLMask, 0.0f);
        if(locLAlb >= 0) p_glUniform1f(locLAlb, 0.0f);
        applyShadow(&slLeaf, 0);
        if(locLFade >= 0) p_glUniform1f(locLFade, 1.0f);
        p_glBindVertexArray(vaoLeaf[drawSet]);
        p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nLeafInstCur);
      }
      p_glDisable(GL_POLYGON_OFFSET_FILL);
      p_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      if(envShadowDump){
        unsigned short *db = (unsigned short *)malloc((size_t)SHADOW_SZ * SHADOW_SZ * 2);
        p_glBindTexture(GL_TEXTURE_2D, shadowTex);
        p_glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, db);
        FILE *fp = fopen("/tmp/64kdemo_shadow.ppm", "wb");
        fprintf(fp, "P5\n%d %d\n65535\n", SHADOW_SZ, SHADOW_SZ);
        for(int i = SHADOW_SZ*SHADOW_SZ - 1; i >= 0; i--){
          unsigned char v[2] = { (unsigned char)(db[i]>>8), (unsigned char)(db[i]&255) };
          fwrite(v, 1, 2, fp);
        }
        fclose(fp); free(db);
        printf("DBG Shadow-Map gedumpt -> /tmp/64kdemo_shadow.ppm (err=0x%x)\n", p_glGetError());
      }
      // Zurueck zum Szenen-FBO — NICHT zu Framebuffer 0: das Demo rendert in
      // fbo/fboMS und blittet danach. Wer hier 0 bindet, zeichnet die Szene ins
      // Nirwana und sieht ein schwarzes Bild.
      p_glBindFramebuffer(GL_FRAMEBUFFER, useMSAA ? fboMS : fbo);
      p_glViewport(0, 0, fboW, fboH);
      // Depth-Funktion zurueck auf LEQUAL: der Szenen-Pass zwingend braucht, weil
      // der Himmel gl_FragDepth = 1.0 gegen den geclearten Depth-Wert 1.0
      // bestehen muss. Mit GL_LESS aus dem Schatten-Wurf faellt jeder Himmel-
      // Fragment durch -> schwarzer Himmel bei sonst intaktem Bild.
      p_glDepthFunc(GL_LEQUAL);
      if(envGlDbg) printf("DBG nach Shadow-Pass: err=0x%x fb=0x%x vp=%d,%d\n",
                          p_glGetError(), p_glCheckFramebufferStatus(GL_FRAMEBUFFER), fboW, fboH);
    }
    if (!grassOnly && !envNoTerrain) {
      p_glUseProgram(progScene);
      p_glBindVertexArray(vaoTri);
      p_glUniform2f(locRes, (float)fboW, (float)fboH);
      p_glUniform1f(locTime, envFreeze >= 0.0f ? envFreeze : (float)tNow);
      p_glUniform3f(locCamPos, camPos[0], camPos[1], camPos[2]);
      p_glUniform3f(locCamTgt, camTgt[0], camTgt[1], camTgt[2]);
      p_glActiveTexture(GL_TEXTURE0);
      p_glBindTexture(GL_TEXTURE_2D, grassTex);
      p_glUniform1i(locGTex, 0);
      p_glUniform2f(locGBox, capBox[0], capBox[1]);
      p_glUniform1f(locGSide, CAP_M);
      p_glUniform1f(locGTexAmt, (grassTexOn && capAmt > 0.0f) ? 1.0f : 0.0f);
      p_glUniform1f(locGFar0, grassTexFar0);
      p_glUniform1f(locGFar1, grassTexFar1);
      applyShadow(&slScene, (shadowThis && shadowSampleOn) ? (shadowVis ? 2 + shadowVis : (shadowForce ? 2 : 1)) : 0);
      if(envGlDbg) printf("DBG vor Szenen-Draw: err=0x%x tex3=%d\n", p_glGetError(),
                          (int)(slScene.tex >= 0));
      p_glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // Ueberblend-Faktor des neuen Satzes (Runde 27). Der alte Satz laeuft mit
    // 1-treeFadeNew parallel mit, bis die Ueberblendzeit um ist.
    float treeFadeNew = 1.0f;
    if(treeDbg && treeFadeDur > 0.0 && tNow - treeSwapT < treeFadeDur + 0.05){
      static double lastFadeP = -1;
      if(tNow - lastFadeP > 0.25){
        lastFadeP = tNow;
        printf("  Ueberblendung: neu %.2f (Satz %d), alt %.2f (Satz %d)\n",
               (float)((tNow - treeSwapT) / treeFadeDur), drawSet,
               1.0f - (float)((tNow - treeSwapT) / treeFadeDur), treeOldSet);
        fflush(stdout);
      }
    }
    if(treeFadeDur > 0.0){
      treeFadeNew = (float)((tNow - treeSwapT) / treeFadeDur);
      if(treeFadeNew > 1.0f) treeFadeNew = 1.0f;
      if(treeFadeNew < 0.0f) treeFadeNew = 0.0f;
    }

    // 2) Mesh-Pass (Baeume + Gras + Blattdach) mit Depth-Test gegen Szenen-Tiefe
    p_glEnable(GL_DEPTH_TEST);
    p_glDepthFunc(GL_LESS);

    // Baum-Mesh (Stamm/Aeste)
    if (!grassOnly) {
      p_glUseProgram(progTMesh);
      p_glUniformMatrix4fv(locTMVP, 1, GL_FALSE, mvp);
      p_glUniform3f(locTCam, camPos[0], camPos[1], camPos[2]);
      p_glUniform1f(locTTime, envFreeze >= 0.0f ? envFreeze : (float)tNow);
      p_glUniform1f(locTMask, (float)dbgTreeMask);
      applyShadow(&slTree, (shadowThis && shadowSampleOn) ? (shadowForce ? 2 : 1) : 0);
      if(locTFade >= 0) p_glUniform1f(locTFade, 1.0f);
      p_glBindVertexArray(vaoTree[drawSet]);
      p_glDrawArrays(GL_TRIANGLES, 0, nTreeVertsCur);
      if(treeFadeNew < 1.0f && treeOldSet >= 0 && nTreeVertsOld > 0){
        if(locTFade >= 0) p_glUniform1f(locTFade, 1.0f - treeFadeNew);
        p_glBindVertexArray(vaoTree[treeOldSet]);
        p_glDrawArrays(GL_TRIANGLES, 0, nTreeVertsOld);
        if(locTFade >= 0) p_glUniform1f(locTFade, 1.0f);
      }
    }

    // Gras (instanziert) — von WEIT nach NAH zeichnen (Alpha-Blending und
    // MSAA brauchen Raster-Reihenfolge von hinten nach vorne). Die VBO ist
    // in Diamond-Order sortiert (Instance 0 = naechste Zelle), also in
    // Chunks vom Buffer-Ende her (keine Index-Buffer-Sortierung pro Frame).
    p_glUseProgram(progGrass);
    p_glUniformMatrix4fv(locGMVP, 1, GL_FALSE, mvp);
    p_glUniform3f(locGCam, camPos[0], camPos[1], camPos[2]);
    p_glUniform1f(locGTime, envFreeze >= 0.0f ? envFreeze : (float)tNow);
    p_glUniform1f(locGHeight, grassHeightScale);
    p_glUniform1f(locGMaxDist, grassMaxDist);
    p_glUniform1f(locGFade, grassFadeDur);
    // Fade-Gruppen (birth,death) — GRASS_NGRP*8 Byte, pro Frame. Aendert sich
    // nur am Swap, der Upload ist 4 KB und damit gegen 37 MB Halmpuffer frei.
    p_glUniform2fv(locGrp, GRASS_NGRP, grassGrp);
    p_glUniform1f(locGCat, (float)dbgGrassCat);
    p_glUniform1f(locGAngS0, dbgAngThin ? grassAngS0 : 0.0f);
    p_glUniform1f(locGAngQ, grassAngQ);
    p_glUniform1f(locGCap, 0.0f);
    if(locGReedD >= 0) p_glUniform1f(locGReedD, reedDens);
    if(locGReedW >= 0) p_glUniform1f(locGReedW, reedWide);
    if(locGReedH >= 0) p_glUniform1f(locGReedH, reedH);
    p_glUniform1f(locGAlbOnly, 0.0f);
    // Halme duennen genau dort aus, wo die Textur uebernimmt (gleiches Band,
    // damit sich Uebergabe und Cull nicht gegenseitig wiedersprechen).
    p_glUniform1f(locGFarFade, (grassTexOn && capAmt > 0.0f) ? grassTexFar1 : grassMaxDist);
    // Transparenz des Grases: ZWEI sich ausschliessende Wege.
    //  A) Alpha-to-Coverage: Alpha wird in eine MSAA-Sample-Maske uebersetzt.
    //     Die Abdeckung stellt die Transparenz her -> KEIN Blending, sonst
    //     wird doppelt gedmpft (Alpha 0,5 bei 4x = 2 abgedeckte Samples, und
    //     auf denen wird nochmal mit 0,5 gemischt -> 0,25 statt 0,5).
    //  B) Alpha-Blending (SRC_ALPHA): der alte Weg. Reihenfolge-abhangig, und
    //     halbtransparente Halme schreiben Tiefe fuer ALLE Samples -> sie
    //     verdecken eingespielte Halme dahinter (Reveal-Pop beim Cull).
    p_glBindVertexArray(vaoGrass);
    if (dbgAlpha2Cov) {
      p_glDisable(GL_BLEND);
      p_glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    } else {
      p_glEnable(GL_BLEND);
      p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    }
    // DEBUG-Zweig: Depth-Write abschaltbar (J). Mit Depth-Write verdeckt ein
    // naeherer Halm jeden ferneren, der innerhalb des gleichen Chunks spaeter
    // gezeichnet wird; die Chunk-Reihenfolge ist nur grob sortiert (~50% der
    // Nachbarpaare innerhalb eines Chunks liegen falsch, bis 24m). Aendert sich
    // am Swap die Sortierung, werden verdeckte Halme schlagartig sichtbar.
    p_glDepthMask(dbgGrassDepth ? GL_TRUE : GL_FALSE);
    if(!envNoTerrain){
      int chunk = (nGrass + 31) / 32;   // 32 Chunks (der Fade-Crescent liegt
                                         // in den letzten 1-2 Chunks -> genuegend fein)
      if (grassSortedOrderActive()) {
        // Puffer ist fern->nah sortiert (Index 0 = fernste Zelle) -> von 0 aufwaerts
        for (int off = 0; off < nGrass; off += chunk) {
          int c = (nGrass - off) > chunk ? chunk : (nGrass - off);
          p_glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 5, c, (GLuint)off);
        }
      } else {
      for (int off = nGrass; off > 0; off -= chunk) {
        int c = off > chunk ? chunk : off;
        p_glDrawArraysInstancedBaseInstance(GL_TRIANGLE_STRIP, 0, 5, c, (GLuint)(off - c));
      }
      }
    }
    p_glDepthMask(GL_TRUE);
    p_glDisable(GL_BLEND);
    p_glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);   // nicht auf spaetere Passe durchschlagen

    // Blattdach-Billboards: Alpha-Test (discard) -> entweder opak oder unsichtbar,
    // deshalb Depth SCHREIBEN (damit naechste Mesh-Objekte dagegen testen koennen)
    if (!grassOnly) {
      p_glEnable(GL_BLEND);
      p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      p_glDepthMask(GL_TRUE);
      p_glUseProgram(progCrown);
      p_glUniformMatrix4fv(locCMVP, 1, GL_FALSE, mvp);
      p_glUniform3f(locCCam, camPos[0], camPos[1], camPos[2]);
      p_glUniform3f(locCTgt, camTgt[0], camTgt[1], camTgt[2]);
      p_glActiveTexture(GL_TEXTURE0);
      p_glBindTexture(GL_TEXTURE_2D, folTex);
      p_glUniform1i(locCFol, 0);
      p_glActiveTexture(GL_TEXTURE1);
      p_glBindTexture(GL_TEXTURE_2D, crnTex);
      if(locCFol2 >= 0) p_glUniform1i(locCFol2, 1);
      p_glActiveTexture(GL_TEXTURE0);
      p_glUniform1f(locCMask, (float)envCrownMask);
      if(locCLFace >= 0) p_glUniform1f(locCLFace, 0.0f);
      p_glUniform1f(locCTime, envFreeze >= 0.0f ? envFreeze : (float)tNow);
      if(locCFade >= 0) p_glUniform1f(locCFade, treeFadeNew);
      p_glBindVertexArray(vaoCrown[drawSet]);
      p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nCrownVertsCur);
      if(treeFadeNew < 1.0f && treeOldSet >= 0 && nCrownVertsOld > 0){
        if(locCFade >= 0) p_glUniform1f(locCFade, 1.0f - treeFadeNew);
        if(locCOld >= 0) p_glUniform1f(locCOld, 1.0f);
        p_glBindVertexArray(vaoCrown[treeOldSet]);
        p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nCrownVertsOld);
        if(locCOld >= 0) p_glUniform1f(locCOld, 0.0f);
      }
      if(locCFade >= 0) p_glUniform1f(locCFade, 1.0f);
      p_glDisable(GL_BLEND);
    }

    // Einzelblaetter naher Baeume: Alpha-Test im Shader (discard), also opak wie
    // das Baum-Mesh -> Depth schreiben, kein Blend. Vor dem Gras zeichnen, damit
    // der Tiefentest Gras hinter den Blaettern verwirft.
    if(!grassOnly && (nLeafInstCur > 0 || (treeFadeNew < 1.0f && nLeafInstOld > 0)) && !envNoLeaf){
      p_glDepthMask(GL_TRUE);
      p_glUseProgram(progLeaf);
      p_glUniformMatrix4fv(locLMVP, 1, GL_FALSE, mvp);
      p_glUniform3f(locLCam, camPos[0], camPos[1], camPos[2]);
      if(locLLod >= 0) p_glUniform3f(locLLod, camPos[0], camPos[1], camPos[2]);
      p_glUniform1f(locLTime, envFreeze >= 0.0f ? envFreeze : (float)tNow);
      p_glUniform1f(locLMask, (float)envLeafMask);
      if(locLAlb >= 0) p_glUniform1f(locLAlb, 0.0f);
      if(locLFade >= 0) p_glUniform1f(locLFade, treeFadeNew);
      p_glBindVertexArray(vaoLeaf[drawSet]);
      p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nLeafInstCur);
      if(treeFadeNew < 1.0f && treeOldSet >= 0 && nLeafInstOld > 0){
        if(locLFade >= 0) p_glUniform1f(locLFade, 1.0f - treeFadeNew);
        if(locLOld >= 0) p_glUniform1f(locLOld, 1.0f);
        p_glBindVertexArray(vaoLeaf[treeOldSet]);
        p_glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)nLeafInstOld);
        if(locLOld >= 0) p_glUniform1f(locLOld, 0.0f);
      }
      if(locLFade >= 0) p_glUniform1f(locLFade, 1.0f);
    }

    // 3) MSAA-Resolve: fboMS -> fbo (Farbe + Tiefe in einem Blit, Single-Sample)
    //    glBlitNamedFramebuffer (eindeutige 12-Param-Signatur) statt glBlitFramebuffer
    //    (Mesa-GLX liefert fuer letzteren den 10-Param-ARB-Entrypoint -> stille No-Op).
    if (useMSAA) {
      p_glBlitNamedFramebuffer(fboMS, fbo, 0, 0, fboW, fboH, 0, 0, fboW, fboH,
                               GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      p_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    }
    // 4) Blit auf den Screen
    p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    p_glViewport(0, 0, winW, winH);
    p_glDisable(GL_DEPTH_TEST);   // Fullscreen-Triangle nicht gegen Fenster-Depth testen
    p_glUseProgram(progBlit);
    p_glBindVertexArray(vaoTri);   // Fullscreen-Triangle (vorher implizit ueber Szene-Pass)
    p_glActiveTexture(GL_TEXTURE0);
    p_glBindTexture(GL_TEXTURE_2D, texScene);
    p_glUniform1i(locBlitTex, 0);
    p_glDrawArrays(GL_TRIANGLES, 0, 3);

    // 3b) Baum-Wireframe-Overlay (Screen-Space, gleiche Kamera wie Raymarcher)
    if (showTreeWire) {
      float wmvp[16];
      build_mvp(camPos, camTgt, winW, winH, wmvp);
      p_glEnable(GL_BLEND);
      p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      p_glUseProgram(progWire);
      p_glUniformMatrix4fv(locWireMVP, 1, GL_FALSE, wmvp);
      p_glUniform3f(locWireCam, camPos[0], camPos[1], camPos[2]);
      p_glUniform4f(locWireCol, 0.20f, 1.0f, 0.60f, 1.0f);
      p_glBindVertexArray(vaoWire);
      p_glLineWidth(2.0f);
      p_glDrawArrays(GL_LINES, 0, nWireVerts);
      p_glDisable(GL_BLEND);
    }

    // 4) HUD (H-Taste: Vollstats 3 Zeilen -> reduziert 1 Zeile -> aus)
    char hud1[128], hud2[128], hud3[128];
    const char *sc = scaleIdx == 0 ? "1x" : (scaleIdx == 1 ? "1/2" : "1/4");
    snprintf(hud1, sizeof hud1, "FPS %3d %dx%d RES %s CAM %s AA %s TREES %d QUADS %d GRASS %d",
             (int)(fpsEMA + 0.5), winW, winH, sc, flyMode ? "FLY" : "ORB",
             useMSAA ? (msaaSamples == 8 ? "8x" : "4x") : "OFF",
             nTreesD, nCrownVertsCur, nGrass);
    snprintf(hud2, sizeof hud2, "D %.1fx H %.1fx MD %d.%dm%s%s%s%s",
             grassDensityScale, grassHeightScale, (int)grassMaxDist, (int)(grassMaxDist * 10) % 10,
             grassFrozen ? " FRZ" : "", grassOnly ? " NUR-GRAS" : "",
             dbgGrassDepth ? " DW+" : " DW-", dbgGrassEdge ? " EDGE+" : " EDGE-");
    if (dbgGrassCat) strncat(hud2, dbgGrassCat == 1 ? " CAT" : " ART", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (dbgAlpha2Cov) strncat(hud2, " A2C", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (dbgSortOrder) strncat(hud2, " SORT", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (dbgAngThin) strncat(hud2, " WINK", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (grassTexOn) strncat(hud2, " TEX", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (grassFadeDur != 1.0f) strncat(hud2, " FADE", sizeof(hud2) - (int)strlen(hud2) - 1);
    snprintf(hud3, sizeof hud3, "POS %d %d %d%s%s",
             (int)camPos[0], (int)camPos[1], (int)camPos[2],
             nGrass >= GRASS_CAP ? " CAP" : "", grass_build_busy() ? " BUILD" : "");
    int len1 = (int)strlen(hud1), len2 = (int)strlen(hud2), len3 = (int)strlen(hud3);

    p_glEnable(GL_BLEND);
    p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    p_glUseProgram(progHud);
    p_glBindTexture(GL_TEXTURE_2D, texFont);
    p_glUniform1i(locHudFont, 0);
    p_glBindVertexArray(vaoHud);
    p_glBindBuffer(GL_ARRAY_BUFFER, vboHud);
    {
      float hsc = hud_scale(winH);
      float cs = 16.0f * hsc;
      float buv[2] = { (ATLAS_W - 0.5f) / ATLAS_W, (ATLAS_H - 0.5f) / ATLAS_H };
      // Hintergrund-Quad (Statuszeilen) ERST (Text wird drueber gezeichnet)
      if (hudMode > 0) {
        int len = len1 > len2 ? len1 : len2;
        if (hudMode == 2 && len3 > len) len = len3;
        int nLines = hudMode == 2 ? 3 : 1;
        float bx0 = 16.0f * hsc, by0 = 12.0f * hsc;
        float bx1 = 16.0f * hsc + len * cs + 8.0f * hsc;
        float by1 = 12.0f * hsc + nLines * cs + 8.0f * hsc;
        float bg[16] = {
          bx0/winW*2-1, 1-by0/winH*2, buv[0], buv[1],
          bx1/winW*2-1, 1-by0/winH*2, buv[0], buv[1],
          bx0/winW*2-1, 1-by1/winH*2, buv[0], buv[1],
          bx1/winW*2-1, 1-by1/winH*2, buv[0], buv[1],
        };
        p_glBufferData(GL_ARRAY_BUFFER, sizeof bg, bg, GL_DYNAMIC_DRAW);
        p_glUniform4f(locTint, 0.0f, 0.0f, 0.0f, 0.35f);
        p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
      }
      // Status-Text (vollstaeckig opak) und Slider-Labels (mit sliderFade)
      // in getrennten Draws (anderer Tint-Alpha).
      int n = 0;
      if (hudMode > 0) {
        n += build_text_verts(hudVerts + n * 4, hud1, winW, winH, 24.0f * hsc, 20.0f * hsc);
        if (hudMode == 2) {
          n += build_text_verts(hudVerts + n * 4, hud2, winW, winH, 24.0f * hsc, 20.0f * hsc + cs);
          n += build_text_verts(hudVerts + n * 4, hud3, winW, winH, 24.0f * hsc, 20.0f * hsc + 2 * cs);
        }
      }
      if (n > 0) {
        p_glBufferData(GL_ARRAY_BUFFER, n * 4 * sizeof(float), hudVerts, GL_DYNAMIC_DRAW);
        p_glUniform4f(locTint, 1.0f, 1.0f, 1.0f, 1.0f);
        p_glDrawArrays(GL_TRIANGLES, 0, n);
      }
      const char *lbl[3] = { "Sichtweite", "Hohe", "Dichte" };
      float slY[3] = { winH - 100.0f * hsc, winH - 72.0f * hsc, winH - 44.0f * hsc };
      int nl = 0;
      for (int i = 0; i < 3; i++)
        nl += build_text_verts(hudVerts + nl * 4, lbl[i], winW, winH,
                              winW / 2.0f - 150.0f * hsc - (float)strlen(lbl[i]) * cs - 8.0f * hsc, slY[i] + 1.0f * hsc);
      if (nl > 0 && sliderFade > 0.004f) {
        p_glBufferData(GL_ARRAY_BUFFER, nl * 4 * sizeof(float), hudVerts, GL_DYNAMIC_DRAW);
        p_glUniform4f(locTint, 1.0f, 1.0f, 1.0f, sliderFade);
        p_glDrawArrays(GL_TRIANGLES, 0, nl);
      }
      draw_sliders((grassDensityScale - 0.2f) / 5.4f,
                   (grassHeightScale - 0.3f) / 2.7f,
                   (grassMaxDist - 40.0f) / 200.0f, winW, winH, locTint, sliderFade);
    }
    p_glDisable(GL_BLEND);

    // Automatischer Screenshot des vollen Frames (inkl. Wireframe-Overlay) nach dem
    // ersten Frame (Test/Verifikation)
    if (!shotDone) { shotDone = 1; save_screenshot_ppm("/tmp/64kdemo_shot.ppm", winW, winH); }
    // DEBUG: geplanter Screenshot (DEMO_SHOT=<Sekunden>) — deterministisch, ohne
    // X-Tasten (synthetische Tasten kommen in der VM nicht immer an).
    if (envShotAt >= 0.0f && now_sec() - loopT0 >= (double)envShotAt){
      double el = now_sec() - loopT0;
      envShotAt = -1.0f;
      save_screenshot_ppm("/tmp/64kdemo_shot.ppm", winW, winH);
      printf("DEBUG Auto-Screenshot nach %.1f s\n", el);
    }

    p_glXSwapBuffers(disp, win);
  }

  p_glXDestroyContext(disp, ctx);
  p_XCloseDisplay(disp);
  return 0;
}