#include <cstddef>
#ifdef PATCH_NEWMAP

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <stdexcept>

#include "newmap.h"

#include "helpers.h"
#include "mathutil.h"

#pragma pack(push, 1)
struct Minimap {
    struct Room {
        uint16_t room_index;
        uint16_t is_undiscovered;
        float x;
        float y;
        float z;
        int32_t rot_x;
        int32_t rot_y;
        int32_t rot_z;
        float color_alpha;
        float discovery_radius;
        struct VertexData1 {
            uint32_t unk1;
            struct VertexData2 {
                // Flex arrays are not in C++ standard but are supported by GCC and Clang
                struct VertexList {
                    uint16_t flags;
                    uint16_t vertices_size;
                    uint32_t vertex_count_shl16;
                    struct Vertex {
                        float x;
                        float y;
                        float z;
                        float nx;
                        float ny;
                        float nz;
                    } vertices[];
                }* vertex_list;
                struct DrawInfo {
                    uint16_t flags;
                    uint16_t indices_size;
                    uint16_t tri_strip_count;
                    uint16_t indices[];
                }* draw_info;

                VertexData2() : vertex_list(nullptr), draw_info(nullptr) {}
            }* vertex_data2;

            VertexData1() : unk1(0), vertex_data2(nullptr) {}
        }* vertex_data1;

        Room(uint16_t index, const Vec3f&& position, float discovery_radius)
            : room_index(index), is_undiscovered(1),
              x(position.x), y(position.y), z(position.z),
              rot_x(0), rot_y(0), rot_z(0),
              color_alpha(0.0), discovery_radius(discovery_radius),
              vertex_data1(nullptr)
        {}

        Room() : Room(0, Vec3f{0.0, 0.0, 0.0}, 0.0) {}
    }* rooms;
    uint32_t unk1;
    uint32_t room_count;
    uint32_t unk2;

    Minimap(uint32_t room_count) : room_count(room_count), unk1(0), unk2(0), rooms(nullptr) {}
};
#pragma pack(pop)

float triangles[] = {
    // first triangle
     50.0, 0.0,  50.0,  // top right
     50.0, 0.0, -50.0,  // bottom right
    -50.0, 0.0,  50.0,  // top left 
    // second triangle
     50.0, 0.0, -50.0,  // bottom right
    -50.0, 0.0, -50.0,  // bottom left
    -50.0, 0.0,  50.0,   // top left
};

Minimap* __cdecl GetTestMinimap(const char* filename)
{
    if (std::string(filename).find("forest") == std::string::npos) return nullptr;

    using VertexData = Minimap::Room::VertexData1::VertexData2;
    using Vertex = VertexData::VertexList::Vertex;

    auto minimap = new Minimap(1);

    minimap->rooms = new Minimap::Room(0, Vec3f{0.0, 0.0, 0.0}, 10000.0);
    minimap->rooms->vertex_data1 = new Minimap::Room::VertexData1();

    auto the = new Minimap::Room::VertexData1::VertexData2();
    minimap->rooms->vertex_data1->vertex_data2 = the;

    auto strip_count = 2;
    auto vertex_count = 6;
    auto vertices_size = sizeof(Vertex) * vertex_count;
    auto indices_size = (vertex_count + strip_count) * sizeof(uint16_t);

    the->vertex_list = (VertexData::VertexList*) calloc(1, sizeof(VertexData::VertexList) * 2 + vertices_size);
    the->draw_info = (VertexData::DrawInfo*) calloc(1, sizeof(VertexData::DrawInfo) * (strip_count + 1) + indices_size);

    the->vertex_list[0].flags = 0x29;
    the->vertex_list[0].vertex_count_shl16 = vertex_count << 16;
    the->vertex_list[0].vertices_size = vertices_size / 4 + 4;

    for (int i = 0; i < vertex_count; i++) {
        auto vertex = &the->vertex_list[0].vertices[i];
        vertex->x = triangles[i * 3 + 0];
        vertex->y = triangles[i * 3 + 1];
        vertex->z = triangles[i * 3 + 2];
        vertex->nx = 0.0;
        vertex->ny = 1.0;
        vertex->nz = 0.0;
    }

    ((VertexData::VertexList*)(&the->vertex_list[0].vertex_count_shl16 + vertices_size / 4 + 4))->flags = 0xff;

    the->draw_info[0].tri_strip_count = strip_count;
    the->draw_info[0].flags = 0x0340;
    the->draw_info[0].indices_size = indices_size / 2 + 2;

    for (int i = 0; i < strip_count; i++) {
        the->draw_info[i].indices[0] = 3; // Index count
        the->draw_info[i].indices[1] = i + 0;
        the->draw_info[i].indices[2] = i + 1;
        the->draw_info[i].indices[3] = i + 2;
    }

    ((VertexData::DrawInfo*)(&the->draw_info[strip_count - 1].tri_strip_count + indices_size / strip_count / 2 + 2))->flags = 0xff;

    return minimap;
}

