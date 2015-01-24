# NVIDIA commandlist extension over bk3d models

*Note*: CMake MODELS_DOWNLOAD_DISABLED is ON by default so you don't get stuck in cmake downloading the heavy 3D models that this demo can use. Unchecking it will download 8 models in a local folder called *downloaded_resources*. I could take time...

This sample shows how to use NVIDIA command-list extension over basic scenes

The sample uses a pre-baked binary format (bk3d) for various models coming from CAD applications.

The main file 'gl_commandlist_bk3d_models.cpp' shows how to setup command-list on simple geometries such
as the floor grid and the target cross. It is a good starting point to understand how to use command-lists

The second file 'gl_commandlist_bk3d.cpp' contains everything related to bk3d: it will load, setup and display data
coming from this file format.

This samples can either run in MSAA mode but also in 'supersampled' mode (see NVFBOBox)

##Command-line arguments
* -v <VBO max Size>\n-m <bk3d model>
* -c 0 or 1 : use command-lists
* -b 0 or 1 : use bindless when no cmd list
* -o 0 or 1 : display meshes
* -g 0 or 1 : display grid
* -s 0 or 1 : stats
* -a 0 or 1 : animate camera
* -i <file> : use a config file to load models and setup camera animation
* -d 0 or 1 : debug stuff (ui)
* -m <bk3d file> : load a specific model
* <bk3d file> : load a specific model
* -q <msaa> : MSAA
* -r <ss_val> : supersampling (1.0,1.5,2.0)

###Examples on arguments

load a scene and start animation:

gl_commandlist_bk3d_models.exe -i scene_car.txt -a 1

load a bk3d file:

gl_commandlist_bk3d_models.exe -m SubMarine_134.bk3d.gz

or

gl_commandlist_bk3d_models.exe SubMarine_134.bk3d.gz


##in app toggles
* 'h': help
* space: toggles continuous rendering
* 'c': use Commandlist
* 'e': use Commandlist EMULATION
* 'l': use glCallCommandListNV
* 'o': toggles object display
* 'g': toggles grid display
* 's': toggle stats
* 'a': animate camera

##Scene from external file (-i)
This is a simple description made of:
* a list of bk3d objects to load
* a list of position-target coordinates for the camera animation

````
<num of filenames>
file0.bk3d.gz
posX posY posZ scale
file1.bk3d.gz
posX posY posZ scale
...
<num of camera coords>
posX posY posZ TargetX TargetY TargetZ Time_to_wait
posX posY posZ TargetX TargetY TargetZ Time_to_wait
...
````

![Example](https://github.com/nvpro-samples/gl_commandlist_bk3d_models/blob/master/doc/sample.jpg)

````
    Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:
     * Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
     * Neither the name of NVIDIA CORPORATION nor the names of its
       contributors may be used to endorse or promote products derived
       from this software without specific prior written permission.

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

````

