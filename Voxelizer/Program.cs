using ObjLoader.Loader.Data;
using ObjLoader.Loader.Data.Elements;
using ObjLoader.Loader.Data.VertexData;
using ObjLoader.Loader.Loaders;
using System.Drawing;
using System.Globalization;
using System.Numerics;
using static System.Formats.Asn1.AsnWriter;

namespace Voxelizer
{
    public class Voxelizer
    {
        static void Main(string[] _args)
        {
            // To be set as function args
            var modelPath = GetAssetPath("Models\\Objs\\triangle.obj");
            var octreeResolution = 1;


            Print("Voxelizing file at:");
            Print("\t" + modelPath);
            Print($"Octree resolution:\n\t{octreeResolution}");

            var result = Voxelize(modelPath, octreeResolution);
            result.SaveToObj(GetAssetPath("Models\\Objs\\triangleVox.obj"));
        }




        private static string GetAssetPath(string relPath)
        {
            var rootPath = Directory.GetParent(Environment.CurrentDirectory).Parent.Parent.FullName;
            return Path.Join(rootPath, relPath);
        }
        private static void Print(string str)
        {
            Console.WriteLine(str);
        }

        public class VoxelizedMesh
        {
            public int resolution = 1;
            public float scale = 1.0f;
            public List<(Vec3UInt, Voxel)> voxels = [];

            public void SaveToObj(string path)
            {
                using (StreamWriter writer = new StreamWriter(path))
                {
                    var cubeDist = scale / resolution;

                    var voxCount = 0;
                    NumberFormatInfo nfi = new CultureInfo("en-US", false).NumberFormat;
                    foreach (var voxInfo in voxels)
                    {
                        var voxPos = voxInfo.Item1;

                        writer.WriteLine($"v {((voxPos.X + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z + 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z - 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z + 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z - 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z + 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y + 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z - 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z + 0.5f) * cubeDist).ToString(nfi)}");
                        writer.WriteLine($"v {((voxPos.X - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Y - 0.5f) * cubeDist).ToString(nfi)} {((voxPos.Z - 0.5f) * cubeDist).ToString(nfi)}");

                        var vertIdx = voxCount * 8;
                        writer.WriteLine($"f {vertIdx + 1} {vertIdx + 2} {vertIdx + 3}");
                        writer.WriteLine($"f {vertIdx + 2} {vertIdx + 3} {vertIdx + 4}");
                        writer.WriteLine($"f {vertIdx + 5} {vertIdx + 6} {vertIdx + 7}");
                        writer.WriteLine($"f {vertIdx + 6} {vertIdx + 7} {vertIdx + 8}");

                        writer.WriteLine($"f {vertIdx + 1} {vertIdx + 2} {vertIdx + 5}");
                        writer.WriteLine($"f {vertIdx + 2} {vertIdx + 5} {vertIdx + 6}");
                        writer.WriteLine($"f {vertIdx + 3} {vertIdx + 4} {vertIdx + 7}");
                        writer.WriteLine($"f {vertIdx + 4} {vertIdx + 7} {vertIdx + 8}");

                        writer.WriteLine($"f {vertIdx + 1} {vertIdx + 5} {vertIdx + 7}");
                        writer.WriteLine($"f {vertIdx + 3} {vertIdx + 7} {vertIdx + 1}");
                        writer.WriteLine($"f {vertIdx + 2} {vertIdx + 6} {vertIdx + 8}");
                        writer.WriteLine($"f {vertIdx + 4} {vertIdx + 8} {vertIdx + 2}");

                        voxCount++;
                    }
                }
            }
        }

        public struct Voxel
        {
            public Color color;
            public Vector2 uv;
            public Normal normal;
        }

        public struct Vec3UInt(uint x, uint y, uint z)
        {
            public uint X = x;
            public uint Y = y;
            public uint Z = z;
        }

        private class IntermediateVoxelizedMesh
        {
            public Dictionary<Vec3UInt, List<Voxel>> voxelCandidates = [];

            public VoxelizedMesh GetResult(int resolution, float scale)
            {
                var result = new VoxelizedMesh();
                result.resolution = resolution;
                result.scale = scale;
                foreach (var item in voxelCandidates)
                {
                    Vec3UInt pos = item.Key;

                    /// TODO merge colors/normals/uvs appropriately
                    Voxel vox = item.Value[0];

                    result.voxels.Add((pos, vox));
                }

                return result;
            }
        }

        public static VoxelizedMesh Voxelize(string objPath, int resolution)
        {
            var objLoaderFactory = new ObjLoaderFactory();
            var objLoader = objLoaderFactory.Create();

            var fileStream = new FileStream(objPath, FileMode.Open, FileAccess.Read);
            LoadResult objResult = objLoader.Load(fileStream) ?? throw new FileLoadException("Unable to load OBJ file");

            IntermediateVoxelizedMesh intermediateResult = new();

            // Calculate mesh bounding box for coord space transform from model to octree grid
            float minBounds = float.MaxValue;
            float maxBounds = float.MinValue;
            foreach (var vert in objResult.Vertices)
            {
                maxBounds = Math.Max(maxBounds, Math.Max(vert.X, Math.Max(vert.Y, vert.Z)));
                minBounds = Math.Min(minBounds, Math.Min(vert.X, Math.Min(vert.Y, vert.Z)));
            }

            if (minBounds > maxBounds)
                throw new FormatException("Unable to calculate bounding limits of mesh");
            Print($"Bounds: {minBounds} - {maxBounds}");
            // Calculate voxels from triangles
            foreach (var triangle in objResult.Groups[0].Faces)
            {
                for (int i = 0; i < 3; i++)
                {
                    // Get vertex pos in grid space
                    FaceVertex vert = triangle[i];
                    Vertex vertPosModel = objResult.Vertices[vert.VertexIndex - 1];
                    Vec3UInt vertPosGrid = ModelToGridSpace(vertPosModel, maxBounds - minBounds, resolution);
                    Print($"Vertex to voxel coords: ({vertPosGrid.X}, {vertPosGrid.Y}, {vertPosGrid.Z})");

                    // Add vertex as voxel TODO change to intersec octree with triangle

                    if (!intermediateResult.voxelCandidates.ContainsKey(vertPosGrid))
                        intermediateResult.voxelCandidates.Add(vertPosGrid, []);
                    intermediateResult.voxelCandidates[vertPosGrid].Add(new Voxel()
                    {
                        color = Color.White
                        // normal = objResult.Normals[vert.NormalIndex]
                    });
                }
            }

            return intermediateResult.GetResult(resolution, maxBounds - minBounds);
        }

        private static Vec3UInt ModelToGridSpace(Vertex vert, float transformFactor, int octreeDepth)
        {
            return new Vec3UInt(
                (uint)Math.Floor((vert.X / transformFactor) * octreeDepth),
                (uint)Math.Floor((vert.Y / transformFactor) * octreeDepth),
                (uint)Math.Floor((vert.Z / transformFactor) * octreeDepth)
                );
        }

    }
}