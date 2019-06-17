#include "Renderer.h"

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>
#include <Sys/Time_.h>

namespace RendererInternal {
bool bbox_test(const float p[3], const float bbox_min[3], const float bbox_max[3]) {
    return p[0] > bbox_min[0] && p[0] < bbox_max[0] &&
           p[1] > bbox_min[1] && p[1] < bbox_max[1] &&
           p[2] > bbox_min[2] && p[2] < bbox_max[2];
}

const uint8_t bbox_indices[] = { 0, 1, 2,    2, 1, 3,
                                 0, 4, 5,    0, 5, 1,
                                 0, 2, 4,    4, 2, 6,
                                 2, 3, 6,    6, 3, 7,
                                 3, 1, 5,    3, 5, 7,
                                 4, 6, 5,    5, 6, 7
                               };

template <typename SpanType>
void RadixSort_LSB(SpanType *begin, SpanType *end, SpanType *begin1) {
    SpanType *end1 = begin1 + (end - begin);

    for (unsigned shift = 0; shift < sizeof(SpanType::key) * 8; shift += 8) {
        size_t count[0x100] = {};
        for (SpanType *p = begin; p != end; p++) {
            count[(p->key >> shift) & 0xFF]++;
        }
        SpanType *bucket[0x100], *q = begin1;
        for (int i = 0; i < 0x100; q += count[i++]) {
            bucket[i] = q;
        }
        for (SpanType *p = begin; p != end; p++) {
            *bucket[(p->key >> shift) & 0xFF]++ = *p;
        }
        std::swap(begin, begin1);
        std::swap(end, end1);
    }
}

static const Ren::Vec4f ClipFrustumPoints[] = {
    { -1.0f, -1.0f, -1.0f, 1.0f },
    { 1.0f, -1.0f, -1.0f, 1.0f },
    { 1.0f,  1.0f, -1.0f, 1.0f },
    { -1.0f,  1.0f, -1.0f, 1.0f },

    { -1.0f, -1.0f, 1.0f, 1.0f },
    { 1.0f, -1.0f, 1.0f, 1.0f },
    { 1.0f,  1.0f, 1.0f, 1.0f },
    { -1.0f,  1.0f, 1.0f, 1.0f }
};

static const uint8_t SunShadowUpdatePattern[4] = {
    0b11111111,  // update cascade 0 every frame
    0b11111111,  // update cascade 1 every frame
    0b01010101,  // update cascade 2 once in two frames
    0b00100010   // update cascade 3 once in four frames
};
}

#define BBOX_POINTS(min, max) \
    (min)[0], (min)[1], (min)[2],     \
    (max)[0], (min)[1], (min)[2],     \
    (min)[0], (min)[1], (max)[2],     \
    (max)[0], (min)[1], (max)[2],     \
    (min)[0], (max)[1], (min)[2],     \
    (max)[0], (max)[1], (min)[2],     \
    (min)[0], (max)[1], (max)[2],     \
    (max)[0], (max)[1], (max)[2]

#define _MIN(x, y) ((x) < (y) ? (x) : (y))
#define _MAX(x, y) ((x) < (y) ? (y) : (x))

#define _CROSS(x, y) { (x)[1] * (y)[2] - (x)[2] * (y)[1],   \
                       (x)[2] * (y)[0] - (x)[0] * (y)[2],   \
                       (x)[0] * (y)[1] - (x)[1] * (y)[0] }

