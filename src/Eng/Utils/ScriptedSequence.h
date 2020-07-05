#pragma once

#include <string>
#include <vector>

#include <Ren/Context.h>
#include <Snd/Context.h>
#include <Sys/Signal_.h>

struct JsObject;
class SceneManager;
struct SceneObject;

namespace Gui {
class Renderer;
}

enum class eTrackType { Actor, Camera, Invalid };

enum class eActionType { Play, Look, Invalid };

struct SeqAction {
    eActionType type;
    double time_beg, time_end;
    float pos_beg[3], pos_end[3];
    float rot_beg[3], rot_end[3];
    double sound_offset;
    std::string caption;

    // temp data
    bool is_active;
    Ren::AnimSeqRef anim_ref;
    int anim_id;
    Snd::BufferRef sound_ref;

    static constexpr float SoundWaveStepS = 0.02f;
    Ren::TextureRegionRef sound_wave_tex;
};

struct SeqChoice {
    std::string key;
    std::string text;
    std::string seq_name;
    std::string puzzle_name;

    // temp data
    int seq_id;
};

class ScriptedSequence {
    struct Track {
        std::string name, target;
        eTrackType type;
        int action_start, action_count;

        // temp data
        double time_beg, time_end;
        int active_count;
        uint32_t target_actor;
    };

    Ren::Context &ren_ctx_;
    Snd::Context &snd_ctx_;
    SceneManager &scene_manager_;
    std::string lookup_name_, name_;
    std::vector<Track> tracks_;
    std::vector<SeqAction> actions_;

    SeqChoice choices_[8];
    int choices_count_ = 0;

    double end_time_, last_t_ = 0.0;

    void UpdateAction(uint32_t target_actor, SeqAction &action, double time_cur_s,
                      bool playing);

    Ren::TextureRegionRef RenderSoundWaveForm(const char *name, const void *samples_data,
                                              int samples_count,
                                              const Snd::BufParams &params);

  public:
    ScriptedSequence(Ren::Context &ren_ctx, Snd::Context &snd_ctx,
                     SceneManager &scene_manager);

    const char *lookup_name() const {
        return lookup_name_.empty() ? nullptr : lookup_name_.c_str();
    }
    const char *name() const { return name_.empty() ? nullptr : name_.c_str(); }

    double duration() const { return end_time_; }

    const char *GetTrackName(int track) const {
        if (track >= tracks_.size()) {
            return nullptr;
        }
        return tracks_[track].name.c_str();
    }

    const char *GetTrackTarget(int track) const {
        if (track >= tracks_.size()) {
            return nullptr;
        }
        return tracks_[track].target.c_str();
    }

    int GetActionsCount(int track) const {
        if (track >= tracks_.size()) {
            return 0;
        }
        return tracks_[track].action_count;
    }

    SeqAction *GetAction(int track, int action) {
        if (track >= tracks_.size() || action >= tracks_[track].action_count) {
            return nullptr;
        }
        return &actions_[tracks_[track].action_start + action];
    }

    int GetChoicesCount() const { return choices_count_; }

    const SeqChoice *GetChoice(int i) const {
        if (i < choices_count_) {
            return &choices_[i];
        }
        return nullptr;
    }

    SeqChoice *GetChoice(int i) {
        if (i < choices_count_) {
            return &choices_[i];
        }
        return nullptr;
    }

    const SeqChoice *GetChoice(const char *key) const {
        for (int i = 0; i < choices_count_; i++) {
            if (choices_[i].key == key) {
                return &choices_[i];
            }
        }
        return nullptr;
    }

    SeqChoice *GetChoice(const char *key) {
        for (int i = 0; i < choices_count_; i++) {
            if (choices_[i].key == key) {
                return &choices_[i];
            }
        }
        return nullptr;
    }

    void Clear();
    bool Load(const char *lookup_name, const JsObject &js_seq);
    void Save(JsObject &js_seq);

    void Reset();
    void Update(double cur_time_s, bool playing);

    Sys::Signal<void(const char *text, const uint8_t color[4])> push_caption_signal;

    static const char *ActionTypeNames[];
};