Minimap* LoadMiniMap(char* filename)
{
    if (std::string(filename).find("forest") == std::string::npos) return nullptr;

    Assimp::Importer importer;
    // Read file
    auto scene = importer.ReadFile("pso_minimap.obj", aiProcessPreset_TargetRealtime_Fast | aiProcess_TransformUVCoords | aiProcess_FlipWindingOrder);

    if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr)
        throw std::runtime_error(importer.GetErrorString());
    
    using VertexData = Minimap::Room::VertexData1::VertexData2;
    using Vertex = VertexData::VertexList::Vertex;

    auto room_count = scene->mNumMeshes;
    auto minimap = new Minimap(room_count);
    minimap->room_count = room_count;
    minimap->rooms = new Minimap::Room[room_count];

    for (int i = 0; i < room_count; i++) {
        auto mesh = scene->mMeshes[i];
        auto strip_count = mesh->mNumFaces; // Draw each triangle as one strip
        auto vertex_count = strip_count * 3;
        auto vertices_size = sizeof(Vertex) * vertex_count;
        auto indices_size = (vertex_count + strip_count) * sizeof(uint16_t);

        minimap->rooms[i].room_index = i;
        minimap->rooms[i].is_undiscovered = 1;
        minimap->rooms[i].discovery_radius = 1000.0;
        minimap->rooms[i].vertex_data1 = new Minimap::Room::VertexData1();

        auto the = minimap->rooms[i].vertex_data1->vertex_data2 = new VertexData();
        the->vertex_list = (VertexData::VertexList*) malloc(sizeof(VertexData::VertexList) * 2 + vertices_size + 16);

        the->vertex_list[0].flags = 0x29;
        the->vertex_list[0].vertex_count_shl16 = vertex_count << 16;
        the->vertex_list[0].vertices_size = vertices_size / 4 + 4;

        for (int i = 0; i < vertex_count; i++) {
            auto vertex = &the->vertex_list[0].vertices[i];
            auto& pos = mesh->mVertices[i];
            vertex->x = pos.x;
            vertex->y = 0.0;
            vertex->z = pos.z;
            vertex->nx = 0.0;
            vertex->ny = 1.0;
            vertex->nz = 0.0;
        }

        ((VertexData::VertexList*)(&the->vertex_list->vertex_count_shl16 + the->vertex_list->vertices_size))->flags = 0xff;

        the->draw_info = (VertexData::DrawInfo*) malloc(sizeof(VertexData::DrawInfo) * 2 + indices_size + 16);
        the->draw_info[0].tri_strip_count = strip_count;
        the->draw_info[0].flags = 0x0340;
        the->draw_info[0].indices_size = indices_size / 2 + 2;

        for (int i = 0; i < strip_count; i++) {
            auto& face = mesh->mFaces[i];
            the->draw_info[0].indices[i * 4 + 0] = 3;
            the->draw_info[0].indices[i * 4 + 1] = face.mIndices[0];
            the->draw_info[0].indices[i * 4 + 2] = face.mIndices[1];
            the->draw_info[0].indices[i * 4 + 3] = face.mIndices[2];
        }

        ((VertexData::DrawInfo*)(&the->draw_info->tri_strip_count + the->draw_info->indices_size))->flags = 0xff;
    }

    return minimap;
}