void Renderer::GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawList &list) {
    using namespace RendererInternal;

    uint64_t iteration_start = Sys::GetTimeUs();

    list.draw_cam = cam;
    list.env = scene.env;
    list.decals_atlas = &scene.decals_atlas;
    list.probe_storage = &scene.probe_storage;

    // mask render flags with what renderer itself is capable of
    list.render_flags &= render_flags_;

    if (list.render_flags & DebugBVH) {
        // copy nodes list for debugging
        list.temp_nodes = scene.nodes;
        list.root_index = scene.root_node;
    } else {
        // free memory
        list.temp_nodes = {};
    }

    list.light_sources.count = 0;
    list.decals.count = 0;
    list.probes.count = 0;

    list.instances.count = 0;
    list.shadow_batches.count = 0;
    list.main_batches.count = 0;

    list.shadow_lists.count = 0;
    list.shadow_regions.count = 0;

    list.skin_transforms.count = 0;
    list.skin_regions.count = 0;
    list.skin_vertices_count = 0;

    const bool culling_enabled = (list.render_flags & EnableCulling) != 0;
    const bool lighting_enabled = (list.render_flags & EnableLights) != 0;
    const bool decals_enabled = (list.render_flags & EnableDecals) != 0;
    const bool shadows_enabled = (list.render_flags & EnableShadows) != 0;

    int program_index = 0;
    if ((list.render_flags & EnableLightmap) == 0) {
        program_index = 1;
    }

    litem_to_lsource_.count = 0;
    ditem_to_decal_.count = 0;
    decals_boxes_.count = 0;

    proc_objects_.clear();
    proc_objects_.resize(scene.objects.size(), { 0xffffffff, 0xffffffff });

    Ren::Mat4f view_from_world = list.draw_cam.view_matrix(),
               clip_from_view = list.draw_cam.proj_matrix();

    swCullCtxClear(&cull_ctx_);

    Ren::Mat4f view_from_identity = view_from_world * Ren::Mat4f{ 1.0f },
               clip_from_identity = clip_from_view * view_from_identity;

    const uint32_t skip_check_bit = (1u << 31);
    const uint32_t index_bits = ~skip_check_bit;

    uint32_t stack[MAX_STACK_SIZE];
    uint32_t stack_size = 0;

    /**************************************************************************************************/
    /*                                     OCCLUDERS PROCESSING                                       */
    /**************************************************************************************************/

    uint64_t occluders_start = Sys::GetTimeUs();

    {   // Rasterize occluder meshes into a small framebuffer
        stack[stack_size++] = scene.root_node;

        while (stack_size && culling_enabled) {
            uint32_t cur = stack[--stack_size] & index_bits;
            uint32_t skip_check = (stack[stack_size] & skip_check_bit);
            const auto *n = &scene.nodes[cur];

            if (!skip_check) {
                auto res = list.draw_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == Ren::Invisible) continue;
                else if (res == Ren::FullyVisible) skip_check = skip_check_bit;
            }

            if (!n->prim_count) {
                stack[stack_size++] = skip_check | n->left_child;
                stack[stack_size++] = skip_check | n->right_child;
            } else {
                const auto &obj = scene.objects[n->prim_index];

                const uint32_t occluder_flags = CompTransformBit | CompOccluderBit;
                if ((obj.comp_mask & occluder_flags) == occluder_flags) {
                    const auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(obj.components[CompTransform]);

                    // Node has slightly enlarged bounds, so we need to check object's bounding box here
                    if (!skip_check &&
                        list.draw_cam.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                    const Ren::Mat4f &world_from_object = tr->mat;

                    const Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                     clip_from_object = clip_from_view * view_from_object;

                    const auto *occ = (Occluder *)scene.comp_store[CompOccluder]->Get(obj.components[CompOccluder]);
                    const auto *mesh = occ->mesh.get();

                    SWcull_surf surf[16];
                    int surf_count = 0;

                    const Ren::TriGroup *s = &mesh->group(0);
                    while (s->offset != -1) {
                        SWcull_surf *_surf = &surf[surf_count++];

                        _surf->type = SW_OCCLUDER;
                        _surf->prim_type = SW_TRIANGLES;
                        _surf->index_type = SW_UNSIGNED_INT;
                        _surf->attribs = mesh->attribs();
                        _surf->indices = ((const uint8_t *)mesh->indices() + s->offset);
                        _surf->stride = 16;// 13 * sizeof(float);
                        _surf->count = (SWuint)s->num_indices;
                        _surf->base_vertex = -SWint(mesh->attribs_buf1().offset / _surf->stride);
                        _surf->xform = Ren::ValuePtr(clip_from_object);
                        _surf->dont_skip = nullptr;

                        ++s;
                    }

                    swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                }
            }
        }
    }

    /**************************************************************************************************/
    /*                           MESHES/LIGHTS/DECALS/PROBES GATHERING                                */
    /**************************************************************************************************/

    uint64_t main_gather_start = Sys::GetTimeUs();

    {   // Gather meshes and lights, skip occluded and frustum culled
        stack_size = 0;
        stack[stack_size++] = scene.root_node;

        while (stack_size) {
            uint32_t cur = stack[--stack_size] & index_bits;
            uint32_t skip_check = stack[stack_size] & skip_check_bit;
            const auto *n = &scene.nodes[cur];

            if (!skip_check) {
                const float bbox_points[8][3] = { BBOX_POINTS(n->bbox_min, n->bbox_max) };
                auto res = list.draw_cam.CheckFrustumVisibility(bbox_points);
                if (res == Ren::Invisible) continue;
                else if (res == Ren::FullyVisible) skip_check = skip_check_bit;

                if (culling_enabled) {
                    const auto &cam_pos = list.draw_cam.world_position();

                    // do not question visibility of the node in which we are inside
                    if (cam_pos[0] < n->bbox_min[0] - 0.5f || cam_pos[1] < n->bbox_min[1] - 0.5f || cam_pos[2] < n->bbox_min[2] - 0.5f ||
                        cam_pos[0] > n->bbox_max[0] + 0.5f || cam_pos[1] > n->bbox_max[1] + 0.5f || cam_pos[2] > n->bbox_max[2] + 0.5f) {
                        SWcull_surf surf;

                        surf.type = SW_OCCLUDEE;
                        surf.prim_type = SW_TRIANGLES;
                        surf.index_type = SW_UNSIGNED_BYTE;
                        surf.attribs = &bbox_points[0][0];
                        surf.indices = &bbox_indices[0];
                        surf.stride = 3 * sizeof(float);
                        surf.count = 36;
                        surf.base_vertex = 0;
                        surf.xform = Ren::ValuePtr(clip_from_identity);
                        surf.dont_skip = nullptr;

                        swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                        if (surf.visible == 0) continue;
                    }
                }
            }

            if (!n->prim_count) {
                stack[stack_size++] = skip_check | n->left_child;
                stack[stack_size++] = skip_check | n->right_child;
            } else {
                const auto &obj = scene.objects[n->prim_index];

                if ((obj.comp_mask & CompTransformBit) && (obj.comp_mask & (CompDrawableBit | CompLightSourceBit | CompProbeBit))) {
                    const auto *tr = (Transform *)scene.comp_store[CompTransform]->Get(obj.components[CompTransform]);

                    if (!skip_check) {
                        const float bbox_points[8][3] = { BBOX_POINTS(tr->bbox_min_ws, tr->bbox_max_ws) };

                        // Node has slightly enlarged bounds, so we need to check object's bounding box here
                        if (list.draw_cam.CheckFrustumVisibility(bbox_points) == Ren::Invisible) continue;

                        if (culling_enabled) {
                            const auto &cam_pos = list.draw_cam.world_position();

                            // do not question visibility of the object in which we are inside
                            if (cam_pos[0] < tr->bbox_min_ws[0] - 0.5f || cam_pos[1] < tr->bbox_min_ws[1] - 0.5f || cam_pos[2] < tr->bbox_min_ws[2] - 0.5f ||
                                cam_pos[0] > tr->bbox_max_ws[0] + 0.5f || cam_pos[1] > tr->bbox_max_ws[1] + 0.5f || cam_pos[2] > tr->bbox_max_ws[2] + 0.5f) {
                                SWcull_surf surf;

                                surf.type = SW_OCCLUDEE;
                                surf.prim_type = SW_TRIANGLES;
                                surf.index_type = SW_UNSIGNED_BYTE;
                                surf.attribs = &bbox_points[0][0];
                                surf.indices = &bbox_indices[0];
                                surf.stride = 3 * sizeof(float);
                                surf.count = 36;
                                surf.base_vertex = 0;
                                surf.xform = Ren::ValuePtr(clip_from_identity);
                                surf.dont_skip = nullptr;

                                swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                                if (surf.visible == 0) continue;
                            }
                        }
                    }

                    const Ren::Mat4f &world_from_object = tr->mat;
                    const auto world_from_object_trans = Ren::Transpose(world_from_object);

                    proc_objects_[n->prim_index].instance_index = list.instances.count;

                    auto &instance = list.instances.data[list.instances.count++];
                    memcpy(&instance.model_matrix[0][0], Ren::ValuePtr(world_from_object_trans), 12 * sizeof(float));

                    if (obj.comp_mask & CompLightmapBit) {
                        const auto *lm = (Lightmap *)scene.comp_store[CompLightmap]->Get(obj.components[CompLightmap]);
                        memcpy(&instance.lmap_transform[0], Ren::ValuePtr(lm->xform), 4 * sizeof(float));
                    }

                    const Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                     clip_from_object = clip_from_view * view_from_object;

                    if (obj.comp_mask & CompDrawableBit) {
                        const auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(obj.components[CompDrawable]);
                        const auto *mesh = dr->mesh.get();

                        const float max_sort_dist = 100.0f;
                        const uint8_t dist = (uint8_t)_MIN(255 * Ren::Distance(tr->bbox_min_ws, cam.world_position()) / max_sort_dist, 255);

                        uint32_t base_vertex = mesh->attribs_buf1().offset / 16;

                        if (obj.comp_mask & CompAnimStateBit) {
                            const auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(obj.components[CompAnimState]);
                            __push_skeletal_mesh(n->prim_index, as, mesh, list);
                            base_vertex = proc_objects_[n->prim_index].base_vertex;
                        } else {
                            proc_objects_[n->prim_index].base_vertex = base_vertex;
                        }

                        const Ren::TriGroup *s = &mesh->group(0);
                        while (s->offset != -1) {
                            const Ren::Material *mat = s->mat.get();
                            
                            MainDrawBatch &batch = list.main_batches.data[list.main_batches.count++];

                            batch.prog_id = (uint32_t)mat->program(program_index).index();
                            batch.alpha_test_bit = (mat->flags() & Ren::AlphaTest) ? 1 : 0;
                            batch.alpha_blend_bit = (mat->flags() & Ren::AlphaBlend) ? 1 : 0;
                            batch.mat_id = (uint32_t)s->mat.index();
                            batch.cam_dist = (mat->flags() & Ren::AlphaBlend) ? uint32_t(dist) : 0;
                            batch.indices_offset = mesh->indices_buf().offset + s->offset;
                            batch.base_vertex = base_vertex;
                            batch.indices_count = s->num_indices;
                            batch.instance_indices[0] = (uint32_t)(list.instances.count - 1);
                            batch.instance_count = 1;

                            ++s;
                        }
                    }

                    if (lighting_enabled && (obj.comp_mask & CompLightSourceBit)) {
                        const auto *light = (LightSource *)scene.comp_store[CompLightSource]->Get(obj.components[CompLightSource]);

                        Ren::Vec4f pos = { light->offset[0], light->offset[1], light->offset[2], 1.0f };
                        pos = world_from_object * pos;
                        pos /= pos[3];

                        Ren::Vec4f dir = { -light->dir[0], -light->dir[1], -light->dir[2], 0.0f };
                        dir = world_from_object * dir;

                        auto res = Ren::FullyVisible;
                                        
                        if (!skip_check) {
                            for (int k = 0; k < 6; k++) {
                                const auto &plane = list.draw_cam.frustum_plane(k);

                                float dist = plane.n[0] * pos[0] +
                                             plane.n[1] * pos[1] +
                                             plane.n[2] * pos[2] + plane.d;

                                if (dist < -light->influence) {
                                    res = Ren::Invisible;
                                    break;
                                } else if (std::abs(dist) < light->influence) {
                                    res = Ren::PartiallyVisible;
                                }
                            }
                        }

                        if (res != Ren::Invisible) {
                            litem_to_lsource_.data[litem_to_lsource_.count++] = light;
                            auto &ls = list.light_sources.data[list.light_sources.count++];

                            memcpy(&ls.pos[0], &pos[0], 3 * sizeof(float));
                            ls.radius = light->radius;
                            memcpy(&ls.col[0], &light->col[0], 3 * sizeof(float));
                            ls.shadowreg_index = 0xffffffff;
                            memcpy(&ls.dir[0], &dir[0], 3 * sizeof(float));
                            ls.spot = light->spot;
                        }
                    }

                    if (decals_enabled && (obj.comp_mask & CompDecalBit)) {
                        Ren::Mat4f object_from_world = Ren::Inverse(world_from_object);

                        const auto *decal = (Decal *)scene.comp_store[CompDecal]->Get(obj.components[CompDecal]);

                        const Ren::Mat4f &view_from_object = decal->view,
                                         &clip_from_view = decal->proj;

                        Ren::Mat4f view_from_world = view_from_object * object_from_world,
                                   clip_from_world = clip_from_view * view_from_world;

                        Ren::Mat4f world_from_clip = Ren::Inverse(clip_from_world);

                        Ren::Vec4f bbox_points[] = {
                            { -1.0f, -1.0f, -1.0f, 1.0f }, { -1.0f, 1.0f, -1.0f, 1.0f },
                            { 1.0f, 1.0f, -1.0f, 1.0f }, { 1.0f, -1.0f, -1.0f, 1.0f },

                            { -1.0f, -1.0f, 1.0f, 1.0f }, { -1.0f, 1.0f, 1.0f, 1.0f },
                            { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, -1.0f, 1.0f, 1.0f }
                        };

                        Ren::Vec3f bbox_min = Ren::Vec3f{ std::numeric_limits<float>::max() },
                                   bbox_max = Ren::Vec3f{ std::numeric_limits<float>::lowest() };

                        for (int k = 0; k < 8; k++) {
                            bbox_points[k] = world_from_clip * bbox_points[k];
                            bbox_points[k] /= bbox_points[k][3];

                            bbox_min = Ren::Min(bbox_min, Ren::Vec3f{ bbox_points[k] });
                            bbox_max = Ren::Max(bbox_max, Ren::Vec3f{ bbox_points[k] });
                        }

                        auto res = Ren::FullyVisible;

                        if (!skip_check) {
                            for (int p = Ren::LeftPlane; p <= Ren::FarPlane; p++) {
                                const auto &plane = list.draw_cam.frustum_plane(p);

                                int in_count = 8;

                                for (int k = 0; k < 8; k++) {
                                    float dist = plane.n[0] * bbox_points[k][0] +
                                                    plane.n[1] * bbox_points[k][1] +
                                                    plane.n[2] * bbox_points[k][2] + plane.d;
                                    if (dist < 0.0f) {
                                        in_count--;
                                    }
                                }

                                if (in_count == 0) {
                                    res = Ren::Invisible;
                                    break;
                                } else if (in_count != 8) {
                                    res = Ren::PartiallyVisible;
                                }
                            }
                        }

                        if (res != Ren::Invisible) {
                            ditem_to_decal_.data[ditem_to_decal_.count++] = decal;
                            decals_boxes_.data[decals_boxes_.count++] = { bbox_min, bbox_max };

                            Ren::Mat4f clip_from_world_transposed = Ren::Transpose(clip_from_world);

                            auto &de = list.decals.data[list.decals.count++];
                            memcpy(&de.mat[0][0], &clip_from_world_transposed[0][0], 12 * sizeof(float));
                            memcpy(&de.diff[0], &decal->diff[0], 4 * sizeof(float));
                            memcpy(&de.norm[0], &decal->norm[0], 4 * sizeof(float));
                            memcpy(&de.spec[0], &decal->spec[0], 4 * sizeof(float));
                        }
                    }

                    if (obj.comp_mask & CompProbeBit) {
                        const auto *probe = (LightProbe *)scene.comp_store[CompProbe]->Get(obj.components[CompProbe]);
                        
                        Ren::Vec4f pos = { probe->offset[0], probe->offset[1], probe->offset[2], 1.0f };
                        pos = world_from_object * pos;
                        pos /= pos[3];

                        auto &pr = list.probes.data[list.probes.count++];
                        pr.layer = float(probe->layer_index);
                        pr.radius = probe->radius;
                        memcpy(&pr.position[0], &pos[0], 3 * sizeof(float));
                        for (int k = 0; k < 4; k++) {
                            pr.sh_coeffs[0][k] = probe->sh_coeffs[k][0];
                            pr.sh_coeffs[1][k] = probe->sh_coeffs[k][1];
                            pr.sh_coeffs[2][k] = probe->sh_coeffs[k][2];
                        }
                    }
                }
            }
        }
    }

    /**************************************************************************************************/
    /*                                     SHADOWMAP GATHERING                                        */
    /**************************************************************************************************/

    uint64_t shadow_gather_start = Sys::GetTimeUs();

    if (lighting_enabled && shadows_enabled && Ren::Length2(list.env.sun_dir) > 0.9f && Ren::Length2(list.env.sun_col) > std::numeric_limits<float>::epsilon()) {
        // Reserve space for sun shadow
        int sun_shadow_pos[2] = { 0, 0 };
        int sun_shadow_res[2];
        if (shadow_splitter_.FindNode(sun_shadow_pos, sun_shadow_res) == -1 || sun_shadow_res[0] != SUN_SHADOW_RES || sun_shadow_res[1] != SUN_SHADOW_RES) {
            shadow_splitter_.Clear();

            sun_shadow_res[0] = SUN_SHADOW_RES;
            sun_shadow_res[1] = SUN_SHADOW_RES;

            int id = shadow_splitter_.Allocate(sun_shadow_res, sun_shadow_pos);
            assert(id != -1 && sun_shadow_pos[0] == 0 && sun_shadow_pos[1] == 0);
        }

        // Planes, that define shadow map splits
        const float far_planes[] = { float(REN_SHAD_CASCADE0_DIST), float(REN_SHAD_CASCADE1_DIST),
                                     float(REN_SHAD_CASCADE2_DIST), float(REN_SHAD_CASCADE3_DIST) };
        const float near_planes[] = { list.draw_cam.near(), far_planes[0], far_planes[1], far_planes[2] };

        // Reserved positions for sun shadowmap
        const int OneCascadeRes = SUN_SHADOW_RES / 2;
        const int map_positions[][2] = { { 0, 0 }, { OneCascadeRes, 0 }, { 0, OneCascadeRes }, { OneCascadeRes, OneCascadeRes } };

        // Choose up vector for shadow camera
        auto light_dir = list.env.sun_dir;
        auto cam_up = Ren::Vec3f{ 0.0f, 0.0, 1.0f };
        if (std::abs(light_dir[0]) <= std::abs(light_dir[1]) && std::abs(light_dir[0]) <= std::abs(light_dir[2])) {
            cam_up = Ren::Vec3f{ 1.0f, 0.0, 0.0f };
        }
        else if (std::abs(light_dir[1]) <= std::abs(light_dir[0]) && std::abs(light_dir[1]) <= std::abs(light_dir[2])) {
            cam_up = Ren::Vec3f{ 0.0f, 1.0, 0.0f };
        }
        // Calculate side vector of shadow camera
        auto cam_side = Normalize(Cross(light_dir, cam_up));
        cam_up = Cross(cam_side, light_dir);

        const Ren::Vec3f scene_dims = scene.nodes[scene.root_node].bbox_max - scene.nodes[scene.root_node].bbox_min;
        const float max_dist = Ren::Length(scene_dims);

        const Ren::Vec3f view_dir = list.draw_cam.view_dir();

        // Gather drawables for each cascade
        for (int casc = 0; casc < 4; casc++) {
            auto temp_cam = list.draw_cam;
            temp_cam.Perspective(list.draw_cam.angle(), list.draw_cam.aspect(), near_planes[casc], far_planes[casc]);
            temp_cam.UpdatePlanes();

            const Ren::Mat4f &tmp_cam_view_from_world = temp_cam.view_matrix(),
                             &tmp_cam_clip_from_view = temp_cam.proj_matrix();

            const Ren::Mat4f tmp_cam_clip_from_world = tmp_cam_clip_from_view * tmp_cam_view_from_world;
            const Ren::Mat4f tmp_cam_world_from_clip = Ren::Inverse(tmp_cam_clip_from_world);

            Ren::Vec3f bounding_center;
            const float bounding_radius = temp_cam.GetBoundingSphere(bounding_center);
            float object_dim_thres = 0.0f;

            Ren::Vec3f cam_target = bounding_center;

            {   // Snap camera movement to shadow map pixels
                const float move_step = (2 * bounding_radius) / (0.5f * SUN_SHADOW_RES);
                //                      |_shadow map extent_|   |_res of one cascade_|

                // Project target on shadow cam view matrix
                float _dot_f = Ren::Dot(cam_target, light_dir),
                      _dot_s = Ren::Dot(cam_target, cam_side),
                      _dot_u = Ren::Dot(cam_target, cam_up);

                // Snap coordinates to pixels
                _dot_f = std::round(_dot_f / move_step) * move_step;
                _dot_s = std::round(_dot_s / move_step) * move_step;
                _dot_u = std::round(_dot_u / move_step) * move_step;

                // Update target coordinates in world space
                cam_target = _dot_f * light_dir + _dot_s * cam_side + _dot_u * cam_up;

                // Set object size requirenment
                object_dim_thres = 2.0f * move_step;
            }

            Ren::Vec3f cam_center = cam_target + max_dist * light_dir;

            Ren::Camera shadow_cam;
            shadow_cam.SetupView(cam_center, cam_target, cam_up);
            shadow_cam.Orthographic(-bounding_radius, bounding_radius, bounding_radius, -bounding_radius, 0.0f, max_dist + bounding_radius);
            shadow_cam.UpdatePlanes();

            Ren::Mat4f sh_clip_from_world = shadow_cam.proj_matrix() * shadow_cam.view_matrix();

            auto &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

            sh_list.shadow_map_pos[0] = map_positions[casc][0];
            sh_list.shadow_map_pos[1] = map_positions[casc][1];
            sh_list.shadow_map_size[0] = OneCascadeRes;
            sh_list.shadow_map_size[1] = OneCascadeRes;
            sh_list.shadow_batch_start = list.shadow_batches.count;
            sh_list.shadow_batch_count = 0;
            sh_list.solid_batches_count = 0;
            sh_list.cam_near = shadow_cam.near();
            sh_list.cam_far = shadow_cam.far();

            Ren::Frustum sh_clip_frustum;

            {   // Construct shadow clipping frustum
                Ren::Vec4f frustum_points[8] = { Ren::Uninitialize, Ren::Uninitialize, Ren::Uninitialize, Ren::Uninitialize,
                                                 Ren::Uninitialize, Ren::Uninitialize, Ren::Uninitialize, Ren::Uninitialize };

                for (int k = 0; k < 8; k++) {
                    frustum_points[k] = tmp_cam_world_from_clip * ClipFrustumPoints[k];
                    frustum_points[k] /= frustum_points[k][3];
                }

                Ren::Vec2f frustum_points_proj[8];

                for (int k = 0; k < 8; k++) {
                    Ren::Vec4f projected_p = sh_clip_from_world * frustum_points[k];
                    projected_p /= projected_p[3];

                    frustum_points_proj[k] = { projected_p[0], projected_p[1] };
                }

                Ren::Vec2i frustum_edges[] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
                                               { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
                                               { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };

                int silhouette_edges[12], silhouette_edges_count = 0;

                for (int i = 0; i < 12; i++) {
                    int k1 = frustum_edges[i][0], k2 = frustum_edges[i][1];

                    int last_sign = 0;
                    bool is_silhouette = true;

                    for (int k = 0; k < 8; k++) {
                        if (k == k1 || k == k2) continue;

                        float d = (frustum_points_proj[k][0] - frustum_points_proj[k1][0]) * (frustum_points_proj[k2][1] - frustum_points_proj[k1][1]) -
                                  (frustum_points_proj[k][1] - frustum_points_proj[k1][1]) * (frustum_points_proj[k2][0] - frustum_points_proj[k1][0]);

                        int sign = (d > 0.0f) ? 1 : -1;

                        if (last_sign && sign != last_sign) {
                            is_silhouette = false;
                            break;
                        }

                        last_sign = sign;
                    }

                    if (is_silhouette) {
                        silhouette_edges[silhouette_edges_count++] = i;
                        if (last_sign == 1) {
                            std::swap(frustum_edges[i][0], frustum_edges[i][1]);
                        }

                        float k0 = (frustum_points_proj[frustum_edges[i][0]][1] - frustum_points_proj[frustum_edges[i][1]][1]) /
                                   (frustum_points_proj[frustum_edges[i][0]][0] - frustum_points_proj[frustum_edges[i][1]][0]);

                        float b0 = frustum_points_proj[frustum_edges[i][0]][1] - k0 * frustum_points_proj[frustum_edges[i][0]][0];

                        // Check if it is duplicate
                        for (int k = 0; k < silhouette_edges_count - 1; k++) {
                            int j = silhouette_edges[k];

                            float k1 = (frustum_points_proj[frustum_edges[j][0]][1] - frustum_points_proj[frustum_edges[j][1]][1]) /
                                       (frustum_points_proj[frustum_edges[j][0]][0] - frustum_points_proj[frustum_edges[j][1]][0]);

                            float b1 = frustum_points_proj[frustum_edges[j][0]][1] - k1 * frustum_points_proj[frustum_edges[j][0]][0];

                            if (std::abs(k1 - k0) < 0.001f && std::abs(b1 - b0) < 0.001f) {
                                silhouette_edges_count--;
                                break;
                            }
                        }
                    }
                }

                assert(silhouette_edges_count <= 6);
                
                sh_clip_frustum.planes_count = silhouette_edges_count;
                sh_list.view_frustum_outline_count = 2 * silhouette_edges_count;

                auto scissor_min = Ren::Vec2i{ SHADOWMAP_WIDTH }, scissor_max = Ren::Vec2i{ 0 };

                for (int i = 0; i < silhouette_edges_count; i++) {
                    const Ren::Vec2i edge = frustum_edges[silhouette_edges[i]];

                    const auto p1 = Ren::Vec3f{ frustum_points[edge[0]] },
                               p2 = Ren::Vec3f{ frustum_points[edge[1]] };

                    // Extrude edge in direction of light
                    const Ren::Vec3f p3 = p2 + light_dir;

                    // Construct clipping plane
                    sh_clip_frustum.planes[i] = Ren::Plane{ p1, p2, p3 };

                    // Store projected points for debugging
                    sh_list.view_frustum_outline[2 * i + 0] = frustum_points_proj[edge[0]];
                    sh_list.view_frustum_outline[2 * i + 1] = frustum_points_proj[edge[1]];

                    // Find region for scissor test
                    const Ren::Vec2i p1i = { sh_list.shadow_map_pos[0] + int((0.5f * sh_list.view_frustum_outline[2 * i + 0][0] + 0.5f) * sh_list.shadow_map_size[0]),
                                             sh_list.shadow_map_pos[1] + int((0.5f * sh_list.view_frustum_outline[2 * i + 0][1] + 0.5f) * sh_list.shadow_map_size[1]) };

                    const Ren::Vec2i p2i = { sh_list.shadow_map_pos[0] + int((0.5f * sh_list.view_frustum_outline[2 * i + 1][0] + 0.5f) * sh_list.shadow_map_size[0]),
                                             sh_list.shadow_map_pos[1] + int((0.5f * sh_list.view_frustum_outline[2 * i + 1][1] + 0.5f) * sh_list.shadow_map_size[1]) };

                    scissor_min = Ren::Min(scissor_min, Ren::Min(p1i, p2i));
                    scissor_max = Ren::Max(scissor_max, Ren::Max(p1i, p2i));
                }

                sh_list.scissor_test_pos[0] = scissor_min[0];
                sh_list.scissor_test_pos[1] = scissor_min[1];
                sh_list.scissor_test_size[0] = scissor_max[0] - scissor_min[0];
                sh_list.scissor_test_size[1] = scissor_max[1] - scissor_min[1];

                // add near and far planes
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] = shadow_cam.frustum_plane(Ren::NearPlane);
                sh_clip_frustum.planes[sh_clip_frustum.planes_count++] = shadow_cam.frustum_plane(Ren::FarPlane);
            }

            auto &reg = list.shadow_regions.data[list.shadow_regions.count++];

            reg.transform = Ren::Vec4f{ float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH, float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                        float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH, float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT };

            float cached_dist = Ren::Distance(list.draw_cam.world_position(), sun_shadow_cache_[casc].view_pos),
                  cached_dir_dist = Ren::Distance(view_dir, sun_shadow_cache_[casc].view_dir);
            
            // discard cached cascade if view changed significantly
            sun_shadow_cache_[casc].valid &= (cached_dist < 1.0f && cached_dir_dist < 0.1f);

            const uint8_t pattern_bit = (1 << (frame_counter_ % 8));
            const bool should_update = (pattern_bit & SunShadowUpdatePattern[casc]) != 0;

            if (sun_shadow_cache_[casc].valid && !should_update) {
                // keep this cascade unchanged
                reg.clip_from_world = sun_shadow_cache_[casc].clip_from_world;
                continue;
            } else {
                reg.clip_from_world = sh_clip_from_world;

                sun_shadow_cache_[casc].valid = true;
                sun_shadow_cache_[casc].view_pos = list.draw_cam.world_position();
                sun_shadow_cache_[casc].view_dir = view_dir;
                sun_shadow_cache_[casc].clip_from_world = sh_clip_from_world;
            }

#if 0
            if (shadow_cam.CheckFrustumVisibility(cam.world_position()) != Ren::FullyVisible) {
                // Check if shadowmap frustum is visible to main camera
                
                Ren::Mat4f world_from_clip = Ren::Inverse(sh_clip_from_world);

                Ren::Vec4f frustum_points[] = {
                    { -1.0f, -1.0f, 0.0f, 1.0f },
                    {  1.0f, -1.0f, 0.0f, 1.0f },
                    { -1.0f, -1.0f, 1.0f, 1.0f },
                    {  1.0f, -1.0f, 1.0f, 1.0f },

                    { -1.0f,  1.0f, 0.0f, 1.0f },
                    {  1.0f,  1.0f, 0.0f, 1.0f },
                    { -1.0f,  1.0f, 1.0f, 1.0f },
                    {  1.0f,  1.0f, 1.0f, 1.0f }
                };

                for (int k = 0; k < 8; k++) {
                    frustum_points[k] = world_from_clip * frustum_points[k];
                    frustum_points[k] /= frustum_points[k][3];
                }

                SWcull_surf surf;

                surf.type = SW_OCCLUDEE;
                surf.prim_type = SW_TRIANGLES;
                surf.index_type = SW_UNSIGNED_BYTE;
                surf.attribs = &frustum_points[0][0];
                surf.indices = &bbox_indices[0];
                surf.stride = 4 * sizeof(float);
                surf.count = 36;
                surf.base_vertex = 0;
                surf.xform = Ren::ValuePtr(clip_from_identity);
                surf.dont_skip = nullptr;

                swCullCtxSubmitCullSurfs(&cull_ctx_, &surf, 1);

                if (surf.visible == 0) {
                    continue;
                }
            }
#endif

            const uint32_t skip_check_bit = (1u << 31);
            const uint32_t index_bits = ~skip_check_bit;

            stack_size = 0;
            stack[stack_size++] = scene.root_node;

            while (stack_size) {
                uint32_t cur = stack[--stack_size] & index_bits;
                uint32_t skip_check = stack[stack_size] & skip_check_bit;
                const auto *n = &scene.nodes[cur];

                if (!skip_check) {
                    auto res = sh_clip_frustum.CheckVisibility(n->bbox_min, n->bbox_max);
                    if (res == Ren::Invisible) continue;
                    else if (res == Ren::FullyVisible) skip_check = skip_check_bit;
                }

                if (!n->prim_count) {
                    stack[stack_size++] = skip_check | n->left_child;
                    stack[stack_size++] = skip_check | n->right_child;
                } else {
                    const auto& obj = scene.objects[n->prim_index];

                    const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
                    if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                        const auto* tr = (Transform *)scene.comp_store[CompTransform]->Get(obj.components[CompTransform]);
                        const auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(obj.components[CompDrawable]);
                        if ((dr->flags & Drawable::DrVisibleToShadow) == 0) continue;

                        if (!skip_check &&
                            sh_clip_frustum.CheckVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                        if ((tr->bbox_max_ws[0] - tr->bbox_min_ws[0]) < object_dim_thres &&
                            (tr->bbox_max_ws[1] - tr->bbox_min_ws[1]) < object_dim_thres &&
                            (tr->bbox_max_ws[2] - tr->bbox_min_ws[2]) < object_dim_thres) continue;

                        const auto &world_from_object = tr->mat;
                        const auto *mesh = dr->mesh.get();

                        if (proc_objects_[n->prim_index].instance_index == 0xffffffff) {
                            proc_objects_[n->prim_index].instance_index = list.instances.count;

                            auto world_from_object_trans = Ren::Transpose(world_from_object);

                            auto &instance = list.instances.data[list.instances.count++];
                            memcpy(&instance.model_matrix[0][0], Ren::ValuePtr(world_from_object_trans), 12 * sizeof(float));
                        }

                        if (proc_objects_[n->prim_index].base_vertex == 0xffffffff) {
                            proc_objects_[n->prim_index].base_vertex = mesh->attribs_buf1().offset / 16;

                            if (obj.comp_mask & CompAnimStateBit) {
                                const auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(obj.components[CompAnimState]);
                                __push_skeletal_mesh(n->prim_index, as, mesh, list);
                            }
                        }

                        const auto *s = &mesh->group(0);
                        while (s->offset != -1) {
                            const Ren::Material *mat = s->mat.get();
                            if ((mat->flags() & Ren::AlphaBlend) == 0) {
                                ShadowDrawBatch &batch = list.shadow_batches.data[list.shadow_batches.count++];

                                batch.mat_id = (mat->flags() & Ren::AlphaTest) ? (uint32_t)s->mat.index() : 0;
                                batch.alpha_test_bit = (mat->flags() & Ren::AlphaTest) ? 1 : 0;
                                batch.indices_offset = mesh->indices_buf().offset + s->offset;
                                batch.base_vertex = proc_objects_[n->prim_index].base_vertex;
                                batch.indices_count = s->num_indices;
                                batch.instance_indices[0] = proc_objects_[n->prim_index].instance_index;
                                batch.instance_count = 1;
                            }

                            ++s;
                        }
                    }
                }
            }

            sh_list.shadow_batch_count = list.shadow_batches.count - sh_list.shadow_batch_start;
        }
    }

    const Ren::Vec3f cam_pos = cam.world_position();

    for (int i = 0; i < int(list.light_sources.count) && shadows_enabled; i++) {
        auto &l = list.light_sources.data[i];
        const auto *ls = litem_to_lsource_.data[i];

        if (!ls->cast_shadow) continue;

        const auto light_center = Ren::Vec3f{ l.pos[0], l.pos[1], l.pos[2] };
        const float distance = Ren::Distance(light_center, cam_pos);

        const int resolutions[][2] = { { 512, 512 }, { 256, 256 }, { 128, 128 }, { 64, 64 } };

        // choose resolution based on distance
        int res_index = std::min(int(distance * 0.01f), 3);

        ShadReg *region = nullptr;

        for (int j = 0; j < (int)allocated_shadow_regions_.count; j++) {
            auto &reg = allocated_shadow_regions_.data[j];

            if (reg.ls == ls) {
                if (reg.size[0] != resolutions[res_index][0] || reg.size[1] != resolutions[res_index][1]) {
                    // free and reallocate region
                    shadow_splitter_.Free(reg.pos);
                    reg = allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                } else {
                    region = &reg;
                }
                break;
            }
        }

        // try to allocate best resolution possible
        for (; res_index < 4 && !region; res_index++) {
            int pos[2];
            int node = shadow_splitter_.Allocate(resolutions[res_index], pos);
            if (node == -1 && allocated_shadow_regions_.count) {
                ShadReg *oldest = &allocated_shadow_regions_.data[0];
                for (int j = 0; j < (int)allocated_shadow_regions_.count; j++) {
                    if (allocated_shadow_regions_.data[j].last_visible < oldest->last_visible) {
                        oldest = &allocated_shadow_regions_.data[j];
                    }
                }
                if ((scene.update_counter - oldest->last_visible) > 10) {
                    // kick out one of old cached region
                    shadow_splitter_.Free(oldest->pos);
                    *oldest = allocated_shadow_regions_.data[--allocated_shadow_regions_.count];
                    // try again to insert
                    node = shadow_splitter_.Allocate(resolutions[res_index], pos);
                }
            }
            if (node != -1) {
                region = &allocated_shadow_regions_.data[allocated_shadow_regions_.count++];
                region->ls = ls;
                region->pos[0] = pos[0];
                region->pos[1] = pos[1];
                region->size[0] = resolutions[res_index][0];
                region->size[1] = resolutions[res_index][1];
                region->last_update = region->last_visible = 0xffffffff;
            }
        }

        if (region) {
            const auto light_dir = Ren::Vec3f{ -l.dir[0], -l.dir[1], -l.dir[2] };

            auto light_up = Ren::Vec3f{ 0.0f, 0.0, 1.0f };
            if (std::abs(light_dir[0]) <= std::abs(light_dir[1]) && std::abs(light_dir[0]) <= std::abs(light_dir[2])) {
                light_up = Ren::Vec3f{ 1.0f, 0.0, 0.0f };
            } else if (std::abs(light_dir[1]) <= std::abs(light_dir[0]) && std::abs(light_dir[1]) <= std::abs(light_dir[2])) {
                light_up = Ren::Vec3f{ 0.0f, 1.0, 0.0f };
            }

            float light_angle = 2.0f * std::acos(l.spot) * 180.0f / Ren::Pi<float>();

            Ren::Camera shadow_cam;
            shadow_cam.SetupView(light_center, light_center + light_dir, light_up);
            shadow_cam.Perspective(light_angle, 1.0f, 0.1f, ls->influence);
            shadow_cam.UpdatePlanes();

            // TODO: Check visibility of shadow frustum

            auto clip_from_world = shadow_cam.proj_matrix() * shadow_cam.view_matrix();

            auto &sh_list = list.shadow_lists.data[list.shadow_lists.count++];

            sh_list.shadow_map_pos[0] = sh_list.scissor_test_pos[0] = region->pos[0];
            sh_list.shadow_map_pos[1] = sh_list.scissor_test_pos[1] = region->pos[1];
            sh_list.shadow_map_size[0] = sh_list.scissor_test_size[0] = region->size[0];
            sh_list.shadow_map_size[1] = sh_list.scissor_test_size[1] = region->size[1];
            sh_list.shadow_batch_start = list.shadow_batches.count;
            sh_list.shadow_batch_count = 0;
            sh_list.cam_near = region->cam_near = shadow_cam.near();
            sh_list.cam_far = region->cam_far = shadow_cam.far();
            sh_list.view_frustum_outline_count = 0;

            l.shadowreg_index = (int)list.shadow_regions.count;
            auto &reg = list.shadow_regions.data[list.shadow_regions.count++];

            reg.transform = Ren::Vec4f{ float(sh_list.shadow_map_pos[0]) / SHADOWMAP_WIDTH, float(sh_list.shadow_map_pos[1]) / SHADOWMAP_HEIGHT,
                                        float(sh_list.shadow_map_size[0]) / SHADOWMAP_WIDTH, float(sh_list.shadow_map_size[1]) / SHADOWMAP_HEIGHT };
            reg.clip_from_world = clip_from_world;

            bool light_sees_dynamic_objects = false;

            const uint32_t skip_check_bit = (1u << 31);
            const uint32_t index_bits = ~skip_check_bit;

            stack_size = 0;
            stack[stack_size++] = scene.root_node;

            while (stack_size) {
                uint32_t cur = stack[--stack_size] & index_bits;
                uint32_t skip_check = stack[stack_size] & skip_check_bit;
                const auto* n = &scene.nodes[cur];

                auto res = shadow_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == Ren::Invisible) continue;
                else if (res == Ren::FullyVisible) skip_check = skip_check_bit;

                if (!n->prim_count) {
                    stack[stack_size++] = skip_check | n->left_child;
                    stack[stack_size++] = skip_check | n->right_child;
                } else {
                    const auto& obj = scene.objects[n->prim_index];

                    const uint32_t drawable_flags = CompDrawableBit | CompTransformBit;
                    if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                        const auto* tr = (Transform *)scene.comp_store[CompTransform]->Get(obj.components[CompTransform]);

                        if (!skip_check &&
                            shadow_cam.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                        const auto &world_from_object = tr->mat;
                        const auto *dr = (Drawable *)scene.comp_store[CompDrawable]->Get(obj.components[CompDrawable]);
                        if ((dr->flags & Drawable::DrVisibleToShadow) == 0) continue;

                        const auto *mesh = dr->mesh.get();

                        auto world_from_object_trans = Ren::Transpose(world_from_object);

                        if (proc_objects_[n->prim_index].instance_index == 0xffffffff) {
                            proc_objects_[n->prim_index].instance_index = list.instances.count;

                            auto &instance = list.instances.data[list.instances.count++];
                            memcpy(&instance.model_matrix[0][0], Ren::ValuePtr(world_from_object_trans), 12 * sizeof(float));
                        }

                        if (proc_objects_[n->prim_index].base_vertex == 0xffffffff) {
                            proc_objects_[n->prim_index].base_vertex = mesh->attribs_buf1().offset / 16;

                            if (obj.comp_mask & CompAnimStateBit) {
                                const auto *as = (AnimState *)scene.comp_store[CompAnimState]->Get(obj.components[CompAnimState]);
                                __push_skeletal_mesh(n->prim_index, as, mesh, list);
                            }
                        }

                        const Ren::TriGroup *s = &mesh->group(0);
                        while (s->offset != -1) {
                            const Ren::Material *mat = s->mat.get();
                            if ((mat->flags() & Ren::AlphaBlend) == 0) {
                                ShadowDrawBatch &batch = list.shadow_batches.data[list.shadow_batches.count++];

                                batch.mat_id = (mat->flags() & Ren::AlphaTest) ? (uint32_t)s->mat.index() : 0;
                                batch.alpha_test_bit = (mat->flags() & Ren::AlphaTest) ? 1 : 0;
                                batch.indices_offset = mesh->indices_buf().offset + s->offset;
                                batch.base_vertex = proc_objects_[n->prim_index].base_vertex;
                                batch.indices_count = s->num_indices;
                                batch.instance_indices[0] = proc_objects_[n->prim_index].instance_index;
                                batch.instance_count = 1;
                            }
                            ++s;
                        }
                    }

                    if (obj.last_change_mask & CompTransformBit) {
                        light_sees_dynamic_objects = true;
                    }
                }
            }

            if (!light_sees_dynamic_objects && region->last_update != 0xffffffff && (scene.update_counter - region->last_update > 2)) {
                // nothing was changed within last two frames, discard added batches
                list.shadow_batches.count = sh_list.shadow_batch_start;
                sh_list.shadow_batch_count = 0;
            } else {
                if (light_sees_dynamic_objects || region->last_update == 0xffffffff) {
                    region->last_update = scene.update_counter;
                }
                sh_list.shadow_batch_count = list.shadow_batches.count - sh_list.shadow_batch_start;
            }

            region->last_visible = scene.update_counter;
        }
    }

    if (shadows_enabled && (list.render_flags & DebugShadow)) {
        list.cached_shadow_regions.count = 0;
        for (int i = 0; i < (int)allocated_shadow_regions_.count; i++) {
            const auto &r = allocated_shadow_regions_.data[i];
            if (r.last_visible != scene.update_counter) {
                list.cached_shadow_regions.data[list.cached_shadow_regions.count++] = r;
            }
        }
    }

    /**************************************************************************************************/
    /*                                    OPTIMIZING DRAW LISTS                                       */
    /**************************************************************************************************/

    uint64_t drawables_sort_start = Sys::GetTimeUs();

    // Sort drawables to optimize state switches
    temp_sort_spans_64_[0].count = list.main_batches.count;
    temp_sort_spans_64_[1].count = list.main_batches.count;
    uint32_t spans_count = 0;

    list.main_batch_indices.count = list.main_batches.count;
    

    // compress batches into spans with indentical key values (makes sorting faster)
    for (uint32_t start = 0, end = 1; end <= list.main_batches.count; end++) {
        if (end == list.main_batches.count || (list.main_batches.data[start].sort_key != list.main_batches.data[end].sort_key)) {
            temp_sort_spans_64_[0].data[spans_count].key = list.main_batches.data[start].sort_key;
            temp_sort_spans_64_[0].data[spans_count].base = start;
            temp_sort_spans_64_[0].data[spans_count++].count = end - start;
            start = end;
        }
    }

    RadixSort_LSB<SortSpan64>(temp_sort_spans_64_[0].data.get(), temp_sort_spans_64_[0].data.get() + spans_count, temp_sort_spans_64_[1].data.get());

    // decompress sorted spans
    size_t counter = 0;
    for (uint32_t i = 0; i < spans_count; i++) {
        for (uint32_t j = 0; j < temp_sort_spans_64_[0].data[i].count; j++) {
            list.main_batch_indices.data[counter++] = temp_sort_spans_64_[0].data[i].base + j;
        }
    }

    // Merge similar batches
    for (uint32_t start = 0, end = 1; end <= list.main_batch_indices.count; end++) {
        if (end == list.main_batch_indices.count ||
            list.main_batches.data[list.main_batch_indices.data[start]].sort_key != list.main_batches.data[list.main_batch_indices.data[end]].sort_key) {

            auto &b1 = list.main_batches.data[list.main_batch_indices.data[start]];
            for (uint32_t i = start + 1; i < end; i++) {
                auto &b2 = list.main_batches.data[list.main_batch_indices.data[i]];

                if (b1.base_vertex == b2.base_vertex && b1.instance_count + b2.instance_count < REN_MAX_BATCH_SIZE) {
                    memcpy(&b1.instance_indices[b1.instance_count], &b2.instance_indices[0], b2.instance_count * sizeof(int));
                    b1.instance_count += b2.instance_count;
                    b2.instance_count = 0;
                }
            }

            start = end;
        }
    }

    list.shadow_batch_indices.count = list.shadow_batches.count;

    uint32_t sh_batch_indices_counter = 0;

    for (int i = 0; i < (int)list.shadow_lists.count; i++) {
        auto &sh_list = list.shadow_lists.data[i];

        uint32_t shadow_batch_end = sh_list.shadow_batch_start + sh_list.shadow_batch_count;

        temp_sort_spans_32_[0].count = sh_list.shadow_batch_count;
        temp_sort_spans_32_[1].count = sh_list.shadow_batch_count;
        uint32_t spans_count = 0;

        // compress batches into spans with indentical key values (makes sorting faster)
        for (uint32_t start = sh_list.shadow_batch_start, end = sh_list.shadow_batch_start + 1;
             end <= shadow_batch_end; end++) {
            if (end == shadow_batch_end || (list.shadow_batches.data[start].sort_key != list.shadow_batches.data[end].sort_key)) {
                temp_sort_spans_32_[0].data[spans_count].key = list.shadow_batches.data[start].sort_key;
                temp_sort_spans_32_[0].data[spans_count].base = start;
                temp_sort_spans_32_[0].data[spans_count++].count = end - start;
                start = end;
            }
        }

        RadixSort_LSB<SortSpan32>(temp_sort_spans_32_[0].data.get(), temp_sort_spans_32_[0].data.get() + spans_count, temp_sort_spans_32_[1].data.get());

        // decompress sorted spans
        for (uint32_t i = 0; i < spans_count; i++) {
            for (uint32_t j = 0; j < temp_sort_spans_32_[0].data[i].count; j++) {
                list.shadow_batch_indices.data[sh_batch_indices_counter++] = temp_sort_spans_32_[0].data[i].base + j;
            }
        }
        assert(sh_batch_indices_counter == shadow_batch_end);

        sh_list.solid_batches_count = 0xffffffff;

        // Merge similar batches
        for (uint32_t start = sh_list.shadow_batch_start, end = sh_list.shadow_batch_start + 1; end <= shadow_batch_end; end++) {
            if (sh_list.solid_batches_count == 0xffffffff && list.shadow_batches.data[list.shadow_batch_indices.data[start]].alpha_test_bit) {
                sh_list.solid_batches_count = (start - sh_list.shadow_batch_start);
            }

            if (end == shadow_batch_end ||
                list.shadow_batches.data[list.shadow_batch_indices.data[start]].sort_key != list.shadow_batches.data[list.shadow_batch_indices.data[end]].sort_key) {

                auto &b1 = list.shadow_batches.data[list.shadow_batch_indices.data[start]];
                for (uint32_t i = start + 1; i < end; i++) {
                    auto &b2 = list.shadow_batches.data[list.shadow_batch_indices.data[i]];

                    if (b1.base_vertex == b2.base_vertex && b1.instance_count + b2.instance_count < REN_MAX_BATCH_SIZE) {
                        memcpy(&b1.instance_indices[b1.instance_count], &b2.instance_indices[0], b2.instance_count * sizeof(int));
                        b1.instance_count += b2.instance_count;
                        b2.instance_count = 0;
                    }
                }

                start = end;
            }
        }

        if (sh_list.solid_batches_count == 0xffffffff) {
            sh_list.solid_batches_count = sh_list.shadow_batch_count;
        }
    }

    /**************************************************************************************************/
    /*                                    ASSIGNING TO CLUSTERS                                       */
    /**************************************************************************************************/

    uint64_t items_assignment_start = Sys::GetTimeUs();

    if (list.light_sources.count || list.decals.count || list.probes.count) {
        list.draw_cam.ExtractSubFrustums(REN_GRID_RES_X, REN_GRID_RES_Y, REN_GRID_RES_Z, temp_sub_frustums_.data.get());

        std::future<void> futures[REN_GRID_RES_Z];
        std::atomic_int a_items_count = {};

        for (int i = 0; i < REN_GRID_RES_Z; i++) {
            futures[i] = threads_->enqueue(GatherItemsForZSlice_Job, i, temp_sub_frustums_.data.get(), list.light_sources.data.get(), list.light_sources.count, list.decals.data.get(), list.decals.count, decals_boxes_.data.get(),
                                           list.probes.data.get(), list.probes.count, litem_to_lsource_.data.get(), list.cells.data.get(), list.items.data.get(), std::ref(a_items_count));
        }

        for (int i = 0; i < REN_GRID_RES_Z; i++) {
            futures[i].wait();
        }

        list.items.count = std::min(a_items_count.load(), REN_MAX_ITEMS_TOTAL);
    } else {
        CellData _dummy = {};
        std::fill(list.cells.data.get(), list.cells.data.get() + REN_CELLS_COUNT, _dummy);
        list.items.count = 0;
    }

    if ((list.render_flags & (EnableCulling | DebugCulling)) == (EnableCulling | DebugCulling)) {
        const float NEAR_CLIP = 0.5f;
        const float FAR_CLIP = 10000.0f;

        int w = cull_ctx_.zbuf.w, h = cull_ctx_.zbuf.h;
        depth_pixels_[1].resize(w * h * 4);
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                float z = cull_ctx_.zbuf.depth[(h - y - 1) * w + x];
                z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                depth_pixels_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                depth_pixels_[1][4 * (y * w + x) + 3] = 255;
            }
        }

        depth_tiles_[1].resize(w * h * 4);
        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                const auto *zr = swZbufGetTileRange(&cull_ctx_.zbuf, x, (h - y - 1));

                float z = zr->min;
                z = (2.0f * NEAR_CLIP) / (FAR_CLIP + NEAR_CLIP - z * (FAR_CLIP - NEAR_CLIP));
                depth_tiles_[1][4 * (y * w + x) + 0] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 1] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 2] = (uint8_t)(z * 255);
                depth_tiles_[1][4 * (y * w + x) + 3] = 255;
            }
        }
    }

    uint64_t iteration_end = Sys::GetTimeUs();

    if (list.render_flags & EnableTimers) {
        list.frontend_info.start_timepoint_us = iteration_start;
        list.frontend_info.end_timepoint_us = iteration_end;
        list.frontend_info.occluders_time_us = uint32_t(main_gather_start - occluders_start);
        list.frontend_info.main_gather_time_us = uint32_t(shadow_gather_start - main_gather_start);
        list.frontend_info.shadow_gather_time_us = uint32_t(drawables_sort_start - shadow_gather_start);
        list.frontend_info.drawables_sort_time_us = uint32_t(items_assignment_start - drawables_sort_start);
        list.frontend_info.items_assignment_time_us = uint32_t(iteration_end - items_assignment_start);
    }
}

