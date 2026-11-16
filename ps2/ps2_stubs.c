// PS2 port: the OpenGL output path is not available, SDL's PS2 render driver is used instead.
#include <stdbool.h>
struct RendererFuncs;
void OpenGLRenderer_Create(struct RendererFuncs *funcs, bool use_opengl_es) { (void)funcs; (void)use_opengl_es; }
