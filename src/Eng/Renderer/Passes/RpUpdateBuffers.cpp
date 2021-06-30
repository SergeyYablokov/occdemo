#include "RpUpdateBuffers.h"

#include "../Renderer_Structs.h"

void RpUpdateBuffers::Setup(RpBuilder &builder, const DrawList &list, const ViewState *view_state, int orphan_index,
                            void **fences, const char skin_transforms_buf[], const char shape_keys_buf[],
                            const char instances_buf[], const char cells_buf[], const char lights_buf[],
                            const char decals_buf[], const char items_buf[], const char shared_data_buf[]) {
    assert(list.instances.count < REN_MAX_INSTANCES_TOTAL);
    assert(list.skin_transforms.count < REN_MAX_SKIN_XFORMS_TOTAL);
    assert(list.skin_regions.count < REN_MAX_SKIN_REGIONS_TOTAL);
    assert(list.skin_vertices_count < REN_MAX_SKIN_VERTICES_TOTAL);
    assert(list.light_sources.count < REN_MAX_LIGHTS_TOTAL);
    assert(list.decals.count < REN_MAX_DECALS_TOTAL);
    assert(list.probes.count < REN_MAX_PROBES_TOTAL);
    assert(list.ellipsoids.count < REN_MAX_ELLIPSES_TOTAL);
    assert(list.items.count < REN_MAX_ITEMS_TOTAL);

    orphan_index_ = orphan_index;

    fences_ = fences;
    skin_transforms_ = list.skin_transforms;
    shape_keys_ = list.shape_keys_data;
    instances_ = list.instances;
    cells_ = list.cells;
    light_sources_ = list.light_sources;
    decals_ = list.decals;
    items_ = list.items;
    shadow_regions_ = list.shadow_regions;
    probes_ = list.probes;
    ellipsoids_ = list.ellipsoids;
    render_flags_ = list.render_flags;

    env_ = &list.env;

    draw_cam_ = &list.draw_cam;
    view_state_ = view_state;

    char name_buf[32];

    { // create skin transforms buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = SkinTransformsBufChunkSize;
        skin_transforms_buf_ = builder.WriteBuffer(skin_transforms_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", skin_transforms_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        skin_transforms_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create shape keys buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ShapeKeysBufChunkSize;
        shape_keys_buf_ = builder.WriteBuffer(shape_keys_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", shape_keys_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        shape_keys_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create instances buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = InstanceDataBufChunkSize;
        instances_buf_ = builder.WriteBuffer(instances_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", instances_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        instances_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create cells buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = CellsBufChunkSize;
        cells_buf_ = builder.WriteBuffer(cells_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", cells_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        cells_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create lights buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = LightsBufChunkSize;
        lights_buf_ = builder.WriteBuffer(lights_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", lights_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        lights_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create decals buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = DecalsBufChunkSize;
        decals_buf_ = builder.WriteBuffer(decals_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", decals_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        decals_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create items buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Texture;
        desc.size = ItemsBufChunkSize;
        items_buf_ = builder.WriteBuffer(items_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", items_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        items_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
    { // create uniform buffer
        RpBufDesc desc;
        desc.type = Ren::eBufType::Uniform;
        desc.size = SharedDataBlockSize;
        shared_data_buf_ = builder.WriteBuffer(shared_data_buf, desc, *this);

        sprintf(name_buf, "%s (Stage)", shared_data_buf);
        desc.type = Ren::eBufType::Stage;
        desc.size *= FrameSyncWindow;
        shared_data_stage_buf_ = builder.WriteBuffer(name_buf, desc, *this);
    }
}