void Renderer::__push_skeletal_mesh(uint32_t obj_index, const AnimState *as, const Ren::Mesh *mesh, DrawList &list) {
    const Ren::Skeleton *skel = mesh->skel();

    uint16_t palette_start = (uint16_t)list.skin_transforms.count;
    auto *matr_palette = &list.skin_transforms.data[list.skin_transforms.count];
    list.skin_transforms.count += (uint32_t)skel->bones.size();

    for (int i = 0; i < (int)skel->bones.size(); i++) {
        Ren::Mat4f matr_trans = Ren::Transpose(as->matr_palette[i]);
        memcpy(&matr_palette[i].matr[0][0], Ren::ValuePtr(matr_trans), 12 * sizeof(float));
    }

    const auto &buf = mesh->sk_attribs_buf();

    uint32_t vertex_beg = buf.offset / 48,
             vertex_end = (buf.offset + buf.size) / 48;

    proc_objects_[obj_index].base_vertex = (skinned_buf1_vtx_offset_ / 16) + list.skin_vertices_count;

    for (uint32_t i = vertex_beg; i < vertex_end; i += REN_SKIN_REGION_SIZE) {
        uint16_t count = (uint16_t)_MIN(vertex_end - i, REN_SKIN_REGION_SIZE);
        uint32_t out_offset = (skinned_buf1_vtx_offset_ / 16) + list.skin_vertices_count;
        list.skin_regions.data[list.skin_regions.count++] = { i, out_offset, palette_start, count };
        list.skin_vertices_count += count;
    }
}

