
// fm2 - ReXGlue Recompiled Project
//
// This file is yours to edit. 'rexglue migrate' will NOT overwrite it.

#include "generated/fm2_init.h"

#include "fm2_app.h"

#include <cmath>

extern "C" float roundevenf(float v) {
    float r = std::roundf(v);
    float d = r - v;
    if (std::fabsf(d) == 0.5f && (static_cast<int32_t>(r) & 1))
        r = v - d;
    return r;
}

REX_DEFINE_APP(fm2, Fm2App::Create)