#pragma pack(push, 1)
struct Nrel {
    struct Color {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    struct Vertex4 {
        float x;
        float y;
        float z;
        Color diffuse;
    };

    char magic[4];
    uint32_t unk1;
    uint16_t chunk_count;
    uint16_t unk2;
    uint32_t unk3;
    struct Chunk {
        uint32_t id;
        float x;
        float y;
        float z;
        int32_t rot_x;
        int32_t rot_y;
        int32_t rot_z;
        uint32_t unk1;
        struct MeshTableEntry {
            struct MeshList {
                uint32_t flags;
                struct Mesh {
                    uint32_t flags;
                    struct VertexData {
                        uint32_t vertex_format;
                        void* vertex_buffer;
                        uint32_t vertex_size;
                        uint32_t vertex_count;
                    }* vertex_data;
                    uint32_t vertex_buffer_count;
                    struct IndexData {
                        void* renderstate;
                        uint32_t renderstate_count;
                        uint16_t* index_buffer;
                        uint32_t index_count;
                        uint32_t unk1;
                    }* index_data;
                    uint32_t index_buffer_count;
                    uint32_t unk1;
                    uint32_t unk2;

                    Mesh() :
                        flags(0), vertex_data(nullptr), vertex_buffer_count(0), index_data(nullptr), index_buffer_count(0),
                        unk1(0), unk2(0) {}
                }* mesh;
                float x;
                float y;
                float z;
                int32_t rot_x;
                int32_t rot_y;
                int32_t rot_z;
                float unk1;
                float unk2;
                float unk3;
                MeshList* child;
                MeshList* next;

                MeshList() :
                    flags(0), mesh(nullptr), x(0.0), y(0.0), z(0.0), rot_x(0), rot_y(0), rot_z(),
                    unk1(1.0), unk2(1.0), unk3(1.0), child(nullptr), next(nullptr) {}
            }* mesh_list;
            void* unk1;
            uint32_t unk2;
            uint32_t flags;

            MeshTableEntry() : mesh_list(nullptr), unk1(nullptr), unk2(0), flags(0) {}
        }* static_meshlist_table;
        void* animated_meshlist_table;
        uint32_t static_meshlist_count;
        uint32_t animated_meshlist_count;
        uint32_t flags;

        Chunk() :
            id(0), x(0.0), y(0.0), z(0.0), rot_x(0), rot_y(0), rot_z(0), unk1(0),
            static_meshlist_table(nullptr), animated_meshlist_table(nullptr),
            static_meshlist_count(0), animated_meshlist_count(0), flags(0) {}

    }* chunks;
    void* texture;

