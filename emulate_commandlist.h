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

*/ //--------------------------------------------------------------------
namespace emucmdlist
{
  //
  // Ridiculous State capture system. The intent is not to make it work for more
  // than what is neededed in this sample to do the emulation
  // for a solid state tracking, see other samples and in the shared_sources
  //
  struct MiniStateCapture {
      struct Attr {
          bool enabledVertexAttrib;
          GLuint stride;
          GLuint size;
          GLuint offset;
      };
      GLenum mode; // always used
      GLuint primRestartIndex;
      // we could add more: depending on what in this sample gets modified along the
      // recording of the Meshes. But so far the sample is simple enough to not do a lot
      GLint prog;
      Attr attrs[2]; // only 2 used in this demo... that's okay ;-)
  };
  typedef std::map<GLuint, MiniStateCapture> MapStates;

#ifdef EMUCMDLIST_EXTERN
  extern void InitHeaders(GLuint *headers);
  extern void DeleteStatesNV();
  extern void StateCaptureNV(GLuint state, GLenum mode);
  extern void StateCaptureNV_Extra(GLuint state
      , GLuint stride0, GLuint size0, GLuint offset0
      , GLuint stride1, GLuint size1, GLuint offset1
      );
  extern void StateApply(GLuint curID, GLuint prevID=~0);
  //extern GLenum nvtokenRenderSW( const void* stream, size_t streamSize, GLenum mode, GLenum type);
  extern void nvtokenRenderStatesSW(const void* __restrict stream, size_t streamSize, 
    const GLintptr* __restrict offsets, const GLsizei* __restrict sizes, 
    const GLuint* __restrict states, const GLuint* __restrict fbos, GLuint count);
#else
  MapStates mapStates;
  struct Header {
      GLuint   cmd;
      GLuint   sz;
  };
  typedef std::map<GLenum, Header> HWHeaders;
  HWHeaders hwHeaders;

  void InitHeaders(GLuint *headers, GLuint *headerSizes)
  {
      for(int i=0; i<GL_MAX_COMMANDS_NV; i++)
      {
        hwHeaders[headers[i]].cmd = i;
        hwHeaders[headers[i]].sz = headerSizes[i];
      }
  }

