#include "cpu.h"
#include "bus.h"
#include <stdint.h>

static uint8_t rd(uint16_t a) { return bus_read(a); }
static void wr(uint16_t a, uint8_t v) { bus_write(a, v); }
static uint16_t word(uint16_t a) { return (uint16_t)(rd(a) | (rd((uint16_t)(a + 1)) << 8)); }
static uint16_t zp(uint8_t a) { return rd(a) | ((uint16_t)rd((uint8_t)(a + 1)) << 8); }
static uint16_t ind(uint16_t a) { return rd(a) | ((uint16_t)rd((a & 0xff00) | ((a + 1) & 0xff)) << 8); }
static void flags(uint8_t v) { cpu.z = v == 0; cpu.n = v >> 7; }
static void push(uint8_t v) { wr((uint16_t)(0x100 | cpu.sp--), v); }
static uint8_t pop(void) { return rd((uint16_t)(0x100 | ++cpu.sp)); }
static uint8_t status(void) { return (uint8_t)(cpu.c | cpu.z << 1 | cpu.i << 2 | cpu.d << 3 | 0x20 | cpu.v << 6 | cpu.n << 7); }
static void set_status(uint8_t v) { cpu.c=v&1; cpu.z=(v>>1)&1; cpu.i=(v>>2)&1; cpu.d=(v>>3)&1; cpu.v=(v>>6)&1; cpu.n=(v>>7)&1; }
static void adc(uint8_t v) { uint16_t s=cpu.a+v+cpu.c; cpu.v=((~(cpu.a^v)&(cpu.a^(uint8_t)s))&0x80)!=0; cpu.c=s>255; cpu.a=(uint8_t)s; flags(cpu.a); }
static void cmp(uint8_t a, uint8_t v) { uint8_t r=(uint8_t)(a-v); cpu.c=a>=v; flags(r); }
static void branch(int take) { int8_t d=(int8_t)rd(cpu.pc++); if(take) { uint16_t old=cpu.pc; cpu.pc=(uint16_t)(cpu.pc+d); cpu.cycles++; if((old&0xff00)!=(cpu.pc&0xff00)) cpu.cycles++; } }

