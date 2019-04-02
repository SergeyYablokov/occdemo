#include "Renderer.h"

#include <chrono>

#include <Ren/Context.h>
#include <Sys/Log.h>
#include <Sys/ThreadPool.h>

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

void Renderer::GatherDrawables(const SceneData &scene, const Ren::Camera &cam, DrawablesData &data) {
    using namespace RendererInternal;

    auto iteration_start = std::chrono::high_resolution_clock::now();

    data.draw_cam = cam;
    data.env = scene.env;
    data.decals_atlas = &scene.decals_atlas;
    data.render_flags = render_flags_;

    if (data.render_flags & DebugBVH) {
        // copy nodes list for debugging
        data.temp_nodes = scene.nodes;
        data.root_index = scene.root_node;
    } else {
        // free memory
        data.temp_nodes = {};
    }

    data.transforms.clear();
    data.transforms.reserve(scene.objects.size() * 6);

    data.draw_list.clear();
    data.draw_list.reserve(scene.objects.size() * 16);

    data.light_sources.clear();
    data.decals.clear();

    const bool culling_enabled = (data.render_flags & EnableCulling) != 0;

    object_to_drawable_.clear();
    object_to_drawable_.resize(scene.objects.size(), 0xffffffff);

    litem_to_lsource_.clear();
    ditem_to_decal_.clear();
    decals_boxes_.clear();

    for (int i = 0; i < 4; i++) {
        data.shadow_list[i].clear();
        data.shadow_list[i].reserve(scene.objects.size() * 16);
    }

    Ren::Mat4f view_from_world = data.draw_cam.view_matrix(),
               clip_from_view = data.draw_cam.proj_matrix();

    swCullCtxClear(&cull_ctx_);

    Ren::Mat4f view_from_identity = view_from_world * Ren::Mat4f{ 1.0f },
               clip_from_identity = clip_from_view * view_from_identity;

    data.transforms.push_back(view_from_identity);
    data.transforms.push_back(clip_from_view);

    const uint32_t skip_check_bit = (1u << 31);
    const uint32_t index_bits = ~skip_check_bit;

    uint32_t stack[MAX_STACK_SIZE];
    uint32_t stack_size = 0;

    auto occluders_start = std::chrono::high_resolution_clock::now();

    {   // Rasterize occluder meshes into a small framebuffer
        stack[stack_size++] = (uint32_t)scene.root_node;

        while (stack_size && culling_enabled) {
            uint32_t cur = stack[--stack_size] & index_bits;
            uint32_t skip_check = (stack[stack_size] & skip_check_bit);
            const auto *n = &scene.nodes[cur];

            if (!skip_check) {
                auto res = data.draw_cam.CheckFrustumVisibility(n->bbox_min, n->bbox_max);
                if (res == Ren::Invisible) continue;
                else if (res == Ren::FullyVisible) skip_check = skip_check_bit;
            }

            if (!n->prim_count) {
                stack[stack_size++] = skip_check | n->left_child;
                stack[stack_size++] = skip_check | n->right_child;
            } else {
                const auto &obj = scene.objects[n->prim_index];

                const uint32_t occluder_flags = HasTransform | HasOccluder;
                if ((obj.comp_mask & occluder_flags) == occluder_flags) {
                    const auto *tr = obj.tr.get();

                    // Node has slightly enlarged bounds, so we need to check object's bounding box here
                    if (!skip_check &&
                        data.draw_cam.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                    const Ren::Mat4f &world_from_object = tr->mat;

                    const Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                        clip_from_object = clip_from_view * view_from_object;

                    const auto *mesh = obj.mesh.get();

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
                        _surf->stride = 13 * sizeof(float);
                        _surf->count = (SWuint)s->num_indices;
                        _surf->base_vertex = -SWint(mesh->attribs_offset() / _surf->stride);
                        _surf->xform = Ren::ValuePtr(clip_from_object);
                        _surf->dont_skip = nullptr;

                        ++s;
                    }

                    swCullCtxSubmitCullSurfs(&cull_ctx_, surf, surf_count);
                }
            }
        }
    }

    auto main_gather_start = std::chrono::high_resolution_clock::now();

    {   // Gather meshes and lights, skip occluded and frustum culled
        stack_size = 0;
        stack[stack_size++] = (uint32_t)scene.root_node;

        while (stack_size) {
            uint32_t cur = stack[--stack_size] & index_bits;
            uint32_t skip_check = stack[stack_size] & skip_check_bit;
            const auto *n = &scene.nodes[cur];

            if (!skip_check) {
                const float bbox_points[8][3] = { BBOX_POINTS(n->bbox_min, n->bbox_max) };
                auto res = data.draw_cam.CheckFrustumVisibility(bbox_points);
                if (res == Ren::Invisible) continue;
                else if (res == Ren::FullyVisible) skip_check = skip_check_bit;

                if (culling_enabled) {
                    const auto &cam_pos = data.draw_cam.world_position();

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

                const uint32_t drawable_flags = HasMesh | HasTransform;
                const uint32_t lightsource_flags = HasLightSource | HasTransform;
                if ((obj.comp_mask & drawable_flags) == drawable_flags ||
                    (obj.comp_mask & lightsource_flags) == lightsource_flags) {
                    const auto *tr = obj.tr.get();

                    if (!skip_check) {
                        const float bbox_points[8][3] = { BBOX_POINTS(tr->bbox_min_ws, tr->bbox_max_ws) };

                        // Node has slightly enlarged bounds, so we need to check object's bounding box here
                        if (data.draw_cam.CheckFrustumVisibility(bbox_points) == Ren::Invisible) continue;

                        if (culling_enabled) {
                            const auto &cam_pos = data.draw_cam.world_position();

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

                    const Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                        clip_from_object = clip_from_view * view_from_object;

                    if (obj.comp_mask & HasMesh) {
                        data.transforms.push_back(clip_from_object);

                        const auto *mesh = obj.mesh.get();

                        size_t dr_start = data.draw_list.size();

                        object_to_drawable_[n->prim_index] = (uint32_t)data.draw_list.size();

                        const Ren::TriGroup *s = &mesh->group(0);
                        while (s->offset != -1) {
                            data.draw_list.push_back({ &data.transforms.back(), &world_from_object, s->mat.get(), mesh, s });

                            if (obj.comp_mask & HasLightmap) {
                                data.draw_list.back().lm_transform = obj.lm->xform;
                            }

                            ++s;
                        }
                    }

                    if (obj.comp_mask & HasLightSource) {
                        for (int li = 0; li < LIGHTS_PER_OBJECT; li++) {
                            if (!obj.ls[li]) break;

                            const auto *light = obj.ls[li].get();

                            Ren::Vec4f pos = { light->offset[0], light->offset[1], light->offset[2], 1.0f };
                            pos = world_from_object * pos;
                            pos /= pos[3];

                            Ren::Vec4f dir = { -light->dir[0], -light->dir[1], -light->dir[2], 0.0f };
                            dir = world_from_object * dir;

                                        
                            if (!skip_check) {
                                auto res = Ren::FullyVisible;

                                for (int k = 0; k < 6; k++) {
                                    const auto &plane = data.draw_cam.frustum_plane(k);

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

                                if (res == Ren::Invisible) continue;
                            }

                            data.light_sources.emplace_back();
                            litem_to_lsource_.push_back(light);

                            auto &ls = data.light_sources.back();

                            memcpy(&ls.pos[0], &pos[0], 3 * sizeof(float));
                            ls.radius = light->radius;
                            memcpy(&ls.col[0], &light->col[0], 3 * sizeof(float));
                            ls.brightness = light->brightness;
                            memcpy(&ls.dir[0], &dir[0], 3 * sizeof(float));
                            ls.spot = light->spot;
                        }
                    }

                    if (obj.comp_mask & HasDecal) {
                        Ren::Mat4f object_from_world = Ren::Inverse(world_from_object);

                        for (int di = 0; di < DECALS_PER_OBJECT; di++) {
                            if (!obj.de[di]) break;

                            const auto *decal = obj.de[di].get();

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
                                    const auto &plane = data.draw_cam.frustum_plane(p);

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
                                data.decals.emplace_back();
                                ditem_to_decal_.push_back(decal);
                                decals_boxes_.push_back({ bbox_min, bbox_max });

                                Ren::Mat4f clip_from_world_transposed = Ren::Transpose(clip_from_world);

                                auto &de = data.decals.back();
                                memcpy(&de.mat[0][0], &clip_from_world_transposed[0][0], 12 * sizeof(float));
                                memcpy(&de.diff[0], &decal->diff[0], 4 * sizeof(float));
                                memcpy(&de.norm[0], &decal->norm[0], 4 * sizeof(float));
                                memcpy(&de.spec[0], &decal->spec[0], 4 * sizeof(float));
                            }
                        }
                    }
                }
            }
        }
    }

    auto shadow_gather_start = std::chrono::high_resolution_clock::now();

    if (Ren::Length2(data.env.sun_dir) > 0.9f && Ren::Length2(data.env.sun_col) > FLT_EPSILON) {
        // Planes, that define shadow map splits
        const float far_planes[] = { float(REN_SHAD_CASCADE0_DIST), float(REN_SHAD_CASCADE1_DIST),
                                     float(REN_SHAD_CASCADE2_DIST), float(REN_SHAD_CASCADE3_DIST) };
        const float near_planes[] = { data.draw_cam.near(), far_planes[0], far_planes[1], far_planes[2] };

        // Choose up vector for shadow camera
        auto light_dir = data.env.sun_dir;
        auto cam_up = Ren::Vec3f{ 0.0f, 0.0, 1.0f };
        if (light_dir[0] <= light_dir[1] && light_dir[0] <= light_dir[2]) {
            cam_up = Ren::Vec3f{ 1.0f, 0.0, 0.0f };
        }
        else if (light_dir[1] <= light_dir[0] && light_dir[1] <= light_dir[2]) {
            cam_up = Ren::Vec3f{ 0.0f, 1.0, 0.0f };
        }
        // Calculate side vector of shadow camera
        auto cam_side = Normalize(Cross(light_dir, cam_up));
        cam_up = Cross(cam_side, light_dir);

        const Ren::Vec3f scene_dims = scene.nodes[scene.root_node].bbox_max - scene.nodes[scene.root_node].bbox_min;
        const float max_dist = Ren::Length(scene_dims);

        // Gather drawables for each cascade
        for (int casc = 0; casc < 4; casc++) {
            auto& shadow_cam = data.shadow_cams[casc];

            auto temp_cam = data.draw_cam;
            temp_cam.Perspective(data.draw_cam.angle(), data.draw_cam.aspect(), near_planes[casc], far_planes[casc]);
            temp_cam.UpdatePlanes();

            const Ren::Mat4f& _view_from_world = temp_cam.view_matrix(),
                & _clip_from_view = temp_cam.proj_matrix();

            const Ren::Mat4f _clip_from_world = _clip_from_view * _view_from_world;
            const Ren::Mat4f _world_from_clip = Ren::Inverse(_clip_from_world);

            Ren::Vec3f bounding_center;
            const float bounding_radius = temp_cam.GetBoundingSphere(bounding_center);

            auto cam_target = bounding_center;

            {
                // Snap camera movement to shadow map pixels
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
            }

            auto cam_center = cam_target + max_dist * light_dir;

            shadow_cam.SetupView(cam_center, cam_target, cam_up);
            shadow_cam.Orthographic(-bounding_radius, bounding_radius, bounding_radius, -bounding_radius, 0.0f, max_dist + bounding_radius);
            shadow_cam.UpdatePlanes();

            view_from_world = shadow_cam.view_matrix(),
            clip_from_view = shadow_cam.proj_matrix();

            const uint32_t skip_check_bit = (1u << 31);
            const uint32_t index_bits = ~skip_check_bit;

            stack_size = 0;
            stack[stack_size++] = (uint32_t)scene.root_node;

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
                }
                else {
                    const auto& obj = scene.objects[n->prim_index];

                    const uint32_t drawable_flags = HasMesh | HasTransform;
                    if ((obj.comp_mask & drawable_flags) == drawable_flags) {
                        const auto* tr = obj.tr.get();

                        if (!skip_check &&
                            shadow_cam.CheckFrustumVisibility(tr->bbox_min_ws, tr->bbox_max_ws) == Ren::Invisible) continue;

                        const Ren::Mat4f & world_from_object = tr->mat;

                        Ren::Mat4f view_from_object = view_from_world * world_from_object,
                                    clip_from_object = clip_from_view * view_from_object;

                        data.transforms.push_back(clip_from_object);

                        auto dr_index = object_to_drawable_[n->prim_index];
                        if (dr_index != 0xffffffff) {
                            auto* dr = &data.draw_list[dr_index];
                            const auto* mesh = dr->mesh;

                            const Ren::TriGroup* s = &mesh->group(0);
                            while (s->offset != -1) {
                                dr->sh_clip_from_object[casc] = &data.transforms.back();
                                ++dr;
                                ++s;
                            }
                        }

                        const auto* mesh = obj.mesh.get();

                        const Ren::TriGroup* s = &mesh->group(0);
                        while (s->offset != -1) {
                            data.shadow_list[casc].push_back({ &data.transforms.back(), nullptr, s->mat.get(), mesh, s });
                            ++s;
                        }
                    }
                }
            }
        }
    }

    auto drawables_sort_start = std::chrono::high_resolution_clock::now();

    // Sort drawables to optimize state switches
    std::sort(std::begin(data.draw_list), std::end(data.draw_list));

    auto items_assignment_start = std::chrono::high_resolution_clock::now();

    if (!data.light_sources.empty() || !data.decals.empty()) {
        std::vector<Ren::Frustum> sub_frustums;
        sub_frustums.resize(CELLS_COUNT);

        data.draw_cam.ExtractSubFrustums(GRID_RES_X, GRID_RES_Y, GRID_RES_Z, &sub_frustums[0]);

        const int lights_count = (int)data.light_sources.size();
        const auto *lights = lights_count ? &data.light_sources[0] : nullptr;
        const auto *litem_to_lsource = lights_count ? &litem_to_lsource_[0] : nullptr;

        const int decals_count = (int)data.decals.size();
        const auto *decals = decals_count ? &data.decals[0] : nullptr;
        const auto *decals_boxes = decals_count ? &decals_boxes_[0] : nullptr;

        std::vector<std::future<void>> futures;
        std::atomic_int a_items_count = {};

        for (int i = 0; i < GRID_RES_Z; i++) {
            futures.push_back(
                threads_->enqueue(GatherItemsForZSlice_Job, i, &sub_frustums[0], lights, lights_count, decals, decals_count, decals_boxes, litem_to_lsource, &data.cells[0], &data.items[0], std::ref(a_items_count))
            );
        }

        for (int i = 0; i < GRID_RES_Z; i++) {
            futures[i].wait();
        }

        data.items_count = std::min(a_items_count.load(), MAX_ITEMS_TOTAL);
    }

    if ((data.render_flags & (EnableCulling | DebugCulling)) == (EnableCulling | DebugCulling)) {
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

    auto iteration_end = std::chrono::high_resolution_clock::now();

    data.frontend_info.start_timepoint_us = (uint64_t)std::chrono::duration<double, std::micro>{ iteration_start.time_since_epoch() }.count();
    data.frontend_info.end_timepoint_us = (uint64_t)std::chrono::duration<double, std::micro>{ iteration_end.time_since_epoch() }.count();
    data.frontend_info.occluders_time_us = (uint32_t)std::chrono::duration<double, std::micro>{ main_gather_start - occluders_start }.count();
    data.frontend_info.main_gather_time_us = (uint32_t)std::chrono::duration<double, std::micro>{ shadow_gather_start - main_gather_start }.count();
    data.frontend_info.shadow_gather_time_us = (uint32_t)std::chrono::duration<double, std::micro>{ items_assignment_start - shadow_gather_start }.count();
    data.frontend_info.items_assignment_time_us = (uint32_t)std::chrono::duration<double, std::micro>{ iteration_end - items_assignment_start }.count();
}

void Renderer::GatherItemsForZSlice_Job(int slice, const Ren::Frustum *sub_frustums, const LightSourceItem *lights, int lights_count,
                                        const DecalItem *decals, int decals_count, const BBox *decals_boxes,
                                        const LightSource * const*litem_to_lsource, CellData *cells, ItemData *items, std::atomic_int &items_count) {
    using namespace RendererInternal;

    const int frustums_per_slice = GRID_RES_X * GRID_RES_Y;
    const int base_index = slice * frustums_per_slice;
    const auto *first_sf = &sub_frustums[base_index];

    // Reset cells information for slice
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[base_index + s];
        cell = {};
    }

    // Gather to local list first
    ItemData local_items[GRID_RES_X * GRID_RES_Y][MAX_ITEMS_PER_CELL];

    for (int j = 0; j < lights_count; j++) {
        const auto &l = lights[j];
        const float influence = litem_to_lsource[j]->influence;
        const float *l_pos = &l.pos[0];

        auto visible_to_slice = Ren::FullyVisible;

        // Check if light is inside of a whole slice
        for (int k = Ren::NearPlane; k <= Ren::FarPlane; k++) {
            float dist = first_sf->planes[k].n[0] * l_pos[0] +
                first_sf->planes[k].n[1] * l_pos[1] +
                first_sf->planes[k].n[2] * l_pos[2] + first_sf->planes[k].d;
            if (dist < -influence) {
                visible_to_slice = Ren::Invisible;
            }
        }

        // Skip light for whole slice
        if (visible_to_slice == Ren::Invisible) continue;

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += GRID_RES_X) {
            const auto *first_line_sf = first_sf + row_offset;

            auto visible_to_line = Ren::FullyVisible;

            // Check if light is inside of grid line
            for (int k = Ren::TopPlane; k <= Ren::BottomPlane; k++) {
                float dist = first_line_sf->planes[k].n[0] * l_pos[0] +
                    first_line_sf->planes[k].n[1] * l_pos[1] +
                    first_line_sf->planes[k].n[2] * l_pos[2] + first_line_sf->planes[k].d;
                if (dist < -influence) {
                    visible_to_line = Ren::Invisible;
                }
            }

            // Skip light for whole line
            if (visible_to_line == Ren::Invisible) continue;

            for (int col_offset = 0; col_offset < GRID_RES_X; col_offset++) {
                const auto *sf = first_line_sf + col_offset;

                auto res = Ren::FullyVisible;

                // Can skip near, far, top and bottom plane check
                for (int k = Ren::LeftPlane; k <= Ren::RightPlane; k++) {
                    float dist = sf->planes[k].n[0] * l_pos[0] +
                        sf->planes[k].n[1] * l_pos[1] +
                        sf->planes[k].n[2] * l_pos[2] + sf->planes[k].d;

                    if (dist < -influence) {
                        res = Ren::Invisible;
                    }
                }

                if (res != Ren::Invisible) {
                    const int index = base_index + row_offset + col_offset;
                    auto &cell = cells[index];
                    if (cell.light_count < MAX_LIGHTS_PER_CELL) {
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

        for (int row_offset = 0; row_offset < frustums_per_slice; row_offset += GRID_RES_X) {
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

            for (int col_offset = 0; col_offset < GRID_RES_X; col_offset++) {
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
                    if (cell.decal_count < MAX_DECALS_PER_CELL) {
                        local_items[row_offset + col_offset][cell.decal_count].decal_index = (uint16_t)j;
                        cell.decal_count++;
                    }
                }
            }
        }
    }

    // Pack gathered local item data to total list
    for (int s = 0; s < frustums_per_slice; s++) {
        auto &cell = cells[base_index + s];

        int local_items_count = std::max((int)cell.light_count, (int)cell.decal_count);

        if (local_items_count) {
            cell.item_offset = items_count.fetch_add(local_items_count);
            if (cell.item_offset > MAX_ITEMS_TOTAL) {
                cell.item_offset = 0;
                cell.light_count = 0;
                cell.decal_count = 0;
            } else {
                int free_items_left = MAX_ITEMS_TOTAL - cell.item_offset;

                if ((int)cell.light_count > free_items_left) cell.light_count = free_items_left;
                if ((int)cell.decal_count > free_items_left) cell.decal_count = free_items_left;

                memcpy(&items[cell.item_offset], &local_items[s][0], local_items_count * sizeof(ItemData));
            }
        }
    }
}

#undef BBOX_POINTS