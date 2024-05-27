#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"
#include "Octree/voxelizer.hpp"

//#define EXIT_ON_NO_ARGS

#define PARALLEL_VOXELIZATION

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
// Values to use when executing from IDE
std::string loadPath = "assets/octree.bin";
std::string savePath = "assets/octree.bin";
std::string modelPath = "assets/Interior/interior.obj";
uint8_t depth = 12;
bool loadFlag = false;
bool voxelizeFlag = !loadFlag;
bool saveFlag = true;
#endif

void printHelpAndExit()
{
    std::cout << "Usage: GPU_SVOEngine.exe [options]\n"
        << "Options:\n"
        << "  -d <depth>          Set the depth of the octree, ignored if -l is added\n"
        << "  -m <path>           Load model from file, ignored if -l is added\n"
        << "  -s <path>           Save octree to file, ignored if -m is not added or if -l is added\n"
        << "  -l <path>           Load octree from file\n";
    exit(EXIT_SUCCESS);
}

void parseCommands(const int argc, char* argv[])
{
    if (argc == 1 || argc % 2 == 0)
    {
#ifdef EXIT_ON_NO_ARGS
        printHelpAndExit();
#else
        return;
#endif
    }

    bool depthProvided = false;
    for (int i = 1; i < argc; i += 2)
    {
        if (strcmp(argv[i], "-d") == 0)
        {
            try 
            {
                depth = static_cast<uint8_t>(std::stoul(argv[i + 1]));
            }
            catch (const std::exception& e)
            {
                Logger::print("Invalid depth value, using default value of " + std::to_string(depth), Logger::LevelBits::WARN);
            }
            depthProvided = true;
        }
        else if (strcmp(argv[i], "-m") == 0)
        {
            modelPath = argv[i + 1];
            voxelizeFlag = true;
        }
        else if (strcmp(argv[i], "-s") == 0)
        {
            savePath = argv[i + 1];
            saveFlag = true;
        }
        else if (strcmp(argv[i], "-l") == 0)
        {
            loadPath = argv[i + 1];
            loadFlag = true;
        }
    }
    if (loadFlag && (saveFlag || voxelizeFlag))
    {
        Logger::print("Cannot load and generate at the same time, ignoring save and/or voxelize flags", Logger::LevelBits::WARN);
        saveFlag = false;
        voxelizeFlag = false;
    }
    if (depthProvided && loadFlag)
    {
        Logger::print("Depth provided but loading octree, ignoring depth", Logger::LevelBits::WARN);
    }
    if (!voxelizeFlag && !loadFlag)
    {
        Logger::print("No model provided and no octree to load, exiting", Logger::LevelBits::ERR);
        exit(EXIT_FAILURE);
    }
    if (saveFlag && !voxelizeFlag)
    {
        Logger::print("No model provided, ignoring save flag", Logger::LevelBits::WARN);
        saveFlag = false;
    }
    if (!depthProvided && voxelizeFlag)
    {
        Logger::print("No depth provided, using default value of " + std::to_string(depth), Logger::LevelBits::WARN);
    }
    if (voxelizeFlag && !saveFlag)
    {
        Logger::print("No save path provided, octree will be lost on exit", Logger::LevelBits::WARN);
    }
}

int main(const int argc, char* argv[])
{
    parseCommands(argc, argv);

#ifndef _DEBUG
    try
    {
        Logger::setLevels(Logger::INFO | Logger::WARN | Logger::ERR);
#else
        Logger::setLevels(Logger::ALL);
#endif

        Logger::setRootContext("Octree init");
        Octree octree{ depth };
        
        if (loadFlag)
        {
            octree.load(loadPath);
            depth = octree.getDepth();
        }
        else if (voxelizeFlag)
        {
            // The octree is kept independent from the voxelizer, because maybe you want to generate an octree
            // that is not voxelizing a model, like a procedural octree or one that voxelizes a mathematical function
            // The octree requests a function (ProcessFunc or ParallelProcessFunc) that will be called for each node.
            // This function is supposed to say if a node exists or not given an AABB shape and other metadata.
            // It is also responsible for setting the leaf data, if the node is a leaf.
            // It also accepts a void pointer that can be used to pass data to the function.
            Voxelizer voxelizer{ modelPath, depth };
#ifdef PARALLEL_VOXELIZATION
            octree.generateParallel(voxelizer.getModelAABB(), Voxelizer::parallelVoxelize, &voxelizer);
#else
            octree.generate(voxelizer.getModelAABB(), Voxelizer::voxelize, &voxelizer);
#endif
            // Material data is stored separately in the octree, since voxels contain material IDs that point to the specific material
            // Materials will also point to different images, the octree stores the paths and resolves the map IDs in the material
            octree.setMaterialPath(voxelizer.getMaterialFilePath());
            for (const Material& mat : voxelizer.getMaterials())
                octree.addMaterial(mat.toOctreeMaterial(), mat.diffuseMap, mat.normalMap, mat.specularMap);
            // Optionally, all octree data can be dumped. This is a very simple binary dump but it stores all necessary data and some statistics of the octree
            if (saveFlag)
                octree.dump(savePath);
        }

        // Called to generate a possible sample material if none are provided (as safeguard) and to finalize some statistics
        // This is not necessary, but it is recommended to call it before packing the octree
        octree.packAndFinish();

        // The engine initializes all Vulkan resources using VkPlayground (https://github.com/AsperTheDog/VkPlayground)
        Engine engine{ static_cast<uint32_t>(octree.getMaterialTextures().size()), depth };

        Logger::setRootContext("Engine context init");
        // Send the octree and textures to the GPU
        engine.configureOctreeBuffer(octree, 100.0f);
        engine.run();

#ifndef _DEBUG
    }
    catch (const std::exception& e)
    {
        Logger::print(e.what(), Logger::LevelBits::ERR);
        return EXIT_FAILURE;
    }
#endif

    return EXIT_SUCCESS;
}
