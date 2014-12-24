/*
 * Copyright 1993-2014 NVIDIA Corporation.  All rights reserved.
 *
 * THIS FILE IS RELEASED UNDER NDA
 *
 */

#ifndef NV_COMMANDLIST_H__
#define NV_COMMANDLIST_H__

#include <GL/glew.h>


#  if defined(__MINGW32__) || defined(__CYGWIN__)
#    define GLEXT_APIENTRY __stdcall
#  elif (_MSC_VER >= 800) || defined(_STDCALL_SUPPORTED) || defined(__BORLANDC__)
#    define GLEXT_APIENTRY __stdcall
#  else
#    define GLEXT_APIENTRY
#  endif


#define GL_TERMINATE_SEQUENCE_COMMAND_NV                      0x0000
#define GL_NOP_COMMAND_NV                                     0x0001
#define GL_DRAW_ELEMENTS_COMMAND_NV                           0x0002
#define GL_DRAW_ARRAYS_COMMAND_NV                             0x0003
#define GL_DRAW_ELEMENTS_STRIP_COMMAND_NV                     0x0004
#define GL_DRAW_ARRAYS_STRIP_COMMAND_NV                       0x0005
#define GL_DRAW_ELEMENTS_INSTANCED_COMMAND_NV                 0x0006
#define GL_DRAW_ARRAYS_INSTANCED_COMMAND_NV                   0x0007
#define GL_ELEMENT_ADDRESS_COMMAND_NV                         0x0008
#define GL_ATTRIBUTE_ADDRESS_COMMAND_NV                       0x0009
#define GL_UNIFORM_ADDRESS_COMMAND_NV                         0x000a
#define GL_BLEND_COLOR_COMMAND_NV                             0x000b
#define GL_STENCIL_REF_COMMAND_NV                             0x000c
#define GL_LINE_WIDTH_COMMAND_NV                              0x000d
#define GL_POLYGON_OFFSET_COMMAND_NV                          0x000e
#define GL_ALPHA_REF_COMMAND_NV                               0x000f
#define GL_VIEWPORT_COMMAND_NV                                0x0010
#define GL_SCISSOR_COMMAND_NV                                 0x0011
#define GL_MAX_COMMANDS_NV                                    0x0012

#pragma pack(push,1)

typedef struct {
    GLuint      encoded;
} CommandHeaderNV;
        
typedef struct {
    // no data
} TerminateSequenceCommandNV;

typedef struct {
    // no data
} NOPCommandNV;
        
typedef  struct {
    GLuint  count;
    GLuint  firstIndex;
    GLuint  baseVertex;
} DrawElementsCommandNV;

typedef  struct {
    GLuint  count;
    GLuint  first;
} DrawArraysCommandNV;
        
typedef  struct {
    GLuint  mode;
    GLuint  count;
    GLuint  instanceCount;
    GLuint  firstIndex;
    GLuint  baseVertex;
    GLuint  baseInstance;
} DrawElementsInstancedCommandNV;

typedef  struct {
    GLuint  mode;
    GLuint  count;
    GLuint  instanceCount;
    GLuint  first;
    GLuint  baseInstance;
} DrawArraysInstancedCommandNV;

typedef struct {
    GLuint64 address;
    GLuint typeSizeInByte;
} ElementAddressCommandNV;

typedef struct {
    GLuint   index;
    GLuint64 address;
} AttributeAddressCommandNV;

typedef struct {
    GLushort   index;
    GLushort   stage;
    GLuint64   address;
} UniformAddressCommandNV;

typedef struct {
    GLfloat red;
    GLfloat green;
    GLfloat blue;
    GLfloat alpha;
} BlendColorCommandNV;

typedef struct {
    GLuint frontStencilRef;
    GLuint backStencilRef;
} StencilRefCommandNV;
        
typedef struct {
    GLfloat lineWidth;
} LineWidthCommandNV;

typedef struct {
    GLfloat scale;
    GLfloat bias;
} PolygonOffsetCommandNV;

typedef struct {
    GLfloat alphaRef;
} AlphaRefCommandNV;

