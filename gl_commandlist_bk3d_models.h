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
#include <assert.h>
#include "main.h"

#include "nv_math/nv_math.h"
#include "nv_math/nv_math_glsltypes.h"
using namespace nv_math;

#include "GLSLShader.h"
#include "gl_nv_command_list.h"
#include "nv_helpers_gl/profiler.hpp"

#include "nv_helpers_gl/WindowInertiaCamera.h"

#include "helper_fbo.h"

#include "emulate_commandlist.h"

#include "svcmfcui.h"
#ifdef USESVCUI
#   define  LOGFLUSH()  { g_pWinHandler->HandleMessageLoop_OnePass(); }
#else
#   define  LOGFLUSH()
#endif

#ifndef NOGZLIB
#   include "zlib.h"
#endif
#include "bk3dEx.h" // a baked binary format for few models

#if 1//SUPPORT_PROFILE
#define PROFILE_SECTION(name)   nv_helpers_gl::Profiler::Section _tempTimer(g_profiler ,name)
#define PROFILE_SPLIT()         g_profiler.accumulationSplit()
#else
#define PROFILE_SECTION(name)
#define PROFILE_SPLIT()
#endif

#define UBO_MATRIX   1
#define UBO_MATRIXOBJ 3
#define UBO_MATERIAL 2
#define UBO_LIGHT    0
#define TOSTR_(x) #x
#define TOSTR(x) TOSTR_(x)

//
// Let's assume we would put any matrix that don't get impacted by the local object transformation
//
NV_ALIGN(256, struct MatrixBufferGlobal
{ 
  mat4f mW; 
  mat4f mVP;
} );
//
// Let's assume these are the ones that can change for each object
// will used at an array of MatrixBufferObject
//
NV_ALIGN(256, struct MatrixBufferObject
{
    mat4f mO;
} );
//
// if we create arrays with a structure, we must be aligned according to
// GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT (to query)
//
NV_ALIGN(256, struct MaterialBuffer
{
    vec3f diffuse;
    float a;
} );

NV_ALIGN(256, struct LightBuffer
{
    vec3f dir;
} );

struct BO {
    GLuint      Id;
    GLuint      Sz;
    GLuint64    Addr;
};

//
// Shader stages for command-list
//
enum ShaderStages {
    STAGE_VERTEX,
    STAGE_TESS_CONTROL,
    STAGE_TESS_EVALUATION,
    STAGE_GEOMETRY,
    STAGE_FRAGMENT,
    STAGES,
};

//
// Put together all what is needed to give to the extension function
// for a token buffer
//
struct TokenBuffer
{
    GLuint                  bufferID;   // buffer containing all
    GLuint64EXT             bufferAddr; // buffer GPU-pointer
    std::string             data;       // bytes of data containing the structures to send to the driver
};
//
// Grouping together what is needed to issue a single command made of many states, fbos and Token Buffer pointers
//
struct CommandStatesBatch
{
    CommandStatesBatch() { numItems = 0; }
    void clear()
    {
        for(int i=0; i<stateGroups.size(); i++)
            glDeleteStatesNV(1, &stateGroups[i]);
        dataPtrs.clear();
        dataGPUPtrs.clear();
        sizes.clear();
        fbos.clear();
        stateGroups.clear();
        numItems = 0;
    }
    void pushBatch(GLuint stateGroup_, GLuint fbo_, GLuint64EXT dataGPUPtr_, const GLvoid* dataPtr_, GLsizei size_)
    {
        dataGPUPtrs.push_back(dataGPUPtr_);
        dataPtrs.push_back(dataPtr_);
        sizes.push_back(size_);
        stateGroups.push_back(stateGroup_);
        fbos.push_back(fbo_);
        numItems = fbos.size();
    }
    std::vector<GLuint64EXT> dataGPUPtrs;   // pointer in data where to locate each separate groups (for glListDrawCommandsStatesClientNV)
    std::vector<const GLvoid*>     dataPtrs;   // pointer in data where to locate each separate groups (for glListDrawCommandsStatesClientNV)
    std::vector<GLsizei>    sizes;      // sizes of each groups
    std::vector<GLuint>     stateGroups;// state-group IDs used for each groups
    std::vector<GLuint>     fbos;       // FBOs being used for each groups
    size_t                  numItems;   // == fbos.size() or sizes.size()...
};

