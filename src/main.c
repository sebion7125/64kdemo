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
static void (*p_glBindTexture)(GLenum target, GLuint texture);
static void (*p_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
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
  LOADGL2(glBindTexture);
  LOADGL2(glTexImage2D);
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
static int dbgGrassCat = 0;
static float gCX = 0, gCZ = 0;   // letztes Gras-Build-Zentrum (auch fuer R-Reset)
static int grassOnly = 0;     // 1 = TEST: nur Gras zeichnen (V-Taste), kein Terrain/Baeume/Blattdach
static int hudMode = 2;       // H-Taste: 2 = Vollstats (2 Zeilen), 1 = reduziert (1 Zeile), 0 = aus
float grassDensityScale = 1.0f;  // 0.2 - 8.0 (Slider unten im Fenster, extern in scene.h)
float grassHeightScale = 1.0f;   // 0.3 - 3.0 (Halmhoehe-Slider)
float grassMaxDist = 80.0f;      // 40 - 120m (max. Gras-Sichtweite, Slider)
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
static GLuint vboTree = 0, vaoTree = 0;
static GLuint vboCrown = 0, vaoCrown = 0;
static GLuint vboGrass = 0, vaoGrass = 0, vboGrassBase = 0;
static GLuint vboWire = 0, vaoWire = 0;

// Grundformen (per-Vertex-Attribute der Instanz-Shader)
// Gras-Grundform: 3 Dreiecke (ohne Index-Buffer -> duplizierte Vertices).
// Draw-Call: GL_TRIANGLES count=9 -> 9 Vertices noetig.
// (0,1,2) = unterer Keil, (2,1,3) = oberer Keil, (2,3,4) = Spitze.
static const float grassBase[9][2] = {
  {0.0f, -0.5f}, {0.0f, 0.5f}, {0.55f, -0.34f},   // Tri 1: (0,1,2)
  {0.55f, -0.34f}, {0.0f, 0.5f}, {0.55f, 0.34f},   // Tri 2: (2,1,3)
  {0.55f, -0.34f}, {0.55f, 0.34f}, {1.0f, 0.0f}    // Tri 3: (2,3,4)
};

// Baum-Buffer nach trees_build() hochladen (Gras wird separat geladen)
static void upload_mesh_buffers(void){
  p_glBindBuffer(GL_ARRAY_BUFFER, vboTree);
  p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nTreeVerts * TREE_FLOATS * sizeof(float), treeVerts, GL_DYNAMIC_DRAW);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboCrown);
  p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nCrownVerts * CROWN_FLOATS * sizeof(float), crownVerts, GL_DYNAMIC_DRAW);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboWire);
  p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)nWireVerts * WIRE_FLOATS * sizeof(float), wireVerts, GL_DYNAMIC_DRAW);
}

static void build_tree_lines(void){   // VAOs + Grundformen (Geometrie selbst: trees_cpu.c)
  p_glGenBuffers(1, &vboTree);
  p_glGenBuffers(1, &vboCrown);
  p_glGenBuffers(1, &vboGrass);
  p_glGenBuffers(1, &vboWire);
  p_glGenBuffers(1, &vboGrassBase);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboGrassBase);
  p_glBufferData(GL_ARRAY_BUFFER, sizeof grassBase, grassBase, GL_STATIC_DRAW);

  // Baum-Mesh: aPos(3) aNrm(3) aCol(4)
  p_glGenVertexArrays(1, &vaoTree);
  p_glBindVertexArray(vaoTree);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboTree);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)0);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)12);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, TREE_FLOATS * 4, (const void *)24);

  // Kronen-Billboards: aCenter(3) aOff(2) aData(4)
  p_glGenVertexArrays(1, &vaoCrown);
  p_glBindVertexArray(vaoCrown);
  p_glBindBuffer(GL_ARRAY_BUFFER, vboCrown);
  p_glEnableVertexAttribArray(0);
  p_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)0);
  p_glEnableVertexAttribArray(1);
  p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)12);
  p_glEnableVertexAttribArray(2);
  p_glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, CROWN_FLOATS * 4, (const void *)20);

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
  p_glEnableVertexAttribArray(5);
  p_glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, GRASS_FLOATS * 4, (const void *)40);
  p_glVertexAttribDivisor(5, 1);

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

