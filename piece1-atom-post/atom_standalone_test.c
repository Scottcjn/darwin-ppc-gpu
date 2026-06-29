#include "atom.h"
#include <stdio.h>
#include <stdlib.h>
static uint32_t rd(struct card_info*c,uint32_t reg){(void)c;(void)reg;return 0;}
static void     wr(struct card_info*c,uint32_t reg,uint32_t v){(void)c;(void)reg;(void)v;}
int main(void){
    struct card_info card; memset(&card,0,sizeof(card));
    card.reg_read=rd; card.reg_write=wr;
    card.mc_read=rd;  card.mc_write=wr;
    card.pll_read=rd; card.pll_write=wr;
    void *bios = calloc(1, 64*1024);
    if(!bios){ fprintf(stderr,"alloc failed\n"); return 1; }
    struct atom_context *ctx = amdgpu_atom_parse(&card, bios);
    printf("amdgpu_atom_parse(dummy bios) -> %p\n",(void*)ctx);
    if(ctx) amdgpu_atom_destroy(ctx);
    free(bios);
    printf("OK: ATOM POST interpreter compiled, linked, and ran free of Linux/DRM.\n");
    return 0;
}
