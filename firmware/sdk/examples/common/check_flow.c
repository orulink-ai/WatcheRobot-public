#include "check_flow.h"
#include <string.h>

static const char *const names[] = {"Screen & touch", "Camera",      "Speaker",       "Mic & replay",
                                    "Body touch",     "Body lights", "Motion & stop", "Run together"};
static const hc_item_t screen[] = {
    {"Screen & touch", "Test touch", "Tap targets 1, 2, then 3.", "Screen clear? All taps worked?", HC_OP_SCREEN, 0,
     HC_DISPLAY},
};
static const hc_item_t camera[] = {
    {"Photo", "Take photo", "Point at an object. Hold still.", "Is this a clear, NEW photo?", HC_OP_PHOTO, 0,
     HC_DISPLAY | HC_CAMERA_MODULE},
    {"Camera stream", "Stream 5 seconds", "Watch the frame count increase.", "Did the live picture update?",
     HC_OP_STREAM, 5, HC_DISPLAY | HC_CAMERA_MODULE},
};
static const hc_item_t speaker[] = {
    {"Speaker voice", "Test voice", "Listen for: one, two, three.", "Did you hear the voice clearly?", HC_OP_VOICE, 0,
     HC_AUDIO},
    {"Speaker tone", "Test tone", "A short tone. Not a loudness test.", "Did you hear a clean tone?", HC_OP_TONE, 0,
     HC_AUDIO},
};
static const hc_item_t mic[] = {
    {"Microphone", "Record 3 seconds", "Wait for REC. Say one, two, three.", "Did input level follow your voice?",
     HC_OP_RECORD, 0, HC_AUDIO},
    {"Replay", "Play recording", "Listen to your saved recording.", "Voice clear, without squeal?", HC_OP_REPLAY, 0,
     HC_AUDIO},
};
static const hc_item_t touch[] = {
    {"Body touch", "Listen for touch", "Body sensor: tap, release; hold, release.",
     "Did all events match your touches?", HC_OP_TOUCH, 0, HC_BODY},
};
#define LIGHT(title, hint, arg) {title, "Apply light", hint, "Did the indicated zone match?", HC_OP_LIGHT, arg, HC_BODY}
static const hc_item_t lights[] = {
    LIGHT("Side: red", "Only SIDE should light RED.", 0),
    LIGHT("Side: green", "Only SIDE should light GREEN.", 1),
    LIGHT("Side: blue", "Only SIDE should light BLUE.", 2),
    LIGHT("Side: dim", "SIDE green, low brightness.", 3),
    LIGHT("Side: bright", "SIDE green, higher brightness.", 4),
    LIGHT("Side: breathing", "SIDE green, smoothly pulsing.", 5),
    LIGHT("Side: off", "SIDE lights should turn OFF.", 6),
    LIGHT("Bottom: red", "Only BOTTOM should light RED.", 7),
    LIGHT("Bottom: green", "Only BOTTOM should light GREEN.", 8),
    LIGHT("Bottom: blue", "Only BOTTOM should light BLUE.", 9),
    LIGHT("Bottom: dim", "BOTTOM green, low brightness.", 10),
    LIGHT("Bottom: bright", "BOTTOM green, higher brightness.", 11),
    LIGHT("Bottom: breathing", "BOTTOM green, smoothly pulsing.", 12),
    LIGHT("Bottom: off", "BOTTOM lights should turn OFF.", 13),
};
#undef LIGHT
static const hc_item_t motion[] = {
    {"Move X", "Space clear: move X", "Reference X90/Y120, then X93/Y120. Clear full travel.",
     "Did reference then X step move? No position sensor.", HC_OP_MOVE, 0, HC_BODY},
    {"Move Y", "Space clear: move Y", "Reference X90/Y120, then X90/Y123. Clear full travel.",
     "Did reference then Y step move? No position sensor.", HC_OP_MOVE, 1, HC_BODY},
    {"Stop test", "Space clear: start", "Reference X90/Y120, then slow X93; tap STOP. Clear travel.",
     "Did physical movement STOP?", HC_OP_STOP_TEST, 0, HC_BODY},
};
static const hc_item_t combined[] = {
    {"Run together", "Run 20 seconds", "Live photo, voice, lights. NO motor.", "Picture, voice and touch responsive?",
     HC_OP_COMBINED, 20, HC_HEAD | HC_BODY},
};
static const hc_item_t *const items[] = {screen, camera, speaker, mic, touch, lights, motion, combined};
static const unsigned counts[] = {1, 2, 2, 2, 1, 14, 3, 1};

