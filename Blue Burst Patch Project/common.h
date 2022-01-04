#pragma once

#include "math.h"

enum class Episode
{
    Episode1 = 0,
    Episode2 = 1,
    Episode4 = 2,
};

enum class Difficulty
{
    Normal = 0,
    Hard = 1,
    VeryHard = 2,
    Ultimate = 3
};

/// Booleans are often 4 byte values in PSOBB
typedef uint32_t bool32;

Episode GetCurrentEpisode();
bool IsSoloMode();

// Forward declaration to prevent circular dependency
namespace Map { enum class MapType : uint32_t; }

Map::MapType GetCurrentMap();
Difficulty GetCurrentDifficulty();

/// Get the size of a regular array as a constexpr
template<class T, size_t N>
constexpr size_t arraySize(T (&)[N]) { return N; }

#define CONCAT_(a, b) a##b
/// Concatenate two macro arguments
#define CONCAT(a, b) CONCAT_(a, b)

enum class MinimapIconShape : uint16_t
{
    RectangleH = 0,
    Circle = 1,
    Arrow = 2,
    RectangleV = 3
};

enum class MinimapIconColor : uint32_t
{
    Enemy = 0xffffa24c
};

/// Must be called every frame
extern bool32 (__cdecl *AddMinimapIcon)(Vec3<float>* worldPos, uint32_t argb, MinimapIconShape shape, bool32 checkDistance);

namespace Graphics
{
    /// The texture will be used for subsequent draw operations
    extern bool32 (__thiscall *EnableTexture)(void* njtl);
    /// Render an entity
    extern void (__fastcall *Render3D)(void* entity);
};

namespace Transform
{
#pragma pack(push, 1)
    struct Matrix {
        float _11;
        float _12;
        float _13;
        float _14;
        float _21;
        float _22;
        float _23;
        float _24;
        float _31;
        float _32;
        float _33;
        float _34;
        float _41;
        float _42;
        float _43;
        float _44;
    };
#pragma pack(pop)

    extern const Matrix* identityMatrix;

    extern void (__fastcall *PushTransformStackCopy)(void);
    extern void (__fastcall *PushTransformStack)(const Matrix* matrix);
    extern void (__fastcall *PopTransformStack)(void);
    extern void (__fastcall *RotateMatrix)(Matrix* matrix, uint32_t angle);
    extern void (__fastcall *TranslateTransformStackHead)(Vec3<float>* translation);
};

extern void (__thiscall *InitEnemyCollisionBoxes)(void* self, void* collisionData, uint32_t collisionBoxCount);
/// The unknown parameter seems to commonly be set to 0
extern void (__thiscall *InitEnemyAnimations)(void* self, uint16_t unknown);
/// Moves the current animation forward by one or loops back to the start
extern void (__fastcall *AnimateEntity)(void* entity);
extern void* (__cdecl *GetPlayer)(uint16_t id);

struct Ground
{
    uint32_t type;
    Vec3<float>* position;
};

/// Raycasts down to find the map geometry at the specified position
extern Ground* (__cdecl *GetGround)(Vec3<float>* position, uint32_t unknown);
/// Computes collisions with other entities
extern void (__thiscall *CollideWithEntities)(void* entity);
/// Allows entity to be targetable by the player. Must be called every frame.
extern void (__cdecl *EnableReticle)(void* entity);
/// Free the fields initialized by InitEnemyCollisionBoxes
extern void (__thiscall *FreeCollisionBoxes)(void* entity);
/// Uses the entity's RNG state
extern float (__thiscall *GetRandomFloat)(void* entity);
/// Check if current animation has been running for at least the specified duration
extern bool32 (__thiscall *CheckAnimationDuration)(void* entity, float duration);
extern void (__cdecl *PlaySoundEffect)(uint32_t soundId, Vec3<float>* position);

typedef int16_t EntityIndex;
const EntityIndex UndefinedEntityIndex = -1;

