// Original: volition/render_lib/code/rl_primitive_state.cpp
//
// The bit widths and the predefined macro states of rl_primitive_state (the members are inline in the header).

#include "volition/render_lib/code/rl_primitive_state.h"

const unsigned int rl_primitive_state::kAlphaBits = 5; // 0x012B8018
const unsigned int rl_primitive_state::kClampBits = 1; // 0x012B801C
const unsigned int rl_primitive_state::kZBufBits = 3; // 0x012B8020
const unsigned int rl_primitive_state::kStencilBits = 4; // 0x012B8024
const unsigned int rl_primitive_state::kCullBits = 2; // 0x012B8028
const unsigned int rl_primitive_state::kColorWriteBits = 3; // 0x012B802C
const unsigned int rl_primitive_state::kAlphaRefBits = 8; // 0x012B8030
const unsigned int rl_primitive_state::kMsaaBits = 1; // 0x012B8034
const unsigned int rl_primitive_state::kScissorBits = 1; // 0x012B8038
const unsigned int rl_primitive_state::kDepthBiasBits = 3; // 0x012B803C

// Magic numbers: constant alpha 255, 128 for the alpha test states.
const rl_primitive_state rl_primitive_state::NO_ALPHA(RLPAM_NONE, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515028
const rl_primitive_state rl_primitive_state::NO_ALPHA_Z(RLPAM_NONE, RLPCM_WRAP, RLPZM_FULL, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x0151502C
const rl_primitive_state rl_primitive_state::FILTER(RLPAM_FILTER, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515030
const rl_primitive_state rl_primitive_state::FILTER_SCREEN(RLPAM_FILTER_SCREEN, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515048
const rl_primitive_state rl_primitive_state::FILTER_Z(RLPAM_FILTER, RLPCM_WRAP, RLPZM_FULL, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515034
const rl_primitive_state rl_primitive_state::FILTER_Z_READ(RLPAM_FILTER, RLPCM_WRAP, RLPZM_READ, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515038
const rl_primitive_state rl_primitive_state::ADDITIVE(RLPAM_ADDITIVE, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x0151503C
const rl_primitive_state rl_primitive_state::ADDITIVE_ALPHA(RLPAM_ALPHA_ADDITIVE, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515040
const rl_primitive_state rl_primitive_state::ADDITIVE_Z(RLPAM_ADDITIVE, RLPCM_WRAP, RLPZM_FULL, RLPSM_NONE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515044
const rl_primitive_state rl_primitive_state::STENCIL_WRITE(RLPAM_NONE, RLPCM_WRAP, RLPZM_NONE, RLPSM_WRITE, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x0151504C
const rl_primitive_state rl_primitive_state::STENCIL_TEST(RLPAM_NONE, RLPCM_WRAP, RLPZM_NONE, RLPSM_TEST, 0xFF, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515050
const rl_primitive_state rl_primitive_state::ALPHA_TEST(RLPAM_ALPHA_TEST, RLPCM_WRAP, RLPZM_NONE, RLPSM_NONE, 0x80, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515054
const rl_primitive_state rl_primitive_state::ALPHA_TEST_Z(RLPAM_ALPHA_TEST, RLPCM_WRAP, RLPZM_FULL, RLPSM_NONE, 0x80, RLPCM_NONE, false, false, RLPZB_NONE, RLPCW_UNDEFINED); // 0x01515058
// Copies made by dynamic initializers.
const rl_primitive_state rl_primitive_state::TEXT(rl_primitive_state::FILTER); // 0x01C230D8
const rl_primitive_state rl_primitive_state::DEFAULT(rl_primitive_state::FILTER_Z); // 0x01B7BD7C

// Not in the binary: the packed bits (see the header).
unsigned int rl_primitive_state::get_bits() const
{
    return d.m_bits;
}

void rl_primitive_state::set_bits(unsigned int bits)
{
    d.m_bits = bits;
}
