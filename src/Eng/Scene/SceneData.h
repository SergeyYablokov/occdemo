#pragma once

#include <cstdint>

#include <atomic>

#include <Ren/HashMap32.h>
#include <Ren/Mesh.h>
#include <Ren/MMat.h>
#include <Ren/Storage.h>
#include <Ren/TextureAtlas.h>

#include "ProbeStorage.h"

#include "Comp/AnimState.h"
#include "Comp/Decal.h"
#include "Comp/Drawable.h"
#include "Comp/Lightmap.h"
#include "Comp/LightProbe.h"
#include "Comp/LightSource.h"
#include "Comp/Occluder.h"
#include "Comp/Physics.h"
#include "Comp/SoundSource.h"
#include "Comp/Transform.h"
#include "Comp/VegState.h"

enum eObjectComp : uint32_t {
    CompTransform   = 0,
    CompDrawable    = 1,
    CompOccluder    = 2,
    CompLightmap    = 3,
    CompLightSource = 4,
    CompDecal       = 5,
    CompProbe       = 6,
    CompAnimState   = 7,
    CompVegState    = 8,
    CompSoundSource = 9,
    CompPhysics     = 10,
};

enum eObjectCompBit : uint32_t {
    CompTransformBit    = (1u << CompTransform),
    CompDrawableBit     = (1u << CompDrawable),
    CompOccluderBit     = (1u << CompOccluder),
    CompLightmapBit     = (1u << CompLightmap),
    CompLightSourceBit  = (1u << CompLightSource),
    CompDecalBit        = (1u << CompDecal),
    CompProbeBit        = (1u << CompProbe),
    CompAnimStateBit    = (1u << CompAnimState),
    CompVegStateBit     = (1u << CompVegState),
    CompSoundSourceBit  = (1u << CompSoundSource),
    CompPhysicsBit      = (1u << CompPhysics),
};

const int MAX_COMPONENT_TYPES = 32;

const float LIGHT_ATTEN_CUTOFF = 0.004f;

struct SceneObject {
    uint32_t    comp_mask, change_mask, last_change_mask;
    uint32_t    components[MAX_COMPONENT_TYPES];
    Ren::String name;

    SceneObject() : comp_mask(0), change_mask(0), last_change_mask(0) {}    // NOLINT
    SceneObject(const SceneObject &rhs) = delete;
    SceneObject(SceneObject &&rhs) noexcept = default;

    SceneObject &operator=(const SceneObject &rhs) = delete;
    SceneObject &operator=(SceneObject &&rhs) = default;
};
//static_assert(sizeof(SceneObject) == 156 + 4, "!");

struct bvh_node_t { // NOLINT
    uint32_t prim_index, prim_count,
             left_child, right_child;
    Ren::Vec3f bbox_min;
    uint32_t parent;
    Ren::Vec3f bbox_max;
    uint32_t space_axis; // axis with maximal child's centroids distance
};
static_assert(sizeof(bvh_node_t) == 48, "!");

const int MAX_STACK_SIZE = 64;

struct Environment {
    Ren::Vec3f          sun_dir, sun_col;
    float               sun_softness = 0.0f;
    Ren::Vec3f          wind_vec;
    float               wind_turbulence = 0.0f;
    Ren::Vec2f          prev_wind_scroll_lf, prev_wind_scroll_hf;
    Ren::Vec2f          curr_wind_scroll_lf, curr_wind_scroll_hf;
    Ren::Tex2DRef       env_map;
    Ren::Tex2DRef       lm_direct, lm_indir,
                        lm_indir_sh[4];
    float               sun_shadow_bias[2] = { 4.0f, 8.0f };

    Ren::String         env_map_name, env_map_name_pt;
};

struct BBox {
    Ren::Vec3f bmin, bmax;
};

struct TexEntry {
    uint32_t index;
    union {
        struct {
            uint32_t cam_dist : 16;
            uint32_t prio : 4;
            uint32_t _unused : 12;
        };
        uint32_t sort_key = 0;
    };
};
static_assert(sizeof(TexEntry) == 8, "!");

class CompStorage {
public:
    virtual ~CompStorage() = default;
    virtual const char *name() const = 0;

    virtual uint32_t Create() = 0;
    virtual void Delete(uint32_t i) = 0;
    virtual void *Get(uint32_t i) = 0;
    virtual const void *Get(uint32_t i) const = 0;

    virtual uint32_t First() const = 0;
    virtual uint32_t Next(uint32_t i) const = 0;

    virtual int Count() const = 0;

    virtual void ReadFromJs(const JsObjectP &js_obj, void *comp) = 0;
    virtual void WriteToJs(const void *comp, JsObjectP &js_obj) const = 0;

    // returns contiguous array of data or null if storage does not support it
    virtual const void *SequentialData() const { return nullptr; }
    virtual void *SequentialData() { return nullptr; }
};

struct PersistentBuffers {
    Ren::BufHandle                  materials_buf;
    std::pair<uint32_t, uint32_t>   materials_buf_range;
    Ren::BufHandle                  textures_buf;
    std::pair<uint32_t, uint32_t>   textures_buf_range;
};

struct SceneData {
    Ren::String                             name;
    
    Ren::Texture2DStorage                   textures;
    Ren::MaterialStorage                    materials;
    std::vector<uint32_t>                   material_changes;
    Ren::BufferRef                          materials_buf, textures_buf;
    Ren::SyncFence                          mat_buf_fences[4];
    std::pair<uint32_t, uint32_t>           mat_update_ranges[4];
    uint32_t                                mat_buf_index = 0;
    Ren::MeshStorage                        meshes;

    std::vector<uint32_t>                   texture_mem_buckets;
    uint32_t                                tex_mem_bucket_index = 0;
    std::atomic<uint64_t>                   estimated_texture_mem = {};

    Environment                             env;

    Ren::HashMap32<Ren::String, Ren::Vec4f> decals_textures;
    Ren::TextureAtlas                       decals_atlas;
    Ren::TextureSplitter                    lm_splitter;
    ProbeStorage                            probe_storage;

    CompStorage                             *comp_store[MAX_COMPONENT_TYPES] = {};

    std::vector<SceneObject>                objects;
    Ren::HashMap32<Ren::String, uint32_t>   name_to_object;

    std::vector<bvh_node_t>                 nodes;
    std::vector<uint32_t>                   free_nodes;
    uint32_t                                root_node = 0xffffffff;

    uint32_t                                update_counter = 0;
};