    Nrel() :
        magic{'f', 'm', 't', '2'}, unk1(0), chunk_count(0), unk2(0), unk3(0),
        chunks(nullptr), texture(nullptr) {}
};
#pragma pack(pop)

Nrel* __cdecl GetTestNrel(const char* filename, void* dst)
{
    if (std::string(filename).find("forest") == std::string::npos) return reinterpret_cast<decltype(GetTestNrel)*>(0x005b9bf4)(filename, dst);

    auto nrel = new Nrel();

    nrel->chunk_count = 1;
    auto chunk = nrel->chunks = new Nrel::Chunk();
    chunk->static_meshlist_count = 1;
    chunk->flags = 0x00010000;
    chunk->x = 25.0;

    auto entry = chunk->static_meshlist_table = new Nrel::Chunk::MeshTableEntry();
    entry->flags = 0x00220000;

    auto mesh_list = entry->mesh_list = new Nrel::Chunk::MeshTableEntry::MeshList();
    mesh_list->flags = 0x17;

    auto mesh = mesh_list->mesh = new Nrel::Chunk::MeshTableEntry::MeshList::Mesh();
    mesh->vertex_buffer_count = 1;
    mesh->index_buffer_count = 1;
    auto vertex_data = mesh->vertex_data = new Nrel::Chunk::MeshTableEntry::MeshList::Mesh::VertexData();
    auto index_data = mesh->index_data = new Nrel::Chunk::MeshTableEntry::MeshList::Mesh::IndexData();

    vertex_data->vertex_format = 4;
    vertex_data->vertex_size = sizeof(Nrel::Vertex4);
    vertex_data->vertex_count = 3;
    vertex_data->vertex_buffer = (void*) new Nrel::Vertex4[] {
        {
            -100.0, 0.0, -100.0,
            {0xff, 0, 0, 0xff}
        },
        {
            -100.0, 0.0, 100.0,
            {0, 0xff, 0, 0xff}
        },
        {
            100.0, 0.0, 100.0,
            {0, 0, 0xff, 0xff}
        }
    };

    index_data->renderstate = nullptr;
    index_data->renderstate_count = 0;
    index_data->index_count = 3;
    index_data->unk1 = 0;
    index_data->index_buffer = new uint16_t[]{0, 1, 2};

    return nrel;
}

#pragma pack(push, 1)
struct CollisionMeshContainer {
    struct Mesh {
        uint32_t unk1;
        Vec3f* vertices;
        uint32_t face_count;
        struct Face {
            uint16_t indices[3];
            uint16_t flags;
            float rot_x;
            float rot_y;
            float rot_z;
            float x;
            float y;
            float z;
            float radius;
        }* faces;
    }* mesh;
    float x;
    float y;
    float z;
    float radius;
    uint32_t flags;
};
#pragma pack(pop)

CollisionMeshContainer** __cdecl GetTestCrel(const char* filename, void* dst)
{
    if (std::string(filename).find("forest") == std::string::npos) return reinterpret_cast<decltype(GetTestCrel)*>(0x005b9bf4)(filename, dst);

    auto nodes = new CollisionMeshContainer*[] { new CollisionMeshContainer, new CollisionMeshContainer };
    auto the = nodes[0];
    nodes[1]->mesh = nullptr; // Terminator

    the->x = 0.0;
    the->y = 0.0;
    the->z = 0.0;
    the->radius = 300.0;
    the->flags = 0x00000921;

    auto mesh = the->mesh = new CollisionMeshContainer::Mesh;
    mesh->face_count = 1;
    mesh->vertices = new Vec3f[] {
        { -100.0, 0.0, -100.0 },
        { -100.0, 0.0, 100.0 },
        { 100.0, 0.0, 100.0 }
    };

    auto face = mesh->faces = new CollisionMeshContainer::Mesh::Face;
    face->flags = 0x0101;
    face->indices[0] = 0;
    face->indices[1] = 1;
    face->indices[2] = 2;
    face->rot_x = 0.0;
    face->rot_y = 1.0;
    face->rot_z = 0.0;
    face->x = 0.0;
    face->y = 0.0;
    face->z = 0.0;
    face->radius = 300.0;

    return nodes;
}

void PatchMapAssetGetters()
{
    PatchCALL(0x00805b30, 0x00805b35, (int) LoadMiniMap);
    PatchCALL(0x0080a239, 0x0080a23e, (int) GetTestNrel);
    PatchCALL(0x0080a46f, 0x0080a474, (int) GetTestCrel);
}

#pragma pack(push, 1)
struct MapDesignation
{
    uint32_t map_id;
    uint32_t map_variant;
    uint32_t object_set;
};

struct MapAssetPrefixes
{
    struct Prefixes {
        const char* basename;
        const char* variant_names[];
    }* prefixes;
    uint32_t variant_count;
};
#pragma pack(pop)

const size_t VANILLA_FLOOR_COUNT = 18;
const size_t VANILLA_MAP_COUNT = 47;
auto vanillaFloorMapDesignations = reinterpret_cast<MapDesignation*>(0x00aafce0);
auto vanillaMapAssetPrefixes = reinterpret_cast<MapAssetPrefixes*>(0x00a116e0);
auto vanillaUltMapAssetPrefixes = reinterpret_cast<MapAssetPrefixes*>(0x00a114e0);

const size_t CUSTOM_MAP_SET_COUNT = 1;
MapAssetPrefixes::Prefixes snowMapAssetPrefixes = {
    "map_snow01",
    "map_snow01"
};
std::array<std::array<MapAssetPrefixes, VANILLA_MAP_COUNT>, CUSTOM_MAP_SET_COUNT> customMapSets =
{
    // Map sets
    {
        // Asset prefixes of maps in map set
        MapAssetPrefixes { reinterpret_cast<MapAssetPrefixes::Prefixes*>(0x00a11d48), 1 }, // Use vanilla Pioneer 2
        MapAssetPrefixes { &snowMapAssetPrefixes, 1 }, // Use Forest 1 slot for snow map
    }
};

auto IsUltEp1 = reinterpret_cast<bool (__cdecl *)()>(0x0078b220);

/**
 * @brief The map set value is set from the custom opcode and reset from Before_InitEpisodeMaps.
 *  Vanilla maps are considered to be the 0th map set.
 *  Custom map sets will use the objects and enemies from the matching vanilla maps, but can have custom map geometry files.
 */
auto currentMapSet = 0;

const MapAssetPrefixes::Prefixes* __cdecl GetMapAssetPrefixes(uint32_t map)
{
    if (currentMapSet == 0)
    {
        // Do vanilla behavior
        if (IsUltEp1()) return vanillaUltMapAssetPrefixes[map].prefixes;
        return vanillaMapAssetPrefixes[map].prefixes;
    }

    // Get map asset prefixes from custom map set
    auto customMapSetIndex = currentMapSet - 1;
    if (customMapSets.size() <= customMapSetIndex) return nullptr;
    const auto& mapSet = customMapSets[customMapSetIndex];
    if (mapSet.size() <= map) return nullptr;
    return mapSet[map].prefixes;
}

__attribute__((regparm(1))) // Take argument in EAX
uint32_t __cdecl Before_InitEpisodeMaps(uint32_t episode)
{
    // This gets called when entering or leaving a game or the lobby and from set_episode opcode
    // (but not when going to main menu, but will get called when entering lobby again).
    // Seems like a good place to reset the map set.
    currentMapSet = 0;
    // Code we overwrote
    *reinterpret_cast<uint32_t*>(0x00aafdb8) = episode;
    // Return to original code
    return episode;
}

using OpcodeSetupFn = void (__cdecl*)(uint32_t opcode);
using OpcodeFn = void*;

#pragma pack(push, 1)
struct OpcodeHandler
{
    OpcodeSetupFn setupFn;
    OpcodeFn opcodeFn;
};
#pragma pack(pop)

auto opcodeTable = reinterpret_cast<OpcodeHandler*>(0x009ccc00);
auto SetupOpcodeOperand1 = reinterpret_cast<OpcodeSetupFn>(0x006b1040);

void SetOpcode(uint16_t opcode, OpcodeSetupFn setupFn, OpcodeFn opcodeFn)
{
    uint8_t firstByte = opcode & 0xff;
    uint8_t secondByte = opcode >> 8;
    uint32_t opcodeIndex = firstByte;

    if (secondByte == 0xf8) opcodeIndex += 0x100;
    else if (secondByte == 0xf9) opcodeIndex += 0x200;

    opcodeTable[opcodeIndex].setupFn = setupFn;
    opcodeTable[opcodeIndex].opcodeFn = opcodeFn;
}

void __cdecl NewOpcodeMapSet(uint8_t mapSet)
{
    currentMapSet = mapSet;
}

void PatchMapDesignateOpcode()
{
    PatchJMP(0x0080bee8, 0x0080bf11, (int) GetMapAssetPrefixes);
    PatchCALL(0x0080c7a0, 0x0080c7a5, (int) Before_InitEpisodeMaps);
    SetOpcode(0xf962, SetupOpcodeOperand1, (void*) NewOpcodeMapSet);
}

void ApplyNewMapPatch()
{
    PatchMapDesignateOpcode();
}

#endif // PATCH_NEWMAP
