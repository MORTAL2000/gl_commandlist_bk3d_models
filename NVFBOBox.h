#pragma once
//--------------------------------------------------------------------------------------
// Author: Tristan Lorach
// Email: tlorach@nvidia.com
//
// Copyright (c) NVIDIA Corporation 2009
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA OR ITS SUPPLIERS
// BE  LIABLE  FOR  ANY  SPECIAL,  INCIDENTAL,  INDIRECT,  OR  CONSEQUENTIAL DAMAGES
// WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS OF BUSINESS PROFITS,
// BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
// ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//
// Implementation of different antialiasing methods.
// - typical MSAA
// - CSAA
// - Hardware AA mixed with FBO for supersampling pass
//   - simple downsampling
//   - downsampling with 1 or 2 kernel filters
//
// AABox is the class that will handle everything related to supersampling through 
// an offscreen surface defined thanks to FBO
// Basic use is :
//
//  Initialize()
//  ...
//  Activate(int x=0, int y=0)
//	Draw the scene (so, in the offscreen supersampled buffer)
//  Deactivate()
//  Draw() : downsample to backbuffer
//  ...
//  Finish()
//
//
// Copyright (c) NVIDIA Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#ifndef GL_FRAMEBUFFER_EXT
#	define GL_FRAMEBUFFER_EXT				0x8D40
typedef unsigned int GLenum;
#endif

class InvFBOBox
{
public:
	enum DownSamplingTechnique
	{
		DS1 = 0,
		DS2 = 1,
		DS3 = 2,
		NONE = 3
	};
	virtual bool Initialize(int w, int h, float ssfact, int depthSamples, int coverageSamples=0, int tilesW=1, int tilesH=1, bool bOneFBOPerTile=false) = 0;
    virtual bool resize(int w, int h, float ssfact= -1.0, int depthSamples=-1, int coverageSamples=-1) = 0;
    virtual void MakeResourcesResident() = 0;
	virtual void Finish() = 0;

	virtual int getTilesW() = 0;
	virtual int getTilesH() = 0;

	virtual int getWidth() = 0;  // width and height of a tile
	virtual int getHeight() = 0;
	virtual int getBufferWidth() = 0;
	virtual int getBufferHeight() = 0;
	virtual float getSSFactor() = 0; // supersampling scale ssfact
	virtual void ActivateBuffer(int tilex=0, int tiley=0, GLenum target = GL_FRAMEBUFFER_EXT) = 0;
	virtual void Activate(int tilex=0, int tiley=0, float m_frustum[][4] = NULL) = 0;
	virtual void Deactivate() = 0;
	virtual bool ResolveAA(DownSamplingTechnique technique=DS1, int tilex=0, int tiley=0) = 0;
	virtual void Draw(DownSamplingTechnique technique, int tilex, int tiley, int windowW, int windowH, float *offset) = 0;

	virtual bool PngWriteFile( const char *file) = 0;
	virtual void PngWriteData(DownSamplingTechnique technique=DS1, int tilex=0, int tiley=0) = 0;

    virtual unsigned int GetFBO(int i=0) = 0;
};
class BaseOwner;
extern InvFBOBox *createNVFBOBox();
extern void destroyNVFBOBox(InvFBOBox **nvFBOBox);

