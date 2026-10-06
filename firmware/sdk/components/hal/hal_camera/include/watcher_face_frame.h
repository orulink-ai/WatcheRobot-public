#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Private JPEG APP15 record, covered by the PTL frame CRC. A complete JPEG
 * remains independently decodable. Never pair telemetry from a UART poll. */
typedef struct { uint16_t x, y, width, height, score, target; } wff_box_t;
typedef struct {
    uint32_t generation, sequence;
    uint16_t width, height;
    uint8_t count;
    wff_box_t boxes[8];
} wff_frame_t;
#define WFF_MAX_OVERHEAD 120U
static inline void wff_put(uint8_t *p, uint32_t n, unsigned bytes) {
    for (unsigned i=0; i<bytes; ++i) p[i]=(uint8_t)(n >> (8U*i));
}
static inline uint32_t wff_get(const uint8_t *p, unsigned bytes) {
    uint32_t n=0;
    for (unsigned i=0; i<bytes; ++i) n|=(uint32_t)p[i] << (8U*i);
    return n;
}
static inline bool wff_valid(const wff_frame_t *f) {
    if (!f || !f->sequence || f->width!=640 || f->height!=480 || f->count>8) return false;
    for (unsigned i=0; i<f->count; ++i) {
        const wff_box_t *b=&f->boxes[i];
        if (b->x>f->width || b->y>f->height || b->width>f->width || b->height>f->height ||
            b->score>100 || b->target>79) return false;
    }
    return true;
}
static inline size_t wff_pack(uint8_t *out, size_t capacity, const uint8_t *jpeg, size_t size,
                              const wff_frame_t *f) {
    if (!out || !jpeg || size<4 || jpeg[0]!=255 || jpeg[1]!=216 || !wff_valid(f)) return 0;
    /* The sensor reports DMA memory size, including trailing alignment bytes. */
    while (size>=4 && !(jpeg[size-2]==255 && jpeg[size-1]==217)) --size;
    if (size<4) return 0;
    const size_t extra=24U+12U*f->count;
    if (size>capacity || extra>capacity-size) return 0;
    out[0]=255; out[1]=216; out[2]=255; out[3]=239;
    out[4]=0; out[5]=(uint8_t)(extra-2);
    memcpy(out+6,"WVFP",4);
    wff_put(out+10,f->generation,4); wff_put(out+14,f->sequence,4);
    wff_put(out+18,f->width,2); wff_put(out+20,f->height,2);
    out[22]=f->count; memset(out+23,0,3);
    for (unsigned i=0; i<f->count; ++i) {
        const wff_box_t *b=&f->boxes[i]; uint8_t *p=out+26+12*i;
        wff_put(p,b->x,2); wff_put(p+2,b->y,2); wff_put(p+4,b->width,2);
        wff_put(p+6,b->height,2); wff_put(p+8,b->score,2); wff_put(p+10,b->target,2);
    }
    memcpy(out+2+extra,jpeg+2,size-2);
    return size+extra;
}
static inline bool wff_unpack(const uint8_t *jpeg, size_t size, wff_frame_t *out) {
    if (!jpeg || !out || size<28 || jpeg[0]!=255 || jpeg[1]!=216 || jpeg[2]!=255 || jpeg[3]!=239 ||
        jpeg[4]!=0 || memcmp(jpeg+6,"WVFP",4) || jpeg[23] || jpeg[24] || jpeg[25] ||
        jpeg[size-2]!=255 || jpeg[size-1]!=217) return false;
    wff_frame_t f={0}; f.count=jpeg[22];
    const size_t extra=24U+12U*f.count;
    if (f.count>8 || size<extra+4 || jpeg[5]!=extra-2) return false;
    f.generation=wff_get(jpeg+10,4); f.sequence=wff_get(jpeg+14,4);
    f.width=(uint16_t)wff_get(jpeg+18,2); f.height=(uint16_t)wff_get(jpeg+20,2);
    for (unsigned i=0; i<f.count; ++i) {
        const uint8_t *p=jpeg+26+12*i;
        f.boxes[i]=(wff_box_t){(uint16_t)wff_get(p,2),(uint16_t)wff_get(p+2,2),
            (uint16_t)wff_get(p+4,2),(uint16_t)wff_get(p+6,2),(uint16_t)wff_get(p+8,2),(uint16_t)wff_get(p+10,2)};
    }
    if (!wff_valid(&f)) return false;
    *out=f; return true;
}

