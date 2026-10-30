#include "check_flow.h"
#include <assert.h>
#include <stdio.h>

static hc_flow_t ready(unsigned step) {
    hc_flow_t f;
    hc_flow_init(&f);
    hc_flow_set_modules(&f, HC_HEAD | HC_BODY, false);
    assert(hc_flow_select(&f, step));
    return f;
}
static void complete(hc_flow_t *f) {
    assert(hc_flow_begin(f, f->step == HC_MOTION));
    assert(hc_flow_finish(f, f->attempt, 0, false));
    assert(f->phase == HC_REVIEW);
    assert(f->results[f->step] != HC_PASS);
    assert(hc_flow_confirm(f));
}
int main(void) {
    hc_flow_t f;
    hc_flow_init(&f);
    assert(f.phase == HC_WELCOME && !hc_flow_busy(&f));
    assert(!hc_flow_begin(&f, true));
    assert(!hc_flow_select(&f, HC_STEP_COUNT));
    f = ready(HC_CAMERA);
    assert(!hc_flow_confirm(&f));
    assert(!hc_flow_next(&f));
    assert(hc_flow_begin(&f, false));
    unsigned old = f.attempt;
    assert(!hc_flow_begin(&f, false));
    assert(!hc_flow_select(&f, HC_LIGHTS));
    assert(!hc_flow_skip(&f));
    assert(!hc_flow_confirm(&f));
    assert(hc_flow_cancel(&f));
    assert(f.phase == HC_CANCELLING && hc_flow_busy(&f));
    assert(!hc_flow_next(&f));
    assert(hc_flow_finish(&f, old, 0, false)); /* Cancel beats a racing success. */
    assert(f.phase == HC_IDLE && f.confirmed[HC_CAMERA] == 0);
    assert(hc_flow_begin(&f, false));
    assert(!hc_flow_finish(&f, old, 0, false));
    assert(hc_flow_finish(&f, f.attempt, 5, false));
    assert(f.phase == HC_ISSUE);
    assert(hc_flow_retry(&f));
    complete(&f); /* Photo accepted, stream still untested. */
    assert(f.phase == HC_IDLE && f.item == 1);
    assert(!hc_flow_next(&f));
    complete(&f);
    assert(f.phase == HC_PASSED && f.results[HC_CAMERA] == HC_PASS);
    assert(hc_flow_next(&f) && f.step == HC_SPEAKER && f.phase == HC_IDLE);

    f = ready(HC_MOTION);
    assert(hc_flow_accept_intent(&f, f.attempt, HC_IDLE, 0, 0));
    assert(!hc_flow_accept_intent(&f, f.attempt, HC_IDLE, 0, 1)); /* Queued tap cannot confirm new safety page. */
    assert(!hc_flow_begin(&f, false));
    assert(hc_flow_begin(&f, true));
    assert(!hc_flow_accept_intent(&f, f.attempt, HC_IDLE, 0, 0));
    assert(hc_flow_accept_intent(&f, f.attempt, HC_RUNNING, 0, 0)); /* Progress changes do not disable Stop. */
    hc_flow_set_modules(&f, HC_HEAD, false);
    assert(f.phase == HC_CANCELLING);
    assert(hc_flow_finish(&f, f.attempt, 0, true));
    assert(f.phase == HC_UNAVAILABLE);
    hc_flow_set_modules(&f, HC_HEAD | HC_BODY, false);
    assert(f.phase == HC_IDLE); /* New explicit start still required. */
    assert(!hc_flow_begin(&f, false));
    assert(hc_flow_begin(&f, true));
    hc_flow_set_modules(&f, HC_HEAD | HC_BODY, true);
    assert(f.body_fault && f.phase == HC_CANCELLING);
    assert(hc_flow_finish(&f, f.attempt, 0, true));
    hc_flow_set_modules(&f, HC_HEAD | HC_BODY, false);
    assert(f.body_fault && f.phase == HC_UNAVAILABLE); /* Link cannot clear loss latch. */
    assert(!hc_flow_begin(&f, true));
    assert(hc_flow_select(&f, HC_CAMERA));
    assert(hc_flow_begin(&f, false)); /* Body fault does not poison head. */
    assert(hc_flow_finish(&f, f.attempt, 0, false));
    assert(hc_flow_reject(&f));
    assert(f.results[HC_CAMERA] == HC_FAIL);
    assert(hc_flow_skip(&f));
    assert(f.results[HC_CAMERA] == HC_FAIL && f.phase == HC_SKIPPED);

    for (unsigned step = 0; step < HC_STEP_COUNT; step++) {
        f = ready(step);
        const unsigned count = hc_item_count(step);
        assert(count > 0 && count < 32);
        for (unsigned item = 0; item < count; item++) {
            assert(hc_current_item(&f));
            complete(&f);
        }
        assert(f.results[step] == HC_PASS);
        assert(hc_flow_retry(&f));
        assert(f.confirmed[step] == 0 && f.results[step] == HC_UNTESTED);
    }
    f = ready(HC_CAMERA);
    hc_flow_set_modules(&f, HC_AUDIO, false);
    assert(f.phase == HC_UNAVAILABLE);
    assert(hc_flow_skip(&f));
    assert(hc_flow_next(&f));
    assert(hc_flow_begin(&f, false));
    assert(hc_flow_finish(&f, f.attempt, 0, false));
    assert(hc_flow_confirm(&f));
    f = ready(HC_CAMERA);
    assert(hc_flow_begin(&f, false));
    assert(hc_flow_finish(&f, f.attempt, 9, false));
    assert(hc_flow_continue(&f));
    assert(f.item == 1 && f.phase == HC_IDLE && f.results[HC_CAMERA] == HC_FAIL);
    complete(&f);
    assert(f.phase == HC_PASSED && f.results[HC_CAMERA] == HC_FAIL);
    assert(hc_flow_select_item(&f, HC_CAMERA, 0));
    assert(f.results[HC_CAMERA] != HC_PASS);
    complete(&f);
    assert(f.results[HC_CAMERA] == HC_PASS); /* Repair only failed subtest. */
    assert(hc_flow_select_item(&f, HC_CAMERA, 1));
    assert(f.results[HC_CAMERA] != HC_PASS); /* Retest revokes old evidence now. */
    assert(hc_flow_skip(&f));
    assert(f.results[HC_CAMERA] == HC_SKIP);
    assert(hc_flow_select(&f, HC_MOTION));
    hc_flow_set_modules(&f, HC_HEAD, true);
    hc_flow_restart(&f);
    assert(f.phase == HC_WELCOME && f.body_fault && f.modules == HC_HEAD);
    assert(f.confirmed[HC_CAMERA] == 0 && f.results[HC_CAMERA] == HC_UNTESTED);
    puts("check flow: readiness, explicit acceptance, cancellation, subtasks, stale results, isolation passed");
    return 0;
}