typedef struct {
    GLfloat x;
    GLfloat y;
    GLfloat width;
    GLfloat height;
} ViewportCommandNV;

typedef struct {
    GLuint x;
    GLuint y;
    GLuint width;
    GLuint height;
} ScissorCommandNV;

#pragma pack(pop)

typedef void (GLEXT_APIENTRY *PFNGLCREATESTATESNVPROC)(GLsizei n, GLuint *states);
typedef void (GLEXT_APIENTRY *PFNGLDELETESTATESNVPROC)(GLsizei n, const GLuint *states);
typedef GLboolean (*PFNGLISSTATENVPROC)(GLuint state);
typedef void (GLEXT_APIENTRY *PFNGLSTATEPROPERTIESDYNAMICNVPROC)(GLuint state, GLsizei count, const GLint *params);
typedef void (GLEXT_APIENTRY *PFNGLSTATECAPTURENVPROC)(GLuint state, GLenum mode);
typedef void (GLEXT_APIENTRY *PFNGLDRAWCOMMANDSSTATESNVPROC)(GLuint buffer, const GLintptr* indirects, const GLsizei* sizes, 
  const GLuint* states, const GLuint* fbos, GLuint count);
typedef void (GLEXT_APIENTRY *PFNGLDRAWCOMMANDSSTATESADDRESSNVPROC)(const GLuint64* indirects, const GLsizei* sizes, 
  const GLuint* states, const GLuint* fbos, GLuint count);
typedef void (GLEXT_APIENTRY *PFNGLCREATECOMMANDLISTSNVPROC)(GLsizei n, GLuint *lists);
typedef void (GLEXT_APIENTRY *PFNGLDELETECOMMANDLISTSNVPROC)(GLsizei n, const GLuint *lists);
typedef GLboolean (GLEXT_APIENTRY *PFNGLISCOMMANDLISTNVPROC)(GLuint list);
typedef void (GLEXT_APIENTRY *PFNGLLISTDRAWCOMMANDSSTATESCLIENTNVPROC)(GLuint list, GLuint segment, const GLvoid** indirects, 
                                              const GLsizei* sizes, const GLuint* states, const GLuint* fbos, GLuint count);
typedef void (GLEXT_APIENTRY *PFNGLCOMMANDLISTSEGMENTSNVPROC)(GLuint list, GLuint segments);
typedef void (GLEXT_APIENTRY *PFNGLCOMPILECOMMANDLISTNVPROC)(GLuint list);
typedef void (GLEXT_APIENTRY *PFNGLCALLCOMMANDLISTNVPROC)(GLuint list);
typedef GLuint (GLEXT_APIENTRY *PFNGLGETCOMMANDHEADERNVPROC)(GLenum id, GLuint tokenSize);
typedef GLushort(GLEXT_APIENTRY* PFNGLGETSTAGEINDEXNVPROC)(GLenum shadertype);

typedef void (GLEXT_APIENTRY *PFNGLDRAWCOMMANDSNVPROC)(GLenum mode, GLenum type, GLuint buffer, const GLintptr* indirects, const GLsizei* sizes, GLuint count);
//typedef void (GLEXT_APIENTRY *PFNGLDRAWCOMMANDSADDRESSNVPROC)(GLenum GLmode, GLenum type, const GLuint64* indirects, const GLsizei* sizes, GLuint count);
extern PFNGLDRAWCOMMANDSNVPROC __glewDrawCommandsNV;
inline void glDrawCommandsNV(GLenum mode, GLenum type, GLuint buffer, const GLintptr* indirects, const GLsizei* sizes, GLuint count)
{
  __glewDrawCommandsNV(mode, type, buffer, indirects, sizes, count);
}

