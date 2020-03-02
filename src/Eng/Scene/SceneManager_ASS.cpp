#include "SceneManager.h"

#include <fstream>
#include <functional>
#include <iterator>
#include <numeric>
#include <regex>

#include <dirent.h>

#ifdef __linux__
#include <sys/stat.h>
#endif

extern "C" {
#include <Ren/SOIL2/image_DXT.h>
}
#include <Ren/SOIL2/SOIL2.h>

#undef max
#undef min

#include <Eng/Gui/Renderer.h>
#include <Eng/Gui/Utils.h>
#include <Ray/internal/TextureSplitter.h>
#include <Ren/Utils.h>
#include <Sys/AssetFile.h>
#include <Sys/MonoAlloc.h>
#include <Sys/ThreadPool.h>

// TODO: pass defines as a parameter
#include "../Renderer/Renderer_GL_Defines.inl"

namespace SceneManagerInternal {
extern const char *MODELS_PATH;
extern const char *TEXTURES_PATH;
extern const char *MATERIALS_PATH;
extern const char *SHADERS_PATH;

void LoadTGA(Sys::AssetFile &in_file, int w, int h, Ray::pixel_color8_t *out_data) {
    auto in_file_size = (size_t)in_file.size();

    std::vector<char> in_file_data(in_file_size);
    in_file.Read(&in_file_data[0], in_file_size);

    Ren::eTexFormat format;
    int _w, _h;
    std::unique_ptr<uint8_t[]> pixels = Ren::ReadTGAFile(&in_file_data[0], _w, _h, format);

    if (_w != w || _h != h) return;

    if (format == Ren::eTexFormat::RawRGB888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = { pixels[3 * (y * w + x)], pixels[3 * (y * w + x) + 1], pixels[3 * (y * w + x) + 2], 255 };
            }
        }
    } else if (format == Ren::eTexFormat::RawRGBA8888) {
        int i = 0;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                out_data[i++] = { pixels[4 * (y * w + x)], pixels[4 * (y * w + x) + 1], pixels[4 * (y * w + x) + 2], pixels[4 * (y * w + x) + 3] };
            }
        }
    } else {
        assert(false);
    }
}

std::vector<Ray::pixel_color_t> FlushSeams(const Ray::pixel_color_t *pixels, int width, int height, float invalid_threshold, int filter_size) {
    std::vector<Ray::pixel_color_t> temp_pixels1{ pixels, pixels + width * height },
                                    temp_pixels2{ (size_t)width * height };

    // Avoid bound checks in debug
    Ray::pixel_color_t *_temp_pixels1 = temp_pixels1.data(),
                       *_temp_pixels2 = temp_pixels2.data();

    // apply dilation filter
    for (int i = 0; i < filter_size; i++) {
        bool has_invalid = false;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                Ray::pixel_color_t in_p = _temp_pixels1[y * width + x];
                Ray::pixel_color_t &out_p = _temp_pixels2[y * width + x];

                float mul = 1.0f;
                if (in_p.a < invalid_threshold) {
                    has_invalid = true;

                    Ray::pixel_color_t new_p = { 0 };
                    int count = 0;

                    int _ys[] = { y - 1, y, y + 1 },
                        _xs[] = { x - 1, x, x + 1 };
                    for (int _y : _ys) {
                        if (_y < 0 || _y > height - 1) continue;

                        for (int _x : _xs) {
                            if (_x < 0 || _x > width - 1) continue;

                            const Ray::pixel_color_t &p = _temp_pixels1[_y * width + _x];
                            if (p.a >= invalid_threshold) {
                                new_p.r += p.r;
                                new_p.g += p.g;
                                new_p.b += p.b;
                                new_p.a += p.a;

                                count++;
                            }
                        }
                    }

                    if (count) {
                        const float inv_c = 1.0f / float(count);
                        new_p.r *= inv_c;
                        new_p.g *= inv_c;
                        new_p.b *= inv_c;
                        new_p.a *= inv_c;

                        in_p = new_p;
                    }
                } else {
                    mul = 1.0f / in_p.a;
                }

                out_p.r = in_p.r * mul;
                out_p.g = in_p.g * mul;
                out_p.b = in_p.b * mul;
                out_p.a = in_p.a * mul;
            }
        }

        std::swap(_temp_pixels1, _temp_pixels2);
        if (!has_invalid) break;
    }

    return temp_pixels1;
}