void Renderer::GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                        const DecalItem *decals, int decals_count, const BBox *decals_boxes, const ProbeItem *probes, int probes_count,
                                        const LightSource * const*litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count) {
    using namespace RendererInternal;

    const float epsilon = 0.001f;

    const int frustums_per_slice = REN_GRID_RES_X * REN_GRID_RES_Y;
    const int base_index = slice * frustums_per_slice;
    const auto *first_sf = &sub_frustums[base_index];

    // Reset cells information for slice
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[base_index + s];
        cell = {};
    }

    // Gather to local list first
    ItemData local_items[REN_GRID_RES_X * REN_GRID_RES_Y][REN_MAX_ITEMS_PER_CELL];

    for (int j = 0; j < lights_count; j++) {
        const auto &l = lights[j];
        const float radius = litem_to_lsource[j]->radius;
        const float influence = litem_to_lsource[j]->influence;
        const float cap_radius = litem_to_lsource[j]->cap_radius;

        auto visible_to_slice = Ren::FullyVisible;

        // Check if light is inside of a whole z-slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            const float *p_n = ValuePtr(first_sf->planes[k].n);
            const float p_d = first_sf->planes[k].d;

            float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
            if (dist < -influence) {
                visible_to_slice = Ren::Invisible;
            } else if (l.spot > epsilon) {
                const float dn[3] = _CROSS(l.dir, p_n);
                const float m[3] = _CROSS(l.dir, dn);

                const float Q[3] = { l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                                     l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                                     l.pos[2] - influence * l.dir[2] - cap_radius * m[2] };

                if (dist < -radius && p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                    visible_to_slice = Ren::Invisible;
                }
            }
        }

        // Skip light for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += REN_GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if light is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                const float *p_n = ValuePtr(first_line_sf->planes[k].n);
                const float p_d = first_line_sf->planes[k].d;

                float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                if (dist < -influence) {
                    visible_to_line = Ren::Invisible;
                } else if (l.spot > epsilon) {
                    const float dn[3] = _CROSS(l.dir, p_n);
                    const float m[3] = _CROSS(l.dir, dn);

                    const float Q[3] = { l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                                         l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                                         l.pos[2] - influence * l.dir[2] - cap_radius * m[2] };

                    float val = p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d;

                    if (dist < -radius && p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                        visible_to_line = Ren::Invisible;
                    }
                }
            }

            // Skip light for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    const float *p_n = ValuePtr(sf->planes[k].n);
                    const float p_d = sf->planes[k].d;

                    float dist = p_n[0] * l.pos[0] + p_n[1] * l.pos[1] + p_n[2] * l.pos[2] + p_d;
                    if (dist < -influence) {
                        res = Ren::Invisible;
                    } else if (l.spot > epsilon) {
                        const float dn[3] = _CROSS(l.dir, p_n);
                        const float m[3] = _CROSS(l.dir, dn);

                        const float Q[3] = { l.pos[0] - influence * l.dir[0] - cap_radius * m[0],
                                             l.pos[1] - influence * l.dir[1] - cap_radius * m[1],
                                             l.pos[2] - influence * l.dir[2] - cap_radius * m[2] };

                        if (dist < -radius && p_n[0] * Q[0] + p_n[1] * Q[1] + p_n[2] * Q[2] + p_d < -epsilon) {
                            res = Ren::Invisible;
                        }
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.light_count < REN_MAX_LIGHTS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.light_count].light_index = (uint16_t)j;
                        cell.light_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < decals_count; j++) {
        const auto &de = decals[j];

        const float bbox_points[8][3] = { BBOX_POINTS(decals_boxes[j].bmin, decals_boxes[j].bmax) };

        auto visible_to_slice = Ren::FullyVisible;

        // Check if decal is inside of a whole slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            int in_count = 8;

            for (int i = 0; i < 8; i++) {
                float dist = first_sf->planes[k].n[0] * bbox_points[i][0] +
                             first_sf->planes[k].n[1] * bbox_points[i][1] +
                             first_sf->planes[k].n[2] * bbox_points[i][2] + first_sf->planes[k].d;
                if (dist < 0.0f) {
                    in_count--;
                }
            }

            if (in_count == 0) {
                visible_to_slice = Ren::Invisible;
                break;
            }
        }

        // Skip decal for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += REN_GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if decal is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                int in_count = 8;

                for (int i = 0; i < 8; i++) {
                    float dist = first_line_sf->planes[k].n[0] * bbox_points[i][0] +
                                 first_line_sf->planes[k].n[1] * bbox_points[i][1] +
                                 first_line_sf->planes[k].n[2] * bbox_points[i][2] + first_line_sf->planes[k].d;
                    if (dist < 0.0f) {
                        in_count--;
                    }
                }

                if (in_count == 0) {
                    visible_to_line = Ren::Invisible;
                    break;
                }
            }

            // Skip decal for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    int in_count = 8;

                    for (int i = 0; i < 8; i++) {
                        float dist = sf->planes[k].n[0] * bbox_points[i][0] +
                                     sf->planes[k].n[1] * bbox_points[i][1] +
                                     sf->planes[k].n[2] * bbox_points[i][2] + sf->planes[k].d;
                        if (dist < 0.0f) {
                            in_count--;
                        }
                    }

                    if (in_count == 0) {
                        res = Ren::Invisible;
                        break;
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.decal_count < REN_MAX_DECALS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.decal_count].decal_index = (uint16_t)j;
                        cell.decal_count++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < probes_count; j++) {
        const auto &p = probes[j];
        const float *p_pos = &p.position[0];

        auto visible_to_slice = Ren::FullyVisible;

        // Check if probe is inside of a whole slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            float dist = first_sf->planes[k].n[0] * p_pos[0] +
                         first_sf->planes[k].n[1] * p_pos[1] +
                         first_sf->planes[k].n[2] * p_pos[2] + first_sf->planes[k].d;
            if (dist < -p.radius) {
                visible_to_slice = Ren::Invisible;
            }
        }

        // Skip probe for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += REN_GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if probe is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                float dist = first_line_sf->planes[k].n[0] * p_pos[0] +
                             first_line_sf->planes[k].n[1] * p_pos[1] +
                             first_line_sf->planes[k].n[2] * p_pos[2] + first_line_sf->planes[k].d;
                if (dist < -p.radius) {
                    visible_to_line = Ren::Invisible;
                }
            }

            // Skip probe for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < REN_GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    float dist = sf->planes[k].n[0] * p_pos[0] +
                                 sf->planes[k].n[1] * p_pos[1] +
                                 sf->planes[k].n[2] * p_pos[2] + sf->planes[k].d;

                    if (dist < -p.radius) {
                        res = Ren::Invisible;
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.probe_count < REN_MAX_PROBES_PER_CELL) {
                        local_items[row_offset + col_offset][cell.probe_count].probe_index = (uint16_t)j;
                        cell.probe_count++;
                    }
                }
            }
        }
    }

    // Pack gathered local item data to total list
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[base_index + s];

        int local_items_count = (int)_MAX(cell.light_count, _MAX(cell.decal_count, cell.probe_count));

        if (local_items_count) {
            cell.item_offset = items_count.fetch_add(local_items_count);
            if (cell.item_offset > REN_MAX_ITEMS_TOTAL) {
                cell.item_offset = 0;
                cell.light_count = 0;
                cell.decal_count = 0;
                cell.probe_count = 0;
            } else {
                int free_items_left = REN_MAX_ITEMS_TOTAL - cell.item_offset;

                if ((int)cell.light_count > free_items_left) cell.light_count = free_items_left;
                if ((int)cell.decal_count > free_items_left) cell.decal_count = free_items_left;
                if ((int)cell.probe_count > free_items_left) cell.probe_count = free_items_left;

                memcpy(&items[cell.item_offset], &local_items[s][0], local_items_count * sizeof(ItemData));
            }
        }
    }
}

#undef BBOX_POINTS
#undef _MAX
#undef _CROSS