void cpu_step(void)
{
    uint8_t op=rd(cpu.pc++), v, r; uint16_t a; int16_t s;
    switch(op) {
    case 0x00: cpu.pc++; push(cpu.pc>>8); push(cpu.pc); push(status()|0x10); cpu.i=1; cpu.pc=word(0xfffe); cpu.cycles+=7; return;
    case 0x20: a=word(cpu.pc); cpu.pc+=2; push((cpu.pc-1)>>8); push(cpu.pc-1); cpu.pc=a; cpu.cycles+=6; return;
    case 0x40: set_status(pop()); cpu.pc=(uint16_t)(pop()|(pop()<<8)); cpu.cycles+=6; return;
    case 0x60: cpu.pc=(uint16_t)(pop()|(pop()<<8)); cpu.pc++; cpu.cycles+=6; return;
    case 0x4c: cpu.pc=word(cpu.pc); cpu.cycles+=3; return;
    case 0x6c: cpu.pc=ind(word(cpu.pc)); cpu.cycles+=5; return;
    case 0x08: push(status()|0x10); cpu.cycles+=3; return;
    case 0x28: set_status(pop()); cpu.cycles+=4; return;
    case 0x48: push(cpu.a); cpu.cycles+=3; return;
    case 0x68: cpu.a=pop(); flags(cpu.a); cpu.cycles+=4; return;
    case 0x18: cpu.c=0; cpu.cycles+=2; return; case 0x38: cpu.c=1; cpu.cycles+=2; return;
    case 0x58: cpu.i=0; cpu.cycles+=2; return; case 0x78: cpu.i=1; cpu.cycles+=2; return;
    case 0xb8: cpu.v=0; cpu.cycles+=2; return; case 0xd8: cpu.d=0; cpu.cycles+=2; return;
    case 0xf8: cpu.d=1; cpu.cycles+=2; return;
    case 0xea: case 0x1a: case 0x3a: case 0x5a: case 0x7a:
    case 0xda: case 0xfa: cpu.cycles+=2; return;
    case 0x10: branch(!cpu.n); cpu.cycles+=2; return; case 0x30: branch(cpu.n); cpu.cycles+=2; return;
    case 0x50: branch(!cpu.v); cpu.cycles+=2; return; case 0x70: branch(cpu.v); cpu.cycles+=2; return;
    case 0x90: branch(!cpu.c); cpu.cycles+=2; return; case 0xb0: branch(cpu.c); cpu.cycles+=2; return;
    case 0xd0: branch(!cpu.z); cpu.cycles+=2; return; case 0xf0: branch(cpu.z); cpu.cycles+=2; return;
    default: break;
    }
    switch(op) {
    case 0xa9: cpu.a=rd(cpu.pc++); flags(cpu.a); cpu.cycles+=2; return;
    case 0xa5: a=rd(cpu.pc++); cpu.a=rd(a); flags(cpu.a); cpu.cycles+=3; return;
    case 0xb5: a=(uint8_t)(rd(cpu.pc++)+cpu.x); cpu.a=rd(a); flags(cpu.a); cpu.cycles+=4; return;
    case 0xad: a=word(cpu.pc); cpu.pc+=2; cpu.a=rd(a); flags(cpu.a); cpu.cycles+=4; return;
    case 0xbd: a=word(cpu.pc); cpu.pc+=2; { uint16_t e=a+cpu.x; cpu.a=rd(e); flags(cpu.a); cpu.cycles+=4+((a&0xff00)!=(e&0xff00)); } return;
    case 0xb9: a=word(cpu.pc); cpu.pc+=2; { uint16_t e=a+cpu.y; cpu.a=rd(e); flags(cpu.a); cpu.cycles+=4+((a&0xff00)!=(e&0xff00)); } return;
    case 0xa1: a=zp((uint8_t)(rd(cpu.pc++)+cpu.x)); cpu.a=rd(a); flags(cpu.a); cpu.cycles+=6; return;
    case 0xb1: {
        uint16_t base = zp(rd(cpu.pc++));
        a = base + cpu.y; cpu.a = rd(a); flags(cpu.a);
        cpu.cycles += 5 + ((base & 0xff00) != (a & 0xff00)); return;
    }
    case 0xa2: cpu.x=rd(cpu.pc++); flags(cpu.x); cpu.cycles+=2; return;
    case 0xa6: a=rd(cpu.pc++); cpu.x=rd(a); flags(cpu.x); cpu.cycles+=3; return;
    case 0xb6: a=(uint8_t)(rd(cpu.pc++)+cpu.y); cpu.x=rd(a); flags(cpu.x); cpu.cycles+=4; return;
    case 0xae: a=word(cpu.pc); cpu.pc+=2; cpu.x=rd(a); flags(cpu.x); cpu.cycles+=4; return;
    case 0xbe: a=word(cpu.pc); cpu.pc+=2; { uint16_t e=a+cpu.y; cpu.x=rd(e); flags(cpu.x); cpu.cycles+=4+((a&0xff00)!=(e&0xff00)); } return;
    case 0xa0: cpu.y=rd(cpu.pc++); flags(cpu.y); cpu.cycles+=2; return;
    case 0xa4: a=rd(cpu.pc++); cpu.y=rd(a); flags(cpu.y); cpu.cycles+=3; return;
    case 0xb4: a=(uint8_t)(rd(cpu.pc++)+cpu.x); cpu.y=rd(a); flags(cpu.y); cpu.cycles+=4; return;
    case 0xac: a=word(cpu.pc); cpu.pc+=2; cpu.y=rd(a); flags(cpu.y); cpu.cycles+=4; return;
    case 0xbc: a=word(cpu.pc); cpu.pc+=2; { uint16_t e=a+cpu.x; cpu.y=rd(e); flags(cpu.y); cpu.cycles+=4+((a&0xff00)!=(e&0xff00)); } return;
    case 0xaa: cpu.x=cpu.a; flags(cpu.x); cpu.cycles+=2; return; case 0x8a: cpu.a=cpu.x; flags(cpu.a); cpu.cycles+=2; return;
    case 0xa8: cpu.y=cpu.a; flags(cpu.y); cpu.cycles+=2; return; case 0x98: cpu.a=cpu.y; flags(cpu.a); cpu.cycles+=2; return;
    case 0xba: cpu.x=cpu.sp; flags(cpu.x); cpu.cycles+=2; return; case 0x9a: cpu.sp=cpu.x; cpu.cycles+=2; return;
    case 0xe8: cpu.x++; flags(cpu.x); cpu.cycles+=2; return; case 0xca: cpu.x--; flags(cpu.x); cpu.cycles+=2; return;
    case 0xc8: cpu.y++; flags(cpu.y); cpu.cycles+=2; return; case 0x88: cpu.y--; flags(cpu.y); cpu.cycles+=2; return;
    default: break;
    }
    switch(op) {
    case 0x85: wr(rd(cpu.pc++),cpu.a); cpu.cycles+=3; return; case 0x95: wr((uint8_t)(rd(cpu.pc++)+cpu.x),cpu.a); cpu.cycles+=4; return;
    case 0x8d: a=word(cpu.pc);cpu.pc+=2;wr(a,cpu.a);cpu.cycles+=4;return; case 0x9d:a=word(cpu.pc);cpu.pc+=2;wr(a+cpu.x,cpu.a);cpu.cycles+=5;return;
    case 0x99:a=word(cpu.pc);cpu.pc+=2;wr(a+cpu.y,cpu.a);cpu.cycles+=5;return; case 0x81:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));wr(a,cpu.a);cpu.cycles+=6;return;
    case 0x91:a=zp(rd(cpu.pc++))+cpu.y;wr(a,cpu.a);cpu.cycles+=6;return;
    case 0x86:wr(rd(cpu.pc++),cpu.x);cpu.cycles+=3;return;case 0x96:wr((uint8_t)(rd(cpu.pc++)+cpu.y),cpu.x);cpu.cycles+=4;return;case 0x8e:a=word(cpu.pc);cpu.pc+=2;wr(a,cpu.x);cpu.cycles+=4;return;
    case 0x84:wr(rd(cpu.pc++),cpu.y);cpu.cycles+=3;return;case 0x94:wr((uint8_t)(rd(cpu.pc++)+cpu.x),cpu.y);cpu.cycles+=4;return;case 0x8c:a=word(cpu.pc);cpu.pc+=2;wr(a,cpu.y);cpu.cycles+=4;return;
    case 0x69:adc(rd(cpu.pc++));cpu.cycles+=2;return;case 0x65:adc(rd(rd(cpu.pc++)));cpu.cycles+=3;return;case 0x75:adc(rd((uint8_t)(rd(cpu.pc++)+cpu.x)));cpu.cycles+=4;return;
    case 0x6d:a=word(cpu.pc);cpu.pc+=2;adc(rd(a));cpu.cycles+=4;return;case 0x7d:a=word(cpu.pc);cpu.pc+=2;adc(rd(a+cpu.x));cpu.cycles+=4;return;case 0x79:a=word(cpu.pc);cpu.pc+=2;adc(rd(a+cpu.y));cpu.cycles+=4;return;
    case 0xe9:adc((uint8_t)~rd(cpu.pc++));cpu.cycles+=2;return;case 0xe5:adc((uint8_t)~rd(rd(cpu.pc++)));cpu.cycles+=3;return;
    case 0xc9:cmp(cpu.a,rd(cpu.pc++));cpu.cycles+=2;return;case 0xc5:cmp(cpu.a,rd(rd(cpu.pc++)));cpu.cycles+=3;return;case 0xcd:a=word(cpu.pc);cpu.pc+=2;cmp(cpu.a,rd(a));cpu.cycles+=4;return;
    case 0xe0:cmp(cpu.x,rd(cpu.pc++));cpu.cycles+=2;return;case 0xc0:cmp(cpu.y,rd(cpu.pc++));cpu.cycles+=2;return;
    case 0x49:cpu.a^=rd(cpu.pc++);flags(cpu.a);cpu.cycles+=2;return;case 0x29:cpu.a&=rd(cpu.pc++);flags(cpu.a);cpu.cycles+=2;return;case 0x09:cpu.a|=rd(cpu.pc++);flags(cpu.a);cpu.cycles+=2;return;
    case 0x01:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));cpu.a|=rd(a);flags(cpu.a);cpu.cycles+=6;return;
    case 0x05:a=rd(cpu.pc++);cpu.a|=rd(a);flags(cpu.a);cpu.cycles+=3;return;
    case 0x0d:a=word(cpu.pc);cpu.pc+=2;cpu.a|=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x11:a=zp(rd(cpu.pc++))+cpu.y;cpu.a|=rd(a);flags(cpu.a);cpu.cycles+=5;return;
    case 0x15:a=(uint8_t)(rd(cpu.pc++)+cpu.x);cpu.a|=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x19:a=word(cpu.pc);cpu.pc+=2;cpu.a|=rd(a+cpu.y);flags(cpu.a);cpu.cycles+=4;return;
    case 0x1d:a=word(cpu.pc);cpu.pc+=2;cpu.a|=rd(a+cpu.x);flags(cpu.a);cpu.cycles+=4;return;
    case 0x21:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));cpu.a&=rd(a);flags(cpu.a);cpu.cycles+=6;return;
    case 0x25:a=rd(cpu.pc++);cpu.a&=rd(a);flags(cpu.a);cpu.cycles+=3;return;
    case 0x2d:a=word(cpu.pc);cpu.pc+=2;cpu.a&=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x31:a=zp(rd(cpu.pc++))+cpu.y;cpu.a&=rd(a);flags(cpu.a);cpu.cycles+=5;return;
    case 0x35:a=(uint8_t)(rd(cpu.pc++)+cpu.x);cpu.a&=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x39:a=word(cpu.pc);cpu.pc+=2;cpu.a&=rd(a+cpu.y);flags(cpu.a);cpu.cycles+=4;return;
    case 0x3d:a=word(cpu.pc);cpu.pc+=2;cpu.a&=rd(a+cpu.x);flags(cpu.a);cpu.cycles+=4;return;
    case 0x41:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));cpu.a^=rd(a);flags(cpu.a);cpu.cycles+=6;return;
    case 0x45:a=rd(cpu.pc++);cpu.a^=rd(a);flags(cpu.a);cpu.cycles+=3;return;
    case 0x4d:a=word(cpu.pc);cpu.pc+=2;cpu.a^=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x51:a=zp(rd(cpu.pc++))+cpu.y;cpu.a^=rd(a);flags(cpu.a);cpu.cycles+=5;return;
    case 0x55:a=(uint8_t)(rd(cpu.pc++)+cpu.x);cpu.a^=rd(a);flags(cpu.a);cpu.cycles+=4;return;
    case 0x59:a=word(cpu.pc);cpu.pc+=2;cpu.a^=rd(a+cpu.y);flags(cpu.a);cpu.cycles+=4;return;
    case 0x5d:a=word(cpu.pc);cpu.pc+=2;cpu.a^=rd(a+cpu.x);flags(cpu.a);cpu.cycles+=4;return;
    case 0x61:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));adc(rd(a));cpu.cycles+=6;return;
    case 0x71:a=zp(rd(cpu.pc++))+cpu.y;adc(rd(a));cpu.cycles+=5;return;
    case 0xc1:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));cmp(cpu.a,rd(a));cpu.cycles+=6;return;
    case 0xd1:a=zp(rd(cpu.pc++))+cpu.y;cmp(cpu.a,rd(a));cpu.cycles+=5;return;
    case 0xd5:a=(uint8_t)(rd(cpu.pc++)+cpu.x);cmp(cpu.a,rd(a));cpu.cycles+=4;return;
    case 0xd9:a=word(cpu.pc);cpu.pc+=2;cmp(cpu.a,rd(a+cpu.y));cpu.cycles+=4;return;
    case 0xdd:a=word(cpu.pc);cpu.pc+=2;cmp(cpu.a,rd(a+cpu.x));cpu.cycles+=4;return;
    case 0xe1:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));adc((uint8_t)~rd(a));cpu.cycles+=6;return;
    case 0xf1:a=zp(rd(cpu.pc++))+cpu.y;adc((uint8_t)~rd(a));cpu.cycles+=5;return;
    case 0xf5:a=(uint8_t)(rd(cpu.pc++)+cpu.x);adc((uint8_t)~rd(a));cpu.cycles+=4;return;
    case 0xf9:a=word(cpu.pc);cpu.pc+=2;adc((uint8_t)~rd(a+cpu.y));cpu.cycles+=4;return;
    case 0xfd:a=word(cpu.pc);cpu.pc+=2;adc((uint8_t)~rd(a+cpu.x));cpu.cycles+=4;return;
    case 0xed:a=word(cpu.pc);cpu.pc+=2;adc((uint8_t)~rd(a));cpu.cycles+=4;return;
    case 0xeb:adc((uint8_t)~rd(cpu.pc++));cpu.cycles+=2;return;
    case 0xc3:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=8;return;
    case 0xc7:a=rd(cpu.pc++);v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=5;return;
    case 0xcf:a=word(cpu.pc);cpu.pc+=2;v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=6;return;
    case 0xd3:a=zp(rd(cpu.pc++))+cpu.y;v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=8;return;
    case 0xd7:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=6;return;
    case 0xdb:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=7;return;
    case 0xdf:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=(uint8_t)(rd(a)-1);wr(a,v);cmp(cpu.a,v);cpu.cycles+=7;return;
    case 0xe3:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=8;return;
    case 0xe7:a=rd(cpu.pc++);v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=5;return;
    case 0xef:a=word(cpu.pc);cpu.pc+=2;v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=6;return;
    case 0xf3:a=zp(rd(cpu.pc++))+cpu.y;v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=8;return;
    case 0xf7:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=6;return;
    case 0xfb:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=7;return;
    case 0xff:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=(uint8_t)(rd(a)+1);wr(a,v);adc((uint8_t)~v);cpu.cycles+=7;return;
    case 0x03:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x07:a=rd(cpu.pc++);v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=5;return;
    case 0x0f:a=word(cpu.pc);cpu.pc+=2;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x13:a=zp(rd(cpu.pc++))+cpu.y;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x17:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x1b:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x1f:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);cpu.a|=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x23:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x27:a=rd(cpu.pc++);r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=5;return;
    case 0x2f:a=word(cpu.pc);cpu.pc+=2;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x33:a=zp(rd(cpu.pc++))+cpu.y;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x37:a=(uint8_t)(rd(cpu.pc++)+cpu.x);r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x3b:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x3f:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);cpu.a&=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x43:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x47:a=rd(cpu.pc++);r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=5;return;
    case 0x4f:a=word(cpu.pc);cpu.pc+=2;r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x53:a=zp(rd(cpu.pc++))+cpu.y;r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=8;return;
    case 0x57:a=(uint8_t)(rd(cpu.pc++)+cpu.x);r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=6;return;
    case 0x5b:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x5f:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;r=rd(a);cpu.c=r&1;v=(uint8_t)(r>>1);wr(a,v);cpu.a^=v;flags(cpu.a);cpu.cycles+=7;return;
    case 0x63:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=8;return;
    case 0x67:a=rd(cpu.pc++);r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=5;return;
    case 0x6f:a=word(cpu.pc);cpu.pc+=2;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=6;return;
    case 0x73:a=zp(rd(cpu.pc++))+cpu.y;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=8;return;
    case 0x77:a=(uint8_t)(rd(cpu.pc++)+cpu.x);r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=6;return;
    case 0x7b:a=word(cpu.pc);cpu.pc+=2;a+=cpu.y;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=7;return;
    case 0x7f:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);adc(v);cpu.cycles+=7;return;
    case 0xe4:a=rd(cpu.pc++);cmp(cpu.x,rd(a));cpu.cycles+=3;return;
    case 0xec:a=word(cpu.pc);cpu.pc+=2;cmp(cpu.x,rd(a));cpu.cycles+=4;return;
    case 0xc4:a=rd(cpu.pc++);cmp(cpu.y,rd(a));cpu.cycles+=3;return;
    case 0xcc:a=word(cpu.pc);cpu.pc+=2;cmp(cpu.y,rd(a));cpu.cycles+=4;return;
    case 0x46:a=rd(cpu.pc++);v=rd(a);cpu.c=v&1;v>>=1;wr(a,v);flags(v);cpu.cycles+=5;return;
    case 0x4e:a=word(cpu.pc);cpu.pc+=2;v=rd(a);cpu.c=v&1;v>>=1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x06:a=rd(cpu.pc++);v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);flags(v);cpu.cycles+=5;return;
    case 0x0e:a=word(cpu.pc);cpu.pc+=2;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x66:a=rd(cpu.pc++);r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);flags(v);cpu.cycles+=5;return;
    case 0x6e:a=word(cpu.pc);cpu.pc+=2;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x56:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=rd(a);cpu.c=v&1;v>>=1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x16:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x76:a=(uint8_t)(rd(cpu.pc++)+cpu.x);r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x36:a=(uint8_t)(rd(cpu.pc++)+cpu.x);r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0xf6:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=(uint8_t)(rd(a)+1);wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0xd6:a=(uint8_t)(rd(cpu.pc++)+cpu.x);v=(uint8_t)(rd(a)-1);wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x5e:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=rd(a);cpu.c=v&1;v>>=1;wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0x1e:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=rd(a);cpu.c=v>>7;v<<=1;wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0x7e:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;r=rd(a);v=(uint8_t)(r>>1|cpu.c<<7);cpu.c=r&1;wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0x3e:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0xfe:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=(uint8_t)(rd(a)+1);wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0xde:a=word(cpu.pc);cpu.pc+=2;a+=cpu.x;v=(uint8_t)(rd(a)-1);wr(a,v);flags(v);cpu.cycles+=7;return;
    case 0x04: case 0x44: case 0x64: cpu.pc++; cpu.cycles+=3; return;
    case 0x0c: cpu.pc+=2; cpu.cycles+=4; return;
    case 0x14: case 0x34: case 0x54: case 0x74: case 0xd4: case 0xf4:
        cpu.pc++; cpu.cycles+=4; return;
    case 0x80: case 0x82: case 0x89: case 0xc2: case 0xe2:
        cpu.pc++; cpu.cycles+=2; return;
    case 0x1c: case 0x3c: case 0x5c: case 0x7c: case 0xdc: case 0xfc:
        a=word(cpu.pc); cpu.pc+=2; { uint16_t e=a+cpu.x; cpu.cycles+=4+((a&0xff00)!=(e&0xff00)); } return;
    case 0xa3:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));v=rd(a);cpu.a=v;cpu.x=v;flags(v);cpu.cycles+=6;return;
    case 0xa7:a=rd(cpu.pc++);v=rd(a);cpu.a=v;cpu.x=v;flags(v);cpu.cycles+=3;return;
    case 0xaf:a=word(cpu.pc);cpu.pc+=2;v=rd(a);cpu.a=v;cpu.x=v;flags(v);cpu.cycles+=4;return;
    case 0xb3:{ uint16_t base=zp(rd(cpu.pc++)); a=base+cpu.y; v=rd(a); cpu.a=v; cpu.x=v; flags(v); cpu.cycles+=5+((base&0xff00)!=(a&0xff00)); return; }
    case 0xb7:a=(uint8_t)(rd(cpu.pc++)+cpu.y);v=rd(a);cpu.a=v;cpu.x=v;flags(v);cpu.cycles+=4;return;
    case 0xbf:a=word(cpu.pc);cpu.pc+=2;v=rd(a+cpu.y);cpu.a=v;cpu.x=v;flags(v);cpu.cycles+=4;return;
    case 0x83:a=zp((uint8_t)(rd(cpu.pc++)+cpu.x));wr(a,cpu.a&cpu.x);cpu.cycles+=6;return;
    case 0x87:a=rd(cpu.pc++);wr(a,cpu.a&cpu.x);cpu.cycles+=3;return;
    case 0x8f:a=word(cpu.pc);cpu.pc+=2;wr(a,cpu.a&cpu.x);cpu.cycles+=4;return;
    case 0x97:a=(uint8_t)(rd(cpu.pc++)+cpu.y);wr(a,cpu.a&cpu.x);cpu.cycles+=4;return;
    case 0x26:a=rd(cpu.pc++);r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);flags(v);cpu.cycles+=5;return;
    case 0x2e:a=word(cpu.pc);cpu.pc+=2;r=rd(a);v=(uint8_t)(r<<1|cpu.c);cpu.c=r>>7;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x24:a=rd(cpu.pc++);v=rd(a);cpu.z=!(cpu.a&v);cpu.n=v>>7;cpu.v=(v>>6)&1;cpu.cycles+=3;return;
    case 0x2c:a=word(cpu.pc);cpu.pc+=2;v=rd(a);cpu.z=!(cpu.a&v);cpu.n=v>>7;cpu.v=(v>>6)&1;cpu.cycles+=4;return;
    default: break;
    }
    switch(op) {
    case 0xc6:a=rd(cpu.pc++);v=rd(a)-1;wr(a,v);flags(v);cpu.cycles+=5;return;case 0xe6:a=rd(cpu.pc++);v=rd(a)+1;wr(a,v);flags(v);cpu.cycles+=5;return;
    case 0xce:a=word(cpu.pc);cpu.pc+=2;v=rd(a)-1;wr(a,v);flags(v);cpu.cycles+=6;return;case 0xee:a=word(cpu.pc);cpu.pc+=2;v=rd(a)+1;wr(a,v);flags(v);cpu.cycles+=6;return;
    case 0x4a:v=cpu.a;cpu.c=v&1;cpu.a=v>>1;flags(cpu.a);cpu.cycles+=2;return;case 0x0a:cpu.c=cpu.a>>7;cpu.a<<=1;flags(cpu.a);cpu.cycles+=2;return;
    case 0x2a:r=(uint8_t)(cpu.a<<1|cpu.c);cpu.c=cpu.a>>7;cpu.a=r;flags(r);cpu.cycles+=2;return;case 0x6a:r=(uint8_t)(cpu.a>>1|cpu.c<<7);cpu.c=cpu.a&1;cpu.a=r;flags(r);cpu.cycles+=2;return;
    default: s=0; break;
    }
    (void)v; (void)s; (void)r; cpu.cycles += 2;
}

void interp_run(uint16_t addr)
{
    cpu.pc = addr;
    for (;;) {
        uint8_t op = bus_read(cpu.pc);
        cpu_step();
        if (op == 0x60 || op == 0x40) return;
    }
}

static void (*registered_dispatch)(uint16_t addr);

void register_dispatch(void (*dispatch)(uint16_t addr))
{
    registered_dispatch = dispatch;
}

void call_addr(uint16_t addr)
{
    if (registered_dispatch != NULL) registered_dispatch(addr);
    else interp_run(addr);
}