std::unique_ptr<Ray::pixel_color8_t[]> GetTextureData(const Ren::Texture2DRef &tex_ref) {
    const Ren::Texture2DParams &params = tex_ref->params();

    std::unique_ptr<Ray::pixel_color8_t[]> tex_data(new Ray::pixel_color8_t[params.w * params.h]);
#if defined(__ANDROID__)
    Sys::AssetFile in_file((std::string("assets/textures/") + tex_ref->name().c_str()).c_str());
    SceneManagerInternal::LoadTGA(in_file, params.w, params.h, &tex_data[0]);
#else
    tex_ref->ReadTextureData(Ren::eTexFormat::RawRGBA8888, (void *)&tex_data[0]);
#endif

    return tex_data;
}

void ReadAllFiles_r(assets_context_t &ctx, const char *in_folder, const std::function<void(assets_context_t &ctx, const char *)> &callback) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        ctx.log->Error("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while ((in_ent = readdir(in_dir))) {
        if (in_ent->d_type == DT_DIR) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_r(ctx, path.c_str(), callback);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            callback(ctx, path.c_str());
        }
    }

    closedir(in_dir);
}

void ReadAllFiles_MT_r(assets_context_t &ctx, const char *in_folder, const std::function<void(assets_context_t &ctx, const char *)> &callback, Sys::ThreadPool *threads, std::vector<std::future<void>> &events) {
    DIR *in_dir = opendir(in_folder);
    if (!in_dir) {
        ctx.log->Error("Cannot open folder %s", in_folder);
        return;
    }

    struct dirent *in_ent = nullptr;
    while ((in_ent = readdir(in_dir))) {
        if (in_ent->d_type == DT_DIR) {
            if (strcmp(in_ent->d_name, ".") == 0 || strcmp(in_ent->d_name, "..") == 0) {
                continue;
            }
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            ReadAllFiles_r(ctx, path.c_str(), callback);
        } else {
            std::string path = in_folder;
            path += '/';
            path += in_ent->d_name;

            events.push_back(threads->enqueue([path, &ctx, &callback]() {
                callback(ctx, path.c_str());
            }));
        }
    }

    closedir(in_dir);
}

bool CheckCanSkipAsset(const char *in_file, const char *out_file, Ren::ILog *log) {
#if !defined(NDEBUG) && 0
    log->Info("Warning: glsl is forced to be not skipped!");
    if (strstr(in_file, ".glsl")) return false;
#endif

#ifdef _WIN32
    HANDLE in_h = CreateFile(in_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
    if (in_h == INVALID_HANDLE_VALUE) {
        log->Info("[PrepareAssets] Failed to open file %s", in_file);
        CloseHandle(in_h);
        return true;
    }
    HANDLE out_h = CreateFile(out_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, NULL, NULL);
    LARGE_INTEGER out_size = {};
    if (out_h != INVALID_HANDLE_VALUE && GetFileSizeEx(out_h, &out_size) && out_size.QuadPart) {
        FILETIME in_t, out_t;
        GetFileTime(in_h, NULL, NULL, &in_t);
        GetFileTime(out_h, NULL, NULL, &out_t);

        if (CompareFileTime(&in_t, &out_t) == -1) {
            CloseHandle(in_h);
            CloseHandle(out_h);
            return true;
        }
    }

    CloseHandle(in_h);
    CloseHandle(out_h);
#else
    struct stat st1 = {}, st2 = {};
    const int
        res1 = stat(in_file, &st1),
        res2 = stat(out_file, &st2);
    if (res1 != -1 && res2 != -1) {
        struct tm tm1 = {}, tm2 = {};
        localtime_r(&st1.st_ctime, &tm1);
        localtime_r(&st2.st_ctime, &tm2);
        
        time_t t1 = mktime(&tm1), t2 = mktime(&tm2);

        double diff_s = difftime(t1, t2);
        if (diff_s < 0) {
            return true;
        }
    } else if (res1 == -1) {
        log->Info("[PrepareAssets] Failed to open input file %s!", in_file);
    }
    
#endif
    return false;
}

bool CreateFolders(const char *out_file, Ren::ILog *log) {
    const char *end = strchr(out_file, '/');
    while (end) {
        char folder[256] = {};
        strncpy(folder, out_file, end - out_file + 1);
#ifdef _WIN32
        if (!CreateDirectory(folder, NULL)) {
            if (GetLastError() != ERROR_ALREADY_EXISTS) {
                log->Error("[PrepareAssets] Failed to create directory!");
                return false;
            }
        }
#else
        struct stat st = {};
        if (stat(folder, &st) == -1) {
            if (mkdir(folder, 0777) != 0) {
                log->Error("[PrepareAssets] Failed to create directory!");
                return false;
            }
        }
#endif
        end = strchr(end + 1, '/');
    }
    return true;
}

void ReplaceTextureExtension(const char *platform, std::string &tex) {
    size_t n;
    if ((n = tex.find(".uncompressed")) == std::string::npos) {
        if ((n = tex.find(".tga")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, 3, "ktx");
            }
        } else if ((n = tex.find(".png")) != std::string::npos ||
                   (n = tex.find(".img")) != std::string::npos) {
            if (strcmp(platform, "pc") == 0) {
                tex.replace(n + 1, 3, "dds");
            } else if (strcmp(platform, "android") == 0) {
                tex.replace(n + 1, 3, "ktx");
            }
        }
    }
}