  void DeleteStatesNV()
  {
      mapStates.clear();
  }
  void StateCaptureNV(GLuint state, GLenum mode)
  {
      //assert(mapStates.find(state) == mapStates.end());
      MiniStateCapture &s = mapStates[state];
      s.mode = mode;
      // TODO: setup the few states we dare to track ;-)
      glGetIntegerv(GL_CURRENT_PROGRAM, &s.prog);
      GLint res;
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &res);
      s.attrs[0].enabledVertexAttrib = res ? true:false;
      glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &res);
      s.attrs[1].enabledVertexAttrib = res ? true:false;
      // ...
  }
  // additional arguments for states that I couldn't grab through OpenGL API :-(
  void StateCaptureNV_Extra(GLuint state
      , GLuint stride0, GLuint size0, GLuint offset0
      , GLuint stride1, GLuint size1, GLuint offset1
      )
  {
      MiniStateCapture &s = mapStates[state];
      s.attrs[0].stride = stride0;
      s.attrs[0].size = size0;
      s.attrs[0].offset = offset1;
      s.attrs[1].stride = stride1;
      s.attrs[1].size = size1;
      s.attrs[1].offset = offset1;
      // ...
  }
  void StateApply(GLuint curID, GLuint prevID=~0)
  {
    // TODO seriously !!
    // Now, just to a band-aid version for the demo here
    MiniStateCapture &s = mapStates[curID];
    if((prevID == ~0)||(curID != prevID))
    {
        glUseProgramObjectARB(s.prog);
        for(int i=0; i<2; i++)
        {
            if(s.attrs[i].enabledVertexAttrib)
                glEnableVertexAttribArray(i);
            else
                glDisableVertexAttribArray(i);
            if(s.attrs[i].size) {
                glBindVertexBuffer(i, 0, 0, s.attrs[i].stride);
                glVertexAttribFormat(i,s.attrs[i].size, GL_FLOAT, GL_FALSE, s.attrs[i].offset);
            }
        }
    }
  }

  GLenum nvtokenRenderSW( const void* stream, size_t streamSize, GLenum mode, GLenum type) 
  {
    const GLubyte* __restrict current = (GLubyte*)stream;
    const GLubyte* streamEnd = current + streamSize;

    GLenum modeStrip;
    if      (mode == GL_LINES)                modeStrip = GL_LINE_STRIP;
    else if (mode == GL_TRIANGLES)            modeStrip = GL_TRIANGLE_STRIP;
    else if (mode == GL_QUADS)                modeStrip = GL_QUAD_STRIP;
    else if (mode == GL_LINES_ADJACENCY)      modeStrip = GL_LINE_STRIP_ADJACENCY;
    else if (mode == GL_TRIANGLES_ADJACENCY)  modeStrip = GL_TRIANGLE_STRIP_ADJACENCY;
    else    modeStrip = mode;

    GLenum modeSpecial;
    if      (mode == GL_LINES)      modeSpecial = GL_LINE_LOOP;
    else if (mode == GL_TRIANGLES)  modeSpecial = GL_TRIANGLE_FAN;
    else    modeSpecial = mode;

    while (current < streamEnd){
      const CommandHeaderNV*    header  = (const CommandHeaderNV*)current;
      const void*               data    = (const void*)(header+1);

      Header hd = hwHeaders[header->encoded];

      switch(hd.cmd)
      {
      case GL_TERMINATE_SEQUENCE_COMMAND_NV:
        {
          return type;
        }
        break;
      case GL_NOP_COMMAND_NV:
        {
        }
        break;
      case GL_DRAW_ELEMENTS_COMMAND_NV:
        {
          const DrawElementsCommandNV* cmd = (const DrawElementsCommandNV*)data;
          glDrawElementsBaseVertex(mode, cmd->count, type, (const GLvoid*)(cmd->firstIndex * sizeof(GLuint)), cmd->baseVertex);
        }
        break;
      case GL_DRAW_ARRAYS_COMMAND_NV:
        {
          const DrawArraysCommandNV* cmd = (const DrawArraysCommandNV*)data;
          glDrawArrays(mode, cmd->first, cmd->count);
        }
        break;
      case GL_DRAW_ELEMENTS_STRIP_COMMAND_NV:
        {
          const DrawElementsCommandNV* cmd = (const DrawElementsCommandNV*)data;
          glDrawElementsBaseVertex(modeStrip, cmd->count, type, (const GLvoid*)(cmd->firstIndex * sizeof(GLuint)), cmd->baseVertex);
        }
        break;
      case GL_DRAW_ARRAYS_STRIP_COMMAND_NV:
        {
          const DrawArraysCommandNV* cmd = (const DrawArraysCommandNV*)data;
          glDrawArrays(modeStrip, cmd->first, cmd->count);
        }
        break;
      case GL_DRAW_ELEMENTS_INSTANCED_COMMAND_NV:
        {
          const DrawElementsInstancedCommandNV* cmd = (const DrawElementsInstancedCommandNV*)data;

          assert (cmd->mode == mode || cmd->mode == modeStrip || cmd->mode == modeSpecial);

          glDrawElementsIndirect(cmd->mode, type, &cmd->count);
        }
        break;
      case GL_DRAW_ARRAYS_INSTANCED_COMMAND_NV:
        {
          const DrawArraysInstancedCommandNV* cmd = (const DrawArraysInstancedCommandNV*)data;

          assert (cmd->mode == mode || cmd->mode == modeStrip || cmd->mode == modeSpecial);

          glDrawArraysIndirect(cmd->mode, &cmd->count);
        }
        break;
      case GL_ELEMENT_ADDRESS_COMMAND_NV:
        {
          const ElementAddressCommandNV* cmd = (const ElementAddressCommandNV*)data;
          type = cmd->typeSizeInByte == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
          glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV, 0, cmd->address, 0x7FFFFFFF);
        }
        break;
      case GL_ATTRIBUTE_ADDRESS_COMMAND_NV:
        {
          const AttributeAddressCommandNV* cmd = (const AttributeAddressCommandNV*)data;
          glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, cmd->index, cmd->address, 0x7FFFFFFF);
        }
        break;
      case GL_UNIFORM_ADDRESS_COMMAND_NV:
        {
          const UniformAddressCommandNV* cmd = (const UniformAddressCommandNV*)data;
          glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, cmd->index, cmd->address, 0x10000);
        }
        break;
      case GL_BLEND_COLOR_COMMAND_NV:
        {
          const BlendColorCommandNV* cmd = (const BlendColorCommandNV*)data;
          glBlendColor(cmd->red,cmd->green,cmd->blue,cmd->alpha);
        }
        break;
      case GL_STENCIL_REF_COMMAND_NV:
        {
            assert(!"TODO");
          const StencilRefCommandNV* cmd = (const StencilRefCommandNV*)data;
          //glStencilFuncSeparate(GL_FRONT, state.stencil.funcs[StateSystem::FACE_FRONT].func, cmd->frontStencilRef, state.stencil.funcs[StateSystem::FACE_FRONT].mask);
          //glStencilFuncSeparate(GL_BACK,  state.stencil.funcs[StateSystem::FACE_BACK ].func, cmd->backStencilRef,  state.stencil.funcs[StateSystem::FACE_BACK ].mask);
        }
        break;

      case GL_LINE_WIDTH_COMMAND_NV:
        {
          const LineWidthCommandNV* cmd = (const LineWidthCommandNV*)data;
          glLineWidth(cmd->lineWidth);
        }
        break;
      case GL_POLYGON_OFFSET_COMMAND_NV:
        {
          const PolygonOffsetCommandNV* cmd = (const PolygonOffsetCommandNV*)data;
          glPolygonOffset(cmd->scale,cmd->bias);
        }
        break;
      case GL_ALPHA_REF_COMMAND_NV:
        {
            assert(!"TODO");
          const AlphaRefCommandNV* cmd = (const AlphaRefCommandNV*)data;
          //glAlphaFunc(state.alpha.mode, cmd->alphaRef);
        }
        break;
      case GL_VIEWPORT_COMMAND_NV:
        {
          const ViewportCommandNV* cmd = (const ViewportCommandNV*)data;
          glViewport(cmd->x, cmd->y, cmd->width, cmd->height);
        }
        break;
      case GL_SCISSOR_COMMAND_NV:
        {
          const ScissorCommandNV* cmd = (const ScissorCommandNV*)data;
          glScissor(cmd->x,cmd->y,cmd->width,cmd->height);
        }
        break;
      }
      current += hd.sz;
    }
    return type;
  }

  void nvtokenRenderStatesSW(const void* __restrict stream, size_t streamSize, 
    const GLintptr* __restrict offsets, const GLsizei* __restrict sizes, 
    const GLuint* __restrict states, const GLuint* __restrict fbos, GLuint count)
  {
    glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    int lastFbo = ~0;
    int lastID = ~0;
    const char* __restrict tokens = (const char*)stream;

    GLenum type = GL_UNSIGNED_SHORT;
    for (GLuint i = 0; i < count; i++)
    {
      GLuint fbo;

      GLuint curID = states[i];
      MiniStateCapture &state = mapStates[curID];

      if (fbos[i]){
        fbo = fbos[i];
      }
      else{
        assert(!"TODO");
        //fbo = ;
      }

      if (fbo != lastFbo){
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        lastFbo = fbo;
      }

      // idea would be to apply the difference only
      if (i == 0){
        StateApply(curID);
      }
      else {
        StateApply(curID, lastID);
      }
      lastID = curID;

      size_t offset = offsets[i];
      size_t size   = sizes[i];

      assert(size + offset <= streamSize);

      type = nvtokenRenderSW(&tokens[offset], size, state.mode, type);
    }
    glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
  }
#endif //EMUCMDLIST_EXTERN

}// emucmdlist