extern PFNGLCREATESTATESNVPROC __glewCreateStatesNV;
inline void glCreateStatesNV(GLsizei n, GLuint *states)
{
  __glewCreateStatesNV(n,states);
}
extern PFNGLDELETESTATESNVPROC __glewDeleteStatesNV;
inline void glDeleteStatesNV(GLsizei n, const GLuint *states)
{
  __glewDeleteStatesNV(n,states);
}
extern PFNGLISSTATENVPROC __glewIsStateNV;
inline GLboolean glIsStateNV(GLuint state)
{
  return __glewIsStateNV(state);
}
extern PFNGLSTATEPROPERTIESDYNAMICNVPROC __glewStatePropertiesDynamicNV;
inline void glStatePropertiesDynamicNV(GLuint state, GLsizei count, const GLint *params)
{
  __glewStatePropertiesDynamicNV(state,count,params);
}
extern PFNGLSTATECAPTURENVPROC __glewStateCaptureNV;
inline void glStateCaptureNV(GLuint state, GLenum mode)
{
  __glewStateCaptureNV(state,mode);
}
extern PFNGLDRAWCOMMANDSSTATESNVPROC __glewDrawCommandsStatesNV;
inline void glDrawCommandsStatesNV(GLuint buffer, const GLintptr* indirects, const GLsizei* sizes, 
  const GLuint* states, const GLuint* fbos, GLuint count)
{
  __glewDrawCommandsStatesNV(buffer,indirects,sizes,states,fbos,count);
}
extern PFNGLDRAWCOMMANDSSTATESADDRESSNVPROC __glewDrawCommandsStatesAddressNV;
inline void glDrawCommandsStatesAddressNV(const GLuint64* indirects, const GLsizei* sizes, 
  const GLuint* states, const GLuint* fbos, GLuint count)
{
  __glewDrawCommandsStatesAddressNV(indirects,sizes,states,fbos,count);
}
extern PFNGLCREATECOMMANDLISTSNVPROC __glewCreateCommandListsNV;
inline void glCreateCommandListsNV(GLsizei n, GLuint *lists)
{
  __glewCreateCommandListsNV(n,lists);
}
extern PFNGLDELETECOMMANDLISTSNVPROC __glewDeleteCommandListsNV;
inline void glDeleteCommandListsNV(GLsizei n, const GLuint *lists)
{
  __glewDeleteCommandListsNV(n,lists);
}
extern PFNGLISCOMMANDLISTNVPROC __glewIsCommandListNV;
inline GLboolean glIsCommandListNV(GLuint list)
{
  return __glewIsCommandListNV(list);
}
extern PFNGLLISTDRAWCOMMANDSSTATESCLIENTNVPROC __glewListDrawCommandsStatesClientNV;
inline void glListDrawCommandsStatesClientNV(GLuint list, GLuint segment, const GLvoid** indirects, 
                                              const GLsizei* sizes, const GLuint* states, const GLuint* fbos,
                                              GLuint count)
{
  __glewListDrawCommandsStatesClientNV(list, segment, indirects, sizes, states, fbos, count);
}
extern PFNGLCOMMANDLISTSEGMENTSNVPROC __glewCommandListSegmentsNV;
inline void glCommandListSegmentsNV(GLuint list, GLuint segments)
{
  __glewCommandListSegmentsNV(list,segments);
}
extern PFNGLCOMPILECOMMANDLISTNVPROC __glewCompileCommandListNV;
inline void glCompileCommandListNV(GLuint list)
{
  __glewCompileCommandListNV(list);
}
extern PFNGLCALLCOMMANDLISTNVPROC __glewCallCommandListNV;
inline void glCallCommandListNV(GLuint list)
{
  __glewCallCommandListNV(list);
}
extern PFNGLGETCOMMANDHEADERNVPROC __glewGetCommandHeaderNV;
inline GLuint glGetCommandHeaderNV(GLenum tokenId, GLuint tokenSize)
{
  return __glewGetCommandHeaderNV(tokenId,tokenSize);
}
extern PFNGLGETSTAGEINDEXNVPROC __glewGetStageIndexNV;
inline GLushort glGetStageIndexNV(GLenum shadertype)
{
  return __glewGetStageIndexNV(shadertype);
}

int initNVcommandList();

#define GL_UNIFORM_BUFFER_UNIFIED_NV                      0x936E
#define GL_UNIFORM_BUFFER_ADDRESS_NV                      0x936F


#endif

