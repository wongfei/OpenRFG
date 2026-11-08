// The --audio-log test seam of OpenRFG (SFX.md deviation 3.1; new code, not recovered). One line per
// audiolib_cue_play result: the frame, the cue (its name once a lookup by name has given its caller cue id, else the
// id), the play id, the error code and, for a 3D cue, the position. The retail data has no cue names (SFX.md SX0), so
// the names come from audiolib_cue_get_id, which the tables and the game call with every name they play. Off unless
// the option is given; only the programs that link the audio library (OPENRFG_AUDIO) have it.

#include "main_audio_log.h"

#ifdef OPENRFG_AUDIO

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/vlib/code/os/frametime.h"

#include <windows.h>

#include <stdio.h>
#include <string>
#include <unordered_map>

namespace
{
    CRITICAL_SECTION Lock;
    FILE* Log;
    std::unordered_map<int, std::string>* Names; // caller cue id -> name

    // Lookups by name and plays come from the simulation, main and audio threads.
    struct scoped_lock
    {
        scoped_lock()
        {
            EnterCriticalSection(&Lock);
        }
        ~scoped_lock()
        {
            LeaveCriticalSection(&Lock);
        }
    };

    void cue_get_id_hook(const char* cue_name, int caller_cue_id)
    {
        scoped_lock lock;
        (*Names)[caller_cue_id] = cue_name;
    }

    void cue_play_hook(int cue_id, const audiolib_params* params, int play_id, audiolib_result error)
    {
        scoped_lock lock;
        const auto name = Names->find(cue_id);
        char cue[96];
        if (name != Names->end())
        {
            snprintf(cue, sizeof(cue), "%s", name->second.c_str());
        }
        else
        {
            snprintf(cue, sizeof(cue), "#0x%08X", (unsigned int)cue_id);
        }
        // Params flag 1: a 3D cue at params->pos.
        if (params != nullptr && (params->flags & 1) != 0)
        {
            fprintf(Log, "%d\t%s\t%d\t%d\t%.2f %.2f %.2f\n", Framecount, cue, play_id, (int)error, params->pos.x, params->pos.y, params->pos.z);
        }
        else
        {
            fprintf(Log, "%d\t%s\t%d\t%d\t2d\n", Framecount, cue, play_id, (int)error);
        }
        fflush(Log);
    }
}

bool main_audio_log_start(const char* path)
{
    if (fopen_s(&Log, path, "w") != 0 || Log == nullptr)
    {
        return false;
    }
    fprintf(Log, "frame\tcue\tplay_id\terror\tposition\n");
    InitializeCriticalSection(&Lock);
    Names = new std::unordered_map<int, std::string>();
    Audiolib_cue_get_id_log_hook = cue_get_id_hook;
    Audiolib_cue_play_log_hook = cue_play_hook;
    return true;
}

#else

bool main_audio_log_start(const char*)
{
    return false;
}

#endif
