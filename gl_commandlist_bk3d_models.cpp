/*-----------------------------------------------------------------------
    Copyright (c) 2013, NVIDIA. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Neither the name of its contributors may be used to endorse 
       or promote products derived from this software without specific
       prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
    OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    feedback to tlorach@nvidia.com (Tristan Lorach)
*/ //--------------------------------------------------------------------
#include "gl_commandlist_bk3d_models.h"
#include "NVFBOBox.h"

#define GRIDDEF 20
#define GRIDSZ 1.0f
#define CROSSSZ 0.01f

//-----------------------------------------------------------------------------
// Derive the Window for this sample
//-----------------------------------------------------------------------------
class MyWindow: public WindowInertiaCamera
{
private:
    NVFBOBox  m_fboBox;
    NVFBOBox::DownSamplingTechnique downsamplingMode;
public:
    MyWindow();

    virtual bool init();
    virtual void shutdown();
    virtual void reshape(int w, int h);
    //virtual void motion(int x, int y);
    //virtual void mousewheel(short delta);
    //virtual void mouse(NVPWindow::MouseButton button, ButtonAction action, int mods, int x, int y);
    //virtual void menu(int m);
    virtual void keyboard(MyWindow::KeyCode key, ButtonAction action, int mods, int x, int y);
    virtual void keyboardchar(unsigned char key, int mods, int x, int y);
    //virtual void idle();
    virtual void display();
};

MyWindow::MyWindow() :
    WindowInertiaCamera(vec3f(0.0f,1.0f,-3.0f), vec3f(0,0,0))
    , downsamplingMode(NVFBOBox::DS2)
{
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
void sample_print(int level, const char * txt)
{
#ifdef USESVCUI
    switch(level)
    {
    case 0:
    case 1:
        logMFCUI(level, txt);
        break;
    case 2:
        logMFCUI(level, txt);
        break;
    default:
        logMFCUI(level, txt);
        break;
    }
#else
#endif
}


//-----------------------------------------------------------------------------
// Grid
//-----------------------------------------------------------------------------
static const char *g_glslv_grid = 
"#version 430\n"
"#extension GL_ARB_separate_shader_objects : enable\n"
"#extension GL_NV_command_list : enable\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_MATRIX) ") uniform matrixBuffer {\n"
"   uniform mat4 mW;\n"
"   uniform mat4 mVP;\n"
"} matrix;\n"
"layout(location=0) in  vec3 P;\n"
"out gl_PerVertex {\n"
"    vec4  gl_Position;\n"
"};\n"
"void main() {\n"
"   gl_Position = matrix.mVP * (matrix.mW * ( vec4(P, 1.0)));\n"
"}\n"
;
static const char *g_glslf_grid = 
"#version 430\n"
"#extension GL_ARB_separate_shader_objects : enable\n"
"#extension GL_NV_command_list : enable\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_LIGHT) ") uniform lightBuffer {\n"
"   uniform vec3 dir;"
"} light;\n"
"layout(location=0) out vec4 outColor;\n"
"void main() {\n"
"   outColor = vec4(0.5,0.7,0.5,1);\n"
"}\n"
;

//-----------------------------------------------------------------------------
// Help
//-----------------------------------------------------------------------------
static const char* s_sampleHelp = 
    "space: toggles continuous rendering\n"
    "'c': use Commandlist\n"
    "'e': use Commandlist EMULATION\n"
    "'l': use glCallCommandListNV\n"
    "'o': toggles object display\n"
    "'g': toggles grid display\n"
    "'s': toggle stats\n"
    "'a': animate camera\n"
;
static const char* s_sampleHelpCmdLine = 
    "---------- Cmd-line arguments ----------\n"
    "-v <VBO max Size>\n-m <bk3d model>\n"
    "-c 0 or 1 : use command-lists\n"
    "-b 0 or 1 : use bindless when no cmd list\n"
    "-o 0 or 1 : display meshes\n"
    "-g 0 or 1 : display grid\n"
    "-s 0 or 1 : stats\n"
    "-a 0 or 1 : animate camera\n"
    "-i <file> : use a config file to load models and setup camera animation\n"
    "-d 0 or 1 : debug stuff (ui)\n"
    "-m <bk3d file> : load a specific model\n"
    "<bk3d>    : load a specific model\n"
    "-q <msaa> : MSAA\n"
    "-r <ss_val> : supersampling (1.0,1.5,2.0)\n"
    "----------------------------------------\n"
;

//-----------------------------------------------------------------------------
// Prototype(s)
//-----------------------------------------------------------------------------
void updateViewportTokenBufferAndLineWidth(GLint x, GLint y, GLsizei width, GLsizei height, float lineW);

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
#ifdef USESVCUI
IWindowFolding*   g_pTweakContainer = NULL;
#endif
nv_helpers_gl::Profiler      g_profiler;

GLSLShader g_shaderGrid;

bool        g_bUseCommandLists  = true;
bool        g_bUseEmulation     = false;
bool        g_bUseCallCommandListNV = false;
bool        g_bUseGridBindless  = true;
bool        g_bDisplayObject    = true;

float       g_Supersampling    = 1.0f;

MatrixBufferGlobal      g_globalMatrices;

BO g_uboMatrix      = {0,0,0};
BO g_uboLight       = {0,0,0};
//-----------------------------------------------------------------------------
// Static variables for setting up the scene...
//-----------------------------------------------------------------------------
std::vector<Bk3dModel*> s_bk3dModels;

#define FOREACHMODEL(cmd) {\
    for(int m=0;m<s_bk3dModels.size(); m++) {\
    s_bk3dModels[m]->cmd ;\
    }\
}
static int      s_curObject = 0;
static bool     s_bCreateDebugUI = false;
//-----------------------------------------------------------------------------
// Static variables
//-----------------------------------------------------------------------------
static int      s_MSAA             = 8;

//
// Camera animation: captured using '1' in the sample. Then copy and paste...
//
struct CameraAnim {    vec3f eye, focus; float sleep; };
static std::vector<CameraAnim> s_cameraAnim;

static int     s_cameraAnimItem     = 0;
static int     s_cameraAnimItems    = 15;
static float   s_cameraAnimIntervals= 0.1;
static bool    s_bCameraAnim        = false;

#define        HELPDURATION         5.0
static float   s_helpText           = 0.0;

static int      s_maxItems          = -1;
static bool     s_bDisplayGrid      = true;
static bool     s_bRecordGrid       = true;
static bool     s_bStats            = false;
static GLuint   s_header[GL_MAX_COMMANDS_NV] = {0};
static GLuint   s_headerSizes[GL_MAX_COMMANDS_NV] = {0};

static GLuint      s_vboGrid;
static GLuint      s_vboGridSz;
static GLuint64    s_vboGridAddr;

static GLuint      s_vboCross;
static GLuint      s_vboCrossSz;
static GLuint64    s_vboCrossAddr;

static MaterialBuffer*  s_material          = NULL;
static int              s_materialNItems    = 0;

static LightBuffer     s_light              = { vec3f(0.4f,0.8f,0.3f) };

static GLuint      s_vao                    = 0;

static CommandStatesBatch   s_commandGrid;

static TokenBuffer          s_tokenBufferGrid;
TokenBuffer                 g_tokenBufferViewport;