const char *hc_step_name(unsigned step) {
    return step < HC_STEP_COUNT ? names[step] : "Unknown";
}
unsigned hc_item_count(unsigned step) {
    return step < HC_STEP_COUNT ? counts[step] : 0;
}
const hc_item_t *hc_current_item(const hc_flow_t *f) {
    return f && f->step < HC_STEP_COUNT && f->item < counts[f->step] ? &items[f->step][f->item] : NULL;
}
bool hc_flow_busy(const hc_flow_t *f) {
    return f && (f->phase == HC_RUNNING || f->phase == HC_CANCELLING);
}
bool hc_flow_accept_intent(const hc_flow_t *f, uint32_t attempt, hc_phase_t phase, unsigned page,
                           unsigned current_page) {
    return f && f->attempt == attempt && f->phase == phase && page == current_page;
}
bool hc_flow_available(const hc_flow_t *f) {
    const hc_item_t *item = hc_current_item(f);
    return item && (f->modules & item->modules) == item->modules && (!(item->modules & HC_BODY) || !f->body_fault);
}
void hc_flow_init(hc_flow_t *f) {
    if (f) {
        memset(f, 0, sizeof(*f));
        f->phase = HC_WELCOME;
    }
}
void hc_flow_restart(hc_flow_t *f) {
    if (!f || hc_flow_busy(f))
        return;
    unsigned modules = f->modules;
    bool fault = f->body_fault;
    uint32_t attempt = f->attempt + 1;
    hc_flow_init(f);
    f->modules = modules;
    f->body_fault = fault;
    f->attempt = attempt;
}
static void result(hc_flow_t *f) {
    unsigned s = f->step;
    f->results[s] = f->failed[s]                               ? HC_FAIL
                    : f->skipped[s]                            ? HC_SKIP
                    : f->confirmed[s] == (1u << counts[s]) - 1 ? HC_PASS
                                                               : HC_UNTESTED;
}
void hc_flow_set_modules(hc_flow_t *f, unsigned modules, bool fault) {
    if (!f)
        return;
    f->modules = modules;
    f->body_fault |= fault; /* Only an explicit process/runtime recovery clears event loss. */
    if (hc_flow_busy(f)) {
        if (!hc_flow_available(f))
            f->phase = HC_CANCELLING;
    } else if (f->phase == HC_IDLE || f->phase == HC_UNAVAILABLE || f->phase == HC_REVIEW) {
        if (!hc_flow_available(f))
            f->phase = HC_UNAVAILABLE;
        else if (f->phase == HC_UNAVAILABLE)
            f->phase = HC_IDLE;
    }
}
bool hc_flow_select(hc_flow_t *f, unsigned step) {
    if (!f || step >= HC_STEP_COUNT || hc_flow_busy(f))
        return false;
    f->step = step;
    f->item = 0;
    while (f->item + 1 < counts[step] && (f->confirmed[step] & (1u << f->item)))
        f->item++;
    f->attempt++;
    f->last_error = 0;
    f->phase = f->results[step] == HC_PASS ? HC_PASSED : hc_flow_available(f) ? HC_IDLE : HC_UNAVAILABLE;
    return true;
}
bool hc_flow_begin(hc_flow_t *f, bool confirmed_motion) {
    const hc_item_t *item = hc_current_item(f);
    if (!item || f->phase != HC_IDLE || !hc_flow_available(f))
        return false;
    if ((item->operation == HC_OP_MOVE || item->operation == HC_OP_STOP_TEST) && !confirmed_motion)
        return false;
    f->attempt++;
    f->last_error = 0;
    f->phase = HC_RUNNING;
    return true;
}
bool hc_flow_select_item(hc_flow_t *f, unsigned step, unsigned item) {
    if (!f || step >= HC_STEP_COUNT || item >= counts[step] || hc_flow_busy(f))
        return false;
    f->step = step;
    f->item = item;
    f->phase = HC_IDLE;
    return hc_flow_retry(f);
}
bool hc_flow_cancel(hc_flow_t *f) {
    if (!f || f->phase != HC_RUNNING)
        return false;
    f->phase = HC_CANCELLING;
    return true;
}
bool hc_flow_finish(hc_flow_t *f, uint32_t attempt, int error, bool cancelled) {
    if (!f || attempt != f->attempt || !hc_flow_busy(f))
        return false;
    cancelled |= f->phase == HC_CANCELLING;
    f->last_error = error;
    if (error) {
        f->failed[f->step] |= 1u << f->item;
        result(f);
    }
    if (!hc_flow_available(f))
        f->phase = HC_UNAVAILABLE;
    else if (error)
        f->phase = HC_ISSUE;
    else
        f->phase = cancelled ? HC_IDLE : HC_REVIEW;
    return true;
}
bool hc_flow_confirm(hc_flow_t *f) {
    if (!f || f->phase != HC_REVIEW || !hc_flow_available(f))
        return false;
    f->confirmed[f->step] |= 1u << f->item;
    f->failed[f->step] &= ~(1u << f->item);
    f->skipped[f->step] &= ~(1u << f->item);
    result(f);
    if (f->item + 1 == counts[f->step] || f->results[f->step] == HC_PASS) {
        f->phase = HC_PASSED;
    } else {
        f->item++;
        f->phase = HC_IDLE; /* Never automatically execute the next subtest. */
        f->attempt++;
    }
    return true;
}
bool hc_flow_reject(hc_flow_t *f) {
    if (!f || f->phase != HC_REVIEW)
        return false;
    f->phase = HC_ISSUE;
    f->failed[f->step] |= 1u << f->item;
    result(f);
    return true;
}
bool hc_flow_skip(hc_flow_t *f) {
    if (!f || hc_flow_busy(f) || f->phase == HC_WELCOME)
        return false;
    f->skipped[f->step] |= 1u << f->item;
    result(f);
    f->phase = HC_SKIPPED;
    f->attempt++;
    return true;
}
bool hc_flow_retry(hc_flow_t *f) {
    if (!f || hc_flow_busy(f) || f->phase == HC_WELCOME)
        return false;
    f->confirmed[f->step] &= ~(1u << f->item);
    if (f->phase == HC_PASSED || f->phase == HC_SKIPPED) {
        f->confirmed[f->step] = f->failed[f->step] = f->skipped[f->step] = 0;
        f->item = 0;
    }
    result(f);
    f->last_error = 0;
    f->attempt++;
    f->phase = hc_flow_available(f) ? HC_IDLE : HC_UNAVAILABLE;
    return true;
}
bool hc_flow_continue(hc_flow_t *f) {
    if (!f || hc_flow_busy(f) || !(f->phase == HC_ISSUE || f->phase == HC_SKIPPED))
        return false;
    if (f->item + 1 < counts[f->step]) {
        f->item++;
        f->attempt++;
        f->last_error = 0;
        f->phase = hc_flow_available(f) ? HC_IDLE : HC_UNAVAILABLE;
    } else
        f->phase = HC_PASSED; /* End of group, not a claim of passing. */
    return true;
}
bool hc_flow_next(hc_flow_t *f) {
    if (!f || (f->phase != HC_PASSED && f->phase != HC_SKIPPED) || f->step + 1 >= HC_STEP_COUNT)
        return false;
    return hc_flow_select(f, f->step + 1);
}
