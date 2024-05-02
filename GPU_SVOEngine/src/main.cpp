#include "engine.hpp"
#include "utils/logger.hpp"

#include "Octree/octree.hpp"
#include "Octree/voxelizer.hpp"

//#define EXIT_ON_NO_ARGS

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
std::string modelPath = "assets/sponza/sponza.obj";
uint8_t depth = 12;
bool loadFlag = false;
bool voxelizeFlag = true;
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
        }
        else if (voxelizeFlag)
        {
            Voxelizer voxelizer{ modelPath, depth };
            octree.generate(voxelizer.getModelAABB(), Voxelizer::voxelize, &voxelizer);
            octree.setMaterialPath(voxelizer.getMaterialFilePath());
            for (const Material& mat : voxelizer.getMaterials())
                octree.addMaterial(mat.toOctreeMaterial(), mat.diffuseMap, mat.normalMap, mat.specularMap);
            if (saveFlag)
                octree.dump(savePath);
        }
        
        octree.packAndFinish();

        Engine engine{ static_cast<uint32_t>(octree.getMaterialTextures().size()) };

        Logger::setRootContext("Engine context init");
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
