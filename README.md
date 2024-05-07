# GPU_SVOEngine
![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/b3308642-a0d2-464b-9fe3-332b17c10002)

A reimplementation and improvement of the [Sparse Voxel Octree Engine](https://github.com/AsperTheDog/SVOEngine) I made long ago, but this time using the GPU via Vulkan.

## How to use
The release exe only requires the user to have a Vulkan compatible GPU and the latest version of [vcredist](https://aka.ms/vs/17/release/vc_redist.x64.exe) installed. GPU requirements depend on the complexity of the SVO being loaded.
This program is made to be used from a terminal. If the release exe is run by double clicking the program will exit instantly. This is because the different options must be supplied by command line arguments. Executing the program with no command line arguments will print the different options and close the program. The options can also be seen below:
```
Usage: ./GPU_SVOEngine [options]
Options:
  -d <depth>          Set the depth of the octree, ignored if -l is added
  -m <path>           Load model from file, ignored if -l is added
  -s <path>           Save octree to file, ignored if -m is not added or if -l is added
  -l <path>           Load octree from file
```
The exe must always have the shaders folder next to it with the raytracing.vert file and the raytracing.frag file inside it. I plan on baking these into the code itself but while I am developing the application they will stay there as it is easier for me to edit them when they are in their own files.
The release also comes with a basic model called test_ico.obj for people to test easily.

## What it is

The basic objective of this project is to be able to render a Sparse Voxel Octree (SVO) in real-time using the GPU. To create SVOs that look coherent I have created a voxelization algorithm that takes obj files and processes them to generate an SVO of them. Data about what normals and UVs these voxels should have are encoded into the SVO alongside material and texture data so that the visualizer can display it with color and basic lighting. The results are the following:

![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/f6872af3-20e4-4786-bd6c-1f22bc1402e9)
**Model:** San Miguel 2.0  
**SVO Data:** 12 levels of depth (2048x2048x2048)

![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/5dc5f87d-03f8-4bb3-b633-3ccd28127987)
**Model:** Amazon Lumberyard Bistro  
**SVO Data:** 12 levels of depth (2048x2048x2048)

![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/800d9f56-4dd3-4113-b7b6-738d42c00eb5)
**Model**: Erato  
**SVO Data:** 14 levels of depth (4196x4196x4196)

![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/1f51c7e2-b60d-4a89-9984-36554a208773)
**Model:** Crytek Sponza  
**SVO Data:** 12 levels of depth (2048x2048x2048)

Models downloaded from Morgan McGuire's [Computer Graphics Archive](https://casual-effects.com/data)

## How it works
The main reference for the data structure and ray traversal algorithm is [NVIDIA's Efficient Sparse Voxel Octrees research paper](https://research.nvidia.com/publication/2010-02_efficient-sparse-voxel-octrees). The implementation has been done in Vulkan using a fragment shader for the raytracer. The SVO is generated and sent to the GPU as a storage buffer. Images are also loaded into GPU memory for the shader to fetch.

The two main parts of the project are the Octree class and the Engine class. The octree class is the one in charge of loading or generating the octree data structure, while the engine is the one responsible for initializing all the vulkan environment and loading the data to the GPU.
The Voxelizer class is a helper class that contains all the context the program needs to generate an SVO from a 3D model. The Octree, however, does not need this class at all to work.

The main structure for the generation of the SVO is designed to be detached from any predefined rules. When generating the SVO, it will ask for a process function and a void* as arguments. The function provided as an argument is the one responsible for, given the proper context, decide if a specific voxel is to exist or not and deciding the properties of it if it's a leaf node. The Octree on the other hand has a general algorithm that will automatically handle the internal octree structure. The Octree class also provides tools to serialize and load octrees to and from a binary file.

```cpp
Octree octree{ depth };
Voxelizer voxelizer{ modelPath, depth };
// Definition: NodeRef voxelize(const AABB& nodeShape, const uint8_t depth, const uint8_t maxDepth, void* data)
// The voxelize function is a global function that returns if a specific node exists and its properties
// The voxelizer is given as a void* so the octree can give it to the voxelize function as the argument "data"
octree.generate(voxelizer.getModelAABB(), voxelize, &voxelizer);
```

As an important note. The generation algorithm generates a reversed octree. This is because in order to properly dispose of possible branches in the octree that end up having no leaves, the algorithm will generate the SVO bottom to top. This greatly increases the efficiency of the algorithm and the quality of the SVO. The class does not reverse the octree on generation automatically because the operation can be very slow and the Engine can more efficiently reverse it when sending it to the GPU.

## Building
The project is currently a direct upload of my Visual Studio project. It has been made with VS 2022 and uses C++ 20. I have plans on making an scons or premake build configuration but I have not done it yet since it's low priority for me right now.
While I can assure that the release configuration generates a platform independent program, the debug program could crash on other devices or with other compilers. This is because the debug version uses some data structures that may be reordered by the compiler, corrupting the data given to the GPU. The releases are all of course compiled using the release configuration.

This project assumes that the Vulkan SDK is installed and that its path is written to an environment variable called **VULKAN_SDK**. This environment variable is added to PATH by the SDK installer automatically unless the user specifies otherwise on installation.

When running it from Visual Studio make sure the macro EXIT_ON_NO_ARGS is not defined. Defining this macro will close the program immediately if no command line arguments are provided. This is for releases only and the code will always have it commented in the repository.
To comfortably configure the different options it is recommended to change the variable declarations at the beginning of the main.cpp file.
```cpp
#ifdef EXIT_ON_NO_ARGS
// Default values for release
std::string loadPath = "assets/octree.bin";
std::string savePath = "assets/octree.bin";
std::string modelPath = "assets/test_ico.obj";
uint8_t depth = 11;
bool loadFlag = false;
bool voxelizeFlag = false;
bool saveFlag = false;
#else
// Values to use when executing from IDE. Change these paths
std::string loadPath = "assets/octree.bin";
std::string savePath = "assets/octree.bin";
std::string modelPath = "assets/sponza/sponza.obj";
uint8_t depth = 12;
bool loadFlag = false;
bool voxelizeFlag = !loadFlag;
bool saveFlag = false;
#endif

```

## Future plans
- Transparency via alpha blend. Semitransparent voxels would be a very nice addition to the visualizer, specially for scenes like San Miguel 2.0.
- Voxelization on the GPU. This is far and probably never happen, but I would like to try.
- More advanced lighting techniques like reflections or GI. I would first need the project to allow accumulating samples and improve performance so it remains responsive. Probably won't happen either.

Truth is I am quite happy with the result. I may add some small things here and there but I doubt there will be any major changes to the project.

## Other

This project was incredibly exciting to create but the resources were scarce and it was very hard to wrap my head around a lot of concepts. If anyone has any question on how anything works don't hesitate to open an issue asking.

![image](https://github.com/AsperTheDog/GPU_SVOEngine/assets/45227294/b6294512-c8fb-4858-b4ce-622f23be1171)