// Slider-Setzen (1=Dichte, 2=Hoehe, 3=Sichtweite), pos = 0..1
static void set_slider(int which, float pos){
  if (pos < 0.0f) pos = 0.0f; if (pos > 1.0f) pos = 1.0f;
  if (which == 1) grassDensityScale = 0.2f + pos * 5.4f;   // max 5.6x (staubt bei MD=120 am 5M-CAP)
  else if (which == 2) grassHeightScale = 0.3f + pos * 2.7f;
  else grassMaxDist = 40.0f + pos * 80.0f;
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
  GLint locGMVP    = p_glGetUniformLocation(progGrass, "uMVP");
  GLint locGCam    = p_glGetUniformLocation(progGrass, "uCamPos");
  GLint locGTime   = p_glGetUniformLocation(progGrass, "uTime");
  GLint locGHeight = p_glGetUniformLocation(progGrass, "uHeightScale");
  GLint locGMaxDist = p_glGetUniformLocation(progGrass, "uMaxDist");
  GLint locGFade = p_glGetUniformLocation(progGrass, "uFadeDur");
  GLint locGCat = p_glGetUniformLocation(progGrass, "uDbgCat");
  GLint locCMVP    = p_glGetUniformLocation(progCrown, "uMVP");
  GLint locCCam    = p_glGetUniformLocation(progCrown, "uCamPos");
  GLint locCTgt    = p_glGetUniformLocation(progCrown, "uCamTgt");

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

  // ---------- Hauptloop ----------
  double tPrev = now_sec();
  double fpsEMA = 60.0;
  int running = 1;
  int shotDone = 0;
  static float hudVerts[HUD_MAX_CHARS * 6 * 4];

  while (running) {
    double tNow = now_sec();
    gWorldTime = (float)tNow;   // fuer Birth-Time der Halme (grass_cpu.c)
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
    float dt = rawDt > 0.1f ? 0.1f : (float)rawDt;

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
          if (ks == XK_p) save_screenshot_ppm("/tmp/64kdemo_shot.ppm", winW, winH);
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
          if (ks == XK_c) { dbgGrassCat = !dbgGrassCat;
            printf("DEBUG Gras-Farbmodus: %s (gelb = laeuft aus, cyan = laeuft ein)\n",
                   dbgGrassCat ? "AN" : "AUS"); }
          if (ks == XK_l) {   // Fade-Dauer strecken/stauchen: VM laeuft mit 1-2 FPS,
            // ein 2s-Fade ist dort zeitlich nicht aufloesbar. CPU und Shader
            // nutzen dieselbe Dauer.
            static const float durs[] = { 2.0f, 1.0f, 10.0f, 30.0f };
            static int durIdx = 0;
            durIdx = (durIdx + 1) % (int)(sizeof durs / sizeof durs[0]);
            grassFadeDur = durs[durIdx];
            printf("DEBUG Fade-Dauer: %.0fs\n", grassFadeDur); }
          if (ks == XK_h) {   // HUD zyklisch: Vollstats -> reduziert -> aus
            hudMode = (hudMode + 2) % 3;
            printf("HUD: %s\n", hudMode == 2 ? "Vollstats (2 Zeilen)" : (hudMode == 1 ? "reduziert (1 Zeile)" : "aus"));
          }
          if (ks == XK_v) { grassOnly = !grassOnly; printf("TEST: nur Gras zeichnen: %s\n", grassOnly ? "AN" : "aus"); }
          if (ks == XK_b) grass_debug_dump();
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
    // Baeume: Schwellwert 12m (Flaeche +/-144m, kein Pop-in).
    // Gras: inkrementell (Zweipuffer, ~8000 Zellen/Frame), Schwellwert 12m.
    {
      static int builtT = 0, builtG = 0;
      static float tCX = 0, tCZ = 0;
      if(!builtT || fabsf(camTgt[0] - tCX) > 12.0f || fabsf(camTgt[2] - tCZ) > 12.0f){
        tCX = camTgt[0]; tCZ = camTgt[2];
        trees_build(tCX, tCZ);
        upload_mesh_buffers();
        builtT = 1;
        printf("Baeume um (%.0f,%.0f): %d Baeume, %d Vertices, %d Canopy-Quads\n", tCX, tCZ, nTrees, nTreeVerts, nCrownVerts / 6);
      }
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
    if (!grassOnly) {
      p_glUseProgram(progScene);
      p_glBindVertexArray(vaoTri);
      p_glUniform2f(locRes, (float)fboW, (float)fboH);
      p_glUniform1f(locTime, (float)tNow);
      p_glUniform3f(locCamPos, camPos[0], camPos[1], camPos[2]);
      p_glUniform3f(locCamTgt, camTgt[0], camTgt[1], camTgt[2]);
      p_glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    // 2) Mesh-Pass (Baeume + Gras + Blattdach) mit Depth-Test gegen Szenen-Tiefe
    p_glEnable(GL_DEPTH_TEST);
    p_glDepthFunc(GL_LESS);

    // Baum-Mesh (Stamm/Aeste)
    if (!grassOnly) {
      p_glUseProgram(progTMesh);
      p_glUniformMatrix4fv(locTMVP, 1, GL_FALSE, mvp);
      p_glUniform3f(locTCam, camPos[0], camPos[1], camPos[2]);
      p_glUniform1f(locTTime, (float)tNow);
      p_glBindVertexArray(vaoTree);
      p_glDrawArrays(GL_TRIANGLES, 0, nTreeVerts);
    }

    // Gras (instanziert) — von WEIT nach NAH zeichnen (Alpha-Blending und
    // MSAA brauchen Raster-Reihenfolge von hinten nach vorne). Die VBO ist
    // in Diamond-Order sortiert (Instance 0 = naechste Zelle), also in
    // Chunks vom Buffer-Ende her (keine Index-Buffer-Sortierung pro Frame).
    p_glUseProgram(progGrass);
    p_glUniformMatrix4fv(locGMVP, 1, GL_FALSE, mvp);
    p_glUniform3f(locGCam, camPos[0], camPos[1], camPos[2]);
    p_glUniform1f(locGTime, (float)tNow);
    p_glUniform1f(locGHeight, grassHeightScale);
    p_glUniform1f(locGMaxDist, grassMaxDist);
    p_glUniform1f(locGFade, grassFadeDur);
    p_glUniform1f(locGCat, dbgGrassCat ? 1.0f : 0.0f);
    // Blending fuer Fade-in (neue Halme transparent beim Einblenden)
    p_glEnable(GL_BLEND);
    p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    p_glBindVertexArray(vaoGrass);
    // DEBUG-Zweig: Depth-Write abschaltbar (J). Mit Depth-Write verdeckt ein
    // naeherer Halm jeden ferneren, der innerhalb des gleichen Chunks spaeter
    // gezeichnet wird; die Chunk-Reihenfolge ist nur grob sortiert (~50% der
    // Nachbarpaare innerhalb eines Chunks liegen falsch, bis 24m). Aendert sich
    // am Swap die Sortierung, werden verdeckte Halme schlagartig sichtbar.
    p_glDepthMask(dbgGrassDepth ? GL_TRUE : GL_FALSE);
    {
      int chunk = (nGrass + 31) / 32;   // 32 Chunks (der Fade-Crescent liegt
                                         // in den letzten 1-2 Chunks -> genuegend fein)
      for (int off = nGrass; off > 0; off -= chunk) {
        int c = off > chunk ? chunk : off;
        p_glDrawArraysInstancedBaseInstance(GL_TRIANGLES, 0, 9, c, (GLuint)(off - c));
      }
    }
    p_glDepthMask(GL_TRUE);
    p_glDisable(GL_BLEND);

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
      p_glBindVertexArray(vaoCrown);
      p_glDrawArrays(GL_TRIANGLES, 0, nCrownVerts);
      p_glDisable(GL_BLEND);
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
             nTrees, nCrownVerts / 6, nGrass);
    snprintf(hud2, sizeof hud2, "D %.1fx H %.1fx MD %d.%dm%s%s%s%s",
             grassDensityScale, grassHeightScale, (int)grassMaxDist, (int)(grassMaxDist * 10) % 10,
             grassFrozen ? " FRZ" : "", grassOnly ? " NUR-GRAS" : "",
             dbgGrassDepth ? " DW+" : " DW-", dbgGrassEdge ? " EDGE+" : " EDGE-");
    if (dbgGrassCat) strncat(hud2, " CAT", sizeof(hud2) - (int)strlen(hud2) - 1);
    if (grassFadeDur > 2.5f || grassFadeDur < 1.9f) strncat(hud2, " FADE", sizeof(hud2) - (int)strlen(hud2) - 1);
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
                   (grassMaxDist - 40.0f) / 80.0f, winW, winH, locTint, sliderFade);
    }
    p_glDisable(GL_BLEND);

    // Automatischer Screenshot des vollen Frames (inkl. Wireframe-Overlay) nach dem
    // ersten Frame (Test/Verifikation)
    if (!shotDone) { shotDone = 1; save_screenshot_ppm("/tmp/64kdemo_shot.ppm", winW, winH); }

    p_glXSwapBuffers(disp, win);
  }

  p_glXDestroyContext(disp, ctx);
  p_XCloseDisplay(disp);
  return 0;
}