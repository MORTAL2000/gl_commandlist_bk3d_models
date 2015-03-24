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

    Note: this section of the code is showing a basic implementation of
    Command-lists using a binary format called bk3d.
    This format has no value for command-list. However you will see that
    it allows to use pre-baked art asset without too parsing: all is
    available from structures in the file (after pointer resolution)

*/ //--------------------------------------------------------------------
#define EXTERNSVCUI
#define WINDOWINERTIACAMERA_EXTERN
#define EMUCMDLIST_EXTERN
#include "gl_commandlist_bk3d_models.h"

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------
int         g_MaxBOSz = 200000;
int         g_TokenBufferGrouping    = 0;

//-----------------------------------------------------------------------------
// Shaders
//-----------------------------------------------------------------------------
static const char *s_glslv_mesh = 
"#version 430\n"
"#extension GL_ARB_separate_shader_objects : enable\n"
"#extension GL_NV_command_list : enable\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_MATRIX) ") uniform matrixBuffer {\n"
"   uniform mat4 mW;\n"
"   uniform mat4 mVP;\n"
"} matrix;\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_MATRIXOBJ) ") uniform matrixObjBuffer {\n"
"   uniform mat4 mO;\n"
"} object;\n"
"layout(location=0) in  vec3 P;\n"
"layout(location=1) in  vec3 N;\n"
"layout(location=1) out vec3 outN;\n"
"out gl_PerVertex {\n"
"    vec4  gl_Position;\n"
"};\n"
"void main() {\n"
"   outN = N;\n"
"   gl_Position = matrix.mVP * (matrix.mW * (object.mO * vec4(P, 1.0)));\n"
"}\n"
;
static const char *s_glslf_mesh = 
"#version 430\n"
"#extension GL_ARB_separate_shader_objects : enable\n"
"#extension GL_NV_command_list : enable\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_MATERIAL) ") uniform materialBuffer {\n"
"   uniform vec3 diffuse;"
"} material;\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_LIGHT) ") uniform lightBuffer {\n"
"   uniform vec3 dir;"
"} light;\n"
"layout(location=1) in  vec3 N;\n"
"layout(location=0) out vec4 outColor;\n"
"void main() {\n"
"\n"
"   float d1 = max(0.0, abs(dot(N, light.dir)) );\n"
"   //float d2 = 0.6 * max(0.0, abs(dot(N, -light.dir)) );\n"
"   outColor = vec4(material.diffuse * (/*d2 +*/ d1),1);\n"
"}\n"
;
static const char *s_glslf_mesh_line = 
"#version 430\n"
"#extension GL_ARB_separate_shader_objects : enable\n"
"#extension GL_NV_command_list : enable\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_MATERIAL) ") uniform materialBuffer {\n"
"   uniform vec3 diffuse;"
"} material;\n"
"layout(std140,commandBindableNV,binding=" TOSTR(UBO_LIGHT) ") uniform lightBuffer {\n"
"   uniform vec3 dir;"
"} light;\n"
"layout(location=1) in  vec3 N;\n"
"layout(location=0) out vec4 outColor;\n"
"void main() {\n"
"\n"
"   outColor = vec4(0.7,0.7,0.8,1);\n"
"}\n"
;
GLSLShader  s_shaderMesh;
GLSLShader  s_shaderMeshLine;


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
Bk3dModel::Bk3dModel(const char *name, vec3f *pPos, float *pScale)
{
    assert(name);
    m_name                  = std::string(name);
    m_bRecordObject         = true;
    m_objectMatrices        = NULL;
    m_objectMatricesNItems  = 0;
    m_material              = NULL;
    m_materialNItems        = 0;
    m_commandList           = 0;
    m_meshFile              = NULL;
    m_posOffset             = pPos ? *pPos : vec3f(0,0,0);
    m_scale                 = pScale ? *pScale : 0.0f;
    m_tokenBufferModel.bufferID = 0;
    memset(&m_uboObjectMatrices,0, sizeof(BO));
    memset(&m_uboMaterial,      0, sizeof(BO));
    memset(&m_stats,            0, sizeof(Stats));
}

Bk3dModel::~Bk3dModel()
{
    deleteCommandListData();

    for(int i=0;i<m_ObjVBOs.size(); i++)
    {
        glMakeNamedBufferNonResidentNV(m_ObjVBOs[i].Id);
        glDeleteBuffers(1, &m_ObjVBOs[i].Id);
    }
    for(int i=0;i<m_ObjEBOs.size(); i++)
    {
        glMakeNamedBufferNonResidentNV(m_ObjEBOs[i].Id);
        glDeleteBuffers(1, &m_ObjEBOs[i].Id);
    }
    glMakeNamedBufferNonResidentNV(m_uboObjectMatrices.Id);
    glDeleteBuffers(1, &m_uboObjectMatrices.Id);
    glMakeNamedBufferNonResidentNV(m_uboMaterial.Id);
    glDeleteBuffers(1, &m_uboMaterial.Id);
    delete [] m_objectMatrices;
    delete [] m_material;
    if(m_meshFile)
        free(m_meshFile);
}
//------------------------------------------------------------------------------
// destroy the command buffers and states
//------------------------------------------------------------------------------
void Bk3dModel::deleteCommandListData()
{
    releaseState(0); // 0 : release all
    if(m_tokenBufferModel.bufferID)
        glDeleteBuffers(1, &m_tokenBufferModel.bufferID);
    m_tokenBufferModel.bufferAddr = 0;
    m_tokenBufferModel.bufferID = 0;
    m_tokenBufferModel.data.clear();

    // delete FBOs... m_tokenBufferModel.fbos
    for(int i=0; i<m_commandModel.stateGroups.size(); i++)
        glDeleteStatesNV(1, &m_commandModel.stateGroups[i]);
    m_commandModel.dataPtrs.clear();
    m_commandModel.sizes.clear();
    m_commandModel.fbos.clear();
    m_commandModel.stateGroups.clear();
    m_commandModel.numItems = 0;

    m_bRecordObject     = true;
    if(m_commandList)
        glDeleteCommandListsNV(1, &m_commandList);
    m_commandList = 0;
    memset(&m_stats,            0, sizeof(Stats));
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void Bk3dModel::releaseState(GLuint s)
{
    std::map<States, GLuint, StateLess >::iterator iM = m_glStates.begin();
    while(iM != m_glStates.end())
    {
        if(s>0 && (iM->second == s)) {
            glDeleteStatesNV(1, &iM->second);
            m_glStates.erase(iM);
            return;
        }
        else if(s==0) {
            glDeleteStatesNV(1, &iM->second);
        }
        ++iM;
    }
    if(s<=0)
        m_glStates.clear();
}
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLenum Bk3dModel::topologyWithoutStrips(GLenum topologyGL)
{
    switch(topologyGL)
    {
    case GL_TRIANGLE_STRIP:
        //topologyGL = GL_NONE;
        //break;
    case GL_TRIANGLES:
        topologyGL = GL_TRIANGLES;
        break;
    case GL_QUAD_STRIP:
        //topologyGL = GL_NONE;
        //break;
    case GL_QUADS:
        topologyGL = GL_QUADS;
        break;
    case GL_LINE_STRIP:
        //topologyGL = GL_NONE;
        //break;
    case GL_LINES:
        topologyGL = GL_LINES;
        break;
    case GL_TRIANGLE_FAN: // Not supported by command-lists
        topologyGL = GL_NONE;
        break;
    case GL_POINTS:
        topologyGL = GL_POINTS;//GL_NONE;
        break;
    case GL_LINE_LOOP: // Should be supported but raised errors, so far (?)
        topologyGL = GL_NONE;
        break;
    }
    return topologyGL;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
GLuint Bk3dModel::findStateOrCreate(bk3d::Mesh *pMesh, bk3d::PrimGroup* pPG)
{
    States sl = { pPG->topologyGL };
    std::map<States, GLuint, StateLess >::iterator iM = m_glStates.find(sl);
    if(iM == m_glStates.end())
    {
        GLuint id;
        GLenum topologyGL = topologyWithoutStrips(pPG->topologyGL);
        if(topologyGL == GL_NONE) // fail
            return 0;
        glCreateStatesNV(1, &id);
        //
        // CAPTURE the states here
        //
        glStateCaptureNV(id, topologyGL);
        emucmdlist::StateCaptureNV(id, topologyGL); // for emulation purpose
        bk3d::AttributePool* pA = pMesh->pAttributes;
        if(pA->n > 1)
        {
            emucmdlist::StateCaptureNV_Extra(id
                , pA->p[0]->strideBytes, pA->p[0]->numComp, pA->p[0]->dataOffsetBytes
                , pA->p[1]->strideBytes, pA->p[1]->numComp, pA->p[1]->dataOffsetBytes);
        } else {
            emucmdlist::StateCaptureNV_Extra(id
                , pA->p[0]->strideBytes, pA->p[0]->numComp, pA->p[0]->dataOffsetBytes, 0,0,0);
        }
        m_glStates[sl] = id;
        return id;
    }
    return iM->second;
}
//------------------------------------------------------------------------------
// if states from one to the other are different, return true
//------------------------------------------------------------------------------
bool Bk3dModel::comparePG(const bk3d::PrimGroup* pPrevPG, const bk3d::PrimGroup* pPG)
{
    if((pPrevPG == NULL) && pPG)
        return true;
    if(topologyWithoutStrips(pPrevPG->topologyGL) != topologyWithoutStrips(pPG->topologyGL))
        return true;
    if(pPrevPG->primRestartIndex != pPG->primRestartIndex)
        return true;
    return false;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool Bk3dModel::compareAttribs(bk3d::Mesh* pPrevMesh, bk3d::Mesh* pMesh)
{
    if(pMesh->pAttributes->n != pPrevMesh->pAttributes->n)
        return true;
    for(int s=0; s<pMesh->pAttributes->n; s++)
    {
        bk3d::Attribute* pA = pMesh->pAttributes->p[s];
        bk3d::Slot*      pS = pMesh->pSlots->p[pA->slot];
        bk3d::Attribute* pPrevA = pMesh->pAttributes->p[s];
        bk3d::Slot*      pPrevS = pMesh->pSlots->p[pA->slot];
        if(pA->strideBytes != pA->strideBytes)
            return true;
        if(pA->numComp != pA->numComp)
            return true;
        if(pA->formatGL != pA->formatGL)
            return true;
        if(pA->dataOffsetBytes != pA->dataOffsetBytes)
            return true;
    }
    return false;
}

//------------------------------------------------------------------------------
// topology to 0 means we just build things as we get them
// specific topology will only retain these ones
//------------------------------------------------------------------------------
int Bk3dModel::recordMeshes(GLenum topology, std::vector<int> &offsets, GLsizei &tokenTableOffset, int &totalDCs, GLuint m_fboMSAA8x)
{
    int nDCs = 0;
    // first default state capture
    GLuint              curState = -1;
    GLuint              curMaterial = 0xFFFFFFFF;
    GLuint              curObjectTransform = 0xFFFFFFFF;
    bk3d::PrimGroup*    pPrevPG = NULL;
    bk3d::Mesh*         pPrevMesh = NULL;
    bool                changed = false;
    int                 prevNAttr = -1;
    std::string         tokentable;

    BO curVBO;
    BO curEBO;

    //////////////////////////////////////////////
    // Loop through meshes
    //
    // Hack: my captured models do have a bad geometry in mesh #0... *start with 1*
    for(int i=1; i< m_meshFile->pMeshes->n; i++)
	{
		bk3d::Mesh *pMesh = m_meshFile->pMeshes->p[i];
        int idx = (int)pMesh->userPtr;
        curVBO = m_ObjVBOs[idx];
        curEBO = m_ObjEBOs[idx];
        int n = pMesh->pAttributes->n;
        //
        // the Mesh can (should) have a transformation associated to itself
        // this is the mode where the primitive groups share the same transformation
        // Change the uniform pointer of object transformation if it changed
        //
        if(pMesh->pTransforms
            && (pMesh->pTransforms->n > 0)
            && (curObjectTransform != pMesh->pTransforms->p[0]->ID))
        {
            curObjectTransform = pMesh->pTransforms->p[0]->ID;
            m_tokenBufferModel.data += buildUniformAddressCommand(UBO_MATRIXOBJ, m_uboObjectMatrices.Addr + (curObjectTransform * sizeof(MatrixBufferObject)), sizeof(MatrixBufferObject), STAGE_VERTEX);
            m_stats.uniform_update++;
        }
        //
        // check if attribute info changed
        //
        if(pPrevMesh && compareAttribs(pPrevMesh, pMesh))
        {
            changed = true;
            if(pPrevPG)
            {
                // search for a state already available for our needs
                curState = findStateOrCreate(pPrevMesh, pPrevPG);
                m_commandModel.stateGroups.push_back(curState);
                m_commandModel.fbos.push_back(m_fboMSAA8x);

                // store the size of the token table
                m_commandModel.sizes.push_back((GLsizei)m_tokenBufferModel.data.size() - tokenTableOffset);
                offsets.push_back(tokenTableOffset);

                // new offset and ptr
                tokenTableOffset = (GLsizei)m_tokenBufferModel.data.size();
                pPrevPG = NULL;
            }
        }
        //
        // build COMMANDS to assign pointers to attributes
        //
        int s=0;
        for(; s<n; s++)
        {
            bk3d::Attribute* pA = pMesh->pAttributes->p[s];
            bk3d::Slot*      pS = pMesh->pSlots->p[pA->slot];
            tokentable += buildAttributeAddressCommand(s, curVBO.Addr + (GLuint64)pS->userPtr.p, pS->vtxBufferSizeBytes);
            m_stats.attr_update++;

            // set the OpenGL part... not necessary, normally
            //if((pPrevMesh==NULL) || changed)
            {
                //glVertexAttribFormatNV(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3f));
                glBindVertexBuffer(s, 0, 0, pA->strideBytes);
                glVertexAttribFormat(s,pA->numComp, pA->formatGL, GL_FALSE, pA->dataOffsetBytes);
                //glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, s, (GLuint64)pS->userPtr.p, pS->vtxBufferSizeBytes);
	            glEnableVertexAttribArray(s);
            }
        }
        changed = false;
        for(int a=s; a<prevNAttr; a++)
	        glDisableVertexAttribArray(a);
        prevNAttr = n;
        ////////////////////////////////////////
        // Primitive groups in the mesh
        //
        for(int pg=0; pg<pMesh->pPrimGroups->n; pg++)
        {
            bk3d::PrimGroup* pPG = pMesh->pPrimGroups->p[pg];
            GLenum PGTopo = topologyWithoutStrips(pPG->topologyGL);
            // filter unsuported primitives: FANS
            if(((topology != 0xFFFFFFFF) && (topology != PGTopo)) || (PGTopo == GL_NONE))
                continue;
            //
            // change the program is lines vs. polygons: no normals for lines
            //
            if(pPG->topologyGL == GL_LINES)
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
                s_shaderMeshLine.bindShader();
            } else {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0, 1.0); // no issue with redundant call here: the state capture will just deal with simplifying things
                s_shaderMesh.bindShader();

            }

            //
            // Change the uniform pointer if material changed
            //
            if(pPG->pMaterial && (curMaterial != pPG->pMaterial->ID))
            {
                curMaterial = pPG->pMaterial->ID;
                m_tokenBufferModel.data += buildUniformAddressCommand(UBO_MATERIAL, m_uboMaterial.Addr + (curMaterial * sizeof(MaterialBuffer)), sizeof(MaterialBuffer), STAGE_FRAGMENT);
                m_stats.uniform_update++;
            }
            //
            // the Primitive group can also have its own transformation
            // this is the mode where the mesh don't own the transformation but its primitive groups do
            // Change the uniform pointer of object transformation if it changed
            //
            if(pPG->pTransforms
                && (pPG->pTransforms->n > 0)
                && (curObjectTransform != pPG->pTransforms->p[0]->ID))
            {
                curObjectTransform = pPG->pTransforms->p[0]->ID;
                m_tokenBufferModel.data += buildUniformAddressCommand(UBO_MATRIXOBJ, m_uboObjectMatrices.Addr + (curObjectTransform * sizeof(MatrixBufferObject)), sizeof(MatrixBufferObject), STAGE_VERTEX);
                m_stats.uniform_update++;
            }
            // if something changed: mark the cut for the previous stuff
            // and start a new section
            if(pPrevPG && comparePG(pPrevPG, pPG))
            {
                // search for a state already available for our needs
                curState = findStateOrCreate(pMesh, pPrevPG);
                m_commandModel.stateGroups.push_back(curState);
                m_commandModel.fbos.push_back(m_fboMSAA8x);

                // store the size of the token table
                m_commandModel.sizes.push_back((GLsizei)m_tokenBufferModel.data.size() - tokenTableOffset);
                offsets.push_back(tokenTableOffset);

                // new offset
                tokenTableOffset = (GLsizei)m_tokenBufferModel.data.size();
            }
            if(!tokentable.empty())
            {
                m_tokenBufferModel.data += tokentable;
                tokentable.clear();
            }
//            m_tokenBufferModel.data += buildUniformAddressCommand(UBO_LIGHT, g_uboLight.Addr, sizeof(LightBuffer), STAGE_FRAGMENT);
///                m_stats.uniform_update++;
            // add other token COMMANDS: elements + drawcall
            if(pPG->indexArrayByteSize > 0)
            {
                m_tokenBufferModel.data += buildElementAddressCommand(curEBO.Addr + (GLuint64)pPG->userPtr, pPG->indexFormatGL);
                m_tokenBufferModel.data += buildDrawElementsCommand(pPG->topologyGL, pPG->indexCount);
                nDCs++;
            } else {
                m_tokenBufferModel.data += buildDrawArraysCommand(pPG->topologyGL, pPG->indexCount);
                nDCs++;
            }
            // gather some stats
            switch(pPG->topologyGL)
            {
            case GL_LINES:
                m_stats.primitives += pPG->indexCount/2;
                break;
            case GL_LINE_STRIP:
                m_stats.primitives += pPG->indexCount-1;
                break;
            case GL_TRIANGLES:
                m_stats.primitives += pPG->indexCount/3;
                break;
            case GL_TRIANGLE_STRIP:
                m_stats.primitives += pPG->indexCount-2;
                break;
            case GL_QUADS:
                m_stats.primitives += pPG->indexCount/4;
                break;
            case GL_QUAD_STRIP:
                m_stats.primitives += pPG->indexCount-3;
                break;
            case GL_POINTS:
                //m_stats.primitives += pPG->indexCount;
                break;
            }
            m_stats.drawcalls++;

            pPrevPG = pPG;
        } // for(int pg=0; pg<pMesh->pPrimGroups->n; pg++)
        pPrevMesh = pMesh;
    } // for(int i=0; i< m_meshFile->pMeshes->n; i++)
    if(pPrevPG)
    {
        // search for a state already available for our needs
        curState = findStateOrCreate(pPrevMesh, pPrevPG);
        m_commandModel.stateGroups.push_back(curState);
        m_commandModel.fbos.push_back(m_fboMSAA8x);

        // store the size of the token table
        m_commandModel.sizes.push_back((GLsizei)m_tokenBufferModel.data.size() - tokenTableOffset);
        offsets.push_back(tokenTableOffset);

        // new offset and ptr
        tokenTableOffset = (GLsizei)m_tokenBufferModel.data.size();
        pPrevPG = NULL;
    }
    totalDCs += nDCs;
    return nDCs;
}
//------------------------------------------------------------------------------
// the command-list is the ultimate optimization of the extension called with
// the same name. It is very close from old Display-lists but offer more flexibility
//------------------------------------------------------------------------------
void Bk3dModel::init_command_list()
{
    if(m_commandModel.numItems == 0)
        return;
    if(m_commandList)
        glDeleteCommandListsNV(1, &m_commandList);
    glCreateCommandListsNV(1, &m_commandList);
    {
        glCommandListSegmentsNV(m_commandList, 1);
        glListDrawCommandsStatesClientNV(m_commandList, 0, &m_commandModel.dataPtrs[0], &m_commandModel.sizes[0], &m_commandModel.stateGroups[0], &m_commandModel.fbos[0], int(m_commandModel.fbos.size() )); 
    }
    glCompileCommandListNV(m_commandList);
}
//------------------------------------------------------------------------------
// it is possible that the target for rendering end-up to another FBO
// for example when MSAA mode changed from 8x to 1x...
// therefore we must make sure that command list know about it
//------------------------------------------------------------------------------
void Bk3dModel::update_fbo_target(GLuint fbo)
{
    // simple case now: same for all
    for(int i=0; i<m_commandModel.fbos.size(); i++)
        m_commandModel.fbos[i] = fbo;
}
//------------------------------------------------------------------------------
// build token buffer, states objects and commandList for the 3D Object
// Note that this part is like a scene-traversal
// it must somehow update OpenGL states accordingly as if it was used to render
// then the states will be captured. The idea is to capture them everytime one
// changed.
//------------------------------------------------------------------------------
bool Bk3dModel::recordTokenBufferObject(GLuint m_fboMSAA8x)
{
    deleteCommandListData();

    if(!m_meshFile)
        return false;

    GLsizei tokenTableOffset = 0;
    std::vector<int> offsets;

    glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);

    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_MATRIX, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_LIGHT, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, UBO_MATRIXOBJ, 0);

    // token buffer for the viewport setting. Need to have a state object and a fbo
    // would be better to use the same as the next state created by the mesh (see below: findStateOrCreate...)
    // but easier to write the code in this case:
    GLuint st;
    glCreateStatesNV(1, &st);
    glStateCaptureNV(st, GL_TRIANGLES);
    emucmdlist::StateCaptureNV(st, GL_TRIANGLES); // for emulation purpose
    m_commandModel.pushBatch(st, m_fboMSAA8x, 
        g_tokenBufferViewport.bufferAddr, 
        &g_tokenBufferViewport.data[0], 
        g_tokenBufferViewport.data.size() );
    //
    // Walk through meshes as if we were traversing a scene...
    //
    LOGI("Creating a command-Buffer for %d Meshes\n", m_meshFile->pMeshes->n);
    LOGFLUSH();
    m_tokenBufferModel.data += buildLineWidthCommand(g_Supersampling);
    m_tokenBufferModel.data += buildUniformAddressCommand(UBO_MATRIX, g_uboMatrix.Addr, sizeof(MatrixBufferGlobal), STAGE_VERTEX);
    m_tokenBufferModel.data += buildUniformAddressCommand(UBO_MATRIXOBJ, m_uboObjectMatrices.Addr, sizeof(MatrixBufferObject), STAGE_VERTEX);
    m_tokenBufferModel.data += buildUniformAddressCommand(UBO_LIGHT, g_uboLight.Addr, sizeof(LightBuffer), STAGE_FRAGMENT);
    m_stats.uniform_update+=3;

    int nDCs = 0;
    int totalDCs = 0;
    switch(g_TokenBufferGrouping)
    {
    case 0:
        nDCs = recordMeshes(-1, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        break;
    case 1:
        LOGI("Sorting by primitive types\n", m_meshFile->pMeshes->n);
        LOGFLUSH();
        nDCs = recordMeshes(GL_LINES, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        LOGI("GL_LINES: %d\n", nDCs);
        LOGFLUSH();
        nDCs = recordMeshes(GL_TRIANGLES, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        LOGI("GL_TRIANGLES/STRIP: %d\n", nDCs);
        LOGFLUSH();
        nDCs = recordMeshes(GL_TRIANGLE_FAN, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        LOGI("GL_TRIANGLE_FAN: %d\n", nDCs);
        LOGFLUSH();
        nDCs = recordMeshes(GL_QUADS, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        LOGI("GL_QUADS/STRIP: %d\n", nDCs);
        LOGFLUSH();
        nDCs = recordMeshes(GL_POINTS, offsets, tokenTableOffset, totalDCs, m_fboMSAA8x);
        LOGI("GL_POINTS: %d\n", nDCs);
        LOGFLUSH();
        break;
    }
    //
    // create the buffer object for this token buffer:
    //
    glGenBuffers(1, &m_tokenBufferModel.bufferID);
    glNamedBufferDataEXT(m_tokenBufferModel.bufferID, m_tokenBufferModel.data.size(), &m_tokenBufferModel.data[0], GL_STATIC_DRAW);
    // get the 64 bits pointer and make it resident: bedcause we will go through its pointer
    glGetNamedBufferParameterui64vNV(m_tokenBufferModel.bufferID, GL_BUFFER_GPU_ADDRESS_NV, &m_tokenBufferModel.bufferAddr);
    glMakeNamedBufferResidentNV(m_tokenBufferModel.bufferID, GL_READ_ONLY);
    LOGOK("Token buffer of %.2f kb created for %d state changes and %d Drawcalls\n", (float)m_tokenBufferModel.data.size()/1024.0, m_commandModel.stateGroups.size(), totalDCs);
    LOGOK("Total of %d primitives\n", m_stats.primitives);
    LOGFLUSH();
    //
    // create a table of client pointer (system memory) for command-list
    // we do it at the end because STL might have re-allocated the 'data' system-memory along the previous process
    //
    m_commandModel.numItems += offsets.size(); // add these batches to the existing one (viewport tokencmd)
    for(int i=0; i<offsets.size(); i++)
    {
        // for compiled command-list: using the system memory pointer
        m_commandModel.dataPtrs.push_back(&m_tokenBufferModel.data[offsets[i]]);
        // for non compile command-state using the GPU pointers
        m_commandModel.dataGPUPtrs.push_back(m_tokenBufferModel.bufferAddr + offsets[i]);
    }
    //
    // Create the command-list
    //
    init_command_list();
 
    m_bRecordObject = false; // done recording

    glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);

    return true;
}

//------------------------------------------------------------------------------
// It is possible to create one VBO for one Mesh; and one EBO for each primitive group
// however, for this sample, we will create only one VBO for all and one EBO
// meshes and primitive groups will have an offset in these buffers
//------------------------------------------------------------------------------
bool Bk3dModel::initBuffersObject()
{
    LOGOK("Init buffers\n");
    LOGFLUSH();
    BO curVBO;
    BO curEBO;
    unsigned int totalVBOSz = 0;
    unsigned int totalEBOSz = 0;
    memset(&curVBO, 0, sizeof(curVBO));
    memset(&curEBO, 0, sizeof(curEBO));
	glGenBuffers(1, &curVBO.Id);
	glGenBuffers(1, &curEBO.Id);

    //m_meshFile->pMeshes->n = 60000;
    //
    // create Buffer Object for materials
    //
    if(m_meshFile->pMaterials && m_meshFile->pMaterials->nMaterials )
    {
        m_material = new MaterialBuffer[m_meshFile->pMaterials->nMaterials];
        m_materialNItems = m_meshFile->pMaterials->nMaterials;
        for(int i=0; i<m_meshFile->pMaterials->nMaterials; i++)
        {
            // 256 bytes aligned...
            // this is for now a fake material: very few items (diffuse)
            // in a real application, material information could contain more
            // we'd need 16 vec4 to fill 256 bytes
            memcpy(m_material[i].diffuse.vec_array, m_meshFile->pMaterials->pMaterials[i]->Diffuse(), sizeof(vec3f));
            //...
            // hack for visual result...
            if(length(m_material[i].diffuse) <= 0.1f)
                m_material[i].diffuse = vec3f(1,1,1);
        }
        //
        // Material UBO: *TABLE* of multiple materials
        // Then offset in it for various drawcalls
        //
        if(m_uboMaterial.Id == 0)
            glGenBuffers(1, &m_uboMaterial.Id);

        m_uboMaterial.Sz = sizeof(MaterialBuffer) * m_materialNItems;
        glNamedBufferDataEXT(m_uboMaterial.Id, m_uboMaterial.Sz, m_material, GL_STREAM_DRAW);
        glGetNamedBufferParameterui64vNV(m_uboMaterial.Id, GL_BUFFER_GPU_ADDRESS_NV, (GLuint64EXT*)&m_uboMaterial.Addr);
        glMakeNamedBufferResidentNV(m_uboMaterial.Id, GL_READ_WRITE);
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL, m_uboMaterial.Id);

        LOGI("%d materials stored in %d Kb\n", m_meshFile->pMaterials->nMaterials, (m_uboMaterial.Sz+512)/1024);
        LOGFLUSH();
    }

    //
    // create Buffer Object for Object-matrices
    //
    if(m_meshFile->pTransforms && m_meshFile->pTransforms->nBones)
    {
        m_objectMatrices = new MatrixBufferObject[m_meshFile->pTransforms->nBones];
        m_objectMatricesNItems = m_meshFile->pTransforms->nBones;
        for(int i=0; i<m_meshFile->pTransforms->nBones; i++)
        {
            // 256 bytes aligned...
            memcpy(m_objectMatrices[i].mO.mat_array, m_meshFile->pTransforms->pBones[i]->Matrix().m, sizeof(mat4f));
        }
        //
        // Transformation UBO: *TABLE* of multiple transformations
        // Then offset in it for various drawcalls
        //
        if(m_uboObjectMatrices.Id == 0)
            glGenBuffers(1, &m_uboObjectMatrices.Id);

        m_uboObjectMatrices.Sz = sizeof(MatrixBufferObject) * m_objectMatricesNItems;
        glNamedBufferDataEXT(m_uboObjectMatrices.Id, m_uboObjectMatrices.Sz, m_objectMatrices, GL_STREAM_DRAW);
        glGetNamedBufferParameterui64vNV(m_uboObjectMatrices.Id, GL_BUFFER_GPU_ADDRESS_NV, (GLuint64EXT*)&m_uboObjectMatrices.Addr);
        glMakeNamedBufferResidentNV(m_uboObjectMatrices.Id, GL_READ_WRITE);
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIXOBJ, m_uboObjectMatrices.Id);

        LOGI("%d matrices stored in %d Kb\n", m_meshFile->pTransforms->nBones, (m_uboObjectMatrices.Sz + 512)/1024);
        LOGFLUSH();
    }

    //
    // First pass: evaluate the size of the single VBO
    // and store offset to where we'll find data back
    //
    bk3d::Mesh *pMesh = NULL;
    for(int i=0; i< m_meshFile->pMeshes->n; i++)
	{
		pMesh = m_meshFile->pMeshes->p[i];
        pMesh->userPtr = (void*)m_ObjVBOs.size(); // keep track of the VBO
        //
        // When we reached the arbitrary limit: switch to another VBO
        //
        if(curVBO.Sz > (g_MaxBOSz * 1024*1024))
        {
	        glGenBuffers(1, &curVBO.Id);
        #if 1
            glNamedBufferDataEXT(curVBO.Id, curVBO.Sz, NULL, GL_STATIC_DRAW);
        #else
            glNamedBufferStorageEXT(curVBO.Id, curVBO.Sz, NULL, 0); // Not working with NSight !!! https://www.opengl.org/registry/specs/ARB/buffer_storage.txt
        #endif
            glGetNamedBufferParameterui64vNV(curVBO.Id, GL_BUFFER_GPU_ADDRESS_NV, &curVBO.Addr);
            glMakeNamedBufferResidentNV(curVBO.Id, GL_READ_ONLY);
            //
            // push this VBO and create a new one
            //
            totalVBOSz += curVBO.Sz;
            m_ObjVBOs.push_back(curVBO);
            memset(&curVBO, 0, sizeof(curVBO));
            //
            // At the same time, create a new EBO... good enough for now
            //
            glGenBuffers(1, &curEBO.Id); // store directly to a user-space dedicated to this kind of things
        #if 1
            glNamedBufferDataEXT(curEBO.Id, curEBO.Sz, NULL, GL_STATIC_DRAW);
        #else
            glNamedBufferStorageEXT(curEBO.Id, curEBO.Sz, NULL, 0); // Not working with NSight !!! https://www.opengl.org/registry/specs/ARB/buffer_storage.txt
        #endif
            glGetNamedBufferParameterui64vNV(curEBO.Id, GL_BUFFER_GPU_ADDRESS_NV, &curEBO.Addr);
            glMakeNamedBufferResidentNV(curEBO.Id, GL_READ_ONLY);
            //
            // push this VBO and create a new one
            //
            totalEBOSz += curEBO.Sz;
            m_ObjEBOs.push_back(curEBO);
            memset(&curEBO, 0, sizeof(curEBO));
        }
        //
        // Slots: buffers for vertices
        //
        int n = pMesh->pSlots->n;
        for(int s=0; s<n; s++)
        {
            bk3d::Slot* pS = pMesh->pSlots->p[s];
            pS->userData = 0;
            pS->userPtr = (int*)curVBO.Sz;
            GLuint alignedSz = (pS->vtxBufferSizeBytes >> 8);
            alignedSz += pS->vtxBufferSizeBytes & 0xFF ? 1 : 0;
            alignedSz = alignedSz << 8;
            curVBO.Sz += alignedSz;
        }
        //
        // Primitive groups
        //
        for(int pg=0; pg<pMesh->pPrimGroups->n; pg++)
        {
            bk3d::PrimGroup* pPG = pMesh->pPrimGroups->p[pg];
            if(pPG->indexArrayByteSize > 0)
            {
                GLuint alignedSz = (pPG->indexArrayByteSize >> 8);
                alignedSz += pPG->indexArrayByteSize & 0xFF ? 1 : 0;
                alignedSz = alignedSz << 8;
                pPG->userPtr = (void*)curEBO.Sz;
                curEBO.Sz += alignedSz;
            } else {
                pPG->userPtr = NULL;
            }
        }
	}
    //
    // Finalize the last set of data
    //
    {
	    glGenBuffers(1, &curVBO.Id);
    #if 1
        glNamedBufferDataEXT(curVBO.Id, curVBO.Sz, NULL, GL_STATIC_DRAW);
    #else
        glNamedBufferStorageEXT(curVBO.Id, curVBO.Sz, NULL, 0); // Not working with NSight !!! https://www.opengl.org/registry/specs/ARB/buffer_storage.txt
    #endif
        glGetNamedBufferParameterui64vNV(curVBO.Id, GL_BUFFER_GPU_ADDRESS_NV, &curVBO.Addr);
        glMakeNamedBufferResidentNV(curVBO.Id, GL_READ_ONLY);
        //
        // push this VBO and create a new one
        //
        totalVBOSz += curVBO.Sz;
        m_ObjVBOs.push_back(curVBO);
        memset(&curVBO, 0, sizeof(curVBO));
        //
        // At the same time, create a new EBO... good enough for now
        //
        glGenBuffers(1, &curEBO.Id); // store directly to a user-space dedicated to this kind of things
    #if 1
        glNamedBufferDataEXT(curEBO.Id, curEBO.Sz, NULL, GL_STATIC_DRAW);
    #else
        glNamedBufferStorageEXT(curEBO.Id, curEBO.Sz, NULL, 0); // Not working with NSight !!! https://www.opengl.org/registry/specs/ARB/buffer_storage.txt
    #endif
        glGetNamedBufferParameterui64vNV(curEBO.Id, GL_BUFFER_GPU_ADDRESS_NV, &curEBO.Addr);
        glMakeNamedBufferResidentNV(curEBO.Id, GL_READ_ONLY);
        //
        // push this VBO and create a new one
        //
        totalEBOSz += curEBO.Sz;
        m_ObjEBOs.push_back(curEBO);
        memset(&curVBO, 0, sizeof(curVBO));
    }
    //
    // second pass: put stuff in the buffer and store offsets
    //

    for(int i=0; i< m_meshFile->pMeshes->n; i++)
	{
		bk3d::Mesh *pMesh = m_meshFile->pMeshes->p[i];
        int idx = (int)pMesh->userPtr;
        curVBO = m_ObjVBOs[idx];
        curEBO = m_ObjEBOs[idx];
        int n = pMesh->pSlots->n;
        for(int s=0; s<n; s++)
        {
            bk3d::Slot* pS = pMesh->pSlots->p[s];
            glNamedBufferSubDataEXT(curVBO.Id, (GLuint)(char*)pS->userPtr, pS->vtxBufferSizeBytes, pS->pVtxBufferData);
        }
        for(int pg=0; pg<pMesh->pPrimGroups->n; pg++)
        {
            bk3d::PrimGroup* pPG = pMesh->pPrimGroups->p[pg];
            if(pPG->indexArrayByteSize > 0)
                glNamedBufferSubDataEXT(curEBO.Id, (GLuint)(char*)pPG->userPtr, pPG->indexArrayByteSize, pPG->pIndexBufferData);
        }
        //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
    LOGI("meshes: %d in :%d VBOs (%f Mb) and %d EBOs (%f Mb) \n", m_meshFile->pMeshes->n, m_ObjVBOs.size(), (float)totalVBOSz/(float)(1024*1024), m_ObjEBOs.size(), (float)totalEBOSz/(float)(1024*1024));
    LOGFLUSH();
    return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
bool Bk3dModel::loadModel()
{
    LOGI("Loading Mesh %s..\n", m_name.c_str());
    LOGFLUSH();
    if(!(m_meshFile = bk3d::load(m_name.c_str() )))
    {
        std::string modelPathName;
        modelPathName = std::string("../../downloaded_resources/") + m_name;
        if(!(m_meshFile = bk3d::load(modelPathName.c_str())))
        {
            modelPathName = std::string(PROJECT_RELDIRECTORY) + std::string("../downloaded_resources/") + m_name;
            if(!(m_meshFile = bk3d::load(modelPathName.c_str())))
            {
                modelPathName = std::string(PROJECT_RELDIRECTORY) + m_name;
                m_meshFile = bk3d::load(modelPathName.c_str());
                if(!(m_meshFile = bk3d::load(modelPathName.c_str())))
                {
                    modelPathName = std::string(PROJECT_ABSDIRECTORY) + m_name;
                    m_meshFile = bk3d::load(modelPathName.c_str());
                }
            }
        }
    }
    if(m_meshFile)
    {
        //
        // Creation of the buffer objects
        // will make them resident
        //
        initBuffersObject();
	    //
	    // Some adjustment for the display
	    //
        if(m_scale <= 0.0)
        {
	        float min[3] = {1000.0, 1000.0, 1000.0};
	        float max[3] = {-1000.0, -1000.0, -1000.0};
	        for(int i=0; i<m_meshFile->pMeshes->n; i++)
	        {
		        bk3d::Mesh *pMesh = m_meshFile->pMeshes->p[i];
		        if(pMesh->aabbox.min[0] < min[0]) min[0] = pMesh->aabbox.min[0];
		        if(pMesh->aabbox.min[1] < min[1]) min[1] = pMesh->aabbox.min[1];
		        if(pMesh->aabbox.min[2] < min[2]) min[2] = pMesh->aabbox.min[2];
		        if(pMesh->aabbox.max[0] > max[0]) max[0] = pMesh->aabbox.max[0];
		        if(pMesh->aabbox.max[1] > max[1]) max[1] = pMesh->aabbox.max[1];
		        if(pMesh->aabbox.max[2] > max[2]) max[2] = pMesh->aabbox.max[2];
	        }
	        m_posOffset[0] = (max[0] + min[0])*0.5f;
	        m_posOffset[1] = (max[1] + min[1])*0.5f;
	        m_posOffset[2] = (max[2] + min[2])*0.5f;
	        float bigger = 0;
	        if((max[0]-min[0]) > bigger) bigger = (max[0]-min[0]);
	        if((max[1]-min[1]) > bigger) bigger = (max[1]-min[1]);
	        if((max[2]-min[2]) > bigger) bigger = (max[2]-min[2]);
	        if((bigger) > 0.001)
	        {
		        m_scale = 1.0f / bigger;
		        PRINTF(("Scaling the model by %f...\n", m_scale));
	        }
            m_posOffset *= m_scale;
        }
    } else {
        LOGE("error in loading mesh %s\n", m_name.c_str());
    }
    return true;
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void Bk3dModel::displayObject(const mat4f& cameraView, const mat4f projection, GLuint fboMSAA8x, int maxItems)
{
    NXPROFILEFUNC(__FUNCTION__);
    PROFILE_SECTION(__FUNCTION__);

    g_globalMatrices.mVP = projection * cameraView;
    g_globalMatrices.mW = mat4f(array16_id);
    //g_globalMatrices.mW.rotate(nv_to_rad*180.0f, vec3f(0,1,0));
    g_globalMatrices.mW.rotate(-nv_to_rad*90.0f, vec3f(1,0,0));
	g_globalMatrices.mW.translate(-m_posOffset);
    g_globalMatrices.mW.scale(m_scale);
    glNamedBufferSubDataEXT(g_uboMatrix.Id, 0, sizeof(g_globalMatrices), &g_globalMatrices);

    if(g_bUseCommandLists)
    {
        //
        // Record draw commands if not already done
        //
        if(m_bRecordObject)
            recordTokenBufferObject(fboMSAA8x);
        //
        // execute the commands from the token buffer
        //
        if(g_bUseEmulation)
        {
            //
            // an emulation of what got captured
            //
            emucmdlist::nvtokenRenderStatesSW(&m_commandModel.dataPtrs[0], &m_commandModel.sizes[0], 
                &m_commandModel.stateGroups[0], &m_commandModel.fbos[0], int(m_commandModel.numItems) );
        } else {
            if(g_bUseCallCommandListNV)
                //
                // real compiled Command-list
                //
                glCallCommandListNV(m_commandList);
            else
            {
                //
                // real Command-list's Token buffer with states execution
                //
                int nitems = int(m_commandModel.numItems);
                if((maxItems > 0)&&(nitems > maxItems))
                    nitems = maxItems;
                if(nitems)
                    glDrawCommandsStatesAddressNV(&m_commandModel.dataGPUPtrs[0], &m_commandModel.sizes[0], &m_commandModel.stateGroups[0], &m_commandModel.fbos[0], nitems); 
            }
            return;
        }
    }
    if(m_meshFile)
    {
        GLuint      curMaterial = 0;
        GLuint      curTransf = 0;
        BO          curVBO, curEBO;
        glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
        glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
        glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATRIX, g_uboMatrix.Addr, g_uboMatrix.Sz);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_LIGHT, g_uboLight.Addr, g_uboLight.Sz);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATRIXOBJ, m_uboObjectMatrices.Addr, sizeof(MatrixBufferObject));

	    glEnableVertexAttribArray(0);
	    glDisableVertexAttribArray(2);
	    for(int i=1; i< m_meshFile->pMeshes->n; i++)
	    {
		    bk3d::Mesh *pMesh = m_meshFile->pMeshes->p[i];
            int idx = (int)pMesh->userPtr;
            curVBO = m_ObjVBOs[idx];
            curEBO = m_ObjEBOs[idx];
            if(pMesh->pTransforms && (pMesh->pTransforms->n>0))
            {
			    bk3d::Bone *pTransf = pMesh->pTransforms->p[0];
                if(pTransf && (curTransf != pTransf->ID))
                {
                    curTransf = pTransf->ID;
                    glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATRIXOBJ, m_uboObjectMatrices.Addr + (curTransf * sizeof(MatrixBufferObject)), sizeof(MatrixBufferObject));
                }
            }
            //====> Pos
            bk3d::Attribute* pAttrPos = pMesh->pAttributes->p[0];
            glBindVertexBuffer(0, 0, 0, pAttrPos->strideBytes);
            glVertexAttribFormat(0,pAttrPos->numComp, pAttrPos->formatGL, GL_FALSE, pAttrPos->dataOffsetBytes);
            glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0, 
                curVBO.Addr + (GLuint64EXT)pMesh->pSlots->p[pAttrPos->slot]->userPtr.p, 
                pMesh->pSlots->p[pAttrPos->slot]->vtxBufferSizeBytes);
            //====> Normal
            if(pMesh->pAttributes->n > 1)
            {
	            glEnableVertexAttribArray(1);
                bk3d::Attribute* pAttrN = pMesh->pAttributes->p[1];
                glBindVertexBuffer(1, 0, 0, pAttrN->strideBytes);
                glVertexAttribFormat(1, pAttrN->numComp, pAttrN->formatGL, GL_TRUE, pAttrN->dataOffsetBytes);
			    glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 1,
                    curVBO.Addr + (GLuint64EXT)pMesh->pSlots->p[pAttrN->slot]->userPtr.p,
                    pMesh->pSlots->p[pAttrN->slot]->vtxBufferSizeBytes);
            } else {
	            glDisableVertexAttribArray(1);
            }
            //====> render primitive groups
		    for(int pg=0; pg<pMesh->pPrimGroups->n; pg++)
		    {
                //
                // Material: point to the right one in the table
                //
			    bk3d::Material *pMat = pMesh->pPrimGroups->p[pg]->pMaterial;
                if(pMat && (curMaterial != pMat->ID))
                {
                    curMaterial = pMat->ID;
                    glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATERIAL, m_uboMaterial.Addr + (curMaterial * sizeof(MaterialBuffer)), sizeof(MaterialBuffer));
                }
                if(pMesh->pPrimGroups->p[pg]->pTransforms->n>0)
                {
			        bk3d::Bone *pTransf = pMesh->pPrimGroups->p[pg]->pTransforms->p[0];
                    if(pTransf && (curTransf != pTransf->ID))
                    {
                        curTransf = pTransf->ID;
                        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATRIXOBJ, m_uboObjectMatrices.Addr + (curTransf * sizeof(MatrixBufferObject)), sizeof(MatrixBufferObject));
                    }
                }
                //
                // Set the right shader
                //
                if(pMesh->pPrimGroups->p[pg]->topologyGL == GL_LINES) {
                    glDisable(GL_POLYGON_OFFSET_FILL);
                    s_shaderMeshLine.bindShader();
                } else {
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(1.0, 1.0); // no issue with redundant call here: the state capture will just deal with simplifying things
                    s_shaderMesh.bindShader();
                }
                if(pMesh->pPrimGroups->p[pg]->userPtr)
                {
			        glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV, 0,
                        curEBO.Addr + (GLuint64EXT)pMesh->pPrimGroups->p[pg]->userPtr,
                        pMesh->pPrimGroups->p[pg]->indexArrayByteSize - pMesh->pPrimGroups->p[pg]->indexArrayByteOffset);
			        glDrawElements(
				        pMesh->pPrimGroups->p[pg]->topologyGL,
				        pMesh->pPrimGroups->p[pg]->indexCount,
				        pMesh->pPrimGroups->p[pg]->indexFormatGL,
				        NULL);
                } else {
			        glDrawArrays(
				        pMesh->pPrimGroups->p[pg]->topologyGL,
				        0, pMesh->pPrimGroups->p[pg]->indexCount);
                }
		    }
	    }
	    glDisableVertexAttribArray(0);
	    glDisableVertexAttribArray(1);

        glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
        glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
        glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    }
}

bool Bk3dModel::initGraphics_bk3d()
{
    //
    // Shader compilation
    //
    if(!s_shaderMesh.addVertexShaderFromString(s_glslv_mesh))
        return false;
    if(!s_shaderMesh.addFragmentShaderFromString(s_glslf_mesh))
        return false;
    if(!s_shaderMesh.link())
        return false;
    if(!s_shaderMeshLine.addVertexShaderFromString(s_glslv_mesh))
        return false;
    if(!s_shaderMeshLine.addFragmentShaderFromString(s_glslf_mesh_line))
        return false;
    if(!s_shaderMeshLine.link())
        return false;
    return true;
}

void Bk3dModel::printPosition()
{
	LOGI("%f %f %f %f\n", m_posOffset[0], m_posOffset[1], m_posOffset[2], m_scale);
}

void Bk3dModel::addStats(Stats &stats)
{
    stats.primitives  += m_stats.primitives;
    stats.drawcalls   += m_stats.drawcalls;
    stats.attr_update += m_stats.attr_update;
    stats.uniform_update += m_stats.uniform_update;
}
