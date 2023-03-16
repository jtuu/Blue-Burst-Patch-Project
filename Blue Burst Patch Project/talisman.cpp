#ifdef PATCH_TALISMAN

#include <cstdint>

#include <windows.h>

#include "common.h"
#include "entity.h"
#include "helpers.h"
#include "object_extension.h"
#include "object.h"

#include "talisman.h"

struct Projectile
{
    struct Vtable
    {
        void (__thiscall *Destruct)(void*, BOOL);
        void (__thiscall *Update)(void*);
        void (__thiscall *Render)(void*);
        void (__thiscall *RenderShadow)(void*);
        void (__thiscall *HitEntity)(void*);
        void (__thiscall *UpdatePosition)(void*);
    };

    enum BehaviorFlag : uint32_t
    {
        HasPrecomputedVelocityVector = 0x1,
        BulletTrail = 0x2,
        FastSpinningProjectile = 0x4,
        CreateUnknownParticleEffect = 0x10,
        SlowSpinningProjectile = 0x20,
        Explosive = 0x80,
        ReducedDamagePerPierce = 0x100,
        DirectHitDmg = 0x200,
        NoPointBlankPierce = 0x800
    };

    struct Settings
    {
        Vec3f start_pos;
        float field1_0xc;
        int32_t max_pierce_count;
        Entity::EntityIndex player_idx;
        Entity::EntityIndex room_id;
        Entity::EntityIndex weapon_instance_id;
        float accuracy_coeff;
        float damage_coeff;
        uint32_t attack_type;
        int direction;
        Vec3f velocity;
        float speed;
        float max_distance;
        enum BehaviorFlag behavior_flags;
        uint32_t unknown_texture1;
        uint32_t unknown_texture2;
        float field16_0x4c;
        uint32_t field17_0x50;
        uint32_t field18_0x54;
        uint32_t field19_0x58;
        float field20_0x5c;
        float field21_0x60;
        float field22_0x64;
        float field23_0x68;
        float field24_0x6c;
        void* unk_func1;
        void* unk_func2;
        int particle_id1;
        int particle_id2;
    };

    union
    {
        Vtable* vtable;

        union {
            DEFINE_FIELD(0x8, ObjectFlag objectFlags);
            DEFINE_FIELD(0x24, Vec3f currentPos);
            DEFINE_FIELD(0xa4, uint32_t remainingPierceCount);
            DEFINE_FIELD(0xa8, Entity::EntityIndex playerIdx);
            DEFINE_FIELD(0xbc, bool shouldCollideWithPlayers);
            DEFINE_FIELD(0xc0, bool shouldCollideWithObjects);
            DEFINE_FIELD(0xc4, Vec3f originPos);
            DEFINE_FIELD(0xd0, Vec3f velocity);
            DEFINE_FIELD(0xdc, float maxDistanceSq);
            DEFINE_FIELD(0xe0, uint32_t collisionFlags);
            DEFINE_FIELD(0xe4, BehaviorFlag behaviorFlags);
            DEFINE_FIELD(0xf0, Vec3<uint32_t> rot1);
            DEFINE_FIELD(0xfc, Vec3<uint32_t> rot2);
        };

        // Ensure object's size is large enough to account for members not defined above
        uint8_t _padding[0x144];
    };

    const Vtable* origVtable;

    Projectile(void* parent, Settings* settings)
    {
        auto ctor = reinterpret_cast<Projectile* (__thiscall *)(void* buf, void* parent, Settings* settings)>(0x005c6164);
        ctor(this, parent, settings);

        origVtable = vtable;
        vtable = InheritVtable(vtable);
    }
};

class TalismanProjectile;

TalismanProjectile* currentTalisman;

class TalismanProjectile : public Projectile
{
private:
    float suspensionDistanceSq;
    bool isSuspended;

public:
    TalismanProjectile(void* parent, Settings* settings) :
        Projectile(parent, settings),
        isSuspended(false),
        suspensionDistanceSq(maxDistanceSq / 3.0)
    {
        // Override some methods
        OVERRIDE_METHOD(TalismanProjectile, Destruct);
        OVERRIDE_METHOD(TalismanProjectile, Update);
        OVERRIDE_METHOD(TalismanProjectile, UpdatePosition);

        remainingPierceCount = 0;

        currentTalisman = this;

        rot1.x = 0;
        rot1.y = 0;
        rot1.z = 0;
    }

    bool IsReadyForCasting() const
    {
        return isSuspended;
    }

    static TalismanProjectile* __cdecl Create(void* parent, Settings* settings)
    {
        void* buf = MainArenaAlloc(sizeof(TalismanProjectile));
        return new (buf) TalismanProjectile(parent, settings);
    }

private:
    ~TalismanProjectile()
    {
        if (currentTalisman == this) currentTalisman = nullptr;
        delete vtable;
    }

    void Destruct(BOOL doFree)
    {
        origVtable->Destruct(this, false);
        TalismanProjectile::~TalismanProjectile();
        if (doFree) MainArenaDealloc(this);
    }

    void Update()
    {
        origVtable->Update(this);

        auto playerPtr = GetPlayer(playerIdx);
        if (playerPtr != nullptr)
        {
            // Set projectile rotation to same as player heading
            auto player = Entity::BaseEntityWrapper(playerPtr);
            rot2.y = player.rotation().y;
        }
    }

    void TryApplyNewPosition(Vec3f& pos)
    {
        // Does a terrain collision check I think
        reinterpret_cast<void (__thiscall *)(void*, Vec3f*)>(0x005c5f9c)(this, &pos);
    }

    bool HasCollidedWithTerrain() const
    {
        return collisionFlags & 1;
    }

    void UpdatePosition()
    {
        bool cantMove = DistanceSquaredXZ(originPos, currentPos) >= suspensionDistanceSq || HasCollidedWithTerrain();
        isSuspended = currentTalisman == this && cantMove;

        if (!isSuspended) {
            Vec3f newPos;
            newPos.x = currentPos.x + velocity.x;
            newPos.y = currentPos.y + velocity.y;
            newPos.z = currentPos.z + velocity.z;
            TryApplyNewPosition(newPos);
        }
    }
};

void __thiscall GetTechCastLocation(void* caster, Vec3f* loc)
{
    if (currentTalisman != nullptr && currentTalisman->IsReadyForCasting())
    {
        loc->x = currentTalisman->currentPos.x;
        loc->y = currentTalisman->currentPos.y;
        loc->z = currentTalisman->currentPos.z;
    }
    else
    {
        // Original function
        reinterpret_cast<decltype(GetTechCastLocation)*>(0x0068c358)(caster, loc);
    }
}

void ApplyTalismanPatch()
{
    // Replace call to function that returns tech cast location
    PatchCALL(0x006e087f, 0x006e087f + 5, (int) GetTechCastLocation);

    PatchCALL(0x005e6f01, 0x005e6f01 + 5, (int) TalismanProjectile::Create);
}

#endif // PATCH_TALISMAN
