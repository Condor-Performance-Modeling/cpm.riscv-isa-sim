#pragma once

#include <string>
#include "processor.h"

uint64_t get_highest_priv_mode(const std::string& stf_priv_modes) {
    uint64_t highest_priv = 0;
    for (char mode : stf_priv_modes) {
        switch (mode) {
            case 'U': highest_priv = std::max(highest_priv, static_cast<uint64_t>(0)); break;
            case 'S': highest_priv = std::max(highest_priv, static_cast<uint64_t>(1)); break;
            case 'H': highest_priv = std::max(highest_priv, static_cast<uint64_t>(2)); break;
            case 'M': highest_priv = std::max(highest_priv, static_cast<uint64_t>(3)); break;
            default:
                std::cerr << "Invalid privilege mode: " << mode << "\n";
                break;
        }
    }
    return highest_priv;
}

uint64_t riscv_get_reg(const state_t* s, int rn) {
    assert(rn >= 0 && rn < NXPR);
    return s->XPR[rn];
}

float128_t riscv_get_fpreg(const state_t* s, int rn) {
    assert(rn >= 0 && rn < NFPR);
    return s->FPR[rn];
}