//-----------------------------------------------------------------------------
// Useful stuff for Command-list
//-----------------------------------------------------------------------------
static GLushort s_stages[STAGES];

struct Token_Nop {
    static const GLenum   ID = GL_NOP_COMMAND_NV;
    NOPCommandNV      cmd;
    Token_Nop() {
      cmd.header  = s_header[ID];
    }
};

struct Token_TerminateSequence {
    static const GLenum   ID = GL_TERMINATE_SEQUENCE_COMMAND_NV;

    TerminateSequenceCommandNV cmd;
    
    Token_TerminateSequence() {
      cmd.header  = s_header[ID];
    }
};

struct Token_DrawElemsInstanced {
    static const GLenum   ID = GL_DRAW_ELEMENTS_INSTANCED_COMMAND_NV;

    DrawElementsInstancedCommandNV   cmd;

    Token_DrawElemsInstanced() {
      cmd.baseInstance = 0;
      cmd.baseVertex = 0;
      cmd.firstIndex = 0;
      cmd.count = 0;
      cmd.instanceCount = 1;

      cmd.header  = s_header[ID];
    }
};

struct Token_DrawArraysInstanced {
    static const GLenum   ID = GL_DRAW_ARRAYS_INSTANCED_COMMAND_NV;

    DrawArraysInstancedCommandNV   cmd;

    Token_DrawArraysInstanced() {
      cmd.baseInstance = 0;
      cmd.first = 0;
      cmd.count = 0;
      cmd.instanceCount = 1;

      cmd.header  = s_header[ID];
    }
};

struct Token_DrawElements {
    static const GLenum   ID = GL_DRAW_ELEMENTS_COMMAND_NV;

    DrawElementsCommandNV   cmd;

    Token_DrawElements() {
      cmd.baseVertex = 0;
      cmd.firstIndex = 0;
      cmd.count = 0;

      cmd.header  = s_header[ID];
    }
};

struct Token_DrawArrays {
    static const GLenum   ID = GL_DRAW_ARRAYS_COMMAND_NV;

    DrawArraysCommandNV   cmd;

    Token_DrawArrays() {
      cmd.first = 0;
      cmd.count = 0;

      cmd.header  = s_header[ID];
    }
};

struct Token_DrawElementsStrip {
    static const GLenum   ID = GL_DRAW_ELEMENTS_STRIP_COMMAND_NV;

    DrawElementsCommandNV   cmd;

    Token_DrawElementsStrip() {
      cmd.baseVertex = 0;
      cmd.firstIndex = 0;
      cmd.count = 0;

      cmd.header  = s_header[ID];
    }
};

struct Token_DrawArraysStrip {
    static const GLenum   ID = GL_DRAW_ARRAYS_STRIP_COMMAND_NV;

    DrawArraysCommandNV   cmd;

    Token_DrawArraysStrip() {
      cmd.first = 0;
      cmd.count = 0;

      cmd.header  = s_header[ID];
    }
};

struct Token_AttributeAddress {
    static const GLenum   ID = GL_ATTRIBUTE_ADDRESS_COMMAND_NV;

    AttributeAddressCommandNV cmd;

    Token_AttributeAddress() {
      cmd.header  = s_header[ID];
    }
};

struct Token_ElementAddress {
    static const GLenum   ID = GL_ELEMENT_ADDRESS_COMMAND_NV;

    ElementAddressCommandNV cmd;

    Token_ElementAddress() {
      cmd.header  = s_header[ID];
    }
};

struct Token_UniformAddress {
    static const GLenum   ID = GL_UNIFORM_ADDRESS_COMMAND_NV;

    UniformAddressCommandNV   cmd;

    Token_UniformAddress() {
      cmd.header  = s_header[ID];
    }
};

struct Token_BlendColor{
    static const GLenum   ID = GL_BLEND_COLOR_COMMAND_NV;

    BlendColorCommandNV     cmd;

    Token_BlendColor() {
      cmd.header  = s_header[ID];
    }
};

struct Token_StencilRef{
    static const GLenum   ID = GL_STENCIL_REF_COMMAND_NV;

    StencilRefCommandNV cmd;

    Token_StencilRef() {
      cmd.header  = s_header[ID];
    }
} ;

struct Token_LineWidth{
    static const GLenum   ID = GL_LINE_WIDTH_COMMAND_NV;

    LineWidthCommandNV  cmd;

    Token_LineWidth() {
      cmd.header  = s_header[ID];
    }
};

struct Token_PolygonOffset{
    static const GLenum   ID = GL_POLYGON_OFFSET_COMMAND_NV;

    PolygonOffsetCommandNV  cmd;

    Token_PolygonOffset() {
      cmd.header  = s_header[ID];
    }
};

struct Token_AlphaRef{
    static const GLenum   ID = GL_ALPHA_REF_COMMAND_NV;

    AlphaRefCommandNV cmd;

    Token_AlphaRef() {
      cmd.header  = s_header[ID];
    }
};

struct Token_Viewport{
    static const GLenum   ID = GL_VIEWPORT_COMMAND_NV;
    ViewportCommandNV cmd;
    Token_Viewport() {
      cmd.header  = s_header[ID];
    }
};

struct Token_Scissor {
    static const GLenum   ID = GL_SCISSOR_COMMAND_NV;
    ScissorCommandNV  cmd;
    Token_Scissor() {
      cmd.header  = s_header[ID];
    }
};

//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
template <class T>
void registerSize()
{
    s_headerSizes[T::ID] = sizeof(T);
}


//-----------------------------------------------------------------------------
// 
//-----------------------------------------------------------------------------
void initTokenInternals()
{
    registerSize<Token_TerminateSequence>();
    registerSize<Token_Nop>();
    registerSize<Token_DrawElements>();
    registerSize<Token_DrawArrays>();
    registerSize<Token_DrawElementsStrip>();
    registerSize<Token_DrawArraysStrip>();
    registerSize<Token_DrawElemsInstanced>();
    registerSize<Token_DrawArraysInstanced>();
    registerSize<Token_AttributeAddress>();
    registerSize<Token_ElementAddress>();
    registerSize<Token_UniformAddress>();
    registerSize<Token_LineWidth>();
    registerSize<Token_PolygonOffset>();
    registerSize<Token_Scissor>();
    registerSize<Token_BlendColor>();
    registerSize<Token_Viewport>();
    registerSize<Token_AlphaRef>();
    registerSize<Token_StencilRef>();

    for (int i = 0; i < GL_MAX_COMMANDS_NV; i++){
        // using i instead of a table of token IDs because the are arranged in the same order as i incrementing.
        // shortcut for the source code. See gl_nv_command_list.h
        s_header[i] = glGetCommandHeaderNV(i/*==Token enum*/,s_headerSizes[i]);
    }
    emucmdlist::InitHeaders(s_header, s_headerSizes);
    s_stages[STAGE_VERTEX]          = glGetStageIndexNV(GL_VERTEX_SHADER);
    s_stages[STAGE_TESS_CONTROL]    = glGetStageIndexNV(GL_TESS_CONTROL_SHADER);
    s_stages[STAGE_TESS_EVALUATION] = glGetStageIndexNV(GL_TESS_EVALUATION_SHADER);
    s_stages[STAGE_GEOMETRY]        = glGetStageIndexNV(GL_GEOMETRY_SHADER);
    s_stages[STAGE_FRAGMENT]        = glGetStageIndexNV(GL_FRAGMENT_SHADER);
}