std::string ExtractHTMLData(assets_context_t &ctx, const char *in_file, std::string &out_caption) {
    std::ifstream src_stream(in_file, std::ios::binary | std::ios::ate);
    int file_size = (int)src_stream.tellg();
    src_stream.seekg(0, std::ios::beg);

    // TODO: buffered read?
    std::unique_ptr<char[]> in_buf(new char[file_size]);
    src_stream.read(&in_buf[0], file_size);

    std::string out_str;
    out_str.reserve(file_size);

    bool body_active = false, header_active = false;
    bool p_active = false;

    int buf_pos = 0;
    while (buf_pos < file_size) {
        int start_pos = buf_pos;

        uint32_t unicode;
        buf_pos += Gui::ConvChar_UTF8_to_Unicode(&in_buf[buf_pos], unicode);

        if (unicode == Gui::g_unicode_less_than) {
            char tag_str[32];
            int tag_str_len = 0;

            while (unicode != Gui::g_unicode_greater_than) {
                buf_pos += Gui::ConvChar_UTF8_to_Unicode(&in_buf[buf_pos], unicode);
                tag_str[tag_str_len++] = (char)unicode;
            }
            tag_str[tag_str_len - 1] = '\0';

            if (strcmp(tag_str, "body") == 0) {
                body_active = true;
                continue;
            } else if (strcmp(tag_str, "/body") == 0) {
                body_active = false;
                continue;
            } else if (strcmp(tag_str, "header") == 0) {
                header_active = true;
                continue;
            } else if (strcmp(tag_str, "p") == 0) {
                p_active = true;
            } else if (strcmp(tag_str, "/p") == 0) {
                out_str += "</p>";
                p_active = false;
                continue;
            } else if (strcmp(tag_str, "/header") == 0) {
                header_active = false;
                continue;
            }
        }

        if (body_active) {
            if (p_active) {
                out_str.append(&in_buf[start_pos], buf_pos - start_pos);
            } else if (header_active) {
                out_caption.append(&in_buf[start_pos], buf_pos - start_pos);
            }
        }
    }

    return out_str;
}

}

Ren::HashMap32<std::string, SceneManager::Handler> SceneManager::g_asset_handlers;

bool g_astc_initialized = false;

void SceneManager::RegisterAsset(const char *in_ext, const char *out_ext, const ConvertAssetFunc &convert_func) {
    g_asset_handlers[in_ext] = { out_ext, convert_func };
}

