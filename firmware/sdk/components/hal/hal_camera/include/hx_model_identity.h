#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Complete 2 MiB slot CRCs. Slot 4 accepts the historical installation and
 * Seeed face model 91128cea... with deterministic 0xff padding (see tools/ptl_face_model.py).
 * Unknown data must never acquire a semantic label from its slot number alone. */
static inline bool hx_model_identity_verified(unsigned slot, uint32_t crc) {
    static const uint32_t historical[] = {3906117410U, 1194593432U, 197646389U, 2517744533U};
    return slot >= 1 && slot <= 4 &&
           (crc == historical[slot - 1] || (slot == 4 && crc == 834922487U));
}