//------------------------------------------------------------------------------
// build 
//------------------------------------------------------------------------------
std::string buildLineWidthCommand(float w)
{
    std::string cmd;
    Token_LineWidth lw;
    lw.cmd.lineWidth = w;
    cmd = std::string((const char*)&lw,sizeof(Token_LineWidth));

    return cmd;
}
//------------------------------------------------------------------------------
// build 
//------------------------------------------------------------------------------
std::string buildUniformAddressCommand(int idx, GLuint64 p, GLsizeiptr sizeBytes, ShaderStages stage)
{
    std::string cmd;
    Token_UniformAddress attr;
    attr.cmd.stage = s_stages[stage];
    attr.cmd.index = idx;
    ((GLuint64EXT*)&attr.cmd.addressLo)[0] = p;
    cmd = std::string((const char*)&attr,sizeof(Token_UniformAddress));

    return cmd;
}
//------------------------------------------------------------------------------
// build 
//------------------------------------------------------------------------------
std::string buildAttributeAddressCommand(int idx, GLuint64 p, GLsizeiptr sizeBytes)
{
    std::string cmd;
    Token_AttributeAddress attr;
    attr.cmd.index = idx;
    ((GLuint64EXT*)&attr.cmd.addressLo)[0] = p;
    cmd = std::string((const char*)&attr,sizeof(Token_AttributeAddress));

    return cmd;
}
//------------------------------------------------------------------------------
// build 
//------------------------------------------------------------------------------
std::string buildElementAddressCommand(GLuint64 ptr, GLenum indexFormatGL)
{
    std::string cmd;
    Token_ElementAddress attr;
    ((GLuint64EXT*)&attr.cmd.addressLo)[0] = ptr;
    switch(indexFormatGL)
    {
    case GL_UNSIGNED_INT:
        attr.cmd.typeSizeInByte = 4;
        break;
    case GL_UNSIGNED_SHORT:
        attr.cmd.typeSizeInByte = 2;
        break;
    }
    cmd = std::string((const char*)&attr,sizeof(Token_AttributeAddress));

    return cmd;
}
//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildDrawElementsCommand(GLenum topologyGL, GLuint indexCount)
{
    std::string cmd;
    Token_DrawElements dc;
    Token_DrawElementsStrip dcstrip;
    switch(topologyGL)
    {
    case GL_TRIANGLE_STRIP:
    case GL_QUAD_STRIP:
    case GL_LINE_STRIP:
        dcstrip.cmd.baseVertex = 0;
        dcstrip.cmd.firstIndex = 0;
        dcstrip.cmd.count = indexCount;
        cmd = std::string((const char*)&dcstrip,sizeof(Token_DrawElementsStrip));
        break;
    default:
        dc.cmd.baseVertex = 0;
        dc.cmd.firstIndex = 0;
        dc.cmd.count = indexCount;
        cmd = std::string((const char*)&dc,sizeof(Token_DrawElements));
        break;
    }
    return cmd;
}
//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildDrawArraysCommand(GLenum topologyGL, GLuint indexCount)
{
    std::string cmd;
    Token_DrawArrays dc;
    Token_DrawArraysStrip dcstrip;
    switch(topologyGL)
    {
    case GL_TRIANGLE_STRIP:
    case GL_QUAD_STRIP:
    case GL_LINE_STRIP:
        dcstrip.cmd.first = 0;
        dcstrip.cmd.count = indexCount;
        cmd = std::string((const char*)&dcstrip,sizeof(Token_DrawArraysStrip));
        break;
    default:
        dc.cmd.first = 0;
        dc.cmd.count = indexCount;
        cmd = std::string((const char*)&dc,sizeof(Token_DrawArrays));
        break;
    }
    return cmd;
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildViewportCommand(GLint x, GLint y, GLsizei width, GLsizei height)
{
    std::string cmd;
    Token_Viewport dc;
    dc.cmd.x = x;
    dc.cmd.y = y;
    dc.cmd.width = width;
    dc.cmd.height = height;
    cmd = std::string((const char*)&dc,sizeof(Token_Viewport));
    return cmd;
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildBlendColorCommand(GLclampf red,GLclampf green,GLclampf blue,GLclampf alpha)
{
    std::string cmd;
    Token_BlendColor dc;
    dc.cmd.red = red;
    dc.cmd.green = green;
    dc.cmd.blue = blue;
    dc.cmd.alpha = alpha;
    cmd = std::string((const char*)&dc,sizeof(Token_BlendColor));
    return cmd;
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildStencilRefCommand(GLuint frontStencilRef, GLuint backStencilRef)
{
    std::string cmd;
    Token_StencilRef dc;
    dc.cmd.frontStencilRef = frontStencilRef;
    dc.cmd.backStencilRef = backStencilRef;
    cmd = std::string((const char*)&dc,sizeof(Token_StencilRef));
    return cmd;
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildPolygonOffsetCommand(GLfloat scale, GLfloat bias)
{
    std::string cmd;
    Token_PolygonOffset dc;
    dc.cmd.bias = bias;
    dc.cmd.scale = scale;
    cmd = std::string((const char*)&dc,sizeof(Token_PolygonOffset));
    return cmd;
}

//------------------------------------------------------------------------------
// 
//------------------------------------------------------------------------------
std::string buildScissorCommand(GLint x, GLint y, GLsizei width, GLsizei height)
{
    std::string cmd;
    Token_Scissor dc;
    dc.cmd.x = x;
    dc.cmd.y = y;
    dc.cmd.width = width;
    dc.cmd.height = height;
    cmd = std::string((const char*)&dc,sizeof(Token_Scissor));
    return cmd;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool initBuffersGrid()
{
    //
    // Grid floor
    //
    glGenBuffers(1, &s_vboGrid);
    vec3f *data = new vec3f[GRIDDEF*4];
    vec3f *p = data;
    int j=0;
    for(int i=0; i<GRIDDEF; i++)
    {
        *(p++) = vec3f(-GRIDSZ, 0.0, GRIDSZ*(-1.0f+2.0f*(float)i/(float)GRIDDEF));
        *(p++) = vec3f( GRIDSZ*(1.0f-2.0f/(float)GRIDDEF), 0.0, GRIDSZ*(-1.0f+2.0f*(float)i/(float)GRIDDEF));
        *(p++) = vec3f(GRIDSZ*(-1.0f+2.0f*(float)i/(float)GRIDDEF), 0.0, -GRIDSZ);
        *(p++) = vec3f(GRIDSZ*(-1.0f+2.0f*(float)i/(float)GRIDDEF), 0.0, GRIDSZ*(1.0f-2.0f/(float)GRIDDEF));
    }
    s_vboGridSz = sizeof(vec3f)*GRIDDEF*4;
    glNamedBufferDataEXT(s_vboGrid,s_vboGridSz , data[0].vec_array, GL_STATIC_DRAW);
    // make the buffer resident and get its pointer
    glGetNamedBufferParameterui64vNV(s_vboGrid, GL_BUFFER_GPU_ADDRESS_NV, &s_vboGridAddr);
    glMakeNamedBufferResidentNV(s_vboGrid, GL_READ_ONLY);
    delete [] data;
    //
    // Target Cross
    //
    glGenBuffers(1, &s_vboCross);
    vec3f crossVtx[6] = {
        vec3f(-CROSSSZ, 0.0f, 0.0f), vec3f(CROSSSZ, 0.0f, 0.0f),
        vec3f(0.0f, -CROSSSZ, 0.0f), vec3f(0.0f, CROSSSZ, 0.0f),
        vec3f(0.0f, 0.0f, -CROSSSZ), vec3f(0.0f, 0.0f, CROSSSZ),
    };
    s_vboCrossSz = sizeof(vec3f)*6;
    glNamedBufferDataEXT(s_vboCross,s_vboCrossSz , crossVtx[0].vec_array, GL_STATIC_DRAW);
    // make the buffer resident and get its pointer
    glGetNamedBufferParameterui64vNV(s_vboCross, GL_BUFFER_GPU_ADDRESS_NV, &s_vboCrossAddr);
    glMakeNamedBufferResidentNV(s_vboCross, GL_READ_WRITE);
    return true;
}
//------------------------------------------------------------------------------
// cleanup commandList for the Grid
//------------------------------------------------------------------------------
void cleanTokenBufferGrid()
{
    glDeleteBuffers(1, &s_tokenBufferGrid.bufferID);
    for(int i=0; i<s_commandGrid.stateGroups.size(); i++)
        glDeleteStatesNV(1, &s_commandGrid.stateGroups[i]);
    s_tokenBufferGrid.bufferID = 0;
    s_tokenBufferGrid.data.clear();
    s_commandGrid.clear();
    s_bRecordGrid       = true;
}
//------------------------------------------------------------------------------
// build commandList for the Grid
//------------------------------------------------------------------------------
bool recordTokenBufferGrid(GLuint fbo)
{
    cleanTokenBufferGrid();
    GLuint stateId;
    g_shaderGrid.bindShader();
    //
    // enable/disable vertex attributes for our needs
    //
	glEnableVertexAttribArray(0);
    for(int i=1; i<16; i++)
        glDisableVertexAttribArray(i);

    glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    //
    // Bind the VBO essentially for the *stride information*
    //
    glBindVertexBuffer(0, s_vboGrid, 0, sizeof(vec3f));
    //
    // Vertex attribute format
    //
    glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    //
    // build address command for attribute #0
    // - assign UBO addresses to uniform index for a given shader stage
    // - assign a VBO address to attribute 0 (only one needed here)
    // - issue draw commands
    //
    std::string data = buildUniformAddressCommand(UBO_MATRIX, g_uboMatrix.Addr, g_uboMatrix.Sz, STAGE_VERTEX);
    data            += buildUniformAddressCommand(UBO_LIGHT, g_uboLight.Addr, g_uboLight.Sz, STAGE_FRAGMENT);
    data            += buildAttributeAddressCommand(0, s_vboGridAddr, s_vboGridSz);
    data            += buildDrawArraysCommand(GL_LINES, GRIDDEF*4);
    //
    // build another drawcall for the target cross
    //
    data            += buildLineWidthCommand(4.0);
    data            += buildAttributeAddressCommand(0, s_vboCrossAddr, s_vboCrossSz);
    data            += buildDrawArraysCommand(GL_LINES, 6);
    s_tokenBufferGrid.data = data;                      // token buffer containing commands
    //
    // Create a state and capture the state-machine of OpenGL
    // *ALL* previously declared states will be taken, plus the topology passed as argument
    //
    glCreateStatesNV(1, &stateId);
    glStateCaptureNV(stateId, GL_LINES);
    emucmdlist::StateCaptureNV(stateId, GL_LINES); // for emulation purpose
    emucmdlist::StateCaptureNV_Extra(stateId, sizeof(vec3f), 3,0, 0,0,0); // for emulation purpose
    //
    // Generate the token buffer in which we copy s_tokenBufferGrid.data
    //
    glGenBuffers(1, &s_tokenBufferGrid.bufferID);
    glNamedBufferDataEXT(s_tokenBufferGrid.bufferID, data.size(), &data[0], GL_STATIC_DRAW);
    glGetNamedBufferParameterui64vNV(s_tokenBufferGrid.bufferID, GL_BUFFER_GPU_ADDRESS_NV, &s_tokenBufferGrid.bufferAddr);
    glMakeNamedBufferResidentNV(s_tokenBufferGrid.bufferID, GL_READ_WRITE);
    //
    // Build the tables for the command-state batch
    //
    // token buffer for the viewport setting
    s_commandGrid.pushBatch(stateId, fbo, 
        g_tokenBufferViewport.bufferAddr, 
        &g_tokenBufferViewport.data[0], 
        g_tokenBufferViewport.data.size() );
    // token buffer for drawing the grid
    s_commandGrid.pushBatch(stateId, fbo, 
        s_tokenBufferGrid.bufferAddr,
        &s_tokenBufferGrid.data[0],
        s_tokenBufferGrid.data.size() );

    LOGOK("Token buffer created for Grid\n");
    LOGFLUSH();

    s_bRecordGrid = false; // done recording

    glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    return true;
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void displayGrid(const InertiaCamera& camera, const mat4f projection, GLuint fbo)
{
    //
    // Update what is inside buffers
    //
    g_globalMatrices.mVP = projection * camera.m4_view;
    g_globalMatrices.mW = mat4f(array16_id);
    glNamedBufferSubDataEXT(g_uboMatrix.Id, 0, sizeof(g_globalMatrices), &g_globalMatrices);
    //
    // The cross vertex change is an example on how command-list are compatible with changing
    // what is inside the vertex buffers. VBOs are outside of the token buffers...
    //
    const vec3f& p = camera.curFocusPos;
    vec3f crossVtx[6] = {
        vec3f(p.x-CROSSSZ, p.y, p.z), vec3f(p.x+CROSSSZ, p.y, p.z),
        vec3f(p.x, p.y-CROSSSZ, p.z), vec3f(p.x, p.y+CROSSSZ, p.z),
        vec3f(p.x, p.y, p.z-CROSSSZ), vec3f(p.x, p.y, p.z+CROSSSZ),
    };
    glNamedBufferSubDataEXT(s_vboCross, 0, sizeof(vec3f)*6, crossVtx);
    // ------------------------------------------------------------------------------------------
    // Case of recorded command-list
    //
    if(g_bUseCommandLists)
    {
        //
        // Record draw commands if not already done
        //
        if(s_bRecordGrid)
            recordTokenBufferGrid(fbo);
        //
        // execute the commands from the token buffer
        //
        if(g_bUseEmulation)
        {
            //
            // an emulation of what got captured
            //
            emucmdlist::nvtokenRenderStatesSW(&s_commandGrid.dataPtrs[0], &s_commandGrid.sizes[0], 
                &s_commandGrid.stateGroups[0], &s_commandGrid.fbos[0], int(s_commandGrid.numItems ) );
        } else {
            //
            // real Command-list's Token buffer with states execution
            //
            glDrawCommandsStatesAddressNV(
                &s_commandGrid.dataGPUPtrs[0], 
                &s_commandGrid.sizes[0], 
                &s_commandGrid.stateGroups[0], 
                &s_commandGrid.fbos[0], 
                int(s_commandGrid.numItems )); 
        }
        return;
    }
    // ------------------------------------------------------------------------------------------
    // Case of regular rendering
    //
    g_shaderGrid.bindShader();
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    if(g_bUseGridBindless)
    {
        // --------------------------------------------------------------------------------------
        // Using NVIDIA VBUM
        //
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX, 0);  // put them to zero for debug purpose
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_LIGHT, 0);
        //
        // Enable Bindless
        //
        glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
        glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
        glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);

        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0, s_vboGridAddr, s_vboGridSz);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATRIX, g_uboMatrix.Addr, g_uboMatrix.Sz);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_LIGHT, g_uboLight.Addr, g_uboLight.Sz);
        // debug test: is alignment good ?
        //{
        //    int offsetAlignment;
        //    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &offsetAlignment);
        //    assert(sizeof(MaterialBuffer) == offsetAlignment);
        //}
        // BUG? Need to add this for proper stride
        glBindVertexBuffer(0, s_vboGrid, 0, sizeof(vec3f));
        glVertexAttribFormat(0,3, GL_FLOAT, GL_FALSE, 0);
        // or this one does have the stride:
        //glVertexAttribFormatNV(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3f));
        glDrawArrays(GL_LINES, 0, GRIDDEF*4);

        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0, s_vboCrossAddr, s_vboCrossSz);
        glDrawArrays(GL_LINES, 0, 6);

        glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
        glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
        glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    } else {
        // --------------------------------------------------------------------------------------
        // Using regular VBO
        //
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX, g_uboMatrix.Id);
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_LIGHT, g_uboLight.Id);
        glBindVertexBuffer(0, s_vboGrid, 0, sizeof(vec3f));
        glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
        //
        // Draw!
        //
        glDrawArrays(GL_LINES, 0, GRIDDEF*4);

        glBindVertexBuffer(0, s_vboCross, 0, sizeof(vec3f));
        glDrawArrays(GL_LINES, 0, 6);
    }
    glDisableVertexAttribArray(0);
    //g_shaderGrid.unbindShader();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool MyWindow::init()
{
	if(!WindowInertiaCamera::init())
		return false;
    //
    // Check mandatory extensions
    //
    if(!glewIsSupported("GL_NV_vertex_buffer_unified_memory"))// GL_NV_uniform_buffer_unified_memory"))
    {
        LOGE("Failed to initialize NVIDIA Bindless graphics\n");
        return false;
    }
    //
    // Initialize basic command-list extension stuff
    //
    extern int initNVcommandList();
    if(!initNVcommandList())
    {
        LOGE("Failed to initialize CommandList extension\n");
        return true;
    }
    initTokenInternals();
    //
    // some offscreen buffer
    //
    m_fboBox.Initialize(m_winSz[0], m_winSz[1], g_Supersampling, s_MSAA, 0);
    m_fboBox.MakeResourcesResident();

    //
    // UI
    //
#ifdef USESVCUI
    initMFCUIBase(0, m_winSz[1]+40, m_winSz[0], 300);
#endif
    //
    // easy Toggles
    //
#ifdef USESVCUI
	class EventUI: public IEventsWnd
	{
	public:
		void Button(IWindow *pWin, int pressed)
            { reinterpret_cast<MyWindow*>(pWin->GetUserData())->m_bAdjustTimeScale = true; };
        void ComboSelectionChanged(IControlCombo *pWin, unsigned int selectedidx)
            {   
                if(!strcmp(pWin->GetID(), "DS"))
                {
                    MyWindow* p = reinterpret_cast<MyWindow*>(pWin->GetUserData());
                    p->downsamplingMode = (NVFBOBox::DownSamplingTechnique)pWin->GetItemData(selectedidx);
                }
                else if(!strcmp(pWin->GetID(), "MSAA"))
                {
                    s_MSAA = pWin->GetItemData(selectedidx);
                    MyWindow* p = reinterpret_cast<MyWindow*>(pWin->GetUserData());
                    p->m_fboBox.resize(p->m_winSz[0], p->m_winSz[1], g_Supersampling, s_MSAA);
                    p->m_fboBox.MakeResourcesResident();
                    FOREACHMODEL(update_fbo_target(p->m_fboBox.GetFBO()));
                    // the comman-list needs to be rebuilt when FBO's resources changed
                    #if 1
                    FOREACHMODEL(invalidateCmdList());
                    s_bRecordGrid = true;
                    #else
                    init_command_list();
                    #endif
                }
                else if(!strcmp(pWin->GetID(), "SS"))
                {
                    g_Supersampling = 0.1f * (float)pWin->GetItemData(selectedidx);
                    MyWindow* p = reinterpret_cast<MyWindow*>(pWin->GetUserData());
                    p->m_fboBox.resize(p->m_winSz[0], p->m_winSz[1], g_Supersampling, s_MSAA);
                    p->m_fboBox.MakeResourcesResident();
                    //
                    // update the token buffer in which the viewport setup happens for token rendering
                    //
                    updateViewportTokenBufferAndLineWidth(0,0,p->m_fboBox.getBufferWidth(),p->m_fboBox.getBufferHeight(), g_Supersampling);
                    //
                    // remember that commands must know which FBO is targeted
                    //
                    FOREACHMODEL(update_fbo_target(p->m_fboBox.GetFBO()));
                    // the comman-list needs to be rebuilt when FBO's resources changed
                    #if 1
                    FOREACHMODEL(invalidateCmdList());
                    s_bRecordGrid = true;
                    #else
                    init_command_list();
                    #endif
                }
                else
                {
                    FOREACHMODEL(deleteCommandListData());
                    g_TokenBufferGrouping = selectedidx;
                }
            }
	};
	static EventUI eventUI;
	//g_pWinHandler->CreateCtrlButton("TIMESCALE", "re-scale timing", g_pToggleContainer)
	//	->SetUserData(this)
	//	->Register(&eventUI);

    g_pToggleContainer->UnFold(false);
    IControlCombo* pCombo = g_pWinHandler->CreateCtrlCombo("CLMODE", "CommandList style", g_pToggleContainer);
	pCombo->SetUserData(this)->Register(&eventUI);
    pCombo->AddItem("Unsorted primitive types", 0);
    pCombo->AddItem("Sort on primitive types", 1);
    pCombo->SetSelectedByIndex(0);

    pCombo = g_pWinHandler->CreateCtrlCombo("MSAA", "MSAA", g_pToggleContainer);
    pCombo->AddItem("MSAA 1x", 1);
    pCombo->AddItem("MSAA 4x", 4);
    pCombo->AddItem("MSAA 8x", 8);
	pCombo->SetUserData(this)->Register(&eventUI);
    pCombo->SetSelectedByData(s_MSAA);

    pCombo = g_pWinHandler->CreateCtrlCombo("SS", "Supersampling", g_pToggleContainer);
    pCombo->AddItem("SS 1", 10);
    pCombo->AddItem("SS 1.5", 15);
    pCombo->AddItem("SS 2.0", 20);
	pCombo->SetUserData(this)->Register(&eventUI);
    pCombo->SetSelectedByData(g_Supersampling*10);
    pCombo->PeekMyself();

    pCombo = g_pWinHandler->CreateCtrlCombo("DS", "DownSampling mode", g_pToggleContainer);
	pCombo->SetUserData(this)->Register(&eventUI);
    pCombo->AddItem("1 tap", NVFBOBox::DS1);
    pCombo->AddItem("5 taps", NVFBOBox::DS2);
    pCombo->AddItem("9 taps on alpha", NVFBOBox::DS3);
    pCombo->SetSelectedByIndex(1);

    g_pToggleContainer->UnFold();

#if 1
    //
    // This code is to adjust models in the scene. Then output the values with '2' or button
    // and create the scene file (used with -i <file>)
    //
	class EventUI2: public IEventsWnd
	{
	public:
		void Button(IWindow *pWin, int pressed)
        {
            if(!strcmp(pWin->GetID(), "CURPRINT"))
            {
                if(s_curObject < s_bk3dModels.size())
                    s_bk3dModels[s_curObject]->printPosition();
            }
        };
        void ScalarChanged(IControlScalar *pWin, float &v, float prev)
        {
            if(!strcmp(pWin->GetID(), "CURO"))
            {
                if(s_bk3dModels.size() > (int)v)
                {
                    LOGI("Object %d %s now current\n", (int)v, s_bk3dModels[(int)v]->m_name.c_str() );
                    g_pWinHandler->VariableBind(g_pWinHandler->Get("CURX"), &(s_bk3dModels[(int)v]->m_posOffset.x));
                    g_pWinHandler->VariableBind(g_pWinHandler->Get("CURY"), &(s_bk3dModels[(int)v]->m_posOffset.y));
                    g_pWinHandler->VariableBind(g_pWinHandler->Get("CURZ"), &(s_bk3dModels[(int)v]->m_posOffset.z));
                    g_pWinHandler->VariableBind(g_pWinHandler->Get("CURS"), &(s_bk3dModels[(int)v]->m_scale));
                }
            }
        };
    };
    static EventUI2 eventUI2;
    (g_pTweakContainer = g_pWinHandler->CreateWindowFolding("TWEAK", "Tweaks", NULL))
        ->SetLocation(0+(m_winSz[0]*70/100), m_winSz[1]+40)
        ->SetSize(m_winSz[0]*30/100, 300)
        ->SetVisible();
    g_pWinHandler->CreateCtrlButton("CURPRINT", "Ouput Pos-scale", g_pTweakContainer)->Register(&eventUI2);
    g_pWinHandler->VariableBind(g_pWinHandler->CreateCtrlScalar("CURO", "Cur Object", g_pTweakContainer)
        ->SetBounds(0.0, 10.)->SetIntMode()->Register(&eventUI2), &s_curObject);
    g_pWinHandler->CreateCtrlScalar("CURX", "CurX", g_pTweakContainer)
        ->SetBounds(-1.0, 1.0);
    g_pWinHandler->CreateCtrlScalar("CURY", "CurY", g_pTweakContainer)
        ->SetBounds(-1.0, 1.0);
    g_pWinHandler->CreateCtrlScalar("CURZ", "CurZ", g_pTweakContainer)
        ->SetBounds(-1.0, 1.0);
    g_pWinHandler->CreateCtrlScalar("CURS", "Cur scale", g_pTweakContainer)
        ->SetBounds(0.0, 2.0);
    g_pWinHandler->VariableBind(g_pWinHandler->CreateCtrlScalar("ITEMS", "MaxItems", g_pTweakContainer)
        ->SetBounds(-1.0, 1000.0)->SetIntMode(), &s_maxItems);
    g_pTweakContainer->UnFold();
    g_pTweakContainer->SetVisible(0);
#endif

#endif
    addToggleKeyToMFCUI(' ', &m_realtime.bNonStopRendering, "space: toggles continuous rendering\n");
    addToggleKeyToMFCUI('c', &g_bUseCommandLists, "'c': use Commandlist\n");
    addToggleKeyToMFCUI('e', &g_bUseEmulation, "'e': use Commandlist EMULATION\n");
    addToggleKeyToMFCUI('l', &g_bUseCallCommandListNV, "'l': use glCallCommandListNV\n");
    //addToggleKeyToMFCUI('b', &g_bUseGridBindless, "'b': regular / bindless for the grid\n");
    addToggleKeyToMFCUI('o', &g_bDisplayObject, "'o': toggles object display\n");
    addToggleKeyToMFCUI('g', &s_bDisplayGrid, "'g': toggles grid display\n");
    addToggleKeyToMFCUI('s', &s_bStats, "'s': toggle stats\n");
    addToggleKeyToMFCUI('a', &s_bCameraAnim, "'a': animate camera\n");

    return true;
}