bool SceneManager::PrepareAssets(const char *in_folder, const char *out_folder, const char *platform, Sys::ThreadPool *p_threads, Ren::ILog *log) {
    using namespace SceneManagerInternal;

    // for astc codec
    if (!g_astc_initialized) {
        InitASTCCodec();
        g_astc_initialized = true;
    }

    WriteCommonShaderIncludes(in_folder);

    g_asset_handlers["bff"]         = { "bff",          HCopy               };
    g_asset_handlers["mesh"]        = { "mesh",         HCopy               };
    g_asset_handlers["anim"]        = { "anim",         HCopy               };
    g_asset_handlers["vert.glsl"]   = { "vert.glsl",    HPreprocessShader   };
    g_asset_handlers["frag.glsl"]   = { "frag.glsl",    HPreprocessShader   };
    g_asset_handlers["comp.glsl"]   = { "comp.glsl",    HPreprocessShader   };
    g_asset_handlers["ttf"]         = { "font",         HConvTTFToFont      };

    if (strcmp(platform, "pc") == 0) {
        g_asset_handlers["json"]    = { "json",         HPreprocessJson     };
        g_asset_handlers["txt"]     = { "txt",          HPreprocessMaterial };
        g_asset_handlers["tga"]     = { "dds",          HConvToDDS          };
        g_asset_handlers["hdr"]     = { "dds",          HConvHDRToRGBM      };
        g_asset_handlers["png"]     = { "dds",          HConvToDDS          };
        g_asset_handlers["img"]     = { "dds",          HConvImgToDDS       };
    } else if (strcmp(platform, "android") == 0) {
        g_asset_handlers["json"]    = { "json",         HPreprocessJson     };
        g_asset_handlers["txt"]     = { "txt",          HPreprocessMaterial };
        g_asset_handlers["tga"]     = { "ktx",          HConvToASTC         };
        g_asset_handlers["hdr"]     = { "ktx",          HConvHDRToRGBM      };
        g_asset_handlers["png"]     = { "ktx",          HConvToASTC         };
        g_asset_handlers["img"]     = { "ktx",          HConvImgToASTC      };
    }

    g_asset_handlers["uncompressed.tga"] = { "uncompressed.tga",  HCopy };
    g_asset_handlers["uncompressed.png"] = { "uncompressed.png",  HCopy };

    auto convert_file = [out_folder](assets_context_t &ctx, const char *in_file) {
        const char *base_path = strchr(in_file, '/');
        if (!base_path) return;
        const char *ext = strchr(in_file, '.');
        if (!ext) return;

        ext++;

        Handler *handler = g_asset_handlers.Find(ext);
        if (!handler) {
            ctx.log->Info("[PrepareAssets] No handler found for %s", in_file);
            return;
        }

        const std::string out_file =
            out_folder +
            std::string(base_path, strlen(base_path) - strlen(ext)) +
            handler->ext;

        if (CheckCanSkipAsset(in_file, out_file.c_str(), ctx.log)) {
            return;
        }

        if (!CreateFolders(out_file.c_str(), ctx.log)) {
            ctx.log->Info("[PrepareAssets] Failed to create directories for %s", out_file.c_str());
            return;
        }

        const auto &conv_func = handler->convert;
        conv_func(ctx, in_file, out_file.c_str());
    };

#ifdef __linux__
    if (system("chmod +x src/libs/spirv/glslangValidator") ||
        system("chmod +x src/libs/spirv/spirv-opt") ||
        system("chmod +x src/libs/spirv/spirv-cross")) {
        log->Info("[PrepareAssets] Failed to chmod executables!");
    }
#endif

    assets_context_t ctx = {
        platform,
        log
    };

    /*if (p_threads) {
        std::vector<std::future<void>> events;
        ReadAllFiles_MT_r(ctx, in_folder, convert_file, p_threads, events);

        for (std::future<void> &e : events) {
            e.wait();
        }
    } else {*/
        ReadAllFiles_r(ctx, in_folder, convert_file);
    //}

    return true;
}

void SceneManager::HSkip(assets_context_t &ctx, const char *in_file, const char *out_file) {
    ctx.log->Info("[PrepareAssets] Skip %s", out_file);
}

