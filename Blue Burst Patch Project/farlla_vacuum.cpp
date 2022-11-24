#ifdef PATCH_FARLLA_VACUUM

#include <cmath>
#include <windows.h>

#include "common.h"
#include "enemy.h"
#include "entitylist.h"
#include "entity.h"
#include "keyboard.h"
#include "map.h"
#include "object_extension.h"
#include "object_wrapper.h"
#include "object.h"

#include "newgfx/model.h"

#include "farlla_vacuum.h"

class Ene : public ObjectWrapper
{
public:
    Ene(void* obj) : ObjectWrapper(obj) {}

    DECLARE_WRAPPED_MEMBER(0x1c, Entity::EntityIndex, entityIndex);
    DECLARE_WRAPPED_MEMBER(0x2e, int16_t, mapSection);
    DECLARE_WRAPPED_MEMBER(0x30, Entity::EntityFlag, entityFlags);
    DECLARE_WRAPPED_MEMBER(0x38, Vec3<float>, position);

    DECLARE_WRAPPED_MEMBER(0x44, Vec3f, xyz5);
    DECLARE_WRAPPED_MEMBER(0x50, Vec3f, xyz1);
    DECLARE_WRAPPED_MEMBER(0x298, Vec3f, xyz4);
    DECLARE_WRAPPED_MEMBER(0x2a4, Vec3f, xyz3);
    DECLARE_WRAPPED_MEMBER(0x300, Vec3f, xyz6);
    DECLARE_WRAPPED_MEMBER(0x30c, Vec3f, xyz7);

    DECLARE_WRAPPED_METHOD(23, void, GetHit, void* attacker, float damageMultiplier);
};

class Vacuum : public BaseObject
{
private:
    static Model* model;
    Vec3f position;
    int counter;

public:
    Vacuum(const Vec3f& position) :
        BaseObject(),
        position(position),
        counter(0)
    {
        BaseGameObjectConstructor(this, *Enemy::rootEnemyObject);

        vtable = new BaseObject::Vtable();

        OVERRIDE_METHOD(Vacuum, Destruct);
        OVERRIDE_METHOD(Vacuum, Update);
        OVERRIDE_METHOD(Vacuum, Render);
        OVERRIDE_METHOD(Vacuum, RenderShadow);

        this->position.y += 5.0;
    }

    ~Vacuum()
    {
        delete vtable;
    }

    void Destruct(BOOL freeMemory)
    {
        BaseGameObjectDestructorNoDealloc(this);

        Vacuum::~Vacuum();

        if (freeMemory)
        {
            MainArenaDealloc(this);
        }
    }

    void Update()
    {
        double speed = 10.0;

        if (++counter > 300)
        {
            objectFlags = ObjectFlag(objectFlags | ObjectFlag::AwaitingDestruction);
        }
        else
        {
            for (auto ptr : EntityList::Enemies())
            {
                auto enemy = Ene(ptr);

                if (enemy.entityFlags() & 0x4040000 || enemy.entityFlags() & 0x800) continue;

                auto centerDist = sqrt(DistanceSquaredXZ(position, enemy.position()));

                if (centerDist > 100.0) continue;

                auto dx = position.x - enemy.position().x;
                auto dz = position.z - enemy.position().z;
                auto angle = atan2(dz, dx);

                double ds, dc;
                sincos(angle, &ds, &dc);
                ds *= speed;
                dc *= speed;

                // Don't move too much when near center
                auto movex = abs(dc) < abs(dx) ? dc : dx;
                auto movez = abs(ds) < abs(dz) ? ds : dz;

                enemy.position().x += movex;
                enemy.position().z += movez;

                enemy.xyz1().x += movex;
                enemy.xyz1().z += movez;

                enemy.xyz3().x += movex;
                enemy.xyz3().z += movez;

                enemy.xyz4().x += movex;
                enemy.xyz4().z += movez;

                enemy.xyz5().x += movex;
                enemy.xyz5().z += movez;

                enemy.xyz6().x += movex;
                enemy.xyz6().z += movez;

                enemy.xyz7().x += movex;
                enemy.xyz7().z += movez;

                reinterpret_cast<void (__fastcall *)(void*)>(0x007b6880)(ptr);
            }
        }
    }

    void Render()
    {
        Transform::PushTransformStackCopy();
        Transform::TranslateTransformStackHead(&position);
        reinterpret_cast<void (__fastcall *)(void*, int x, int y, int z)>(0x0082df5c)(nullptr, 16384, counter, 0);

        Vacuum::model->UseTransparentShading();
        Vacuum::model->Draw();

        Transform::PopTransformStack();
    }

    void RenderShadow()
    {

    }

    static Vacuum* __cdecl Create()
    {
        Vec3f position;
        auto player = Entity::BaseEntityWrapper(*EntityList::Players().begin());
        position.set(player.position());

        void* buf = MainArenaAlloc(sizeof(Vacuum));
        return new (buf) Vacuum(position);
    }

    static void __cdecl LoadAssets()
    {
        Vacuum::model = new Model("vacuum.fbx");
    }

    static void __cdecl UnloadAssets()
    {
        delete Vacuum::model;
        Vacuum::model = nullptr;
    }
};

Model* Vacuum::model = nullptr;

void __thiscall PBDestructorWrapper(void* self, BOOL doFree)
{
    // Call original
    reinterpret_cast<decltype(PBDestructorWrapper)*>(0x0067dd74)(self, doFree);

    Vacuum::Create();
}

void ApplyFarllaVacuumPatch()
{
    // Load assets in forest
    auto& forest1InitList = Map::GetMapInitList(Map::MapType::Forest1);
    forest1InitList.AddFunctionPair(InitList::FunctionPair(Vacuum::LoadAssets, Vacuum::UnloadAssets));

    // Patch PB destructor in vtable
    *reinterpret_cast<decltype(PBDestructorWrapper)**>(0x00b38328) = PBDestructorWrapper;

    Keyboard::onKeyDown({Keyboard::Keycode::Ctrl, Keyboard::Keycode::P}, [](){
        auto player = *EntityList::Players().begin();
        auto pb = (float*)((int) player + 0x520);
        auto full = (BOOL*)((int) player + 0x524);
        *pb = 100.0;
        *full = 2;
    });
}

#endif // PATCH_FARLLA_VACUUM