//------------------------------------------------------------------------------
// this is an example of creating a piece of token buffer that would be put
// as a header before every single glDrawCommandsStatesAddressNV so that
// proper view setup (viewport) get properly done without relying on any
// typical OpenGL command.
// this approach is good avoid messing with OpenGL state machine and later could
// prevent extra driver validation
//------------------------------------------------------------------------------
void updateViewportTokenBufferAndLineWidth(GLint x, GLint y, GLsizei width, GLsizei height, float lineW)
{
    // could have way more commands here...
    // ...
    if(g_tokenBufferViewport.bufferAddr == NULL)
    {
        // first time: create
        g_tokenBufferViewport.data  = buildViewportCommand(x,y,width, height);
        g_tokenBufferViewport.data  += buildLineWidthCommand(lineW);
        glGenBuffers(1, &g_tokenBufferViewport.bufferID);
        glNamedBufferDataEXT(
            g_tokenBufferViewport.bufferID, 
            g_tokenBufferViewport.data.size(), 
            &g_tokenBufferViewport.data[0], GL_STATIC_DRAW);
        glGetNamedBufferParameterui64vNV(
            g_tokenBufferViewport.bufferID, 
            GL_BUFFER_GPU_ADDRESS_NV, 
            &g_tokenBufferViewport.bufferAddr);
        glMakeNamedBufferResidentNV(
            g_tokenBufferViewport.bufferID, 
            GL_READ_WRITE);
    } else {
        // change arguments in the token buffer: better keep the same system memory pointer, too:
        // CPU system memory used by the command-list compilation
        // this is a simple use-case here: only one token cmd with related structure...
        //
        Token_Viewport *dc = (Token_Viewport *)&g_tokenBufferViewport.data[0];
        dc->cmd.x = x;
        dc->cmd.y = y;
        dc->cmd.width = width;
        dc->cmd.height = height;
        Token_LineWidth *lw = (Token_LineWidth *)(dc+1);
        lw->cmd.lineWidth = lineW;
        //
        // just update. Offset is always 0 in our simple case
        glNamedBufferSubDataEXT(
            g_tokenBufferViewport.bufferID, 
            0/*offset*/, g_tokenBufferViewport.data.size(), 
            &g_tokenBufferViewport.data[0]);
    }
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool initGraphics()
{
    //
    // Shader compilation
    //
    g_shaderGrid.addVertexShaderFromString(g_glslv_grid);
    g_shaderGrid.addFragmentShaderFromString(g_glslf_grid);
    if(!g_shaderGrid.link())
        return false;
    
    //
    // Create some UBO for later share their 64 bits
    //
    glGenBuffers(1, &g_uboMatrix.Id);
    g_uboMatrix.Sz = sizeof(MatrixBufferGlobal);
    glNamedBufferDataEXT(g_uboMatrix.Id, g_uboMatrix.Sz, &g_globalMatrices, GL_STREAM_DRAW);
    glGetNamedBufferParameterui64vNV(g_uboMatrix.Id, GL_BUFFER_GPU_ADDRESS_NV, (GLuint64EXT*)&g_uboMatrix.Addr);
    glMakeNamedBufferResidentNV(g_uboMatrix.Id, GL_READ_WRITE);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX, g_uboMatrix.Id);
    //
    // Trivial Light info...
    //
    glGenBuffers(1, &g_uboLight.Id);
    g_uboLight.Sz = sizeof(LightBuffer);
    glNamedBufferDataEXT(g_uboLight.Id, g_uboLight.Sz, &s_light, GL_STREAM_DRAW);
    glGetNamedBufferParameterui64vNV(g_uboLight.Id, GL_BUFFER_GPU_ADDRESS_NV, (GLuint64EXT*)&g_uboLight.Addr);
    glMakeNamedBufferResidentNV(g_uboLight.Id, GL_READ_WRITE);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_LIGHT, g_uboLight.Id);
    //
    // Misc OGL setup
    //
    glClearColor(0.0f, 0.1f, 0.15f, 1.0f);
    glGenVertexArrays(1, &s_vao);
    glBindVertexArray(s_vao);
    //
    // 3D Model shared stuff (shaders)
    //
    Bk3dModel::initGraphics_bk3d();
    FOREACHMODEL(loadModel());
    //
    // Creation of the buffer object for the Grid
    // will make them resident
    //
    initBuffersGrid();
    return true;
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyWindow::shutdown()
{
#ifdef USESVCUI
    shutdownMFCUI();
#endif
	WindowInertiaCamera::shutdown();

	m_fboBox.Finish();

    for(int i=0; i<s_bk3dModels.size(); i++)
    {
        delete s_bk3dModels[i];
    }
    s_bk3dModels.clear();
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyWindow::reshape(int w, int h)
{
    WindowInertiaCamera::reshape(w, h);
    m_fboBox.resize(w, h);
    m_fboBox.MakeResourcesResident();
    //
    // update the token buffer in which the viewport setup happens for token rendering
    //
    updateViewportTokenBufferAndLineWidth(0,0,m_fboBox.getBufferWidth(),m_fboBox.getBufferHeight(), g_Supersampling);
    //
    // the FBOs were destroyed and rebuilt
    // associated 64 bits pointers (as resident resources) might have changed
    // we need to rebuild the *command-lists*
    //
    FOREACHMODEL(init_command_list());
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
#define KEYTAU 0.10f
void MyWindow::keyboard(NVPWindow::KeyCode key, MyWindow::ButtonAction action, int mods, int x, int y)
{
	WindowInertiaCamera::keyboard(key, action, mods, x, y);

	if(action == MyWindow::BUTTON_RELEASE)
        return;
    switch(key)
    {
    case NVPWindow::KEY_F1:
        break;
	//...
    case NVPWindow::KEY_F12:
        break;
    }
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyWindow::keyboardchar( unsigned char key, int mods, int x, int y )
{
    WindowInertiaCamera::keyboardchar(key, mods, x, y);
    switch(key)
    {
    case '1':
        m_camera.print_look_at();
    break;
    case '2': // dumps the position and scale of current object
        if(s_curObject >= s_bk3dModels.size())
            break;
        s_bk3dModels[s_curObject]->printPosition();
    break;
    case '0':
        m_bAdjustTimeScale = true;
    case 'h':
        LOGI(s_sampleHelpCmdLine);
        s_helpText = HELPDURATION;
    break;
    }
#ifdef USESVCUI
    flushMFCUIToggle(key);
#endif
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void MyWindow::display()
{
  WindowInertiaCamera::display();
  float dt = (float)m_realtime.getTiming();
  //
  // Simple camera change for animation
  //
  if(s_bCameraAnim)
  {
      if(s_cameraAnim.empty())
      {
          LOGE("NO Animation loaded (-i <file>)\n");
          s_bCameraAnim = false;
          #ifdef USESVCUI
          g_pWinHandler->VariableFlush(&s_bCameraAnim);
          #endif
      }
      s_cameraAnimIntervals -= dt;
      if( (m_camera.eyeD <= 0.01/*m_camera.epsilon*/)
        &&(m_camera.focusD <= 0.01/*m_camera.epsilon*/)
        &&(s_cameraAnimIntervals <= 0.0) )
      {
          //LOGI("Anim step %d\n", s_cameraAnimItem);
          s_cameraAnimIntervals = s_cameraAnim[s_cameraAnimItem].sleep;
          m_camera.look_at(s_cameraAnim[s_cameraAnimItem].eye, s_cameraAnim[s_cameraAnimItem].focus);
          m_camera.tau = 0.4f;
          s_cameraAnimItem++;
          if(s_cameraAnimItem >= s_cameraAnim.size())
              s_cameraAnimItem = 0;
      }
  }
  //
  // render the scene
  //
  std::string stats;
  static std::string hudStats = "...";
  {
    nv_helpers_gl::Profiler::FrameHelper helper(g_profiler,sysGetTime(), 2.0, stats);
    NXPROFILEFUNC(__FUNCTION__);
    PROFILE_SECTION("MyWindow::display");

    // bind the FBO
    m_fboBox.Activate();

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    //glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    GLuint fbo = m_fboBox.GetFBO();
    //
    // Grid floor
    //
    if(s_bDisplayGrid)
        displayGrid(m_camera, m_projection, fbo);
    //
    // Display Meshes
    //
    if(g_bDisplayObject)
        FOREACHMODEL(displayObject(m_camera.m4_view, m_projection, fbo, s_maxItems));
    //
    // copy FBO to backbuffer
    //
    m_fboBox.Deactivate();
    m_fboBox.Draw(downsamplingMode, 0,0, m_winSz[0], m_winSz[1], NULL);
    //
    // additional HUD stuff
    //
	WindowInertiaCamera::beginDisplayHUD();
    s_helpText -= dt;
    m_oglTextBig.drawString(5, 5, "('h' for help)", 1, vec4f(0.8,0.8,1.0,0.5f).vec_array);
    float h = 30;
    if(s_bStats)
        h += m_oglTextBig.drawString(5, m_winSz[1]-h, hudStats.c_str(), 0, vec4f(0.8,0.8,1.0,0.5).vec_array);
    if(s_helpText > 0)
    {
        // camera help
        const char *txt = getHelpText();
        h += m_oglTextBig.drawString(5, m_winSz[1]-h, txt, 0, vec4f(0.8,0.8,1.0,s_helpText/HELPDURATION).vec_array);
        h += m_oglTextBig.drawString(5, m_winSz[1]-h, s_sampleHelp, 0, vec4f(0.8,0.8,1.0,s_helpText/HELPDURATION).vec_array);
    }
	WindowInertiaCamera::endDisplayHUD();
    {
        //PROFILE_SECTION("SwapBuffers");
        swapBuffers();
    }
  }
  //
  // Stats
  //
  if (s_bStats && (!stats.empty()))
  {
      char tmp[200];
    //LOGOK("%s\n",stats.c_str());
    Bk3dModel::Stats modelstats = {0,0,0,0};
    FOREACHMODEL(addStats(modelstats));
    hudStats = stats; // make a copy for the hud display
    sprintf(tmp,"%.0f primitives/S %.0f drawcalls/S\n", (float)modelstats.primitives/dt, (float)modelstats.drawcalls/dt);
    hudStats += tmp;
    sprintf(tmp,"%.0f attr.updates/S %.0f uniform updates/S\n", (float)modelstats.attr_update/dt, (float)modelstats.uniform_update/dt);
    hudStats += tmp;
    sprintf(tmp,"All Models together: %d Prims; %d drawcalls; %d attribute update; %d uniform update\n"
        , modelstats.primitives, modelstats.drawcalls, modelstats.attr_update, modelstats.uniform_update);
    hudStats += tmp;
  }

}
//------------------------------------------------------------------------------
// Main initialization point
//------------------------------------------------------------------------------
void readConfigFile(const char* fname)
{
    FILE *fp = fopen(fname, "r");
    if(!fp)
    {
        std::string modelPathName;
        modelPathName = std::string(PROJECT_RELDIRECTORY) + std::string(fname);
        fp = fopen(modelPathName.c_str(), "r");
        if(!fp)
        {
            modelPathName = std::string(PROJECT_ABSDIRECTORY) + std::string(fname);
            fp = fopen(modelPathName.c_str(), "r");
            if(!fp) {
                LOGE("Couldn't Load %s\n", fname);
                return;
            }
        }
    }
    int nModels = 0;
    int res = 0;
    // Models and position/scale
    fscanf(fp, "%d", &nModels);
    for(int i=0; i<nModels; i++)
    {
        char name[200];
        vec3f pos;
        float scale;
        res = fscanf(fp, "%s\n", &name);
        if(res != 1) { LOGE("Error during parsing\n"); return; }
        res = fscanf(fp, "%f %f %f %f\n", &pos.x, &pos.y, &pos.z, &scale);
        if(res != 4) { LOGE("Error during parsing\n"); return; }
        LOGI("Load Model set to %s\n", name);
        s_bk3dModels.push_back(new Bk3dModel(name, &pos, &scale));
    }
    // camera movement
    int nCameraPos = 0;
    res = fscanf(fp, "%d", &nCameraPos);
    if(res == 1)
    {
        for(int i=0; i<nCameraPos; i++)
        {
            CameraAnim cam;
            res = fscanf(fp, "%f %f %f %f %f %f %f\n"
                , &cam.eye.x, &cam.eye.y, &cam.eye.z
                , &cam.focus.x, &cam.focus.y, &cam.focus.z
                , &cam.sleep);
            if(res != 7) { LOGE("Error during parsing\n"); return; }
            s_cameraAnim.push_back(cam);
        }
    }
    fclose(fp);
}
//------------------------------------------------------------------------------
// Main initialization point
//------------------------------------------------------------------------------
int sample_main(int argc, const char** argv)
{
    // you can create more than only one
    static MyWindow myWindow;
    NVPWindow::ContextFlags context(
    4,      //major;
    3,      //minor;
    false,   //core;
    1,      //MSAA;
    24,     //depth bits
    8,      //stencil bits
    true,   //debug;
    false,  //robust;
    false,  //forward;
    NULL   //share;
    );

    if(!myWindow.create("Empty", &context, 1280,720))
    {
        LOGE("Failed to initialize the sample\n");
        return false;
    }

    // args
    for(int i=1; i<argc; i++)
    {
        if(argv[i][0] != '-')
        {
            const char* name = argv[i];
            LOGI("Load Model set to %s\n", name);
            s_bk3dModels.push_back(new Bk3dModel(name));
            continue;
        }
        if(strlen(argv[i]) <= 1)
            continue;
        switch(argv[i][1])
        {
        case 'v':
            if(i == argc-1)
                return false;
            g_MaxBOSz = atoi(argv[++i]);
            LOGI("VBO max Size set to %dMb\n", g_MaxBOSz);
            break;
        case 'm':
            if(i == argc-1)
                return false;
            {
                const char* name = argv[++i];
                LOGI("Load Model set to %s\n", name);
                s_bk3dModels.push_back(new Bk3dModel(name));
            }
            break;
        case 'c':
            g_bUseCommandLists = atoi(argv[++i]) ? true : false;
            LOGI("g_bUseCommandLists set to %s\n", g_bUseCommandLists ? "true":"false");
            break;
        case 'l':
            g_bUseCallCommandListNV = atoi(argv[++i]) ? true : false;
            LOGI("g_bUseCallCommandListNV set to %s\n", g_bUseCallCommandListNV ? "true":"false");
            break;
        case 'b':
            g_bUseGridBindless = atoi(argv[++i]) ? true : false;
            LOGI("g_bUseGridBindless set to %s\n", g_bUseGridBindless ? "true":"false");
            break;
        case 'o':
            g_bDisplayObject = atoi(argv[++i]) ? true : false;
            LOGI("g_bDisplayObject set to %s\n", g_bDisplayObject ? "true":"false");
            break;
        case 'g':
            s_bDisplayGrid = atoi(argv[++i]) ? true : false;
            LOGI("s_bDisplayGrid set to %s\n", s_bDisplayGrid ? "true":"false");
            break;
        case 's':
            s_bStats = atoi(argv[++i]) ? true : false;
            LOGI("s_bStats set to %s\n", s_bStats ? "true":"false");
            break;
        case 'a':
            s_bCameraAnim = atoi(argv[++i]) ? true : false;
            LOGI("s_bCameraAnim set to %s\n", s_bCameraAnim ? "true":"false");
            break;
        case 'q':
            s_MSAA = atoi(argv[++i]);
#ifdef USESVCUI
            if(g_pWinHandler) g_pWinHandler->GetCombo("MSAA")->SetSelectedByData(s_MSAA);
#endif
            LOGI("s_MSAA set to %d\n", s_MSAA);
            break;
        case 'r':
            g_Supersampling = atof(argv[++i]);
#ifdef USESVCUI
            if(g_pWinHandler) g_pWinHandler->GetCombo("SS")->SetSelectedByData(g_Supersampling*10);
#endif
            LOGI("g_Supersampling set to %.2f\n", g_Supersampling);
            break;
        case 'i':
            {
                const char* name = argv[++i];
                LOGI("Load Model set to %s\n", name);
                readConfigFile(name);
            }
            break;
        case 'd':
#ifdef USESVCUI
            if(g_pTweakContainer) g_pTweakContainer->SetVisible(atoi(argv[++i]) ? 1 : 0);
#endif
            break;
        default:
            LOGE("Wrong command-line\n");
        case 'h':
            LOGI(s_sampleHelpCmdLine);
            break;
        }
    }
#ifdef NOGZLIB
#   define MODELNAME "Smobby_134.bk3d.gz"
#else
#   define MODELNAME "Smobby_134.bk3d.gz"
//#   define MODELNAME "Driveline_v134.bk3d.gz"
#endif
    if(s_bk3dModels.empty())
    {
        LOGI("Load default Model" MODELNAME "\n");
        s_bk3dModels.push_back(new Bk3dModel(MODELNAME));
    }

    initGraphics();

    myWindow.makeContextCurrent();
    myWindow.swapInterval(0);

    g_profiler.init();

    while(MyWindow::sysPollEvents(false) )
    {
        myWindow.idle();
    }
    return true;
}
