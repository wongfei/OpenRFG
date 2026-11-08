// Original: volition/interface_lib/code/resources/vint_resource_sound.cpp
//
// Sound resources (UI.md UI2): a pool of one (rfg.exe's Sound_resources[1]), loaded by name through the game's sound
// callbacks and reference counted. A handle is the pool index in the low half and a running count in the high half.
// Compiled in interface_lib_unity.cpp.

#include "volition/interface_lib/code/resources/vint_resource_sound.h"

#include "volition/interface_lib/code/vint_external.h"
#include "volition/interface_lib/code/internal/vint_dlist.h"
#include "volition/vlib/code/math/crc.h"

// ---- public globals
vint_resource_sound* Loaded_sounds; // 0x01A48660
vint_resource_sound* Avail_sounds; // 0x01A48664

// ---- file-static globals
static unsigned short Sound_count; // 0x01A48668
static vint_resource_sound Sound_resources[1]; // 0x01A4F01C

// 0x00461560 (.h:30)
vint_resource_sound::vint_resource_sound()
{
}

// 0x00461570 (.h:31)
vint_resource_sound::~vint_resource_sound()
{
}

// 0x00461580 (.h:49)
void vint_resource_sound::add_ref() const
{
    ++const_cast<vint_resource_sound*>(this)->m_ref_count;
}

// 0x00469C20 (.h:53)
void vint_resource_sound::add_ref(unsigned int _handle)
{
    if (vint_resource_sound* s = find(_handle))
    {
        s->add_ref();
    }
}

// 0x00469C50 (.h:62)
void vint_resource_sound::release(unsigned int _handle)
{
    if (vint_resource_sound* s = find(_handle))
    {
        s->release();
    }
}

// 0x00461590 (.h:70)
unsigned int vint_resource_sound::get_handle() const
{
    return m_internal_handle;
}

// 0x004615A0 (:78)
bool vint_resource_sound::system_init()
{
    Sound_resources[0].next = &Sound_resources[0];
    Sound_resources[0].prev = &Sound_resources[0];
    Avail_sounds = &Sound_resources[0];
    Sound_count = 0;
    Loaded_sounds = nullptr;
    return true;
}

// 0x004615D0 (:93)
void vint_resource_sound::system_shutdown()
{
}

// 0x004615E0 (:98)
vint_resource_sound* vint_resource_sound::load(const char* _name)
{
    if (_name == nullptr)
    {
        return nullptr;
    }
    unsigned int name_crc = crc_stri(_name, 0);
    vint_resource_sound* s = Loaded_sounds;
    while (s != nullptr)
    {
        if (s->m_name_crc == name_crc)
        {
            ++s->m_ref_count;
            return s;
        }
        s = s->next;
        if (s == Loaded_sounds)
        {
            break;
        }
    }
    s = Avail_sounds;
    if (s == nullptr || Resource_callbacks == nullptr || Resource_callbacks->sound_load == nullptr)
    {
        return nullptr;
    }
    int title_handle = Resource_callbacks->sound_load(_name);
    if (title_handle == 0)
    {
        return nullptr;
    }
    ++Sound_count;
    s->m_internal_handle = ((unsigned int)(s - Sound_resources) & 0xFFFF) + (unsigned int)Sound_count * 0x10000;
    s->m_title_handle = title_handle;
    s->m_name_crc = crc_stri(_name, 0);
    s->m_ref_count = 1;
    vint_dlist_remove(Avail_sounds, s, &vint_resource_sound::next, &vint_resource_sound::prev);
    vint_dlist_add(Loaded_sounds, s, &vint_resource_sound::next, &vint_resource_sound::prev);
    return s;
}

// 0x00461740 (:155)
vint_resource_sound* vint_resource_sound::find(unsigned int _handle)
{
    if (_handle == 0 || (_handle & 0xFFFF) >= 1)
    {
        return nullptr;
    }
    vint_resource_sound* s = &Sound_resources[_handle & 0xFFFF];
    return s->m_internal_handle == _handle ? s : nullptr;
}

// 0x00461770 (:182)
void vint_resource_sound::release()
{
    if (m_ref_count != 1)
    {
        --m_ref_count;
        return;
    }
    if (Resource_callbacks != nullptr && Resource_callbacks->sound_release != nullptr)
    {
        Resource_callbacks->sound_release(m_title_handle);
    }
    m_internal_handle = 0;
    m_title_handle = 0;
    vint_dlist_remove(Loaded_sounds, this, &vint_resource_sound::next, &vint_resource_sound::prev);
    vint_dlist_add(Avail_sounds, this, &vint_resource_sound::next, &vint_resource_sound::prev);
}

// 0x00461830 (:210)
unsigned int vint_resource_sound::play() const
{
    if (Sound_callbacks == nullptr || Sound_callbacks->play == nullptr)
    {
        return 0;
    }
    unsigned int play_handle;
    if (!Sound_callbacks->play(m_title_handle, play_handle))
    {
        return 0;
    }
    return play_handle;
}

// 0x00461860 (:224)
float vint_resource_sound::get_duration() const
{
    if (Resource_callbacks != nullptr && Resource_callbacks->sound_get_duration != nullptr)
    {
        float duration;
        if (Resource_callbacks->sound_get_duration(m_title_handle, duration))
        {
            return duration;
        }
    }
    return 0.0f;
}

// 0x00461890 (:238)
void vint_resource_sound::pause(unsigned int _handle, bool _pause)
{
    // Original: the stop callback is the one checked before calling pause.
    if (Sound_callbacks != nullptr && Sound_callbacks->stop != nullptr)
    {
        Sound_callbacks->pause(_handle, _pause);
    }
}

// 0x004618B0 (:245)
void vint_resource_sound::stop(unsigned int _handle)
{
    if (Sound_callbacks != nullptr && Sound_callbacks->stop != nullptr)
    {
        Sound_callbacks->stop(_handle);
    }
}

// 0x004618D0 (:252)
bool vint_resource_sound::is_playing(unsigned int _handle)
{
    if (Sound_callbacks != nullptr && Sound_callbacks->get_state != nullptr)
    {
        vint_sound_state state;
        if (Sound_callbacks->get_state(_handle, state))
        {
            return state == VINT_SOUND_PLAYING || state == VINT_SOUND_PAUSED;
        }
    }
    return false;
}

// 0x00461920 (:266)
bool vint_resource_sound::is_paused(unsigned int _handle)
{
    if (Sound_callbacks != nullptr && Sound_callbacks->get_state != nullptr)
    {
        vint_sound_state state;
        if (Sound_callbacks->get_state(_handle, state))
        {
            return state == VINT_SOUND_PAUSED;
        }
    }
    return false;
}
