#include <stdio.h>
#include <stdlib.h>
#include "es7243_adc.h"
#include "es7243e_adc.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s line %d\n",#x,__LINE__);exit(1); } } while(0)
static unsigned writes;
static bool fail_write;
static bool opened(const audio_codec_ctrl_if_t *c) { (void)c;return true; }
static int write_reg(const audio_codec_ctrl_if_t *c,int r,int rl,void *d,int dl) {
    (void)c;(void)r;(void)rl;(void)d;(void)dl;++writes;
    return fail_write ? -1 : 0;
}
static int read_reg(const audio_codec_ctrl_if_t *c,int r,int rl,void *d,int dl) {
    (void)c;(void)r;(void)rl;(void)dl;*(int*)d=0;return 0;
}
int main(void) {
    audio_codec_ctrl_if_t ctrl={.is_open=opened,.write_reg=write_reg,.read_reg=read_reg};
    for (unsigned variant=0;variant<2;++variant) {
        writes=0;
        es7243_codec_cfg_t a={.ctrl_if=&ctrl};
        es7243e_codec_cfg_t b={.ctrl_if=&ctrl};
#ifdef ADC_HAS_DEFERRED_INIT
        a.start_disabled=b.start_disabled=true;
#endif
        const audio_codec_if_t *c=variant ? es7243e_codec_new(&b) : es7243_codec_new(&a);
        CHECK(c && writes==0); /* Not even transiently program ADC before consent. */
        CHECK(c->enable(c,false)==0 && writes==0);
        fail_write=true;
        CHECK(c->enable(c,true)!=0);
        fail_write=false;
        CHECK(c->enable(c,false)==0); /* Failed first initialization retains physical stop obligation. */
        CHECK(c->enable(c,true)==0 && writes>0);
        fail_write=true;
        CHECK(c->enable(c,false)!=0);
        fail_write=false;
        CHECK(c->enable(c,false)==0);
        CHECK(c->close(c)==0);free((void*)c);
    }
    puts("ADC deferred initialization, failure and retry passed");
}