//
// Externs
//
extern TokenBuffer g_tokenBufferViewport;

extern nv_helpers_gl::Profiler  g_profiler;

extern bool         g_bUseEmulation;
extern bool         g_bUseCommandLists;
extern bool         g_bUseCallCommandListNV;
extern bool         g_bUseBindless;
extern bool         g_bDisplayObject;
extern bool         g_bRotateOx90;
extern bool         g_bWireframe;

extern int          g_TokenBufferGrouping;
extern int          g_MaxBOSz;
extern float        g_Supersampling;

extern int          g_firstMesh;

extern MatrixBufferGlobal g_globalMatrices;

extern BO g_uboMatrix;
extern BO g_uboLight;

extern std::string buildLineWidthCommand(float w);
extern std::string buildUniformAddressCommand(int idx, GLuint64 p, GLsizeiptr sizeBytes, ShaderStages stage);
extern std::string buildAttributeAddressCommand(int idx, GLuint64 p, GLsizeiptr sizeBytes);
extern std::string buildElementAddressCommand(GLuint64 ptr, GLenum indexFormatGL);
extern std::string buildDrawElementsCommand(GLenum topologyGL, GLuint indexCount);
extern std::string buildDrawArraysCommand(GLenum topologyGL, GLuint indexCount);

//------------------------------------------------------------------------------
// Class for Object (made of 1 to N meshes)
//------------------------------------------------------------------------------
class Bk3dModel
{
public:
    Bk3dModel(const char *name, vec3f *pPos=NULL, float *pScale=NULL);
    ~Bk3dModel();

    vec3f               m_posOffset;
    float               m_scale;
    std::string         m_name;
    struct Stats {
        unsigned int    primitives;
        unsigned int    drawcalls;
        unsigned int    attr_update;
        unsigned int    uniform_update;
    };
private:
    bool                m_bRecordObject;

    std::vector<BO>     m_ObjVBOs;
    std::vector<BO>     m_ObjEBOs;

    BO                  m_uboObjectMatrices;
    BO                  m_uboMaterial;

    MatrixBufferObject* m_objectMatrices;
    int                 m_objectMatricesNItems;

    MaterialBuffer*     m_material;
    int                 m_materialNItems;

    TokenBuffer         m_tokenBufferModel; // contains the commands to send to the GPU for setup and draw
    CommandStatesBatch  m_commandModel;     // used to gather the GPU pointers of a single batch and where states/fbos do change
    GLuint              m_commandList;      // the list containing the command buffer(s)

    bk3d::FileHeader*   m_meshFile;

    Stats m_stats;
    
    //-----------------------------------------------------------------------------
    // Store state objects according to what was changed while building the mesh
    // for now: only one argument. But could have more...
    //-----------------------------------------------------------------------------
    struct States {
        GLenum topology;
        //GLuint primRestartIndex;
        // we should have more comparison on attribute stride, offset...
    };
    struct StateLess
    {
        bool operator()(const States& _Left, const States& _Right) const
	    {
            // check primRestartIndex, too
	        return (_Left.topology < _Right.topology);
	    }
    };
    std::map<States, GLuint, StateLess > m_glStates;

public:
    void invalidateCmdList() { m_bRecordObject = true; }
    void releaseState(GLuint s);
    void deleteCommandListData();
    GLenum topologyWithoutStrips(GLenum topologyGL);
    GLuint findStateOrCreate(bk3d::Mesh *pMesh, bk3d::PrimGroup* pPG);
    bool comparePG(const bk3d::PrimGroup* pPrevPG, const bk3d::PrimGroup* pPG);
    bool compareAttribs(bk3d::Mesh* pPrevMesh, bk3d::Mesh* pMesh);
    int recordMeshes(GLenum topology, std::vector<int> &offsets, GLsizei &tokenTableOffset, int &totalDCs, GLuint m_fboMSAA8x);
    void init_command_list();
    void update_fbo_target(GLuint fbo);
    bool recordTokenBufferObject(GLuint m_fboMSAA8x);
    bool initBuffersObject();
    bool loadModel();
    void displayObject(const mat4f& cameraView, const mat4f projection, GLuint fboMSAA8x, int maxItems=-1);
    void printPosition();
    void addStats(Stats &stats);

    static bool initGraphics_bk3d();
}; //Class Bk3dModel