void SceneManager::HCopy(assets_context_t &ctx, const char *in_file, const char *out_file) {
    ctx.log->Info("[PrepareAssets] Copy %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    std::ofstream dst_stream(out_file, std::ios::binary);

    std::istreambuf_iterator<char> src_beg(src_stream);
    std::istreambuf_iterator<char> src_end;
    std::ostreambuf_iterator<char> dst_beg(dst_stream);
    std::copy(src_beg, src_end, dst_beg);
}

void SceneManager::HPreprocessMaterial(assets_context_t &ctx, const char *in_file, const char *out_file) {
    ctx.log->Info("[PrepareAssets] Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    std::ofstream dst_stream(out_file, std::ios::binary);

    std::string line;
    while (std::getline(src_stream, line)) {
        SceneManagerInternal::ReplaceTextureExtension(ctx.platform, line);
        dst_stream << line << "\r\n";
    }
}

void SceneManager::HPreprocessJson(assets_context_t &ctx, const char *in_file, const char *out_file) {
    using namespace SceneManagerInternal;

    ctx.log->Info("[PrepareAssets] Prep %s", out_file);

    std::ifstream src_stream(in_file, std::ios::binary);
    std::ofstream dst_stream(out_file, std::ios::binary);

    JsObject js_root;
    if (!js_root.Read(src_stream)) {
        throw std::runtime_error("Cannot load scene!");
    }

    std::string base_path = in_file;
    {   // extract base part of file path
        const size_t n = base_path.find_last_of('/');
        if (n != std::string::npos) {
            base_path = base_path.substr(0, n + 1);
        }
    }

    if (js_root.Has("objects")) {
        JsArray &js_objects = js_root.at("objects").as_arr();
        for (JsElement &js_obj_el : js_objects.elements) {
            JsObject &js_obj = js_obj_el.as_obj();

            if (js_obj.Has("decal")) {
                JsObject &js_decal = js_obj.at("decal").as_obj();
                if (js_decal.Has("diff")) {
                    JsString &js_diff_tex = js_decal.at("diff").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_diff_tex.val);
                }
                if (js_decal.Has("norm")) {
                    JsString &js_norm_tex = js_decal.at("norm").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_norm_tex.val);
                }
                if (js_decal.Has("spec")) {
                    JsString &js_spec_tex = js_decal.at("spec").as_str();
                    SceneManagerInternal::ReplaceTextureExtension(ctx.platform, js_spec_tex.val);
                }
            }
        }
    }

    if (js_root.Has("probes")) {
        JsArray &js_probes = js_root.at("probes").as_arr();
        for (JsElement &js_probe_el : js_probes.elements) {
            JsObject &js_probe = js_probe_el.as_obj();

            if (js_probe.Has("faces")) {
                JsArray &js_faces = js_probe.at("faces").as_arr();
                for (JsElement &js_face_el : js_faces.elements) {
                    JsString &js_face_str = js_face_el.as_str();
                    ReplaceTextureExtension(ctx.platform, js_face_str.val);
                }
            }
        }
    }

    if (js_root.Has("chapters")) {
        JsArray &js_chapters = js_root.at("chapters").as_arr();
        for (JsElement &js_chapter_el : js_chapters.elements) {
            JsObject &js_chapter = js_chapter_el.as_obj();

            JsObject js_caption, js_text_data;

            if (js_chapter.Has("html_src")) {
                JsObject &js_html_src = js_chapter.at("html_src").as_obj();
                for (auto &js_src_pair : js_html_src.elements) {
                    const std::string
                            &js_lang = js_src_pair.first,
                            &js_file_path = js_src_pair.second.as_str().val;

                    const std::string html_file_path = base_path + js_file_path;

                    std::string caption;
                    std::string html_body = ExtractHTMLData(ctx, html_file_path.c_str(), caption);

                    caption = std::regex_replace(caption, std::regex("\n"), "");
                    caption = std::regex_replace(caption, std::regex("\r"), "");
                    caption = std::regex_replace(caption, std::regex("\'"), "&apos;");
                    caption = std::regex_replace(caption, std::regex("\""), "&quot;");
                    caption = std::regex_replace(caption, std::regex("<h1>"), "");
                    caption = std::regex_replace(caption, std::regex("</h1>"), "");

                    html_body = std::regex_replace(html_body, std::regex("\n"), "");
                    html_body = std::regex_replace(html_body, std::regex("\'"), "&apos;");
                    html_body = std::regex_replace(html_body, std::regex("\""), "&quot;");

                    // remove spaces
                    if (!caption.empty()) {
                        int n = 0;
                        while (n < (int)caption.length() && caption[n] == ' ') n++;
                        caption.erase(0, n);
                        while (caption.back() == ' ') caption.pop_back();
                    }

                    if (!html_body.empty()) {
                        int n = 0;
                        while (n < (int)html_body.length() && html_body[n] == ' ') n++;
                        html_body.erase(0, n);
                        while (html_body.back() == ' ') html_body.pop_back();
                    }

                    js_caption[js_lang] = JsString{ caption };
                    js_text_data[js_lang
                    ] = JsString{ html_body };
                }
            }

            js_chapter["caption"] = std::move(js_caption);
            js_chapter["text_data"] = std::move(js_text_data);
        }
    }

    JsFlags flags;
    flags.use_spaces = 1;

    js_root.Write(dst_stream, flags);
}
