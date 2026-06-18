/* ============================================================================
 *  VOIDRUNNER  --  a galaxy small enough to fit in a rounding error,
 *                  but alive enough to rob you blind.
 *  codename: STARCUNT
 *  Copyright 2026 Antoni Norman.
 *  Licensed under the Apache License, Version 2.0; see examples/LICENSE.
 *
 *  Native Linux procedural space-trading / combat sim, built under the same
 *  demoscene constraint philosophy as .murkk:
 *      - one C game core, no asset files (everything generated at runtime)
 *      - direct dependency: libc.so.6 only
 *      - SDL2 + OpenGL dlopen()'d at runtime (not in NEEDED)
 *      - own sin/cos/sqrt/atan2  ->  no libm either
 *      - raw _start in start_syscall.S, raw syscall exit
 *      - LZMA self-extracting runner via pack.sh
 *
 *  GL 2.1 fixed-function / immediate mode on purpose: no shader compilation,
 *  no runtime shader bugs, tiny code, faceted-lit low-poly look for free.
 *
 *  Build with build.sh. You run it; the container that compiled it has no
 *  display, so gameplay is unverified by the author and verified by you.
 * ==========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  i32;
typedef float    f32;

/* ----- tiny mem (kept explicit; compiler may still emit libc mem* for inits) */
static void* m_set(void* d, int c, u64 n){ u8* p=d; while(n--) *p++=(u8)c; return d; }
static void* m_cpy(void* d, const void* s, u64 n){ u8* a=d; const u8* b=s; while(n--) *a++=*b++; return d; }

/* ============================ math (no libm) ============================== */
#define PI   3.14159265f
#define TAU  6.28318531f
static f32 f_abs(f32 x){ return x<0?-x:x; }
static f32 clampf(f32 x,f32 a,f32 b){ return x<a?a:(x>b?b:x); }
static f32 f_floor(f32 x){ i32 i=(i32)x; f32 fi=(f32)i; return (x<0 && fi!=x)?(fi-1):fi; }
static f32 f_sqrt(f32 x){ if(x<=0) return 0; f32 r; __asm__("sqrtss %1,%0":"=x"(r):"x"(x)); return r; }
/* quadratic sine approx, range-reduced to [-PI,PI] */
static f32 f_sin(f32 x){
    x -= TAU * f_floor((x+PI)/TAU);
    f32 s = 1.27323954f*x - 0.405284735f*x*f_abs(x);
    return 0.225f*(s*f_abs(s)-s)+s;           /* extra precision pass */
}
static f32 f_cos(f32 x){ return f_sin(x+1.5707963f); }
static f32 f_tan(f32 x){ f32 c=f_cos(x); return c==0?0:f_sin(x)/c; }
static f32 f_atan2(f32 y, f32 x){
    f32 ax=f_abs(x), ay=f_abs(y);
    f32 a = (ax>ay)? ay/(ax+1e-9f) : ax/(ay+1e-9f);
    f32 s = a*a;
    f32 r = ((-0.0464964749f*s + 0.15931422f)*s - 0.327622764f)*s*a + a;
    if(ay>ax) r = 1.5707963f - r;
    if(x<0)   r = PI - r;
    if(y<0)   r = -r;
    return r;
}

/* ============================ vec3 ======================================== */
typedef struct { f32 x,y,z; } V3;
static V3 v(f32 a,f32 b,f32 c){ V3 r={a,b,c}; return r; }
static V3 vadd(V3 a,V3 b){ return v(a.x+b.x,a.y+b.y,a.z+b.z); }
static V3 vsub(V3 a,V3 b){ return v(a.x-b.x,a.y-b.y,a.z-b.z); }
static V3 vmul(V3 a,f32 s){ return v(a.x*s,a.y*s,a.z*s); }
static f32 vdot(V3 a,V3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
static V3 vcross(V3 a,V3 b){ return v(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static f32 vlen(V3 a){ return f_sqrt(vdot(a,a)); }
static V3 vnorm(V3 a){ f32 l=vlen(a); return l>1e-6f? vmul(a,1.0f/l) : a; }

/* ============================ PRNG ======================================== */
static u32 g_rng = 0x1337c0de;
static u32 xr(void){ u32 x=g_rng; x^=x<<13; x^=x>>17; x^=x<<5; g_rng=x; return x; }
static void seed(u32 s){ g_rng = s?s:0xC0FFEE; }
static f32 frand(void){ return (f32)(xr() & 0xFFFFFF) / 16777215.0f; }     /* [0,1] */
static f32 srand2(void){ return frand()*2.0f-1.0f; }                        /* [-1,1] */
static u32 hash32(u32 a){ a^=a>>16; a*=0x7feb352d; a^=a>>15; a*=0x846ca68b; a^=a>>16; return a; }

/* ====================== GL / SDL function loading ========================= */
/* We declare exactly what we call. GL doubles where the spec says GLdouble. */
#define FN(ret,name,args) typedef ret(*name##_t)args; static name##_t p_##name;

/* --- GL --- */
FN(void, glClearColor,(f32,f32,f32,f32))
FN(void, glClear,(u32))
FN(void, glEnable,(u32))
FN(void, glDisable,(u32))
FN(void, glViewport,(i32,i32,i32,i32))
FN(void, glMatrixMode,(u32))
FN(void, glLoadIdentity,(void))
FN(void, glLoadMatrixf,(const f32*))
FN(void, glPushMatrix,(void))
FN(void, glPopMatrix,(void))
FN(void, glTranslatef,(f32,f32,f32))
FN(void, glRotatef,(f32,f32,f32,f32))
FN(void, glScalef,(f32,f32,f32))
FN(void, glFrustum,(double,double,double,double,double,double))
FN(void, glOrtho,(double,double,double,double,double,double))
FN(void, glBegin,(u32))
FN(void, glEnd,(void))
FN(void, glVertex3f,(f32,f32,f32))
FN(void, glVertex2f,(f32,f32))
FN(void, glColor3f,(f32,f32,f32))
FN(void, glColor4f,(f32,f32,f32,f32))
FN(void, glNormal3f,(f32,f32,f32))
FN(void, glTexCoord2f,(f32,f32))
FN(void, glGenTextures,(i32,u32*))
FN(void, glBindTexture,(u32,u32))
FN(void, glTexParameteri,(u32,u32,i32))
FN(void, glTexImage2D,(u32,i32,i32,i32,i32,i32,u32,u32,const void*))
FN(void, glPointSize,(f32))
FN(void, glLineWidth,(f32))
FN(void, glDepthFunc,(u32))
FN(void, glDepthMask,(u8))
FN(void, glBlendFunc,(u32,u32))
FN(void, glShadeModel,(u32))
FN(void, glHint,(u32,u32))
FN(void, glLightfv,(u32,u32,const f32*))
FN(void, glLightModelfv,(u32,const f32*))
FN(void, glColorMaterial,(u32,u32))

/* --- SDL2 --- */
FN(int,   SDL_Init,(u32))
FN(void,  SDL_Quit,(void))
FN(int,   SDL_GL_SetAttribute,(int,int))
FN(void*, SDL_CreateWindow,(const char*,int,int,int,int,u32))
FN(void,  SDL_DestroyWindow,(void*))
FN(void*, SDL_GL_CreateContext,(void*))
FN(void,  SDL_GL_DeleteContext,(void*))
FN(int,   SDL_GL_SetSwapInterval,(int))
FN(void,  SDL_GL_SwapWindow,(void*))
FN(int,   SDL_PollEvent,(void*))
FN(int,   SDL_SetWindowFullscreen,(void*,u32))
FN(u32,   SDL_GetRelativeMouseState,(int*,int*))
FN(int,   SDL_SetRelativeMouseMode,(int))
FN(const u8*, SDL_GetKeyboardState,(int*))
FN(u32,   SDL_GetTicks,(void))
FN(void,  SDL_Delay,(u32))
FN(int,   SDL_ShowCursor,(int))
FN(u32,   SDL_OpenAudioDevice,(const char*,int,const void*,void*,int))
FN(void,  SDL_PauseAudioDevice,(u32,int))

static void* gl_lib; static void* sdl_lib;
#define LGL(n)  do{ p_##n=(n##_t)dlsym(gl_lib,#n);  if(!p_##n) miss++; }while(0)
#define LSDL(n) do{ p_##n=(n##_t)dlsym(sdl_lib,#n); if(!p_##n) miss++; }while(0)

/* GL enums */
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_DEPTH_TEST       0x0B71
#define GL_BLEND            0x0BE2
#define GL_LIGHTING         0x0B50
#define GL_LIGHT0           0x4000
#define GL_COLOR_MATERIAL   0x0B57
#define GL_PROJECTION       0x1701
#define GL_MODELVIEW        0x1700
#define GL_POINTS           0x0000
#define GL_LINES            0x0001
#define GL_LINE_LOOP        0x0002
#define GL_TRIANGLES        0x0004
#define GL_TRIANGLE_FAN     0x0006
#define GL_QUADS            0x0007
#define GL_POSITION         0x1203
#define GL_DIFFUSE          0x1201
#define GL_AMBIENT          0x1200
#define GL_FRONT_AND_BACK   0x0408
#define GL_AMBIENT_AND_DIFFUSE 0x1602
#define GL_SRC_ALPHA        0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE              0x0001
#define GL_FLAT             0x1D00
#define GL_LESS             0x0201
#define GL_LEQUAL           0x0203
#define GL_LIGHT_MODEL_AMBIENT 0x0B53
#define GL_LINE_SMOOTH      0x0B20
#define GL_LINE_SMOOTH_HINT 0x0C52
#define GL_NICEST           0x1102
#define GL_TEXTURE_2D       0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR           0x2601
#define GL_RGB              0x1907
#define GL_UNSIGNED_BYTE    0x1401

/* SDL enums */
#define SDL_INIT_VIDEO 0x00000020u
#define SDL_INIT_AUDIO 0x00000010u
#define SDL_WINDOW_OPENGL 0x00000002u
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x00001001u
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000
#define SDL_GL_RED_SIZE 0
#define SDL_GL_GREEN_SIZE 1
#define SDL_GL_BLUE_SIZE 2
#define SDL_GL_DOUBLEBUFFER 5
#define SDL_GL_DEPTH_SIZE 6
#define SDL_GL_MULTISAMPLEBUFFERS 13
#define SDL_GL_MULTISAMPLESAMPLES 14
#define SDL_GL_CONTEXT_MAJOR_VERSION 17
#define SDL_GL_CONTEXT_MINOR_VERSION 18
#define SDL_GL_CONTEXT_PROFILE_MASK 21
#define SDL_GL_CONTEXT_PROFILE_COMPATIBILITY 0x0002
#define SDL_QUIT 0x100
#define SDL_WINDOWEVENT 0x200
#define SDL_WINDOWEVENT_SIZE_CHANGED 5
#define SDL_BUTTON_LMASK 1
#define SDL_BUTTON_RMASK 4
#define AUDIO_S16SYS 0x8010

/* SDL scancodes (USB HID) */
#define SC_A 4
#define SC_D 7
#define SC_E 8
#define SC_F 9
#define SC_J 13
#define SC_M 16
#define SC_Q 20
#define SC_S 22
#define SC_W 26
#define SC_1 30
#define SC_2 31
#define SC_3 32
#define SC_4 33
#define SC_5 34
#define SC_7 36
#define SC_8 37
#define SC_9 38
#define SC_0 39
#define SC_RETURN 40
#define SC_ESC 41
#define SC_BACKSPACE 42
#define SC_F11 68
#define SC_RIGHT 79
#define SC_LEFT 80
#define SC_DOWN 81
#define SC_UP 82
#define SC_LSHIFT 225

/* SDL_AudioSpec layout (must match the ABI) */
typedef struct {
    int freq; u16 format; u8 channels; u8 silence; u16 samples; u16 padding;
    u32 size; void(*callback)(void*,u8*,int); void* userdata;
} AudioSpec;

/* ============================ GL helpers ================================== */
static void face3(V3 a, V3 b, V3 c){
    V3 n = vnorm(vcross(vsub(b,a), vsub(c,a)));
    p_glNormal3f(n.x,n.y,n.z);
    p_glVertex3f(a.x,a.y,a.z); p_glVertex3f(b.x,b.y,b.z); p_glVertex3f(c.x,c.y,c.z);
}
/* quad as two tris (a,b,c,d wound) */
static void face4(V3 a,V3 b,V3 c,V3 d){ face3(a,b,c); face3(a,c,d); }

/* ============================ pixel terminal font ========================== */
/* Clean generated 5x7 bitmap font. No assets, just tiny packed rows.          */
typedef struct { char c; u8 r[7]; } BGlyph;
static const BGlyph BFONT[]={
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}},    {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}},  {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,10,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
    {'Y',{17,17,10,4,4,4,4}},    {'Z',{31,1,2,4,8,16,31}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}},    {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}},    {'5',{31,16,16,30,1,1,30}},
    {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
    {'.',{0,0,0,0,0,12,12}},     {',',{0,0,0,0,0,12,4}},
    {'-',{0,0,0,31,0,0,0}},      {'_',{0,0,0,0,0,0,31}},
    {'/',{1,1,2,4,8,16,16}},     {':',{0,12,12,0,12,12,0}},
    {'%',{24,25,2,4,8,19,3}},    {'+',{0,4,4,31,4,4,0}},
    {'>',{16,8,4,2,4,8,16}},     {'<',{1,2,4,8,4,2,1}},
    {'=',{0,0,31,0,31,0,0}},     {'?',{14,17,1,2,4,0,4}},
    {'(',{2,4,8,8,8,4,2}},       {')',{8,4,2,2,2,4,8}},
};
static const BGlyph* bglyph(char c){
    if(c>='a'&&c<='z') c-=32;
    for(u32 i=0;i<sizeof(BFONT)/sizeof(BFONT[0]);i++) if(BFONT[i].c==c) return &BFONT[i];
    return 0;
}
static void text(f32 px, f32 py, f32 sc, const char* s, f32 r,f32 g,f32 b){
    p_glColor3f(r,g,b);
    f32 x=px, cell=sc*0.94f;
    p_glBegin(GL_QUADS);
    for(; *s; s++){
        char c=*s;
        if(c==' '){ x += 4*sc; continue; }
        const BGlyph* gl=bglyph(c);
        if(gl){
            for(int yy=0; yy<7; yy++){
                u8 row=gl->r[yy];
                for(int xx=0; xx<5; xx++) if(row & (1<<(4-xx))){
                    f32 ax=x+xx*sc, ay=py+yy*sc;
                    p_glVertex2f(ax,ay);      p_glVertex2f(ax+cell,ay);
                    p_glVertex2f(ax+cell,ay+cell); p_glVertex2f(ax,ay+cell);
                }
            }
        }
        x += 6*sc;
    }
    p_glEnd();
}
/* int -> string (no stdio); returns ptr into shared buffer */
static char numbuf[32];
static char* itos(i32 n){
    char tmp[16]; int i=0; int neg=n<0; u32 u = neg? (u32)(-(int64_t)n) : (u32)n;
    if(u==0) tmp[i++]='0';
    while(u){ tmp[i++]='0'+(u%10); u/=10; }
    int j=0; if(neg) numbuf[j++]='-';
    while(i) numbuf[j++]=tmp[--i];
    numbuf[j]=0; return numbuf;
}
static f32 textw(const char* s, f32 sc){
    f32 w=0; while(*s){ w += (*s==' ')?4*sc:6*sc; s++; } return w;
}
static void bcat(char* b,int* k,const char* s){ while(*s) b[(*k)++]=*s++; b[*k]=0; }
static void bnum(char* b,int* k,i32 n){ char* a=itos(n); int j=0; while(a[j]) b[(*k)++]=a[j++]; b[*k]=0; }
static void label_num(f32 x,f32 y,f32 sc,const char* lab,i32 n,f32 r,f32 g,f32 bl){ char b[40]; int k=0; bcat(b,&k,lab); bcat(b,&k," "); bnum(b,&k,n); text(x,y,sc,b,r,g,bl); }


/* ============================ galaxy / economy ============================ */
#define NSYS 256
enum { ECO_AGRI, ECO_IND, ECO_MINE, ECO_TECH, ECO_REFINE, ECO_LAWLESS, ECO_N };
enum { GOV_ANARCHY, GOV_FEUDAL, GOV_CORP, GOV_DEMOCRACY, GOV_CONFED, GOV_N };
#define NCOM 6
static const char* COM[NCOM] = {"FOOD","ORE","MACHINERY","MEDICINE","NARCOTICS","WEAPONS"};
static const i32   COMBASE[NCOM] = { 14, 22, 90, 110, 180, 240 };
/* price multiplier x100 per economy per commodity (sells cheap where produced) */
static const i32 PRICEMUL[ECO_N][NCOM] = {
    /*            FOOD  ORE  MACH  MED  NARC  WEAP */
    /* AGRI   */ { 60, 120, 130, 110, 140, 130 },
    /* IND    */ {130,  90,  60, 100, 130, 105 },
    /* MINE   */ {120,  55, 115, 130, 130, 120 },
    /* TECH   */ {115, 130,  90,  65, 120,  70 },
    /* REFINE */ {110,  70,  95, 115, 120, 110 },
    /* LAWLESS*/ {125, 110, 120, 120,  55,  60 },
};
static const char* ECONAME[ECO_N]={"AGRICULTURAL","INDUSTRIAL","MINING","HIGH TECH","REFINERY","LAWLESS"};
static const char* GOVNAME[GOV_N]={"ANARCHY","FEUDAL","CORPORATE","DEMOCRACY","CONFED"};

typedef struct {
    u32 seed;
    char name[12];
    f32 x,y,z;          /* galactic position, light-years-ish */
    u8 eco, gov;
    u8 danger;          /* 0..9 */
    u8 tech;            /* 1..12 */
    i32 price[NCOM];     /* unit price in this market */
} System;
static System gal[NSYS];

static const char* SYL[]={"ze","ti","ra","mur","kk","va","need","ior","za","keth","lave","or","sol","tre","gan","vox","is","an","el","dru","nub","qui","tor","yss","ben","ula","rho","tan","kai","wen"};
static void mkname(char* out, u32 s){
    seed(s);
    int parts = 2 + (xr()%2);
    int o=0;
    for(int i=0;i<parts && o<9;i++){
        const char* p = SYL[xr()%(sizeof(SYL)/sizeof(SYL[0]))];
        for(int k=0; p[k] && o<10; k++) out[o++] = (i==0&&k==0)? (p[k]-32) : p[k];
    }
    out[o]=0;
}
static void price_system(System* s){
    seed(hash32(s->seed ^ 0xA5A5));
    for(int c=0;c<NCOM;c++){
        i32 base = COMBASE[c];
        i32 mul  = PRICEMUL[s->eco][c];
        i32 jitter = 85 + (i32)(xr()%30);     /* +/- local noise */
        s->price[c] = (base*mul/100)*jitter/100;
        if(s->price[c]<1) s->price[c]=1;
    }
}
static void gen_galaxy(u32 master){
    seed(master);
    for(int i=0;i<NSYS;i++){
        System* s=&gal[i];
        s->seed = xr();
        u32 h = hash32(s->seed);
        s->x = (f32)(h%1000)*0.1f; h/=1000;
        s->y = (f32)(h%1000)*0.1f; h/=1000;
        s->z = (f32)(hash32(s->seed^0x9E37)%1000)*0.1f;
        u32 hh = hash32(s->seed^0xBEEF);
        s->eco = hh%ECO_N; hh/=ECO_N;
        s->gov = hh%GOV_N; hh/=GOV_N;
        /* lawless / anarchy => more dangerous */
        s->danger = (hh%6) + (s->eco==ECO_LAWLESS?3:0) + (s->gov==GOV_ANARCHY?2:0);
        if(s->danger>9) s->danger=9;
        s->tech = 1 + (hh/6)%12;
        mkname(s->name, s->seed);
        price_system(s);
    }

    /* Starter pocket: the Elite-like 100CR opening only works if the first
     *      local chart has affordable, readable trades. Keep this deterministic. */
    {
        System* a=&gal[0];
        a->seed=hash32(master^0x101); mkname(a->name,a->seed);
        a->x=50.0f; a->y=50.0f; a->z=50.0f; a->eco=ECO_AGRI; a->gov=GOV_DEMOCRACY; a->danger=1; a->tech=3;
        a->price[0]=6;  a->price[1]=18; a->price[2]=118; a->price[3]=105; a->price[4]=172; a->price[5]=216;
        System* b=&gal[1];
        b->seed=hash32(master^0x202); mkname(b->name,b->seed);
        b->x=62.0f; b->y=51.0f; b->z=53.0f; b->eco=ECO_IND; b->gov=GOV_CORP; b->danger=1; b->tech=6;
        b->price[0]=18; b->price[1]=24; b->price[2]=58;  b->price[3]=96;  b->price[4]=166; b->price[5]=198;
        System* c=&gal[2];
        c->seed=hash32(master^0x303); mkname(c->name,c->seed);
        c->x=43.0f; c->y=49.0f; c->z=64.0f; c->eco=ECO_MINE; c->gov=GOV_CONFED; c->danger=3; c->tech=4;
        c->price[0]=15; c->price[1]=10; c->price[2]=132; c->price[3]=126; c->price[4]=182; c->price[5]=226;
        System* d=&gal[3];
        d->seed=hash32(master^0x404); mkname(d->name,d->seed);
        d->x=58.0f; d->y=48.0f; d->z=36.0f; d->eco=ECO_TECH; d->gov=GOV_DEMOCRACY; d->danger=2; d->tech=10;
        d->price[0]=17; d->price[1]=34; d->price[2]=86;  d->price[3]=64;  d->price[4]=158; d->price[5]=92;
        System* e=&gal[4];
        e->seed=hash32(master^0x505); mkname(e->name,e->seed);
        e->x=35.0f; e->y=52.0f; e->z=44.0f; e->eco=ECO_LAWLESS; e->gov=GOV_ANARCHY; e->danger=8; e->tech=5;
        e->price[0]=21; e->price[1]=28; e->price[2]=150; e->price[3]=142; e->price[4]=70;  e->price[5]=82;
    }
}


/* Generated planet textures: still tiny, but now per-body so a system can have
 *  several planets/moons without all sharing the same skin. */
static void planet_base_rgb(int pt, f32* r, f32* g, f32* b){
    *r = pt==0?0.12f:pt==1?0.55f:pt==2?0.22f:pt==3?0.45f:0.48f;
    *g = pt==0?0.28f:pt==1?0.30f:pt==2?0.50f:pt==3?0.47f:0.30f;
    *b = pt==0?0.72f:pt==1?0.12f:pt==2?0.25f:pt==3?0.52f:0.62f;
}
static u32 make_planet_tex(u32 ps, int pt){
    u8 img[64*64*3];
    f32 base_r,base_g,base_b; planet_base_rgb(pt,&base_r,&base_g,&base_b);
    for(int y=0;y<64;y++) for(int x=0;x<64;x++){
        f32 fy=(f32)y/64.0f;
        f32 n=0, amp=1.0f;
        for(int o=0;o<5;o++,amp*=0.52f){
            u32 h=hash32(ps ^ (x*(73+o*11)) ^ (y*(151+o*17)) ^ (o*0x9E37));
            n += amp*((f32)(h&255)/255.0f);
        }
        n = n*0.42f + 0.20f*f_sin(fy*TAU*(pt==4?7.0f:3.0f) + (f32)(ps&255)*0.01f);
        f32 ice = (fy<0.12f || fy>0.88f) ? 0.55f : 0.0f;
        f32 land = n>0.58f ? 0.45f : 0.0f;
        f32 br=base_r,bg=base_g,bb=base_b;
        if(pt==0){ br += land*0.28f + ice; bg += land*0.22f + ice; bb += ice; }
        if(pt==2){ br += land*0.24f + ice; bg += land*0.14f + ice; bb += land*0.04f + ice; }
        if(pt==4){ br += 0.08f*n; bg += 0.06f*f_abs(f_sin(fy*TAU*5)); bb += 0.12f*f_abs(f_sin(fy*TAU*3)); }
        int k=(y*64+x)*3;
        img[k+0]=(u8)(255.0f*clampf(br*(0.55f+n),0,1));
        img[k+1]=(u8)(255.0f*clampf(bg*(0.55f+n),0,1));
        img[k+2]=(u8)(255.0f*clampf(bb*(0.55f+n),0,1));
    }
    u32 tex=0; p_glGenTextures(1,&tex);
    p_glBindTexture(GL_TEXTURE_2D,tex);
    p_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    p_glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    p_glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,64,64,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    return tex;
}

/* ============================ ship archetypes ============================= */
enum { SHIP_TRADER, SHIP_FIGHTER, SHIP_MINER, SHIP_POLICE, SHIP_PIRATE, SHIP_ALIEN, SHIP_FREIGHTER, SHIP_PLAYER };
static const char* ship_name(int t){
    switch(t){
        case SHIP_TRADER:   return "TRADER";
        case SHIP_FIGHTER:  return "HUNTER";
        case SHIP_MINER:    return "MINER";
        case SHIP_POLICE:   return "POLICE";
        case SHIP_PIRATE:   return "PIRATE";
        case SHIP_ALIEN:    return "UNKNOWN";
        case SHIP_FREIGHTER:return "FREIGHTER";
        default:            return "SHIP";
    }
}
static void ship_rgb(int t,f32* r,f32* g,f32* b){
    switch(t){
        case SHIP_TRADER:   *r=0.35f; *g=0.92f; *b=0.48f; break;  /* green neutral */
        case SHIP_FREIGHTER:*r=0.48f; *g=0.86f; *b=0.62f; break;  /* fat trader */
        case SHIP_FIGHTER:  *r=0.95f; *g=0.42f; *b=0.90f; break;  /* hunter magenta */
        case SHIP_MINER:    *r=0.95f; *g=0.68f; *b=0.24f; break;  /* amber industrial */
        case SHIP_POLICE:   *r=0.38f; *g=0.70f; *b=1.00f; break;  /* blue-white */
        case SHIP_PIRATE:   *r=1.00f; *g=0.24f; *b=0.18f; break;  /* red hostile */
        case SHIP_ALIEN:    *r=0.42f; *g=1.00f; *b=0.82f; break;  /* weird cyan/green */
        default:            *r=0.75f; *g=0.78f; *b=0.82f; break;
    }
}
static void ship_color(int t){ f32 r,g,b; ship_rgb(t,&r,&g,&b); p_glColor3f(r*0.72f,g*0.72f,b*0.72f); }
static void emit_ship(int t){
    /* Compact role templates. No fake seed-body variation: radar/tags carry identity,
     *      these shapes stay cheap and readable at flight distances. */
    p_glBegin(GL_TRIANGLES);
    if(t==SHIP_FIGHTER || t==SHIP_PLAYER || t==SHIP_POLICE){
        V3 nose=v(0,0,2.4f), tl=v(-1.4f,-0.3f,-1.4f), tr=v(1.4f,-0.3f,-1.4f);
        V3 top=v(0,0.7f,-0.9f), bb=v(0,-0.5f,-1.2f);
        face3(nose,tr,top); face3(nose,top,tl);
        face3(nose,tl,bb);  face3(nose,bb,tr);
        face3(tl,top,tr);   face3(tl,tr,bb);
        face3(v(-1.4f,-0.2f,-0.8f),v(-2.6f,0,-1.5f),v(-1.2f,0,-1.6f));
        face3(v( 1.4f,-0.2f,-0.8f),v( 1.2f,0,-1.6f),v( 2.6f,0,-1.5f));
        if(t==SHIP_FIGHTER){
            face3(v(0,0.25f,-1.2f),v(-0.45f,1.15f,-2.1f),v(0.45f,1.15f,-2.1f));
            face3(v(0,-0.25f,-1.3f),v(-0.35f,-1.05f,-2.0f),v(0.35f,-1.05f,-2.0f));
        }
    } else if(t==SHIP_TRADER){
        V3 a=v(-1.0f,-0.8f,2.0f),b=v(1.0f,-0.8f,2.0f),c=v(1.2f,0.9f,1.6f),d=v(-1.2f,0.9f,1.6f);
        V3 e=v(-1.3f,-1.0f,-2.0f),f=v(1.3f,-1.0f,-2.0f),g=v(1.5f,1.1f,-2.0f),h=v(-1.5f,1.1f,-2.0f);
        face4(a,b,c,d); face4(b,f,g,c); face4(f,e,h,g); face4(e,a,d,h);
        face4(d,c,g,h); face4(e,f,b,a);
    } else if(t==SHIP_FREIGHTER){
        V3 a=v(-0.8f,-0.55f,3.0f),b=v(0.8f,-0.55f,3.0f),c=v(0.9f,0.65f,3.0f),d=v(-0.9f,0.65f,3.0f);
        V3 e=v(-1.1f,-0.75f,-3.2f),f=v(1.1f,-0.75f,-3.2f),g=v(1.2f,0.85f,-3.2f),h=v(-1.2f,0.85f,-3.2f);
        face4(a,b,c,d); face4(b,f,g,c); face4(f,e,h,g); face4(e,a,d,h); face4(d,c,g,h); face4(e,f,b,a);
        for(int j=0;j<3;j++){ f32 z=1.3f-j*1.55f;
            face4(v(-1.95f,-0.50f,z+0.45f),v(-1.10f,-0.50f,z+0.45f),v(-1.10f,0.52f,z+0.45f),v(-1.95f,0.52f,z+0.45f));
            face4(v( 1.10f,-0.50f,z+0.45f),v( 1.95f,-0.50f,z+0.45f),v( 1.95f,0.52f,z+0.45f),v( 1.10f,0.52f,z+0.45f));
        }
    } else if(t==SHIP_MINER){
        V3 a=v(-1.2f,-1.2f,1.6f),b=v(1.2f,-1.2f,1.6f),c=v(1.2f,1.2f,1.6f),d=v(-1.2f,1.2f,1.6f);
        V3 e=v(-1.4f,-1.4f,-1.8f),f=v(1.4f,-1.4f,-1.8f),g=v(1.4f,1.4f,-1.8f),h=v(-1.4f,1.4f,-1.8f);
        face4(a,b,c,d); face4(b,f,g,c); face4(f,e,h,g); face4(e,a,d,h);
        face4(d,c,g,h); face4(e,f,b,a);
        face3(v(0,0,1.6f),v(0.4f,0,3.0f),v(-0.4f,0,3.0f));
    } else if(t==SHIP_PIRATE){
        V3 nose=v(0.3f,0.1f,2.6f), tl=v(-1.6f,-0.4f,-1.2f), tr=v(1.1f,-0.5f,-1.5f);
        V3 top=v(-0.2f,0.9f,-0.6f), bb=v(0.1f,-0.6f,-1.0f);
        face3(nose,tr,top); face3(nose,top,tl); face3(nose,tl,bb); face3(nose,bb,tr);
        face3(tl,top,tr); face3(tl,tr,bb);
        face3(v(-1.6f,-0.4f,-1.0f),v(-3.0f,0.4f,-2.0f),v(-1.4f,0.2f,-1.8f));
        face3(v(1.1f,-0.5f,-1.2f),v(1.0f,0.2f,-1.9f),v(2.2f,-0.2f,-2.4f));
    } else { /* UNKNOWN */
        for(int i=0;i<6;i++){
            f32 a0=(f32)i/6.0f*TAU, a1=(f32)(i+1)/6.0f*TAU;
            V3 p0=v(f_cos(a0)*1.2f, f_sin(a0)*0.6f, -1.2f);
            V3 p1=v(f_cos(a1)*1.2f, f_sin(a1)*0.6f, -1.2f);
            face3(v(0,0,2.2f), p0, p1);
            face3(v(0,0,-2.0f), p1, p0);
        }
    }
    p_glEnd();
}

/* UV sphere (planet / star), radius 1, lit. lat x lon segments */
static void emit_sphere(int lat, int lon){
    for(int i=0;i<lat;i++){
        f32 t0=PI*((f32)i/lat - 0.5f), t1=PI*((f32)(i+1)/lat - 0.5f);
        f32 ct0=f_cos(t0),st0=f_sin(t0),ct1=f_cos(t1),st1=f_sin(t1);
        p_glBegin(GL_QUADS);
        for(int j=0;j<lon;j++){
            f32 p0=TAU*(f32)j/lon, p1=TAU*(f32)(j+1)/lon;
            f32 cp0=f_cos(p0),sp0=f_sin(p0),cp1=f_cos(p1),sp1=f_sin(p1);
            V3 a=v(ct0*cp0,st0,ct0*sp0), b=v(ct1*cp0,st1,ct1*sp0);
            V3 c=v(ct1*cp1,st1,ct1*sp1), d=v(ct0*cp1,st0,ct0*sp1);
            f32 u0=(f32)j/lon, u1=(f32)(j+1)/lon, v0=(f32)i/lat, v1=(f32)(i+1)/lat;
            p_glNormal3f(a.x,a.y,a.z); p_glTexCoord2f(u0,v0); p_glVertex3f(a.x,a.y,a.z);
            p_glNormal3f(b.x,b.y,b.z); p_glTexCoord2f(u0,v1); p_glVertex3f(b.x,b.y,b.z);
            p_glNormal3f(c.x,c.y,c.z); p_glTexCoord2f(u1,v1); p_glVertex3f(c.x,c.y,c.z);
            p_glNormal3f(d.x,d.y,d.z); p_glTexCoord2f(u1,v0); p_glVertex3f(d.x,d.y,d.z);
        }
        p_glEnd();
    }
}

/* ============================ game state ================================== */
enum { ST_TITLE, ST_FLIGHT, ST_MARKET, ST_STATUS, ST_DATA, ST_MANIFEST, ST_EQUIP, ST_MAP, ST_DEAD, ST_JUMP };
typedef struct {
    V3 pos, vel; f32 hull, maxhull; int type; int alive;
    f32 fire_cd, radar_flash; int target;            /* AI */
} Ship;
#define MAXE 10
typedef struct {
    V3 a,b; f32 ttl; int hostile;        /* laser tracer */
} Beam;
#define MAXB 40
typedef struct {
    V3 pos, vel; f32 ttl; int target; int alive;
} Missile;
#define MAXM 6
typedef struct { f32 t,d,p,f,a; int type; } Sfx;
#define MAXSFX 12
enum { SFX_LASER, SFX_MISSILE, SFX_HIT, SFX_KILL, SFX_DOCK, SFX_LAUNCH, SFX_JUMP, SFX_BUY, SFX_WARN, SFX_DAMAGE };
#define MAXBOD 12
typedef struct {
    V3 pos;
    f32 r;
    int type;
    int has_ring, has_atmo;
    int parent;
    f32 spin, phase;
    u32 tex;
} Body;
typedef struct {
    V3 pos; f32 r; f32 cr,cg,cb;
} Star;

static struct {
    int  state;
    int  cur;                /* current system index */
    /* player */
    V3   ppos, pvel, pr, pu, pf;
    f32  hull, maxhull, shield, maxshield, shield_cd;
    i32  credits, fuel, maxfuel, cargo_max;
    i32  cargo[NCOM];
    f32  jump_range;
    int  wanted;
    f32  fire_cd, muzzle_flash, laser_heat, cabin_temp, heat_warn_cd;
    int  laser_lockout;
    /* scene objects (per system) */
    Star star[2]; int nstar;
    Body body[MAXBOD]; int nb; int hab_body;
    V3   station; int station_type; f32 station_scale, station_spin;
    Ship e[MAXE]; int ne;
    Beam beam[MAXB];
    Missile missile[MAXM];
    int  target;             /* targeted enemy index, -1 none */
    int  docked_ok;
    int  docked, alert_code, map_back_state; f32 alert_t;
    int  kills, legal;          /* Elite-style progression */
    int  sys_kills[NSYS];
    int  missiles, laser_level, extra_energy;
    /* market/map cursor */
    int  msel;               /* selected commodity row */
    int  esel;               /* selected equipment row */
    int  mapsel;             /* selected system in map */
    f32  t;                  /* elapsed seconds */
    int  win_w, win_h;
    /* audio */
    f32  audio_phase; f32 audio_target_amp; f32 music_t, music_amp; u32 audio_n, adev;
    Sfx  sfx[MAXSFX];
} G;

static char cmdr_name[12]="PINGUY";
static char title_name[12]="PINGUY";
static u32  title_default_seed=0;

static void strset_lim(char* d,const char* s,int cap){ int i=0; while(s[i]&&i<cap-1){ d[i]=s[i]; i++; } d[i]=0; }
static int slen_local(const char* s){ int i=0; while(s[i]) i++; return i; }
static void title_add(char* s,int cap,char c){ int n=slen_local(s); if(n<cap-1){ s[n]=c; s[n+1]=0; } }
static void title_back(char* s){ int n=slen_local(s); if(n>0) s[n-1]=0; }

static void sfx_play(int type){
    static const f32 dur[]={0.12f,0.55f,0.18f,0.46f,0.55f,0.38f,1.05f,0.08f,0.34f,0.20f};
    static const f32 fre[]={880,160,90,55,95,70,80,720,520,65};
    static const f32 amp[]={0.42f,0.38f,0.34f,0.55f,0.42f,0.32f,0.48f,0.22f,0.36f,0.40f};
    int slot=0; f32 oldest=-1;
    for(int i=0;i<MAXSFX;i++){ if(G.sfx[i].t<=0){ slot=i; break; } if(G.sfx[i].t>oldest){ oldest=G.sfx[i].t; slot=i; } }
    G.sfx[slot].type=type; G.sfx[slot].t=dur[type]; G.sfx[slot].d=dur[type];
    G.sfx[slot].f=fre[type]; G.sfx[slot].a=amp[type]; G.sfx[slot].p=0;
}

/* shared generator pool: weapons/boost use ENERGY but never directly drain shields.
 *  ENERGY instead changes shield efficiency when a hit lands. */
static f32 energy_frac(void){ return G.maxhull>0 ? clampf(G.hull/G.maxhull,0,1) : 0; }
static void energy_spend_soft(f32 a){ G.hull-=a; if(G.hull<8.0f) G.hull=8.0f; }
static int energy_try_spend(f32 a){ if(G.hull <= a+8.0f) return 0; G.hull-=a; return 1; }
static f32 shield_hit_scale(void){ return 1.65f - 0.90f*energy_frac(); } /* full energy 0.75x, dry reactor 1.65x */
static f32 laser_energy_cost(void){
    f32 base = G.laser_level>=3 ? 4.2f : (G.laser_level==2 ? 2.7f : 1.35f);
    return base*(1.0f + G.laser_heat*1.55f);
}

/* ---- enemy gunnery tunables (aim-cone + graze model) ----------------------
 *  These are the only dials. Behaviour rules (approach/orbit/flee) are untouched.
 *    EFIRE_RANGE : enemies open fire inside this range (unchanged from old gate)
 *    ETRACK      : seconds of tracking lag -> bigger = a fast crosser spoils aim
 *    PLAYER_R    : player hull radius the shot has to graze to count as a hit  */
#define EFIRE_RANGE 800.0f
#define ETRACK      0.12f
#define PLAYER_R    15.0f

/* spawn the local scene from a system seed */
static V3 body_draw_pos(int i){
    Body* b=&G.body[i];
    if(b->parent<0) return b->pos;
    V3 p=body_draw_pos(b->parent);
    return vadd(p,b->pos);
}

static void choose_star_color(int k, f32* r, f32* g, f32* b){
    switch(k%5){
        case 0: *r=1.00f; *g=0.92f; *b=0.68f; break;
        case 1: *r=1.00f; *g=0.72f; *b=0.45f; break;
        case 2: *r=0.85f; *g=0.92f; *b=1.00f; break;
        case 3: *r=1.00f; *g=0.55f; *b=0.42f; break;
        default:*r=0.95f; *g=0.95f; *b=1.00f; break;
    }
}

static void orient_player_to(V3 aim){
    G.pf = vnorm(vsub(aim, G.ppos));
    G.pu = v(0,1,0);
    G.pr = vnorm(vcross(G.pf,G.pu));
    if(vlen(G.pr)<0.01f) G.pr=v(1,0,0);
    G.pu = vcross(G.pr,G.pf);
}
static void spawn_near_station(void){
    G.ppos = vadd(G.station, v(130, 24, -190));
    G.pvel = v(0,0,0);
    orient_player_to(G.station);
}
static void spawn_jump_arrival(void){
    /* Drop out of jump somewhere on the run-in, but do not hand the station
     *      to the player dead-ahead. The compass/scanner should earn their keep. */
    V3 radial=vnorm(G.station);
    if(vlen(radial)<0.01f) radial=v(0,0,1);
    V3 side=vnorm(vcross(radial,v(0,1,0)));
    if(vlen(side)<0.01f) side=v(1,0,0);
    V3 up=vnorm(vcross(side,radial));
    f32 d=4300.0f + frand()*3300.0f;
    G.ppos = vadd(G.station, vadd(vmul(radial,d), vadd(vmul(side,srand2()*1500.0f), vmul(up,srand2()*650.0f))));
    V3 to=vnorm(vsub(G.station,G.ppos));
    G.pvel = vadd(vmul(to, 8.0f+frand()*22.0f), vadd(vmul(side,srand2()*38.0f), vmul(up,srand2()*18.0f)));
    V3 look=vnorm(vadd(vmul(to,0.35f), vadd(vmul(side,srand2()*0.95f), vmul(up,srand2()*0.45f))));
    orient_player_to(vadd(G.ppos, look));
}

/* spawn the local scene from a system seed */
static void enter_system(int idx, int arrive_far){
    G.cur = idx;
    System* s=&gal[idx];
    seed(hash32(s->seed ^ 0xC0DE));

    /* SANE STATIC SOLAR LAYOUT:
     *      - primary sun at system centre
     *      - optional companion sun near centre, not a far sky prop
     *      - planets on ordered heliocentric lanes
     *      - moons on local child lanes around their parent planet
     *      No orbit rails, no live orbital motion, no control/view changes. */
    G.nstar = (xr()%5==0)?2:1;
    G.star[0].pos = v(0,0,0);
    G.star[0].r = 520 + frand()*260;
    choose_star_color(xr(), &G.star[0].cr, &G.star[0].cg, &G.star[0].cb);
    if(G.nstar>1){
        f32 a=frand()*TAU, d=900 + frand()*900;
        G.star[1].pos = v(f_cos(a)*d, srand2()*120, f_sin(a)*d);
        G.star[1].r = 260 + frand()*220;
        choose_star_color(xr(), &G.star[1].cr, &G.star[1].cg, &G.star[1].cb);
    }

    G.nb=0; G.hab_body=-1;
    int nplan = 3 + (xr()%4);                  /* 3..6 main planets */
    f32 lane = 1800 + frand()*550;             /* first orbit safely outside star */
    f32 golden = 2.39996323f;                  /* spreads angles without tables */
    for(int p=0;p<nplan && G.nb<MAXBOD; p++){
        int bi=G.nb; Body* b=&G.body[G.nb++];
        b->parent=-1;

        /* lane-based planet type: inner rocky, middle habitable-ish, outer giants/ice/rock */
        u32 r=xr()%100;
        if(p==0) b->type = (r<45)?3:((r<75)?1:2);                 /* rock/desert/temperate */
            else if(p<3) b->type = (r<32)?0:((r<62)?2:((r<82)?1:3));  /* ocean/temperate/desert/rock */
                else b->type = (r<48)?4:((r<72)?3:((r<88)?1:0));          /* gas/rock/desert/ocean */

                    if(b->type==4) b->r = 440 + frand()*330;
                    else if(b->type==0 || b->type==2) b->r = 190 + frand()*170;
                    else b->r = 145 + frand()*150;

                    f32 ang = frand()*TAU + golden*(f32)p;
        b->pos = v(f_cos(ang)*lane, srand2()*(35+22*p), f_sin(ang)*lane);
        b->has_ring = (b->type==4 && frand()<0.70f) || (b->type==3 && frand()<0.08f);
        b->has_atmo = (b->type!=3 && frand()<0.68f) || b->type==4;
        b->spin = 3.0f + frand()*8.0f;
        b->phase = frand()*TAU;
        b->tex = make_planet_tex(hash32(s->seed ^ bi ^ 0x7123), b->type);

        if(G.hab_body<0 && b->type!=4 && b->r>185) G.hab_body=bi;

        /* moons: fewer, spaced further out, always relative to this parent */
        int moons = 0;
        if(b->type==4) moons = 1 + (xr()%3);
        else if(b->r>230) moons = xr()%2;
        else if((xr()%9)==0) moons = 1;
        for(int m=0;m<moons && G.nb<MAXBOD; m++){
            Body* q=&G.body[G.nb++];
            q->parent=bi;
            q->type = (xr()%4==0)?2:3;          /* mostly dead rock, occasional icy/soft colour */
            q->r = b->r*(0.075f + frand()*0.085f);
            f32 ma = frand()*TAU + golden*(f32)m;
            f32 md = b->r*(3.3f + 1.75f*m) + 130.0f + frand()*90.0f;
            q->pos = v(f_cos(ma)*md, srand2()*35.0f, f_sin(ma)*md);
            q->has_ring = 0;
            q->has_atmo = (q->type!=3 && frand()<0.25f);
            q->spin = 6.0f + frand()*14.0f;
            q->phase = frand()*TAU;
            q->tex = make_planet_tex(hash32(s->seed ^ G.nb ^ 0x9911), q->type);
        }

        lane += 1450 + p*620 + b->r*1.7f + frand()*420;          /* bigger gaps outward */
    }
    if(G.hab_body<0) G.hab_body=0;

    /* Pick a port world that is genuinely a planet-side/orbital destination, not
     *      an inner rock roasting beside the sun. Prefer non-gas planets safely clear
     *      of all stars; otherwise use the farthest main planet. */
    { int best=G.hab_body; f32 bestscore=-999999.0f;
        for(int i=0;i<G.nb;i++){
            Body* b=&G.body[i]; if(b->parent>=0) continue;
            V3 bp=body_draw_pos(i); f32 mind=999999.0f;
            for(int si=0;si<G.nstar;si++){ f32 d=vlen(vsub(bp,G.star[si].pos))-G.star[si].r-b->r; if(d<mind) mind=d; }
            f32 score=mind + (b->type!=4?900.0f:0.0f) + ((b->type==0||b->type==2)?450.0f:0.0f);
            if(score>bestscore){ bestscore=score; best=i; }
        }
        G.hab_body=best;
    }

    /* station grammar: economy/government/tech choose silhouette, not a stored model */
    if(s->eco==ECO_LAWLESS || s->gov==GOV_ANARCHY) G.station_type=3;          /* black port */
        else if(s->eco==ECO_AGRI) G.station_type=4;                              /* agri hab */
            else if((s->gov==GOV_CONFED || s->gov==GOV_FEUDAL) && s->danger>5) G.station_type=5; /* fortress */
                else if(s->eco==ECO_TECH || s->gov==GOV_CORP || s->tech>8) G.station_type=1; /* corporate/research */
                    else if(s->eco==ECO_MINE || s->eco==ECO_REFINE || s->eco==ECO_IND) G.station_type=2; /* industrial */
                        else G.station_type=0;                                                    /* trade spindle */
                            G.station_scale = 18 + frand()*10;
    G.station_spin  = 6 + frand()*10;
    V3 anchor = body_draw_pos(G.hab_body);
    Body* hb=&G.body[G.hab_body];
    f32 sd = hb->r*3.4f + G.station_scale*3.8f + 240.0f;
    if(G.station_type==1) sd += 180.0f;
    V3 away=vnorm(anchor); if(vlen(away)<0.01f) away=v(0,0,1);
    V3 sidep=vnorm(vcross(away,v(0,1,0))); if(vlen(sidep)<0.01f) sidep=v(1,0,0);
    V3 upp=vnorm(vcross(sidep,away));
    f32 bestclear=-999999.0f; V3 bestpos=anchor;
    for(int tr=0; tr<10; tr++){
        f32 sa = frand()*TAU + tr*0.73f;
        f32 sy = hb->r*(0.10f + 0.12f*frand());
        if(G.station_type==3) sy = -sy;
        V3 off = vadd(vmul(sidep,f_cos(sa)*sd), vadd(vmul(away,f_sin(sa)*sd), vmul(upp,sy)));
        V3 cand = vadd(anchor, off);
        f32 clear=999999.0f;
        for(int si=0;si<G.nstar;si++){ f32 d=vlen(vsub(cand,G.star[si].pos))-G.star[si].r-G.station_scale; if(d<clear) clear=d; }
        if(clear>bestclear){ bestclear=clear; bestpos=cand; }
        if(clear>3600.0f) break;
    }
    G.station = bestpos;

    /* spawn mode is the gameplay loop: docked launch stays near station; route jumps exit far out. */
    if(arrive_far) spawn_jump_arrival(); else spawn_near_station();
    G.docked = arrive_far?0:1;
    G.alert_code = arrive_far?1:0; G.alert_t = arrive_far?3.0f:0.0f;

    int cargounits=0;
    for(int c=0;c<NCOM;c++) cargounits+=G.cargo[c];
    int illegal = G.cargo[4], wild=(s->eco==ECO_LAWLESS)+(s->gov==GOV_ANARCHY);
    int threat = s->danger + illegal*2 + wild*2;
    if(s->danger>5 || wild) threat += cargounits/6;
    if(threat>12) threat=12;

    /* Smuggling into lawful space can still put the run on notice. */
    if(arrive_far && illegal>0 && s->gov!=GOV_ANARCHY){ G.alert_code=3; G.alert_t=4.0f; }

    /* enemies: danger + cargo value make the run-in matter */
    G.ne = arrive_far ? (threat<3 ? (xr()%2) : 1 + threat/2 + (xr()%2)) : (s->danger>6 ? 1+(s->danger/5)+(xr()%2) : 0);
    if(G.ne>MAXE) G.ne=MAXE;
    V3 route=vnorm(vsub(G.station,G.ppos));
    V3 side=vnorm(vcross(route,v(0,1,0))); if(vlen(side)<0.01f) side=v(1,0,0);
    V3 up=vnorm(vcross(side,route));
    for(int i=0;i<G.ne;i++){
        Ship* e=&G.e[i];
        e->alive=1; e->target=-1; e->fire_cd=frand(); e->radar_flash=0;
        int pirate_roll = s->danger + illegal*3 + wild*2 + ((s->danger>5 || wild)?cargounits/6:0);
        if(pirate_roll>9) pirate_roll=9;
        int r=(int)(xr()%100);
        int forced = arrive_far && s->danger>5 && i < ((threat-3)/2 + (s->danger>7));
        if(arrive_far && (xr()%(threat>7?18:26))==0) e->type=SHIP_ALIEN;
        else if((G.wanted||G.kills>5) && arrive_far && r<18) e->type=SHIP_FIGHTER;       /* bounty hunter */
            else if(forced || r < pirate_roll*8 || (s->gov==GOV_ANARCHY && r<46)) e->type=SHIP_PIRATE;
            else if((s->eco==ECO_MINE || s->eco==ECO_REFINE) && r<70) e->type=SHIP_MINER;
            else if(r>82) e->type=SHIP_FREIGHTER;
            else e->type = (xr()%3==0? SHIP_TRADER : SHIP_POLICE);
            e->target = e->type==SHIP_PIRATE||e->type==SHIP_FIGHTER||(e->type==SHIP_ALIEN&&(xr()&1));
        e->maxhull = e->type==SHIP_PIRATE?82:(e->type==SHIP_POLICE?125:(e->type==SHIP_ALIEN?150:(e->type==SHIP_FIGHTER?105:(e->type==SHIP_FREIGHTER?115:(e->type==SHIP_MINER?90:62)))));
        e->hull = e->maxhull;
        if(arrive_far){
            f32 span=3100.0f-threat*150.0f; if(span<900.0f) span=900.0f;
            f32 along=450.0f+frand()*span;
            e->pos = vadd(G.ppos, vadd(vmul(route,along), vadd(vmul(side,srand2()*760.0f), vmul(up,srand2()*420.0f))));
            e->vel = vadd(vmul(route, e->type==SHIP_PIRATE?-25.0f:35.0f), vmul(side,srand2()*70.0f));
        } else {
            V3 dir=v(srand2(),srand2()*0.35f,srand2());
            e->pos = vadd(G.ppos, vmul(vnorm(dir), 850+frand()*1100));
            e->vel = vmul(vnorm(v(-dir.z,0,dir.x)), 35+frand()*55);
        }
    }
    if(arrive_far && threat>5){ G.alert_code=2; G.alert_t=4.0f; }
    for(int i=0;i<MAXB;i++) G.beam[i].ttl=0;
    for(int i=0;i<MAXM;i++) G.missile[i].alive=0;
    G.target=-1;
}

static void new_game(u32 master){
    m_set(&G,0,sizeof(G));
    gen_galaxy(master);
    G.maxhull=100; G.hull=100; G.maxshield=75; G.shield=75;
    G.credits=100; G.maxfuel=7; G.fuel=7; G.cargo_max=20;
    G.jump_range=7.0f; G.missiles=3; G.laser_level=1; G.legal=0; G.kills=0;
    G.target=-1; G.state=ST_STATUS; G.docked=1;
    G.win_w=1280; G.win_h=720;
    enter_system(0,0);
}

static int cargo_used(void){ int t=0; for(int i=0;i<NCOM;i++) t+=G.cargo[i]; return t; }
static int affordable_units(int c){ int free=G.cargo_max-cargo_used(); if(free<=0) return 0; int price=gal[G.cur].price[c]; if(price<=0) return 0; int u=G.credits/price; return u>free?free:u; }
static const char* lifeform_idx(int idx){
    return (hash32(gal[idx].seed)&1) ? "HUMAN COLONIALS" : "UNKNOWN LIFEFORM";
}
static int population_idx(int idx){ System* s=&gal[idx]; return 1 + ((hash32(s->seed^0x1234)%88) + s->tech*3)/10; }
static int radius_idx(int idx){ return 3200 + (hash32(gal[idx].seed^0x7777)%4200); }
static int productivity_idx(int idx){ System* s=&gal[idx]; return (s->tech+1)*(population_idx(idx)+2)*(s->eco==ECO_TECH?55:(s->eco==ECO_LAWLESS?18:35)); }

static const char* legal_name(void){
    if(G.legal>=2) return "FUGITIVE";
    if(G.legal==1 || G.wanted) return "OFFENDER";
    return "CLEAN";
}
static const char* rating_name(void){
    int k=G.kills;
    if(k>=640) return "ELITE";
    if(k>=320) return "DEADLY";
    if(k>=160) return "DANGEROUS";
    if(k>=80) return "COMPETENT";
    if(k>=40) return "ABOVE AVG";
    if(k>=20) return "AVERAGE";
    if(k>=8) return "POOR";
    if(k>=1) return "MOSTLY HARMLESS";
    return "HARMLESS";
}
static const char* laser_name(void){
    return G.laser_level>=3?"MILITARY LASER":G.laser_level==2?"BEAM LASER":"PULSE LASER";
}
static int fuel_cost(void){ int n=G.maxfuel-G.fuel; return n>0?n*2:0; }
static int missile_cost(void){ return 30; }
static int cargo_bay_cost(void){ return 400; }
static int beam_cost(void){ return 1000; }
static int extra_energy_cost(void){ return 1500; }
static int military_cost(void){ return 6000; }

#define NEQ 6
static const char* EQNAME[NEQ]={
    "FUEL", "LANCE DART MISSILE", "HOLD BLADDER", "CUTTER BEAM LASER",
    "COPPER ENERGY UNIT", "LANCE-90 MILITARY LASER"
};
static int eq_tech(int i){
    static const int t[NEQ]={0,0,0,4,8,10};
    return t[i];
}
static int eq_cost(int i){
    switch(i){
        case 0:return fuel_cost(); case 1:return missile_cost(); case 2:return cargo_bay_cost();
        case 3:return beam_cost(); case 4:return extra_energy_cost();
        default:return military_cost();
    }
}
static int eq_present(int i){
    switch(i){
        case 0:return G.fuel>=G.maxfuel; case 1:return G.missiles>=4; case 2:return G.cargo_max>=35;
        case 3:return G.laser_level>=2; case 4:return G.extra_energy;
        default:return G.laser_level>=3;
    }
}
static int eq_available(int i){ int t=eq_tech(i); return t==0 || gal[G.cur].tech>=t; }
static int eq_can_buy(int i){ return eq_available(i) && !eq_present(i) && eq_cost(i)>0 && G.credits>=eq_cost(i); }
static void buy_equipment_item(int i){
    if(i<0||i>=NEQ||!eq_can_buy(i)) return;
    G.credits-=eq_cost(i); sfx_play(SFX_BUY);
    switch(i){
        case 0: G.fuel=G.maxfuel; break;
        case 1: if(G.missiles<4) G.missiles++; break;
        case 2: G.cargo_max=35; break;
        case 3: G.laser_level=2; break;
        case 4: G.extra_energy=1; G.maxshield+=25; G.shield=G.maxshield; break;
        case 5: G.laser_level=3; break;
    }
}
static void set_crime(int lvl){
    if(lvl>G.legal) G.legal=lvl;
    if(G.legal>0) G.wanted=1;
}
static int illegal_cargo_count(void){ return G.cargo[4] + G.cargo[5]; }
static int cargo_value_here(void){ int v=0; System* s=&gal[G.cur]; for(int i=0;i<NCOM;i++) v+=G.cargo[i]*s->price[i]; return v; }
static int ship_hostile(Ship* e){ return e->target||(G.wanted&&e->type==SHIP_POLICE); }


static f32 sys_dist_idx(int a,int b){
    System* A=&gal[a]; System* B=&gal[b];
    return vlen(vsub(v(A->x,A->y,A->z), v(B->x,B->y,B->z))) * 0.35f;
}
static int fuel_need_idx(int a,int b){ int n=(int)(sys_dist_idx(a,b)+0.5f); return n<1?1:n; }
static int can_jump_idx(int dst){
    if(dst==G.cur) return 0;
    return sys_dist_idx(G.cur,dst)<=G.jump_range && G.fuel>=fuel_need_idx(G.cur,dst);
}
static int held_sale_profit_to(int dst){
    int p=0; System* c=&gal[G.cur]; System* t=&gal[dst];
    for(int i=0;i<NCOM;i++) p += G.cargo[i]*(t->price[i]-c->price[i]);
    return p;
}
static int best_trade_to(int dst, int* outc){
    int best=-9999, bc=0, any=0; System* c=&gal[G.cur]; System* t=&gal[dst];
    /* Prefer things the commander can actually afford now. Fallback still shows
     *      the market truth if nothing affordable makes money. */
    for(int i=0;i<NCOM;i++){
        int p=t->price[i]-c->price[i];
        if(affordable_units(i)>0 && p>best){ best=p; bc=i; any=1; }
    }
    if(!any){
        for(int i=0;i<NCOM;i++){ int p=t->price[i]-c->price[i]; if(p>best){ best=p; bc=i; } }
    }
    if(outc) *outc=bc;
    return best;
}
static int best_trade_total_to(int dst, int* outc, int* outu){
    int best=-999999, bc=0, bu=0; System* c=&gal[G.cur]; System* t=&gal[dst];
    for(int i=0;i<NCOM;i++){
        int u=affordable_units(i);
        int p=t->price[i]-c->price[i];
        int tot=p*u;
        if(u>0 && tot>best){ best=tot; bc=i; bu=u; }
    }
    if(outc) *outc=bc;
    if(outu) *outu=bu;
    return best;
}
static int map_best_dest(void){
    int best=G.cur; int bestscore=-999999;
    for(int i=0;i<NSYS;i++) if(i!=G.cur && can_jump_idx(i)){
        int bc=0, bu=0, total=best_trade_total_to(i,&bc,&bu), held=held_sale_profit_to(i);
        int score = (total>0?total*2:total) + held*2 + gal[i].tech*4 - gal[i].danger*5 - fuel_need_idx(G.cur,i);
        if(score>bestscore){ bestscore=score; best=i; }
    }
    if(best!=G.cur) return best;
    /* no fuel/range-valid candidate: fall back to nearest non-current, so the page still teaches why it cannot jump */
    f32 bd=1e9f;
    for(int i=0;i<NSYS;i++) if(i!=G.cur){ f32 d=sys_dist_idx(G.cur,i); if(d<bd){bd=d; best=i;} }
    return best;
}
static int map_next_from(int start,int dir,int reachable_only){
    int first=-1;
    for(int n=1;n<=NSYS;n++){
        int idx=(start + dir*n + NSYS*4) % NSYS;
        if(idx==G.cur) continue;
        if(reachable_only && !can_jump_idx(idx)) continue;
        return idx;
    }
    if(reachable_only) return map_next_from(start,dir,0);
    for(int i=0;i<NSYS;i++) if(i!=G.cur){ first=i; break; }
    return first<0?G.cur:first;
}
static void open_map(void){
    G.map_back_state=G.state; G.state=ST_MAP; G.mapsel=map_best_dest(); p_SDL_SetRelativeMouseMode(0);
}

/* rotate basis vectors about a local axis */
static void rot_axis(V3* vp, V3 axis, f32 ang){
    f32 c=f_cos(ang), s=f_sin(ang);
    V3 cr=vcross(axis,*vp); f32 d=vdot(axis,*vp);
    *vp = vadd(vadd(vmul(*vp,c), vmul(cr,s)), vmul(axis, d*(1-c)));
}
static void reorthonormalize(void){
    G.pf=vnorm(G.pf);
    G.pr=vnorm(vcross(G.pf,G.pu));
    G.pu=vcross(G.pr,G.pf);
}

/* spawn a beam */
static void add_beam(V3 a, V3 b, int hostile){
    for(int i=0;i<MAXB;i++) if(G.beam[i].ttl<=0){ G.beam[i].a=a; G.beam[i].b=b; G.beam[i].ttl=hostile?0.18f:0.22f; G.beam[i].hostile=hostile; return; }
}
/* ray vs sphere (origin o, dir d normalized, centre c, radius r) -> hit t or -1 */
static f32 ray_sphere(V3 o, V3 d, V3 c, f32 r){
    V3 oc=vsub(o,c); f32 b=vdot(oc,d); f32 cc=vdot(oc,oc)-r*r;
    f32 disc=b*b-cc; if(disc<0) return -1; f32 t=-b-f_sqrt(disc); return t;
}
static void player_damage_ship(int i, f32 dmg){
    if(i<0 || i>=G.ne || !G.e[i].alive) return;
    sfx_play(SFX_HIT);
    G.e[i].hull -= dmg;
    if(G.e[i].hull<=0){
        int t=G.e[i].type;
        G.e[i].alive=0;
        if(G.e[i].target){ G.credits += (t==SHIP_ALIEN)?220:(t==SHIP_FIGHTER?170:130); G.kills++; G.sys_kills[G.cur]++; sfx_play(SFX_KILL); }
        else { G.credits -= 40; set_crime(t==SHIP_POLICE?2:1); sfx_play(SFX_KILL); }
        if(G.target==i) G.target=-1;
    }
}
static int missile_lock_target(void){
    int best=-1; f32 bs=0.965f;
    for(int i=0;i<G.ne;i++) if(G.e[i].alive){
        V3 to=vsub(G.e[i].pos,G.ppos); f32 d=vlen(to); if(d<35 || d>1500) continue;
        f32 a=vdot(vnorm(to),G.pf);
        f32 score=a - d*0.00004f;
        if(score>bs){ bs=score; best=i; }
    }
    return best;
}
static void fire_missile(void){
    if(G.missiles<=0){ sfx_play(SFX_WARN); return; }
    int tgt=missile_lock_target();
    if(tgt<0){ sfx_play(SFX_WARN); return; }
    int slot=-1; for(int i=0;i<MAXM;i++) if(!G.missile[i].alive){ slot=i; break; }
    if(slot<0){ sfx_play(SFX_WARN); return; }
    Missile* m=&G.missile[slot];
    m->alive=1; m->ttl=8.0f; m->target=tgt;
    m->pos=vadd(G.ppos,vmul(G.pf,4.0f));
    m->vel=vadd(G.pvel,vmul(G.pf,360.0f));
    G.missiles--; G.muzzle_flash=0.18f; G.target=tgt; sfx_play(SFX_MISSILE);
}

/* ============================ update ====================================== */
static u8 prevkeys[300];
static u32 prevmb=0;
static int edge(const u8* ks, int sc){ return ks[sc] && !prevkeys[sc]; }

static void fire_player(void){
    if(G.fire_cd>0 || G.laser_lockout) return;
    if(!energy_try_spend(laser_energy_cost())){ G.fire_cd=0.22f; sfx_play(SFX_WARN); return; }
    G.fire_cd=0.11f; G.muzzle_flash=0.10f; G.laser_heat+=0.13f;
    if(G.laser_heat>=1.0f){ G.laser_heat=1.0f; G.laser_lockout=1; }
    sfx_play(SFX_LASER);
    V3 base = vadd(G.ppos, vmul(G.pf,2.3f));
    V3 gun[2]={ vadd(vadd(base, vmul(G.pr,-0.85f)), vmul(G.pu,-0.35f)),
        vadd(vadd(base, vmul(G.pr, 0.85f)), vmul(G.pu,-0.35f)) };
        for(int g=0; g<2; g++){
            V3 dir = vnorm(vadd(G.pf, vmul(G.pr, g? -0.012f:0.012f)));
            V3 end = vadd(gun[g], vmul(dir,1900));
            int besthit=-1; f32 bestt=1e9f;
            for(int i=0;i<G.ne;i++){
                if(!G.e[i].alive) continue;
                f32 t=ray_sphere(gun[g], dir, G.e[i].pos, 7.0f);
                if(t>0 && t<bestt){ bestt=t; besthit=i; }
            }
            if(besthit>=0){
                end = vadd(gun[g], vmul(dir,bestt));
                player_damage_ship(besthit, (G.laser_level==3?26:(G.laser_level==2?17:11)));
            }
            add_beam(gun[g],end,0);
        }
}

static void update(f32 dt, const u8* ks){
    G.t += dt;
    /* timers */
    if(G.fire_cd>0) G.fire_cd-=dt;
    if(G.muzzle_flash>0) G.muzzle_flash-=dt;
    if(G.laser_heat>0){ G.laser_heat-=dt*0.24f; if(G.laser_heat<0) G.laser_heat=0; }
    if(G.laser_lockout && G.laser_heat<=0.02f) G.laser_lockout=0;
    if(G.heat_warn_cd>0) G.heat_warn_cd-=dt;
    if(G.shield_cd>0) G.shield_cd-=dt;
    if(G.cabin_temp<0.92f && G.hull<G.maxhull){
        G.hull += (G.extra_energy?6.0f:3.5f)*dt;   /* slow generator recovery */
        if(G.hull>G.maxhull) G.hull=G.maxhull;
    }
    if(G.shield_cd<=0 && G.shield<G.maxshield && G.cabin_temp<0.96f){
        f32 inc=(G.extra_energy?8.5f:5.0f)*dt;   /* slow shield rebuild, no energy drain */
        G.shield+=inc; if(G.shield>G.maxshield) G.shield=G.maxshield;
    }

    if(G.state!=ST_FLIGHT){ for(int i=0;i<MAXB;i++) if(G.beam[i].ttl>0) G.beam[i].ttl-=dt; return; }

    /* ---- mouse look ---- */
    int mx=0,my=0; u32 mb=p_SDL_GetRelativeMouseState(&mx,&my);
    f32 sens=0.0016f;
    if(mx){ rot_axis(&G.pf,G.pu,-mx*sens); rot_axis(&G.pr,G.pu,-mx*sens); }
    if(my){ rot_axis(&G.pf,G.pr,-my*sens); rot_axis(&G.pu,G.pr,-my*sens); }
    /* roll */
    if(ks[SC_Q]){ rot_axis(&G.pr,G.pf,-1.6f*dt); rot_axis(&G.pu,G.pf,-1.6f*dt); }
    if(ks[SC_E]){ rot_axis(&G.pr,G.pf, 1.6f*dt); rot_axis(&G.pu,G.pf, 1.6f*dt); }
    reorthonormalize();

    /* ---- thrust (Newtonian-flavoured, damped arcade) ---- */
    int thrusting = ks[SC_W]||ks[SC_S]||ks[SC_A]||ks[SC_D];
    int boost = ks[SC_LSHIFT] && thrusting && G.hull>13.0f;
    f32 accel = boost?260.0f:130.0f;
    if(ks[SC_W]) G.pvel = vadd(G.pvel, vmul(G.pf, accel*dt));
    if(ks[SC_S]) G.pvel = vsub(G.pvel, vmul(G.pf, accel*dt));
    if(ks[SC_A]) G.pvel = vsub(G.pvel, vmul(G.pr, (boost?125.0f:80.0f)*dt));
    if(ks[SC_D]) G.pvel = vadd(G.pvel, vmul(G.pr, (boost?125.0f:80.0f)*dt));
    G.pvel = vmul(G.pvel, 0.992f);                 /* gentle damping */
    f32 spd=vlen(G.pvel); f32 cap = boost?520:300;
    if(spd>cap) G.pvel=vmul(G.pvel,cap/spd);
    if(boost){ energy_spend_soft((7.0f + 12.0f*clampf(spd/520.0f,0,1))*dt); } /* tuned: boost should pressure generator, not murder it */
        G.ppos = vadd(G.ppos, vmul(G.pvel, dt));
        /* engine/thrust audio: speed hum plus a small boost/load lift */
        G.audio_target_amp = clampf(spd/520.0f + (boost?0.18f:0.0f),0,1);

        /* ---- heat: star proximity only. Ordinary flight must never cook you. ---- */
        { f32 bd=999999.0f; for(int si=0;si<G.nstar;si++){ f32 d=vlen(vsub(G.star[si].pos,G.ppos))-G.star[si].r; if(d<bd) bd=d; }
        f32 target_temp = clampf((3600.0f-bd)/3600.0f, 0, 1);
        f32 rate = (target_temp>G.cabin_temp)?0.55f:0.30f;
        G.cabin_temp += (target_temp-G.cabin_temp)*rate*dt;
        if(G.cabin_temp<0) G.cabin_temp=0;
        if(G.cabin_temp>1) G.cabin_temp=1;
        if(G.cabin_temp>=0.995f){
            f32 burn = 18.0f + clampf((-bd)/900.0f,0,1)*36.0f;
            if(G.shield>0){ G.shield-=burn*dt; if(G.shield<0){ G.hull+=G.shield; G.shield=0; } }
            else G.hull-=burn*dt*1.15f;
            G.shield_cd=3.0f;
            if(G.heat_warn_cd<=0){ sfx_play(SFX_WARN); G.heat_warn_cd=0.75f; }
        }
        }

        /* ---- firing ---- */
        if(mb & SDL_BUTTON_LMASK) fire_player();
        if((mb & SDL_BUTTON_RMASK) && !(prevmb & SDL_BUTTON_RMASK)) fire_missile();
        prevmb=mb;

    /* ---- target nose contact ---- */
    G.target=missile_lock_target();

    /* ---- enemy AI ---- */
    for(int i=0;i<G.ne;i++){
        Ship* e=&G.e[i]; if(!e->alive) continue;
        V3 toP = vsub(G.ppos, e->pos); f32 d=vlen(toP); V3 dir=vnorm(toP);
        int hostile = ship_hostile(e);
        if(e->fire_cd>0) e->fire_cd-=dt;
        if(e->radar_flash>0) e->radar_flash-=dt;
        if(hostile){
            f32 want = (e->hull < e->maxhull*0.3f)? -230 : (d>760? 230 : (d<310? -75: 55));
            V3 side = vnorm(vcross(dir, v(0,1,0)));
            V3 desired = vadd(vmul(dir, want), vmul(side, f_sin(G.t*1.5f+i)*(d>700?45:95)));
            e->vel = vadd(vmul(e->vel,0.94f), vmul(desired, dt*1.5f));
            f32 es=vlen(e->vel); if(es>280) e->vel=vmul(e->vel,280/es);
            e->pos = vadd(e->pos, vmul(e->vel,dt));
            if(d<EFIRE_RANGE && e->fire_cd<=0){
                /* ---- distance‑based fire cooldown (cubic) ---- */
                f32 q=d/EFIRE_RANGE;
                e->fire_cd = 0.3f+frand()*0.3f+16.0f*q*q*q;
                e->radar_flash = 0.24f;

                /* per-class gunnery: smaller cone = tighter shot group.
                 *                  collapse to one constant if you want flat skill across ships. */
                f32 acc = e->type==SHIP_ALIEN  ? 0.009f
                : e->type==SHIP_FIGHTER ? 0.011f
                : e->type==SHIP_POLICE  ? 0.013f
                :                          0.016f;   /* pirate baseline */

                /* instant beam: aim at where the player IS. the gunner's real
                 *                  problem is holding the reticle on a crossing blip, so the cone
                 *                  bloats with the target's transverse angular rate. fly straight
                 *                  (even straight away) and vtan~0 -> dead on the nose; jink and
                 *                  the shot group blows wide. that, finally, is the "lock". */
                V3  mz   = vadd(e->pos, vmul(dir,3.0f));
                V3  vrel = vsub(G.pvel, e->vel);
                f32 vtan = vlen(vsub(vrel, vmul(dir, vdot(vrel,dir))));
                f32 sigma = acc + ETRACK*(vtan/(d+1.0f));

                /* scatter the shot inside that cone, then fire THAT exact ray */
                V3  ar = vnorm(vcross(dir, v(0,1,0))); if(vlen(ar)<0.01f) ar=v(1,0,0);
                V3  au = vcross(ar, dir);
                V3  fdir = vnorm(vadd(dir, vadd(vmul(ar, srand2()*sigma),
                                                vmul(au, srand2()*sigma))));
                add_beam(mz, vadd(mz, vmul(fdir, d)), 1);

                /* honest hit: does the drawn ray graze the player's hull sphere? */
                f32 tc = vdot(vsub(G.ppos, mz), fdir);
                V3  ca = vadd(mz, vmul(fdir, tc>0?tc:0.0f));
                if(tc>0 && vlen(vsub(G.ppos, ca)) < PLAYER_R){
                    f32 dmg = (6 + frand()*8)*(1.0f-q*0.65f);
                    if(G.shield>0){ f32 sd=dmg*shield_hit_scale(); G.shield-=sd; if(G.shield<0){ G.hull+=G.shield; G.shield=0; } }
                    else G.hull-=dmg*1.18f;
                    G.shield_cd=3.0f; sfx_play(SFX_DAMAGE);
                }
            }
        } else {
            /* civilian: drift, keep distance */
            e->pos = vadd(e->pos, vmul(e->vel,dt));
            if(d<200) e->vel = vsub(e->vel, vmul(dir, 30*dt));
            e->vel = vmul(e->vel,0.98f);
        }
    }

    /* ---- missiles ---- */
    for(int mi=0; mi<MAXM; mi++){
        Missile* m=&G.missile[mi]; if(!m->alive) continue;
        m->ttl-=dt; if(m->ttl<=0){ m->alive=0; continue; }
        if(m->target>=0 && m->target<G.ne && G.e[m->target].alive){
            V3 dir=vnorm(vsub(G.e[m->target].pos,m->pos));
            f32 ms=vlen(m->vel); if(ms<760.0f) ms += 210.0f*dt;
            V3 want=vmul(dir,ms);
            m->vel=vadd(vmul(m->vel,0.84f),vmul(want,0.16f));
        }
        m->pos=vadd(m->pos,vmul(m->vel,dt));
        for(int i=0;i<G.ne;i++) if(G.e[i].alive){
            f32 d=vlen(vsub(G.e[i].pos,m->pos));
            if(d<13.0f){ add_beam(m->pos,G.e[i].pos,0); sfx_play(SFX_KILL); player_damage_ship(i,105.0f); m->alive=0; break; }
        }
    }

    /* ---- beams age ---- */
    for(int i=0;i<MAXB;i++) if(G.beam[i].ttl>0) G.beam[i].ttl-=dt;

    /* ---- death ---- */
    if(G.hull<=0){ G.state=ST_DEAD; G.hull=0; sfx_play(SFX_KILL); p_SDL_SetRelativeMouseMode(0); return; }

    /* ---- docking ---- */
    f32 sd = vlen(vsub(G.station, G.ppos));
    G.docked_ok = (sd < 90 && spd < 60);
    if(G.docked_ok && (G.wanted==0 || gal[G.cur].gov==GOV_ANARCHY) && edge(ks,SC_F)){
        G.state=ST_STATUS; G.msel=0; G.docked=1; G.alert_code=5; G.alert_t=3.0f; sfx_play(SFX_DOCK); p_SDL_SetRelativeMouseMode(0);
        /* docking clears local heat in anarchies; elsewhere a fee */
        if(G.wanted && gal[G.cur].gov==GOV_ANARCHY){ G.wanted=0; G.legal=0; }
    }
    /* open map */
    if(edge(ks,SC_M) || edge(ks,SC_J)) open_map();
}

/* docked/menu interactions: one responsibility per page.
 *  Command Slate is the hub. Market is cargo only. Equip Ship is services/kit. */
static void launch_from_dock(void){
    G.state=ST_FLIGHT; G.docked=0; G.alert_code=6; G.alert_t=2.5f; sfx_play(SFX_LAUNCH); p_SDL_SetRelativeMouseMode(1);
}
static int station_nav(const u8* ks){
    if(edge(ks,SC_0) || edge(ks,SC_BACKSPACE)){ G.state=ST_STATUS; return 1; }
    if(edge(ks,SC_1)){ G.state=ST_MARKET; return 1; }
    if(edge(ks,SC_2)){ G.state=ST_EQUIP; return 1; }
    if(edge(ks,SC_3)){ G.mapsel=G.cur; G.state=ST_DATA; return 1; }
    if(edge(ks,SC_4)){ G.state=ST_MANIFEST; return 1; }
    if(edge(ks,SC_J)){ open_map(); return 1; }
    if(edge(ks,SC_F)){ launch_from_dock(); return 1; }
    return 0;
}
static void update_market(const u8* ks){
    if(station_nav(ks)) return;
    if(edge(ks,SC_UP)   && G.msel>0) G.msel--;
    if(edge(ks,SC_DOWN) && G.msel<NCOM-1) G.msel++;
    System* s=&gal[G.cur];
    int c=G.msel;
    if(edge(ks,SC_RIGHT)||edge(ks,SC_D)){
        if(G.credits>=s->price[c] && cargo_used()<G.cargo_max){ G.credits-=s->price[c]; G.cargo[c]++; sfx_play(SFX_BUY); }
    }
    if(edge(ks,SC_LEFT)||edge(ks,SC_A)){
        if(G.cargo[c]>0){ G.cargo[c]--; G.credits+=s->price[c]; sfx_play(SFX_BUY); }
    }
}

static void update_map(const u8* ks){
    int reachable_only = !ks[SC_LSHIFT];
    if(edge(ks,SC_RIGHT)||edge(ks,SC_DOWN)) G.mapsel=map_next_from(G.mapsel, 1, reachable_only);
    if(edge(ks,SC_LEFT) ||edge(ks,SC_UP))   G.mapsel=map_next_from(G.mapsel,-1, reachable_only);
    if(edge(ks,SC_RETURN) || edge(ks,SC_J)){
        if(can_jump_idx(G.mapsel)){
            G.fuel -= fuel_need_idx(G.cur,G.mapsel);
            G.target=G.mapsel; G.alert_t=1.85f; G.state=ST_JUMP; sfx_play(SFX_JUMP);
        }
    }
    if(edge(ks,SC_5)){ G.state=ST_DATA; p_SDL_SetRelativeMouseMode(0); }
    if(edge(ks,SC_BACKSPACE) || edge(ks,SC_M)){ G.state=G.map_back_state?G.map_back_state:ST_FLIGHT; p_SDL_SetRelativeMouseMode(0); }
}

static void update_status(const u8* ks){ station_nav(ks); }
static void update_data(const u8* ks){ station_nav(ks); }
static void update_manifest(const u8* ks){ station_nav(ks); }
static void update_equip(const u8* ks){
    if(station_nav(ks)) return;
    if(edge(ks,SC_UP)   && G.esel>0) G.esel--;
    if(edge(ks,SC_DOWN) && G.esel<NEQ-1) G.esel++;
    if(edge(ks,SC_RIGHT) || edge(ks,SC_RETURN) || edge(ks,SC_D)) buy_equipment_item(G.esel);
}


/* ============================ rendering =================================== */
static void set_view(void){
    /* projection */
    p_glMatrixMode(GL_PROJECTION); p_glLoadIdentity();
    f32 fov=72.0f*PI/180.0f, aspect=(f32)G.win_w/(f32)G.win_h, nr=0.6f, fr=24000.0f;
    f32 top=nr*f_tan(fov*0.5f), right=top*aspect;
    p_glFrustum(-right,right,-top,top,nr,fr);
    /* view matrix from player basis (gluLookAt form), eye = ppos */
    V3 s=G.pr, u=G.pu, f=G.pf, eye=G.ppos;
    f32 m[16];
    m[0]=s.x; m[1]=u.x; m[2]=-f.x; m[3]=0;
    m[4]=s.y; m[5]=u.y; m[6]=-f.y; m[7]=0;
    m[8]=s.z; m[9]=u.z; m[10]=-f.z; m[11]=0;
    m[12]=-vdot(s,eye); m[13]=-vdot(u,eye); m[14]=vdot(f,eye); m[15]=1;
    p_glMatrixMode(GL_MODELVIEW); p_glLoadMatrixf(m);
}

static u32 starseed=0x51EED;
static void draw_starfield(void){
    p_glDisable(GL_LIGHTING); p_glDepthMask(0);
    p_glPushMatrix(); p_glTranslatef(G.ppos.x,G.ppos.y,G.ppos.z);
    p_glPointSize(1.6f);
    p_glBegin(GL_POINTS);
    seed(starseed);
    for(int i=0;i<700;i++){
        V3 d=vnorm(v(srand2(),srand2(),srand2()));
        f32 b=0.35f+frand()*0.65f;
        p_glColor3f(b,b,b*1.05f);
        V3 pnt=vmul(d,18000);
        p_glVertex3f(pnt.x,pnt.y,pnt.z);
    }
    p_glEnd();
    p_glPopMatrix(); p_glDepthMask(1);
}

static void draw_ring(f32 inner, f32 outer, f32 r,f32 g,f32 b){
    p_glEnable(GL_BLEND); p_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    p_glColor4f(r,g,b,0.45f);
    for(int i=0;i<24;i++){
        f32 a0=TAU*(f32)i/24.0f, a1=TAU*(f32)(i+1)/24.0f;
        p_glBegin(GL_QUADS);
        p_glNormal3f(0,1,0);
        p_glVertex3f(f_cos(a0)*inner,0,f_sin(a0)*inner);
        p_glVertex3f(f_cos(a1)*inner,0,f_sin(a1)*inner);
        p_glVertex3f(f_cos(a1)*outer,0,f_sin(a1)*outer);
        p_glVertex3f(f_cos(a0)*outer,0,f_sin(a0)*outer);
        p_glEnd();
    }
    p_glDisable(GL_BLEND);
}

static void draw_atmo_shell(int type){
    f32 r,g,b; planet_base_rgb(type,&r,&g,&b);
    p_glDisable(GL_LIGHTING); p_glEnable(GL_BLEND); p_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    p_glColor4f(clampf(r+0.18f,0,1),clampf(g+0.20f,0,1),clampf(b+0.24f,0,1),0.18f);
    emit_sphere(12,18);
    p_glDisable(GL_BLEND); p_glEnable(GL_LIGHTING);
}

static void emit_station(int t){
    if(t==0){ /* classic ring */
        emit_sphere(6,8);
        for(int i=0;i<6;i++){
            p_glPushMatrix(); p_glRotatef(60.0f*i,0,0,1); p_glTranslatef(2.5f,0,0);
            p_glBegin(GL_QUADS);
            V3 a=v(-1.7f,-0.25f,-0.2f),b=v(1.7f,-0.25f,-0.2f),c=v(1.7f,0.25f,-0.2f),d=v(-1.7f,0.25f,-0.2f);
            p_glNormal3f(0,0,1);
            p_glVertex3f(a.x,a.y,a.z);p_glVertex3f(b.x,b.y,b.z);p_glVertex3f(c.x,c.y,c.z);p_glVertex3f(d.x,d.y,d.z);
            p_glEnd(); p_glPopMatrix();
        }
    } else if(t==1){ /* spindle / corp */
        p_glBegin(GL_TRIANGLES);
        face4(v(-0.4f,-0.4f,3.5f),v(0.4f,-0.4f,3.5f),v(0.5f,0.5f,1.0f),v(-0.5f,0.5f,1.0f));
        face4(v(-0.5f,-0.5f,-1.0f),v(0.5f,-0.5f,-1.0f),v(0.4f,0.4f,-3.8f),v(-0.4f,0.4f,-3.8f));
        p_glEnd();
        p_glPushMatrix(); p_glScalef(1.2f,1.2f,1.2f); emit_sphere(5,7); p_glPopMatrix();
        for(int i=0;i<4;i++){
            p_glPushMatrix(); p_glRotatef(90*i + G.t*32,0,0,1); p_glTranslatef(0,2.6f,0); p_glScalef(0.35f,2.2f,0.14f);
            p_glBegin(GL_QUADS);
            V3 a=v(-1,-1,0),b=v(1,-1,0),c=v(1,1,0),d=v(-1,1,0); p_glNormal3f(0,0,1);
            p_glVertex3f(a.x,a.y,a.z);p_glVertex3f(b.x,b.y,b.z);p_glVertex3f(c.x,c.y,c.z);p_glVertex3f(d.x,d.y,d.z);
            p_glEnd(); p_glPopMatrix();
        }
    } else if(t==2){ /* industrial yard */
        p_glBegin(GL_TRIANGLES);
        face4(v(-1.4f,-0.8f,1.8f),v(1.4f,-0.8f,1.8f),v(1.4f,0.8f,1.8f),v(-1.4f,0.8f,1.8f));
        face4(v(-1.4f,-0.8f,-1.8f),v(1.4f,-0.8f,-1.8f),v(1.4f,0.8f,-1.8f),v(-1.4f,0.8f,-1.8f));
        face4(v(-1.4f,-0.8f,1.8f),v(-1.4f,-0.8f,-1.8f),v(-1.4f,0.8f,-1.8f),v(-1.4f,0.8f,1.8f));
        face4(v(1.4f,-0.8f,1.8f),v(1.4f,-0.8f,-1.8f),v(1.4f,0.8f,-1.8f),v(1.4f,0.8f,1.8f));
        p_glEnd();
        for(int i=0;i<3;i++){
            p_glPushMatrix(); p_glTranslatef(-3.0f+3.0f*i,0,0); p_glScalef(0.55f,0.55f,0.55f); emit_sphere(4,6); p_glPopMatrix();
        }
        for(int i=0;i<2;i++){
            p_glPushMatrix(); p_glTranslatef(i?2.8f:-2.8f,0,0); p_glRotatef(40+i*35,0,1,0);
            p_glBegin(GL_LINES); p_glVertex3f(0,0,0); p_glVertex3f(2.4f,1.0f,0); p_glVertex3f(2.4f,1.0f,0); p_glVertex3f(3.0f,1.6f,0.6f); p_glEnd();
            p_glPopMatrix();
        }
    } else if(t==3){ /* black port / lawless scrapfort */
        for(int i=0;i<6;i++){
            p_glPushMatrix(); p_glRotatef(60*i + i*17,0.5f,1,0.2f); p_glTranslatef(0,0,2.0f + 0.28f*(i&1)); p_glScalef(0.65f+0.22f*(i&1),0.55f+0.12f*(i%3),1.15f);
            p_glBegin(GL_TRIANGLES);
            face3(v(0,0,1.4f),v(-0.8f,-0.5f,-1),v(0.9f,-0.4f,-1.2f));
            face3(v(0,0,1.4f),v(0.9f,-0.4f,-1.2f),v(0.3f,0.8f,-0.8f));
            face3(v(0,0,1.4f),v(0.3f,0.8f,-0.8f),v(-0.8f,-0.5f,-1));
            p_glEnd(); p_glPopMatrix();
        }
        p_glPushMatrix(); p_glScalef(1.1f,1.1f,1.1f); emit_sphere(4,6); p_glPopMatrix();
    } else if(t==4){ /* agri hab: domes and farm ring */
        p_glPushMatrix(); p_glScalef(1.25f,0.82f,1.25f); emit_sphere(5,8); p_glPopMatrix();
        p_glPushMatrix(); p_glRotatef(90,1,0,0); draw_ring(1.9f,3.1f,0.34f,0.80f,0.42f); p_glPopMatrix();
        for(int i=0;i<4;i++){
            p_glPushMatrix(); p_glRotatef(90*i,0,1,0); p_glTranslatef(2.25f,0.15f,0); p_glScalef(0.46f,0.46f,0.46f); emit_sphere(4,6); p_glPopMatrix();
        }
    } else { /* fortress port: armour plates and watch spines */
        p_glBegin(GL_TRIANGLES);
        face4(v(-1.7f,-1.0f,1.7f),v(1.7f,-1.0f,1.7f),v(1.7f,1.0f,1.7f),v(-1.7f,1.0f,1.7f));
        face4(v(-1.7f,-1.0f,-1.7f),v(1.7f,-1.0f,-1.7f),v(1.7f,1.0f,-1.7f),v(-1.7f,1.0f,-1.7f));
        face4(v(-1.7f,-1.0f,1.7f),v(-1.7f,-1.0f,-1.7f),v(-1.7f,1.0f,-1.7f),v(-1.7f,1.0f,1.7f));
        face4(v(1.7f,-1.0f,1.7f),v(1.7f,-1.0f,-1.7f),v(1.7f,1.0f,-1.7f),v(1.7f,1.0f,1.7f));
        p_glEnd();
        for(int i=0;i<4;i++){
            p_glPushMatrix(); p_glRotatef(90*i,0,0,1); p_glTranslatef(2.5f,0,0); p_glScalef(0.25f,0.85f,1.9f);
            p_glBegin(GL_TRIANGLES); face4(v(-1,-1,-1),v(1,-1,-1),v(1,1,-1),v(-1,1,-1)); face4(v(-1,-1,1),v(1,-1,1),v(1,1,1),v(-1,1,1)); p_glEnd();
            p_glPopMatrix();
        }
    }
}

static void draw_cockpit_3d(void){
    p_glDisable(GL_LIGHTING); p_glDisable(GL_TEXTURE_2D); p_glDisable(GL_DEPTH_TEST);
    p_glEnable(GL_BLEND); p_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    p_glMatrixMode(GL_MODELVIEW); p_glLoadIdentity();

    /* Colony-Wars-ish heavy canopy: angular, chunky, PS1 readable. */
    p_glColor4f(0.035f,0.045f,0.055f,1.0f);
    p_glBegin(GL_QUADS);
    /* low dash slabs */
    p_glVertex3f(-3.0f,-1.35f,-2.25f); p_glVertex3f(-1.20f,-1.04f,-3.15f); p_glVertex3f(-1.00f,-1.78f,-4.15f); p_glVertex3f(-3.2f,-2.0f,-2.55f);
    p_glVertex3f( 1.20f,-1.04f,-3.15f); p_glVertex3f( 3.0f,-1.35f,-2.25f); p_glVertex3f( 3.2f,-2.0f,-2.55f); p_glVertex3f( 1.00f,-1.78f,-4.15f);
    p_glVertex3f(-1.25f,-1.05f,-3.15f); p_glVertex3f(1.25f,-1.05f,-3.15f); p_glVertex3f(1.05f,-1.84f,-4.35f); p_glVertex3f(-1.05f,-1.84f,-4.35f);
    /* upper brow */
    p_glVertex3f(-2.7f,1.26f,-2.6f); p_glVertex3f(-0.38f,1.08f,-4.25f); p_glVertex3f(-0.30f,0.86f,-4.7f); p_glVertex3f(-2.85f,1.02f,-2.9f);
    p_glVertex3f( 0.38f,1.08f,-4.25f); p_glVertex3f( 2.7f,1.26f,-2.6f); p_glVertex3f( 2.85f,1.02f,-2.9f); p_glVertex3f( 0.30f,0.86f,-4.7f);
    p_glEnd();

    /* dark blue side screens embedded in dash */
    p_glColor4f(0.03f,0.12f,0.14f,0.88f);
    p_glBegin(GL_QUADS);
    p_glVertex3f(-2.35f,-1.08f,-2.55f); p_glVertex3f(-1.35f,-0.98f,-3.02f); p_glVertex3f(-1.23f,-1.38f,-3.35f); p_glVertex3f(-2.42f,-1.58f,-2.75f);
    p_glVertex3f( 1.35f,-0.98f,-3.02f); p_glVertex3f( 2.35f,-1.08f,-2.55f); p_glVertex3f( 2.42f,-1.58f,-2.75f); p_glVertex3f( 1.23f,-1.38f,-3.35f);
    p_glEnd();

    /* canopy struts */
    p_glColor4f(0.12f,0.15f,0.17f,1.0f);
    p_glBegin(GL_QUADS);
    p_glVertex3f(-2.35f,-1.32f,-2.25f); p_glVertex3f(-1.88f,-1.18f,-2.65f); p_glVertex3f(-1.08f,1.08f,-4.30f); p_glVertex3f(-1.36f,1.20f,-3.72f);
    p_glVertex3f(1.88f,-1.18f,-2.65f); p_glVertex3f(2.35f,-1.32f,-2.25f); p_glVertex3f(1.36f,1.20f,-3.72f); p_glVertex3f(1.08f,1.08f,-4.30f);
    p_glVertex3f(-3.10f,-1.18f,-2.10f); p_glVertex3f(-2.76f,-1.12f,-2.26f); p_glVertex3f(-2.78f,1.06f,-2.80f); p_glVertex3f(-3.18f,1.02f,-2.55f);
    p_glVertex3f(2.76f,-1.12f,-2.26f); p_glVertex3f(3.10f,-1.18f,-2.10f); p_glVertex3f(3.18f,1.02f,-2.55f); p_glVertex3f(2.78f,1.06f,-2.80f);
    p_glEnd();

    /* centre glass/HUD slab removed: crosshair remains in 2D HUD */

    /* chunky twin weapon pods */
    f32 flash = clampf(G.muzzle_flash*9.0f,0,1);
    for(int i=0;i<2;i++){
        f32 sx=i?1.16f:-1.16f;
        p_glColor4f(0.09f,0.11f,0.12f,1.0f);
        p_glBegin(GL_QUADS);
        p_glVertex3f(sx-0.28f,-0.92f,-2.65f); p_glVertex3f(sx+0.28f,-0.92f,-2.65f); p_glVertex3f(sx+0.17f,-0.72f,-4.18f); p_glVertex3f(sx-0.17f,-0.72f,-4.18f);
        p_glVertex3f(sx-0.20f,-1.08f,-2.75f); p_glVertex3f(sx+0.20f,-1.08f,-2.75f); p_glVertex3f(sx+0.12f,-0.94f,-4.05f); p_glVertex3f(sx-0.12f,-0.94f,-4.05f);
        p_glEnd();
        p_glColor4f(0.26f,0.32f,0.34f,1.0f);
        p_glBegin(GL_LINES); p_glVertex3f(sx,-0.82f,-2.95f); p_glVertex3f(sx,-0.80f,-4.35f); p_glEnd();
        if(flash>0){
            p_glColor4f(0.55f+0.45f*flash,1.0f,0.65f,0.28f+0.45f*flash);
            p_glBegin(GL_QUADS);
            p_glVertex3f(sx-0.23f,-0.95f,-4.0f); p_glVertex3f(sx+0.23f,-0.95f,-4.0f); p_glVertex3f(sx+0.06f,-0.68f,-5.55f); p_glVertex3f(sx-0.06f,-0.68f,-5.55f);
            p_glEnd();
        }
    }
    p_glDisable(GL_BLEND); p_glEnable(GL_DEPTH_TEST);
}

static void draw_world(void){
    /* primary sun light */
    V3 ld=vnorm(vsub(G.star[0].pos,G.ppos));
    f32 lp[4]={ld.x,ld.y,ld.z,0.0f};
    f32 amb[4]={0.20f,0.20f,0.24f,1};
    p_glEnable(GL_LIGHTING); p_glEnable(GL_LIGHT0);
    p_glLightfv(GL_LIGHT0,GL_POSITION,lp);
    f32 dif[4]={G.star[0].cr,G.star[0].cg,G.star[0].cb,1}; p_glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);
    p_glLightModelfv(GL_LIGHT_MODEL_AMBIENT,amb);
    p_glEnable(GL_COLOR_MATERIAL); p_glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);
    p_glShadeModel(GL_FLAT);

    /* suns */
    p_glDisable(GL_LIGHTING);
    for(int i=0;i<G.nstar;i++){
        p_glPushMatrix(); p_glTranslatef(G.star[i].pos.x,G.star[i].pos.y,G.star[i].pos.z); p_glScalef(G.star[i].r,G.star[i].r,G.star[i].r);
        p_glColor3f(G.star[i].cr,G.star[i].cg,G.star[i].cb); emit_sphere(10,14); p_glPopMatrix();
    }
    p_glEnable(GL_LIGHTING);

    /* planets / moons */
    for(int i=0;i<G.nb;i++){
        Body* b=&G.body[i]; V3 bp=body_draw_pos(i);
        p_glPushMatrix();
        p_glTranslatef(bp.x,bp.y,bp.z);
        p_glRotatef(G.t*b->spin + b->phase*57.3f,0.2f,1,0);
        if(b->has_ring){ p_glPushMatrix(); p_glRotatef(20 + 14*(i&1),1,0,0); draw_ring(b->r*1.35f,b->r*1.95f,0.74f,0.67f,0.58f); p_glPopMatrix(); }
        p_glScalef(b->r,b->r,b->r);
        p_glEnable(GL_TEXTURE_2D); p_glBindTexture(GL_TEXTURE_2D,b->tex);
        p_glColor3f(0.92f,0.92f,0.92f); emit_sphere(18,26);
        p_glDisable(GL_TEXTURE_2D);
        if(b->has_atmo){ p_glPushMatrix(); p_glScalef(1.08f,1.08f,1.08f); draw_atmo_shell(b->type); p_glPopMatrix(); }
        p_glPopMatrix();
    }

    /* station */
    p_glPushMatrix();
    p_glTranslatef(G.station.x,G.station.y,G.station.z);
    p_glRotatef(G.t*G.station_spin,0,0,1);
    if(G.station_type==3) p_glColor3f(0.48f,0.31f,0.28f);       /* black port */
        else if(G.station_type==2) p_glColor3f(0.62f,0.58f,0.48f);  /* industrial */
            else if(G.station_type==1) p_glColor3f(0.58f,0.70f,0.86f);  /* corp/research */
                else if(G.station_type==4) p_glColor3f(0.46f,0.68f,0.48f);  /* agri */
                    else if(G.station_type==5) p_glColor3f(0.66f,0.68f,0.74f);  /* fortress */
                        else p_glColor3f(0.62f,0.64f,0.68f);
                        p_glScalef(G.station_scale,G.station_scale,G.station_scale);
    emit_station(G.station_type);
    p_glPopMatrix();

    /* enemies / NPCs */
    for(int i=0;i<G.ne;i++){
        Ship* e=&G.e[i]; if(!e->alive) continue;
        V3 fwd = vlen(e->vel)>1? vnorm(e->vel) : vnorm(vsub(G.ppos,e->pos));
        f32 yaw=f_atan2(fwd.x,fwd.z)*180.0f/PI;
        f32 pit=-f_atan2(fwd.y, f_sqrt(fwd.x*fwd.x+fwd.z*fwd.z))*180.0f/PI;
        p_glPushMatrix();
        p_glTranslatef(e->pos.x,e->pos.y,e->pos.z);
        p_glRotatef(yaw,0,1,0); p_glRotatef(pit,1,0,0);
        { f32 sx=1.0f,sy=1.0f,sz=1.0f,sc=4.5f;
            if(e->type==SHIP_TRADER){ sx=1.18f; sy=1.02f; sz=0.96f; sc=4.8f; }
            if(e->type==SHIP_FREIGHTER){ sx=1.22f; sy=1.05f; sz=1.42f; sc=5.4f; }
            if(e->type==SHIP_POLICE){ sx=0.82f; sy=0.74f; sz=1.22f; sc=4.6f; }
            if(e->type==SHIP_FIGHTER){ sx=0.68f; sy=0.68f; sz=1.44f; sc=4.55f; }
            if(e->type==SHIP_MINER){ sx=1.20f; sy=1.16f; sz=0.90f; sc=5.0f; }
            if(e->type==SHIP_PIRATE){ sx=1.12f; sy=0.95f; sz=1.10f; sc=4.9f; }
            if(e->type==SHIP_ALIEN){ sx=1.08f; sy=1.18f; sz=1.06f; sc=4.9f; }
            p_glScalef(sc*sx,sc*sy,sc*sz);
        }
        ship_color(e->type); emit_ship(e->type);
        if(e->radar_flash>0){ f32 r,g,b; ship_rgb(e->type,&r,&g,&b); p_glColor3f(1.0f,0.9f,0.28f); p_glPointSize(7.0f); p_glBegin(GL_POINTS); p_glVertex3f(0,0,2.8f); p_glEnd(); p_glColor3f(r,g,b); }
        p_glPopMatrix();
    }

    /* missiles / beams */
    p_glDisable(GL_LIGHTING);
    p_glEnable(GL_BLEND); p_glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    p_glLineWidth(3.0f);
    p_glBegin(GL_LINES);
    for(int i=0;i<MAXM;i++) if(G.missile[i].alive){
        V3 tail=vsub(G.missile[i].pos,vmul(vnorm(G.missile[i].vel),28.0f));
        p_glColor4f(1.0f,0.82f,0.28f,0.90f);
        p_glVertex3f(tail.x,tail.y,tail.z); p_glVertex3f(G.missile[i].pos.x,G.missile[i].pos.y,G.missile[i].pos.z);
    }
    p_glEnd();
    p_glPointSize(8.0f);
    p_glBegin(GL_POINTS);
    for(int i=0;i<MAXM;i++) if(G.missile[i].alive){ p_glColor4f(1.0f,0.95f,0.55f,0.95f); p_glVertex3f(G.missile[i].pos.x,G.missile[i].pos.y,G.missile[i].pos.z); }
    p_glEnd();
    for(int pass=0; pass<2; pass++){
        p_glLineWidth(pass==0?6.0f:2.2f);
        p_glBegin(GL_LINES);
        for(int i=0;i<MAXB;i++){ if(G.beam[i].ttl<=0)continue;
            f32 a=clampf(G.beam[i].ttl*4.5f,0,1)*(pass==0?0.28f:0.95f);
            if(G.beam[i].hostile) p_glColor4f(1.0f,0.32f,0.22f,a);
            else                  p_glColor4f(0.55f,1.0f,0.70f,a);
            p_glVertex3f(G.beam[i].a.x,G.beam[i].a.y,G.beam[i].a.z);
            p_glVertex3f(G.beam[i].b.x,G.beam[i].b.y,G.beam[i].b.z);
        }
        p_glEnd();
    }
    p_glPointSize(7.0f);
    p_glBegin(GL_POINTS);
    for(int i=0;i<MAXB;i++){ if(G.beam[i].ttl<=0)continue;
        if(G.beam[i].hostile) p_glColor4f(1.0f,0.45f,0.32f,0.8f);
        else                  p_glColor4f(0.8f,1.0f,0.7f,0.8f);
        p_glVertex3f(G.beam[i].a.x,G.beam[i].a.y,G.beam[i].a.z);
        p_glVertex3f(G.beam[i].b.x,G.beam[i].b.y,G.beam[i].b.z);
    }
    p_glEnd();
    p_glDisable(GL_BLEND);

    if(G.state!=ST_TITLE) draw_cockpit_3d();
    p_glEnable(GL_LIGHTING);
}

/* switch to 2D overlay coords (0,0 top-left -> win_w,win_h) */
static void begin2d(void){
    p_glDisable(GL_LIGHTING); p_glDisable(GL_DEPTH_TEST); p_glDisable(GL_TEXTURE_2D);
    p_glMatrixMode(GL_PROJECTION); p_glLoadIdentity();
    p_glOrtho(0, G.win_w, G.win_h, 0, -1, 1);
    p_glMatrixMode(GL_MODELVIEW); p_glLoadIdentity();
}
static void end2d(void){ p_glEnable(GL_DEPTH_TEST); }

static void bar(f32 x,f32 y,f32 w,f32 h,f32 frac,f32 r,f32 g,f32 b){
    if(frac<0)frac=0;
    if(frac>1)frac=1;
    p_glColor4f(0.12f,0.12f,0.14f,0.8f);
    p_glBegin(GL_QUADS); p_glVertex2f(x,y);p_glVertex2f(x+w,y);p_glVertex2f(x+w,y+h);p_glVertex2f(x,y+h); p_glEnd();
    p_glColor3f(r,g,b);
    p_glBegin(GL_QUADS); p_glVertex2f(x,y);p_glVertex2f(x+w*frac,y);p_glVertex2f(x+w*frac,y+h);p_glVertex2f(x,y+h); p_glEnd();
}

static void panel(f32 x,f32 y,f32 w,f32 h,f32 r,f32 g,f32 b){
    p_glColor4f(0.02f,0.03f,0.04f,0.78f);
    p_glBegin(GL_QUADS); p_glVertex2f(x,y);p_glVertex2f(x+w,y);p_glVertex2f(x+w,h+y);p_glVertex2f(x,y+h); p_glEnd();
    p_glColor4f(r,g,b,0.75f);
    p_glBegin(GL_LINE_LOOP); p_glVertex2f(x,y);p_glVertex2f(x+w,y);p_glVertex2f(x+w,y+h);p_glVertex2f(x,y+h); p_glEnd();
}
static void tiny_lamp(f32 x,f32 y,int on,f32 r,f32 g,f32 b){
    p_glColor4f(on?r:0.10f,on?g:0.10f,on?b:0.10f,on?0.95f:0.55f);
    p_glBegin(GL_QUADS); p_glVertex2f(x,y);p_glVertex2f(x+10,y);p_glVertex2f(x+10,y+10);p_glVertex2f(x,y+10);p_glEnd();
}
static int nearest_contact(void){
    int best=-1; f32 bd=999999.0f;
    for(int i=0;i<G.ne;i++){ if(!G.e[i].alive)continue; f32 d=vlen(vsub(G.e[i].pos,G.ppos)); if(d<bd){bd=d;best=i;} }
    return best;
}
static int nearest_hostile(void){
    int found=0; f32 bd=999999.0f;
    for(int i=0;i<G.ne;i++) if(G.e[i].alive){
        Ship* e=&G.e[i]; int h=ship_hostile(e);
        if(h){ f32 d=vlen(vsub(G.e[i].pos,G.ppos)); if(d<bd){bd=d;found=1;} }
    }
    return found && bd<900.0f;
}

static void draw_ship_tags(void){
    const f32 fov=72.0f*PI/180.0f, ty=f_tan(fov*0.5f), asp=(f32)G.win_w/(f32)G.win_h;
    for(int i=0;i<G.ne;i++){
        Ship* e=&G.e[i]; if(!e->alive) continue;
        V3 rel=vsub(e->pos,G.ppos); f32 d=vlen(rel); if(d>2400.0f && i!=G.target) continue; if(d>4200.0f) continue;
        f32 cx=vdot(rel,G.pr), cy=vdot(rel,G.pu), cz=vdot(rel,G.pf); if(cz<25.0f) continue;
        f32 sx=G.win_w*0.5f + (cx/(cz*ty*asp))*G.win_w*0.5f;
        f32 sy=G.win_h*0.5f - ((cy+38.0f)/(cz*ty))*G.win_h*0.5f;
        if(sx<-80||sx>G.win_w+80||sy<-40||sy>G.win_h+40) continue;
        f32 r,g,b; ship_rgb(e->type,&r,&g,&b);
        const char* nm=ship_name(e->type); f32 sc=d<450.0f?1.45f:1.18f;
        text(sx-textw(nm,sc)*0.5f,sy,sc,nm,r,g,b);
        if(i==G.target) text(sx-14,sy+18,0.92f,"LOCK",1.0f,0.35f,0.22f);
        else if(d<900.0f){ char db[18]; int k=0; bnum(db,&k,(int)d); text(sx-textw(db,0.92f)*0.5f,sy+18,0.92f,db,r*0.75f,g*0.75f,b*0.75f); }
    }
}

static void draw_hud(void){
    f32 W=G.win_w, H=G.win_h;
    System* s=&gal[G.cur];
    f32 spd=vlen(G.pvel);
    f32 station_d=vlen(vsub(G.station,G.ppos));
    f32 heat = clampf(G.laser_heat,0,1);
    int hostile=nearest_hostile();

    /* central combiner crosshair */
    p_glColor3f(0.45f,1.0f,0.62f);
    p_glBegin(GL_LINES);
    p_glVertex2f(W/2-18,H/2); p_glVertex2f(W/2-5,H/2);
    p_glVertex2f(W/2+5,H/2);  p_glVertex2f(W/2+18,H/2);
    p_glVertex2f(W/2,H/2-18); p_glVertex2f(W/2,H/2-5);
    p_glVertex2f(W/2,H/2+5);  p_glVertex2f(W/2,H/2+18);
    p_glEnd();

    /* left Elite-style status stack */
    panel(18,22,292,178,0.24f,0.85f,0.62f);
    text(34,48,1.35f,"CMDR",0.45f,0.95f,0.70f); text(84,48,1.35f,cmdr_name,0.60f,1.0f,0.74f);
    text(34,72,2.1f,s->name,0.70f,1.0f,0.78f);
    text(34,100,1.35f,ECONAME[s->eco],0.44f,0.62f,0.70f);
    text(34,122,1.35f,GOVNAME[s->gov],0.44f,0.62f,0.70f);
    label_num(34,146,1.35f,"DANGER",s->danger,s->danger>5?0.95f:0.55f,s->danger>5?0.34f:0.85f,0.45f);
    text(34,172,1.55f,legal_name(),G.wanted?0.95f:0.45f,G.wanted?0.30f:0.95f,0.45f);
    { int pir=0,hun=0,pol=0,trd=0,min=0,ali=0;
        for(int i=0;i<G.ne;i++) if(G.e[i].alive){
            int t=G.e[i].type;
            if(t==SHIP_PIRATE) pir++;
            else if(t==SHIP_FIGHTER) hun++;
            else if(t==SHIP_POLICE) pol++;
            else if(t==SHIP_TRADER||t==SHIP_FREIGHTER) trd++;
            else if(t==SHIP_MINER) min++;
            else if(t==SHIP_ALIEN) ali++;
        }
        text(166,48,1.18f,"SENSOR",0.44f,0.82f,0.68f);
        { char b[16]; int k=0; bcat(b,&k,"PIR "); bnum(b,&k,pir); text(166,68,1.04f,b,pir?1.0f:0.42f,pir?0.35f:0.58f,pir?0.30f:0.55f); }
        { char b[16]; int k=0; bcat(b,&k,"POL "); bnum(b,&k,pol); text(236,68,1.04f,b,0.45f,0.70f,0.95f); }
        { char b[16]; int k=0; bcat(b,&k,"HUN "); bnum(b,&k,hun); text(166,88,1.04f,b,hun?0.95f:0.42f,hun?0.42f:0.58f,hun?0.90f:0.55f); }
        { char b[16]; int k=0; bcat(b,&k,"TRD "); bnum(b,&k,trd); text(236,88,1.04f,b,0.50f,0.84f,0.58f); }
        { char b[16]; int k=0; bcat(b,&k,"MIN "); bnum(b,&k,min); text(166,108,1.04f,b,0.86f,0.62f,0.28f); }
        { char b[16]; int k=0; bcat(b,&k,"UNK "); bnum(b,&k,ali); text(236,108,1.04f,b,ali?0.55f:0.42f,ali?0.95f:0.58f,ali?0.62f:0.55f); }
        { char b[16]; int k=0; bcat(b,&k,"SYS "); bnum(b,&k,G.sys_kills[G.cur]); text(166,132,1.02f,b,0.88f,0.78f,0.48f); }
        { char b[18]; int k=0; bcat(b,&k,"KILLS "); bnum(b,&k,G.kills); text(226,132,1.02f,b,0.78f,0.92f,0.80f); }
    }

    panel(18,H-200,300,178,0.24f,0.85f,0.62f);
    text(34,H-174,1.45f,"SHIP STATUS",0.48f,0.92f,0.70f);
    text(34,H-144,1.3f,"SHIELD",0.56f,0.72f,0.74f); bar(132,H-156,160,11,G.shield/G.maxshield,0.25f,0.75f,1.0f);
    text(34,H-118,1.3f,"ENERGY",0.56f,0.72f,0.74f); bar(132,H-130,160,11,G.hull/G.maxhull,0.88f,0.34f,0.30f);
    text(34,H-92,1.3f,"FUEL",0.56f,0.72f,0.74f); bar(132,H-104,160,11,(f32)G.fuel/(f32)G.maxfuel,0.90f,0.72f,0.25f);
    text(34,H-66,1.3f,"LASER TEMP",0.56f,0.72f,0.74f); bar(132,H-78,160,11,heat,heat>0.75f?1.0f:0.40f,heat>0.75f?0.22f:1.0f,0.50f);

    /* right target/nav panels */
    panel(W-330,22,312,202,0.22f,0.72f,0.95f);
    text(W-312,50,1.45f,"TARGET",0.45f,0.82f,1.0f);
    if(G.target>=0 && G.target<G.ne && G.e[G.target].alive){
        Ship* t=&G.e[G.target]; f32 td=vlen(vsub(t->pos,G.ppos));
        text(W-312,82,2.0f,ship_name(t->type),t->type==SHIP_PIRATE?1.0f:0.75f,t->type==SHIP_PIRATE?0.35f:0.95f,0.65f);
        label_num(W-312,112,1.45f,"DIST",(int)td,0.70f,0.88f,0.92f);
        label_num(W-312,138,1.45f,"HULL",(int)t->hull,0.88f,0.76f,0.50f);
        { int th=ship_hostile(t); text(W-312,166,1.35f,th?"STATUS HOSTILE":"STATUS NEUTRAL",th?1.0f:0.55f,th?0.35f:0.85f,0.45f); }
    } else {
        text(W-312,84,1.8f,"NO TARGET",0.55f,0.70f,0.72f);
    }
    { int ml=G.target; char mbuf[40]; int k=0; bcat(mbuf,&k,"LMB LASER RMB MSL "); bnum(mbuf,&k,G.missiles); text(W-312,190,1.18f,mbuf,G.missiles?0.86f:0.45f,G.missiles?0.82f:0.45f,0.50f);
        text(W-312,210,1.18f,ml>=0?"LOCK":"NO LOCK",ml>=0?0.95f:0.50f,ml>=0?0.35f:0.55f,ml>=0?0.22f:0.58f); }
        tiny_lamp(W-80,184,hostile,1.0f,0.25f,0.18f); text(W-62,194,1.2f,hostile?"RED":"GREEN",hostile?1.0f:0.45f,hostile?0.25f:0.95f,0.40f);

        panel(W-330,H-226,312,204,0.22f,0.72f,0.95f);
        text(W-312,H-200,1.45f,"NAV / TRADE",0.45f,0.82f,1.0f);
        label_num(W-312,H-170,1.35f,"SPEED",(int)spd,0.65f,0.85f,0.80f);
        label_num(W-172,H-170,1.35f,"RANGE",(int)G.jump_range,0.65f,0.75f,0.90f);
        label_num(W-312,H-144,1.35f,"STATION",(int)station_d,0.70f,0.88f,0.95f);
        label_num(W-312,H-118,1.35f,"CARGO",cargo_used(),0.75f,0.82f,0.82f);
        label_num(W-172,H-118,1.35f,"CAP",G.cargo_max,0.75f,0.82f,0.82f);
        label_num(W-312,H-92,1.35f,"CREDITS",G.credits,0.78f,1.0f,0.74f);
        label_num(W-312,H-66,1.35f,"PLANETS",G.nb,0.58f,0.72f,0.84f);
        label_num(W-172,H-66,1.35f,"SUNS",G.nstar,0.88f,0.78f,0.48f);
        if(G.docked_ok) text(W-312,H-40,1.35f,"F DOCK READY",0.48f,1.0f,0.58f);
        else { char rb[44]; int k=0; bcat(rb,&k,"STATION RANGE "); int n=1+(int)(station_d/180.0f); if(n<1)n=1; if(n>12)n=12; for(int i=0;i<n && k<42;i++) rb[k++]='-'; rb[k]=0; text(W-312,H-40,1.35f,rb,0.55f,0.60f,0.58f); }

        /* Elite scanner: bottom centre with lollipop blips */
        f32 cx=W/2, cy=H-104, rw=160, rh=62;
        p_glColor4f(0.07f,0.11f,0.11f,0.82f);
        p_glBegin(GL_QUADS); p_glVertex2f(cx-rw-22,cy-rh-22);p_glVertex2f(cx+rw+22,cy-rh-22);p_glVertex2f(cx+rw+22,cy+rh+26);p_glVertex2f(cx-rw-22,cy+rh+26);p_glEnd();
        p_glColor4f(0.28f,0.90f,0.60f,0.62f);
        p_glBegin(GL_LINE_LOOP); for(int i=0;i<48;i++){ f32 a=TAU*i/48; p_glVertex2f(cx+f_cos(a)*rw, cy+f_sin(a)*rh); } p_glEnd();
        p_glPointSize(5);
        p_glBegin(GL_POINTS); p_glVertex2f(cx,cy); p_glEnd();
        const f32 scan_r = 5200.0f;
        /* scanner uses local position within a fixed range, not pure direction.
         *      That stops nearby contacts from vanishing under far contacts in the same bearing. */
        { V3 rel=vsub(G.station,G.ppos); f32 fx=vdot(rel,G.pr), fy=vdot(rel,G.pu), fz=vdot(rel,G.pf); f32 d=vlen(rel); if(d>0.1f){
            f32 sx=cx+clampf(fx/scan_r,-1,1)*rw*0.92f, sy=cy-clampf(fz/scan_r,-1,1)*rh*0.92f, stem=clampf(fy/900.0f,-1,1)*32.0f;
            p_glColor3f(0.45f,0.75f,1.0f); p_glBegin(GL_LINES); p_glVertex2f(sx,sy); p_glVertex2f(sx,sy-stem); p_glEnd();
            p_glBegin(GL_LINE_LOOP); p_glVertex2f(sx-4,sy-stem-4); p_glVertex2f(sx+4,sy-stem-4); p_glVertex2f(sx+4,sy-stem+4); p_glVertex2f(sx-4,sy-stem+4); p_glEnd(); }}
            for(int i=0;i<G.ne;i++){ if(!G.e[i].alive)continue;
                V3 rel=vsub(G.e[i].pos,G.ppos); f32 d=vlen(rel); if(d<0.1f) d=0.1f;
                f32 fx=vdot(rel,G.pr), fy=vdot(rel,G.pu), fz=vdot(rel,G.pf);
                f32 sx=cx+clampf(fx/scan_r,-1,1)*rw*0.90f, sy=cy-clampf(fz/scan_r,-1,1)*rh*0.90f, stem=clampf(fy/700.0f,-1,1)*30.0f;
                int host=ship_hostile(&G.e[i]);
                int tgt=(i==G.target);
                f32 rr,gg,bb; ship_rgb(G.e[i].type,&rr,&gg,&bb);
                if(G.e[i].type==SHIP_ALIEN){ f32 q=0.55f+0.45f*f_sin(G.t*17.0f+i*1.7f); rr*=q; bb=0.72f+0.28f*q; }
                f32 fl=clampf(G.e[i].radar_flash*6.0f,0,1);
                if(fl>0.02f){ rr=rr*(1.0f-fl)+1.0f*fl; gg=gg*(1.0f-fl)+1.0f*fl; bb=bb*(1.0f-fl)+0.25f*fl; }
                p_glColor3f(rr,gg,bb);
                p_glBegin(GL_LINES); p_glVertex2f(sx,sy); p_glVertex2f(sx,sy-stem); p_glEnd();
                p_glPointSize((tgt?6.5f:4.6f) + fl*5.0f + (host?0.7f:0.0f));
                p_glBegin(GL_POINTS); p_glVertex2f(sx,sy-stem); p_glEnd();
                if(fl>0.02f){ p_glBegin(GL_LINE_LOOP); p_glVertex2f(sx-7,sy-stem-7); p_glVertex2f(sx+7,sy-stem-7); p_glVertex2f(sx+7,sy-stem+7); p_glVertex2f(sx-7,sy-stem+7); p_glEnd(); }
                else if(tgt){ p_glBegin(GL_LINE_LOOP); p_glVertex2f(sx-5,sy-stem-5); p_glVertex2f(sx+5,sy-stem-5); p_glVertex2f(sx+5,sy-stem+5); p_glVertex2f(sx-5,sy-stem+5); p_glEnd(); }
            }
            text(cx-textw(hostile?"CONDITION RED":"CONDITION GREEN",1.45f)/2,cy+rh+20,1.45f,hostile?"CONDITION RED":"CONDITION GREEN",hostile?1.0f:0.45f,hostile?0.28f:1.0f,0.48f);

            draw_ship_tags();
            if(G.alert_t>0){
                const char* msg = G.alert_code==1?"JUMP EXIT":G.alert_code==2?"HOSTILES ON ROUTE":G.alert_code==3?"CONTRABAND TRACE":G.alert_code==5?"DOCKING COMPLETE":G.alert_code==6?"LAUNCH CLEAR":"";
                text(W/2-textw(msg,1.75f)/2, 72, 1.75f, msg, 0.95f,0.78f,0.38f);
            }
            if(G.wanted) text(W/2-textw("WANTED",2.0f)/2, 34, 2.0f, "WANTED", 0.95f,0.3f,0.3f);
            if(G.laser_lockout) text(W/2-textw("LASER COOLING",1.8f)/2, H/2+52, 1.8f, "LASER COOLING", 1.0f,0.28f,0.22f);
            else if(G.laser_heat>0.82f) text(W/2-textw("LASER HOT",1.8f)/2, H/2+52, 1.8f, "LASER HOT", 1.0f,0.55f,0.25f);
            if(G.hull<18.0f) text(W/2-textw("ENERGY LOW",1.7f)/2, H/2+80, 1.7f, "ENERGY LOW", 1.0f,0.52f,0.22f);
            if(G.docked_ok && !G.wanted) text(W/2-textw("PRESS F TO DOCK",1.8f)/2, H/2+58, 1.8f, "PRESS F TO DOCK", 0.7f,0.95f,0.8f);
}

static void draw_station_nav(f32 H){
    text(60,H-92,1.40f,"0 COMMAND   1 MARKET   2 EQUIP SHIP   3 DATA ON   4 MANIFEST",0.55f,0.66f,0.68f);
    text(60,H-64,1.40f,"J CHART   F LAUNCH   BKSP/0 COMMAND",0.55f,0.66f,0.68f);
}

static void draw_market(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h;
    /* backdrop panel */
    p_glColor4f(0.05f,0.06f,0.08f,0.92f);
    p_glBegin(GL_QUADS); p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,G.win_h);p_glVertex2f(0,G.win_h); p_glEnd();
    System* s=&gal[G.cur];
    { char title[32]; int k=0; bcat(title,&k,s->name); bcat(title,&k," MARKET PRICES"); text(60,60,2.45f,title,0.6f,0.95f,0.8f); }
    text(60,90,1.55f,"CORCOM TRADE SYSTEM",0.48f,0.66f,0.70f);
    text(60,112,1.55f,GOVNAME[s->gov],0.45f,0.55f,0.65f);
    { int used=cargo_used(); char b[32]; int k=0; bcat(b,&k,"HOLD "); bnum(b,&k,used); bcat(b,&k,"/"); bnum(b,&k,G.cargo_max); text(W-150,90,1.5f,b,0.60f,0.86f,0.72f); }
    { int used=cargo_used(), free=G.cargo_max-used; char b[24]; int k=0; bcat(b,&k,"FREE "); bnum(b,&k,free); text(W-150,112,1.5f,b,free?0.60f:0.95f,free?0.78f:0.44f,free?0.72f:0.36f); }

    text(60,160,1.45f,"PRODUCT",0.5f,0.6f,0.7f);
    text(360,160,1.45f,"PRICE",0.5f,0.6f,0.7f);
    text(520,160,1.45f,"HOLD",0.5f,0.6f,0.7f);
    for(int c=0;c<NCOM;c++){
        f32 y=200+c*34;
        f32 hl = (c==G.msel)?1.0f:0.6f;
        if(c==G.msel){ p_glColor4f(0.15f,0.2f,0.18f,0.8f);
            p_glBegin(GL_QUADS);p_glVertex2f(54,y-14);p_glVertex2f(700,y-14);p_glVertex2f(700,y+16);p_glVertex2f(54,y+16);p_glEnd(); }
            text(60,y,1.7f,COM[c],hl*0.8f,hl*0.9f,hl*0.8f);
            text(360,y,1.7f,itos(s->price[c]),hl*0.8f,hl*1.0f,hl*0.7f);
            text(520,y,1.7f,itos(G.cargo[c]),hl*0.7f,hl*0.8f,hl*0.9f);
    }
    f32 yb=200+NCOM*34+30;
    text(60,yb,1.6f,"UP/DN BUY/SELL",0.55f,0.6f,0.65f);
    text(60,yb+26,1.5f,"FUEL/HARDWARE IN EQUIP",0.55f,0.6f,0.65f);
    { int d=map_best_dest(), bc=0, bu=0, tot=best_trade_total_to(d,&bc,&bu);
        if(tot>0&&bu>0){
            char b1[20]; int k1=0; bcat(b1,&k1,"BEST STARTER");
            text(60,yb+78,1.45f,b1,0.62f,0.86f,0.66f);
            char b2[64]; int k=0; bcat(b2,&k,COM[bc]); bcat(b2,&k," -> "); bcat(b2,&k,gal[d].name); bcat(b2,&k,"  MAX +"); bnum(b2,&k,tot); bcat(b2,&k,"CR");
            text(60,yb+98,1.45f,b2,0.62f,0.86f,0.66f);
        } else {
            text(60,yb+78,1.45f,"NO GOOD STARTER TRADE",0.70f,0.50f,0.45f);
        }
    }
    text(60,yb+110,1.45f,"LOOP: BUY CARGO  PICK ROUTE  SURVIVE RUN  DOCK",0.52f,0.64f,0.58f);
    draw_station_nav(H);

    char* cr=itos(G.credits);
    text(W-60-textw(cr,2.0f)-textw("CR ",2.0f),60,2.0f,"CR",0.6f,0.8f,0.7f);
    text(W-60-textw(cr,2.0f),60,2.0f,cr,0.8f,1.0f,0.8f);
    end2d();
}


static void draw_status_page(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h; System* s=&gal[G.cur];
    p_glColor4f(0.035f,0.045f,0.060f,0.96f);
    p_glBegin(GL_QUADS); p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();
    text(60,58,2.8f,"COMMAND SLATE",0.62f,0.96f,0.76f);
    text(60,100,1.6f,"COMMANDER",0.46f,0.70f,0.66f); text(190,100,1.6f,cmdr_name,0.72f,1.0f,0.76f);
    text(60,145,1.7f,"PRESENT SYSTEM",0.45f,0.62f,0.70f); text(300,145,1.9f,s->name,0.80f,1.00f,0.74f);
    text(60,178,1.7f,"CONDITION",0.45f,0.62f,0.70f); text(300,178,1.9f,G.docked?"DOCKED":(nearest_hostile()>=0?"RED":(nearest_contact()>=0?"YELLOW":"GREEN")),nearest_hostile()>=0?1.0f:0.55f,nearest_hostile()>=0?0.30f:0.95f,0.50f);
    text(60,211,1.7f,"LEGAL STATUS",0.45f,0.62f,0.70f); text(300,211,1.9f,legal_name(),G.wanted?1.0f:0.55f,G.wanted?0.32f:0.95f,0.50f);
    text(60,244,1.7f,"RATING",0.45f,0.62f,0.70f); text(300,244,1.9f,rating_name(),0.80f,0.95f,0.72f);
    label_num(60,282,1.7f,"KILLS",G.kills,0.74f,0.90f,0.75f);
    label_num(300,282,1.7f,"CASH",G.credits,0.86f,1.00f,0.75f);
    label_num(60,320,1.7f,"FUEL",G.fuel,0.88f,0.75f,0.42f);
    label_num(300,320,1.7f,"RANGE",(int)G.jump_range,0.60f,0.82f,0.95f);
    text(60,374,1.7f,"WORLD PROFILE",0.62f,0.96f,0.76f);
    text(80,412,1.55f,ECONAME[s->eco],0.56f,0.70f,0.78f);
    text(80,440,1.55f,GOVNAME[s->gov],0.56f,0.70f,0.78f);
    label_num(80,468,1.55f,"TECH",s->tech,0.55f,0.78f,0.88f);
    label_num(220,468,1.55f,"DANGER",s->danger,s->danger>5?0.95f:0.55f,s->danger>5?0.34f:0.85f,0.45f);

    text(W-520,100,1.7f,"FITTED EQUIPMENT",0.62f,0.96f,0.76f);
    text(W-500,138,1.55f,laser_name(),0.78f,0.92f,0.72f);
    label_num(W-500,170,1.55f,"MISSILES",G.missiles,0.75f,0.82f,0.95f);
    text(W-500,202,1.45f,G.extra_energy?"COPPER BANK":"STD GENERATOR",G.extra_energy?0.75f:0.45f,G.extra_energy?0.88f:0.55f,0.55f);
    text(W-500,230,1.45f,"DOCKING: F WHEN IN RANGE",0.55f,0.78f,0.68f);
    text(W-500,258,1.45f,"JUMP: SELECT ROUTE ON CHART",0.55f,0.70f,0.78f);

    draw_station_nav(H);
    end2d();
}

static void draw_manifest(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h; System* s=&gal[G.cur];
    p_glColor4f(0.035f,0.040f,0.055f,0.96f);
    p_glBegin(GL_QUADS); p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();
    text(60,58,2.8f,"HOLD MANIFEST",0.62f,0.96f,0.76f);
    text(60,98,1.6f,"CARGO / VALUE AT CURRENT PORT",0.48f,0.66f,0.70f);
    text(60,145,1.55f,"ITEM",0.44f,0.60f,0.68f);
    text(310,145,1.55f,"HOLD",0.44f,0.60f,0.68f);
    text(430,145,1.55f,"PRICE",0.44f,0.60f,0.68f);
    text(560,145,1.55f,"VALUE",0.44f,0.60f,0.68f);
    for(int c=0;c<NCOM;c++){
        f32 y=184+c*32;
        int illegal=(c==4||c==5);
        text(60,y,1.6f,COM[c],illegal?0.95f:0.78f,illegal?0.48f:0.88f,illegal?0.42f:0.74f);
        label_num(310,y,1.6f,"",G.cargo[c],0.75f,0.82f,0.88f);
        label_num(430,y,1.6f,"",s->price[c],0.72f,0.95f,0.70f);
        label_num(560,y,1.6f,"",G.cargo[c]*s->price[c],0.86f,1.00f,0.75f);
    }
    label_num(60,H-145,1.65f,"USED",cargo_used(),0.78f,0.90f,0.85f);
    label_num(180,H-145,1.65f,"CAP",G.cargo_max,0.78f,0.90f,0.85f);
    label_num(340,H-145,1.65f,"HOLD VALUE",cargo_value_here(),0.88f,1.00f,0.74f);
    if(illegal_cargo_count()>0) text(60,H-104,1.55f,"CONTRABAND WARNING",1.0f,0.42f,0.34f);
    else text(60,H-122,1.55f,"NO CONTRABAND REGISTERED",0.55f,0.90f,0.62f);
    draw_station_nav(H);
    end2d();
}

static void draw_equipment(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h; System* s=&gal[G.cur];
    p_glColor4f(0.035f,0.044f,0.060f,0.96f);
    p_glBegin(GL_QUADS); p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();
    text(60,58,2.8f,"EQUIP SHIP",0.62f,0.96f,0.76f);
    text(60,96,1.45f,"SHIP SERVICES",0.48f,0.66f,0.70f);
    label_num(60,128,1.55f,"TECH LEVEL",s->tech,0.62f,0.82f,0.90f);
    label_num(260,128,1.55f,"CASH",G.credits,0.86f,1.00f,0.75f);
    label_num(450,128,1.55f,"FUEL",G.fuel,0.88f,0.75f,0.42f);
    label_num(560,128,1.55f,"MISSILES",G.missiles,0.75f,0.82f,0.95f);

    text(70,166,1.35f,"ITEM",0.50f,0.62f,0.70f);
    text(560,166,1.35f,"PRICE",0.50f,0.62f,0.70f);
    text(690,166,1.35f,"TECH",0.50f,0.62f,0.70f);
    text(820,166,1.35f,"STATUS",0.50f,0.62f,0.70f);

    for(int i=0;i<NEQ;i++){
        f32 y=198+i*28;
        int sel=(i==G.esel);
        if(sel){ p_glColor4f(0.13f,0.19f,0.17f,0.82f); p_glBegin(GL_QUADS); p_glVertex2f(54,y-16);p_glVertex2f(W-56,y-16);p_glVertex2f(W-56,y+9);p_glVertex2f(54,y+9); p_glEnd(); }
        f32 r=eq_can_buy(i)?0.78f:(eq_present(i)?0.45f:0.55f);
        f32 g=eq_can_buy(i)?0.92f:(eq_present(i)?0.60f:0.55f);
        f32 b=eq_can_buy(i)?0.72f:(eq_present(i)?0.62f:0.55f);
        char nm[72]; int k=0;
        nm[k++]=sel?'>' : ' '; nm[k++]=' ';
        bcat(nm,&k,EQNAME[i]);
        text(70,y,1.35f,nm,r,g,b);
        if(i==0 && G.fuel>=G.maxfuel) text(560,y,1.35f,"FULL",0.45f,0.60f,0.62f);
        else label_num(560,y,1.35f,"",eq_cost(i),r,g,b);
        if(eq_tech(i)==0) text(690,y,1.35f,"ALWAYS",0.55f,0.68f,0.70f);
        else label_num(690,y,1.35f,"",eq_tech(i),eq_available(i)?0.62f:0.90f,eq_available(i)?0.82f:0.42f,0.70f);
        if(eq_present(i)) text(820,y,1.35f,i==1?"ALL":"PRESENT",0.48f,0.68f,0.66f);
        else if(!eq_available(i)) text(820,y,1.35f,"NOT HERE",0.82f,0.48f,0.40f);
        else if(G.credits<eq_cost(i)) text(820,y,1.35f,"CASH?",0.82f,0.62f,0.42f);
        else text(820,y,1.35f,"AVAILABLE",0.62f,0.90f,0.62f);
    }

    text(60,H-122,1.45f,"UP/DN RIGHT BUY",0.55f,0.66f,0.68f);
    draw_station_nav(H);
    end2d();
}

static void draw_data_page(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h; int idx=G.mapsel; if(idx<0||idx>=NSYS) idx=G.cur;
    System* s=&gal[idx];
    p_glColor4f(0.030f,0.040f,0.055f,0.96f);
    p_glBegin(GL_QUADS); p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();

    { char title[32]; int k=0; bcat(title,&k,"DATA ON "); bcat(title,&k,s->name); text(60,58,2.8f,title,0.62f,0.96f,0.76f); }
    text(60,96,1.55f,"WORLDATA LINK / PORT AUTHORITY DOSSIER",0.48f,0.66f,0.70f);

    f32 lx=60, rx=W-480, y=148;
    text(lx,y,1.75f,"PLANETARY PROFILE",0.62f,0.96f,0.76f); y+=38;

    { char b[80]; int k=0; bcat(b,&k,"DISTANCE: "); bnum(b,&k,(int)(sys_dist_idx(G.cur,idx)+0.5f)); bcat(b,&k," LIGHT YEARS"); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"ECONOMY: "); bcat(b,&k,ECONAME[s->eco]); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"GOVERNMENT: "); bcat(b,&k,GOVNAME[s->gov]); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"TECH LEVEL: "); bnum(b,&k,s->tech); bcat(b,&k,"   DANGER: "); bnum(b,&k,s->danger); text(lx,y,1.55f,b,s->danger>5?0.92f:0.70f,s->danger>5?0.50f:0.86f,0.62f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"POPULATION: "); bnum(b,&k,population_idx(idx)); bcat(b,&k," BILLION"); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"PRODUCTIVITY: "); bnum(b,&k,productivity_idx(idx)); bcat(b,&k," M CR"); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[80]; int k=0; bcat(b,&k,"AVERAGE RADIUS: "); bnum(b,&k,radius_idx(idx)); bcat(b,&k," KM"); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=30; }
    { char b[96]; int k=0; bcat(b,&k,"MAIN LIFEFORM: "); bcat(b,&k,lifeform_idx(idx)); text(lx,y,1.55f,b,0.70f,0.86f,0.82f); y+=48; }

    text(rx,148,1.75f,"TRADE NOTES",0.62f,0.96f,0.76f);
    int bc=0, bu=0, tot=best_trade_total_to(idx,&bc,&bu); int held=held_sale_profit_to(idx);
    if(idx==G.cur){
        text(rx,188,1.5f,"CURRENT PORT",0.62f,0.82f,0.70f);
        text(rx,216,1.5f,"PRICES GUARANTEED",0.62f,0.82f,0.70f);
    } else if(tot>0 && bu>0){
        char b[80]; int k=0; bcat(b,&k,"EXPORT "); bcat(b,&k,COM[bc]); bcat(b,&k," X"); bnum(b,&k,bu); text(rx,188,1.5f,b,0.62f,0.90f,0.62f);
        k=0; b[0]=0; bcat(b,&k,"EXPECTED PROFIT +"); bnum(b,&k,tot); bcat(b,&k,"CR"); text(rx,216,1.5f,b,0.62f,0.90f,0.62f);
    } else {
        text(rx,188,1.5f,"NO OBVIOUS",0.75f,0.52f,0.45f);
        text(rx,216,1.5f,"AFFORDABLE EXPORT",0.75f,0.52f,0.45f);
    }
    if(cargo_used()>0){ char b[80]; int k=0; bcat(b,&k,"CURRENT HOLD SALE: "); if(held>=0)bcat(b,&k,"+"); bnum(b,&k,held); bcat(b,&k,"CR"); text(rx,252,1.5f,b,held>=0?0.62f:0.92f,held>=0?0.90f:0.42f,0.62f); }

    text(rx,316,1.75f,"NAVIGATION",0.62f,0.96f,0.76f);
    { char b[80]; int k=0; bcat(b,&k,"FUEL: "); bnum(b,&k,G.fuel); bcat(b,&k,"   NEED: "); bnum(b,&k,fuel_need_idx(G.cur,idx)); text(rx,356,1.5f,b,0.76f,0.78f,0.62f); }
    { char b[80]; int k=0; bcat(b,&k,"RANGE: "); bnum(b,&k,(int)G.jump_range); bcat(b,&k," LIGHT YEARS"); text(rx,388,1.5f,b,0.60f,0.82f,0.95f); }
    text(rx,430,1.5f,can_jump_idx(idx)?"JUMP ROUTE AVAILABLE":"ROUTE NOT AVAILABLE",can_jump_idx(idx)?0.58f:0.95f,can_jump_idx(idx)?0.95f:0.42f,0.58f);

    text(60,H-126,1.35f,"PRICES CHANGE BY PORT.",0.50f,0.58f,0.60f);
    draw_station_nav(H);
    end2d();
}

static void draw_map(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h;
    p_glColor4f(0.03f,0.04f,0.06f,0.95f);
    p_glBegin(GL_QUADS);p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();
    text(60,56,2.6f,"GALAXY MAP",0.6f,0.95f,0.8f);

    System* cu=&gal[G.cur]; System* tg=&gal[G.mapsel];
    f32 ox=W*0.48f, oy=H*0.53f, sc=14.0f;
    f32 fuelring = (G.fuel<G.jump_range?G.fuel:G.jump_range);
    /* full range ring */
    p_glColor4f(0.22f,0.34f,0.35f,0.5f);
    p_glBegin(GL_LINE_LOOP);
    for(int i=0;i<56;i++){ f32 a=TAU*i/56; p_glVertex2f(ox+f_cos(a)*G.jump_range*sc, oy+f_sin(a)*G.jump_range*sc); }
    p_glEnd();
    /* fuel-limited ring */
    p_glColor4f(0.45f,0.65f,0.28f,0.65f);
    p_glBegin(GL_LINE_LOOP);
    for(int i=0;i<56;i++){ f32 a=TAU*i/56; p_glVertex2f(ox+f_cos(a)*fuelring*sc, oy+f_sin(a)*fuelring*sc); }
    p_glEnd();

    /* route line */
    if(G.mapsel!=G.cur){
        f32 sx=ox+(tg->x-cu->x)*0.35f*sc, sy=oy+(tg->z-cu->z)*0.35f*sc;
        p_glColor4f(can_jump_idx(G.mapsel)?0.5f:0.85f, can_jump_idx(G.mapsel)?0.95f:0.25f, 0.45f, 0.65f);
        p_glBegin(GL_LINES); p_glVertex2f(ox,oy); p_glVertex2f(sx,sy); p_glEnd();
    }

    p_glPointSize(4);
    p_glBegin(GL_POINTS);
    for(int i=0;i<NSYS;i++){
        f32 sx=ox+(gal[i].x-cu->x)*0.35f*sc, sy=oy+(gal[i].z-cu->z)*0.35f*sc;
        if(sx<12||sx>W-390||sy<82||sy>H-20) continue;
        int rch = (i!=G.cur && can_jump_idx(i));
        if(i==G.cur) p_glColor3f(0.35f,0.95f,1.0f);
        else if(i==G.mapsel) p_glColor3f(1.0f,0.9f,0.28f);
        else if(rch) p_glColor3f(0.55f,0.82f,0.62f);
        else if(gal[i].eco==ECO_LAWLESS) p_glColor3f(0.65f,0.25f,0.25f);
        else p_glColor3f(0.25f,0.32f,0.34f);
        p_glVertex2f(sx,sy);
    }
    p_glEnd();

    /* side panel */
    f32 px=W-360, py=96;
    p_glColor4f(0.05f,0.065f,0.085f,0.78f);
    p_glBegin(GL_QUADS); p_glVertex2f(px-24,74);p_glVertex2f(W-36,74);p_glVertex2f(W-36,H-52);p_glVertex2f(px-24,H-52); p_glEnd();
    text(px,py,1.4f,"CURRENT",0.42f,0.56f,0.62f);
    text(px,py+22,2.0f,cu->name,0.55f,0.9f,0.95f);
    text(px,py+48,1.4f,ECONAME[cu->eco],0.45f,0.55f,0.62f);

    py += 92;
    text(px,py,1.4f,"SELECTED",0.42f,0.56f,0.62f);
    text(px,py+22,2.2f,tg->name,0.9f,1.0f,0.72f);
    text(px,py+52,1.5f,ECONAME[tg->eco],0.55f,0.65f,0.75f);
    text(px,py+74,1.5f,GOVNAME[tg->gov],0.5f,0.6f,0.7f);
    { char b[20]; b[0]='T';b[1]='E';b[2]='C';b[3]='H';b[4]=':';b[5]=' '; char* a=itos(tg->tech); int k=6,j=0; while(a[j])b[k++]=a[j++]; b[k]=0; text(px,py+96,1.5f,b,0.55f,0.7f,0.8f); }
    { char b[20]; b[0]='D';b[1]='A';b[2]='N';b[3]='G';b[4]='E';b[5]='R';b[6]=':';b[7]=' '; char* a=itos(tg->danger); int k=8,j=0; while(a[j])b[k++]=a[j++]; b[k]=0; text(px,py+118,1.5f,b,tg->danger>5?0.95f:0.55f,tg->danger>5?0.35f:0.7f,0.45f); }

    f32 d=sys_dist_idx(G.cur,G.mapsel); int need=fuel_need_idx(G.cur,G.mapsel);
    py += 158;
    { char b[24]; char* a=itos((int)(d+0.5f)); int k=0; b[k++]='D';b[k++]='I';b[k++]='S';b[k++]='T';b[k++]=':';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k++]=' ';b[k++]='L';b[k++]='Y'; b[k]=0;
        text(px,py,1.6f,b, d<=G.jump_range?0.55f:0.95f, d<=G.jump_range?0.8f:0.35f,0.45f); }
        { char b[24]; char* a=itos(G.fuel); int k=0; b[k++]='F';b[k++]='U';b[k++]='E';b[k++]='L';b[k++]=':';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k++]='/'; a=itos(G.maxfuel); j=0; while(a[j])b[k++]=a[j++]; b[k]=0;
            text(px,py+24,1.6f,b, G.fuel>=need?0.55f:0.95f, 0.75f,0.45f); }
            { char b[24]; char* a=itos(need); int k=0; b[k++]='N';b[k++]='E';b[k++]='E';b[k++]='D';b[k++]=':';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k]=0;
                text(px,py+48,1.6f,b, G.fuel>=need?0.55f:0.95f, G.fuel>=need?0.9f:0.35f,0.45f); }
                { char b[24]; char* a=itos((int)G.jump_range); int k=0; b[k++]='R';b[k++]='A';b[k++]='N';b[k++]='G';b[k++]='E';b[k++]=':';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k++]=' ';b[k++]='L';b[k++]='Y'; b[k]=0;
                    text(px,py+72,1.6f,b,0.55f,0.68f,0.78f); }

                    int bc=0, bu=0, total=best_trade_total_to(G.mapsel,&bc,&bu);
                    int tr=best_trade_to(G.mapsel,&bc), held=held_sale_profit_to(G.mapsel);
                    py += 118;
                    text(px,py,1.4f,"TRADE READOUT",0.42f,0.56f,0.62f);
                    if(total>0 && bu>0){
                        text(px,py+24,1.5f,"AFFORDABLE RUN",0.55f,0.78f,0.55f);
                        text(px,py+46,1.7f,COM[bc],0.8f,0.95f,0.65f);
                        { char b[24]; char* a=itos(bu); int k=0; b[k++]='U';b[k++]='N';b[k++]='I';b[k++]='T';b[k++]='S';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k]=0; text(px+140,py+46,1.55f,b,0.68f,0.88f,0.75f); }
                        { char b[24]; char* a=itos(total); int k=0; b[k++]='+'; int j=0; while(a[j])b[k++]=a[j++]; b[k++]=' ';b[k++]='C';b[k++]='R'; b[k]=0; text(px,py+70,1.6f,b,0.7f,1.0f,0.65f); }
                    } else if(tr>0){
                        text(px,py+24,1.5f,"PROFIT EXISTS BUT CASH LOW",0.85f,0.68f,0.42f);
                        text(px,py+46,1.7f,COM[bc],0.8f,0.95f,0.65f);
                    } else text(px,py+24,1.5f,"NO OBVIOUS PROFIT",0.65f,0.55f,0.45f);
                    if(cargo_used()>0){ char b[28]; char* a=itos(held); int k=0; b[k++]='H';b[k++]='E';b[k++]='L';b[k++]='D';b[k++]=' ';b[k++]='C';b[k++]='A';b[k++]='R';b[k++]='G';b[k++]='O';b[k++]=':';b[k++]=' '; int j=0; while(a[j])b[k++]=a[j++]; b[k]=0; text(px,py+94,1.5f,b,held>=0?0.6f:0.9f,held>=0?0.85f:0.35f,0.55f); }

                    text(60,H-76,1.5f,"ARROWS SELECT",0.55f,0.6f,0.65f);
                    text(60,H-50,1.7f,"ENTER/J JUMP 5 DATA M BACK",0.62f,0.75f,0.72f);
                    if(can_jump_idx(G.mapsel)) text(60,H-24,1.8f,"> JUMP READY",0.5f,0.95f,0.7f);
                    else if(G.mapsel==G.cur) text(60,H-24,1.7f,"NO DESTINATION SELECTED",0.9f,0.55f,0.4f);
                    else if(d>G.jump_range) text(60,H-24,1.7f,"OUT OF JUMP RANGE",0.95f,0.45f,0.35f);
                    else text(60,H-24,1.7f,"BUY FUEL IN EQUIP",0.95f,0.45f,0.35f);
                    end2d();
}

static void draw_jump_screen(void){
    begin2d();
    f32 W=G.win_w,H=G.win_h,cx=W*0.5f,cy=H*0.5f;
    p_glColor4f(0.0f,0.01f,0.02f,0.96f);
    p_glBegin(GL_QUADS);p_glVertex2f(0,0);p_glVertex2f(W,0);p_glVertex2f(W,H);p_glVertex2f(0,H);p_glEnd();
    f32 m=W>H?W:H;
    p_glEnable(GL_BLEND); p_glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    for(int q=0;q<2;q++){
        seed(hash32(gal[G.target].seed));
        p_glLineWidth(q?2.8f:1.0f);
        p_glBegin(GL_LINES);
        for(int i=0;i<144;i++){
            f32 a=frand()*TAU+G.t*0.07f, p=frand()+G.t*(q?4.6f:2.7f);
            p-=f_floor(p);
            a += (frand()-0.5f)*0.13f*f_sin(G.t*5.0f+i);
            f32 r=(0.055f+p*p*0.72f)*m, l=(0.04f+p*0.36f)*m, c=(0.18f+p*0.82f)*(q?1.1f:0.45f);
            p_glColor4f(c*0.58f,c*0.76f,c, q?0.92f:0.26f);
            p_glVertex2f(cx+f_cos(a)*r,cy+f_sin(a)*r);
            p_glVertex2f(cx+f_cos(a)*(r+l),cy+f_sin(a)*(r+l));
        }
        p_glEnd();
    }
    p_glDisable(GL_BLEND); p_glLineWidth(1.0f);
    end2d();
}

static void draw_dead(void){
    begin2d();
    p_glColor4f(0.08f,0.0f,0.0f,0.96f);
    p_glBegin(GL_QUADS);p_glVertex2f(0,0);p_glVertex2f(G.win_w,0);p_glVertex2f(G.win_w,G.win_h);p_glVertex2f(0,G.win_h);p_glEnd();
    text(G.win_w/2-textw("SHIP DESTROYED",3.0f)/2, G.win_h/2-20, 3.0f, "SHIP DESTROYED", 0.9f,0.3f,0.3f);
    text(G.win_w/2-textw("PRESS ENTER TO RESPAWN",1.8f)/2, G.win_h/2+30, 1.8f, "PRESS ENTER TO RESPAWN", 0.7f,0.5f,0.5f);
    end2d();
}

/* ============================ audio (optional) ============================ */
#define MDEL_L 18522
#define MDEL_R 25137
static f32 music_dl[MDEL_L], music_dr[MDEL_R];
static int music_di_l=0, music_di_r=0;

static int music_state_on(void){
    return G.state==ST_TITLE || G.state==ST_MARKET || G.state==ST_STATUS ||
    G.state==ST_DATA || G.state==ST_MANIFEST || G.state==ST_EQUIP || G.state==ST_MAP;
}
static f32 note_hz(int deg,int oct){
    static const f32 f[7]={110.00f,123.47f,130.81f,146.83f,164.81f,174.61f,196.00f};
    f32 r=f[deg%7]; while(oct>0){r*=2.0f;oct--;} while(oct<0){r*=0.5f;oct++;} return r;
}
static f32 env_note(f32 x,f32 dur,f32 a,f32 d,f32 sus,f32 rel){
    if(x<0.0f||x>dur) return 0.0f;
    if(x<a) return x/a;
    if(x<a+d){ f32 q=(x-a)/d; return 1.0f+(sus-1.0f)*q; }
    if(x<dur-rel) return sus;
    return clampf(sus*(dur-x)/rel,0.0f,1.0f);
}
static f32 music_note_at(f32 t,int deg,int oct,f32 start,f32 vel){
    f32 x=t-start, dur=3.25f;
    f32 e=env_note(x,dur,0.010f,0.23f,0.18f,1.85f)*vel;
    if(e<=0.0f) return 0.0f;
    f32 f=note_hz(deg,oct);
    f32 w=f_sin(TAU*f*x)*0.18f + f_sin(TAU*f*2.01f*x)*0.065f + f_sin(TAU*f*3.02f*x)*0.030f;
    if(x<0.025f){ f32 nz=((f32)(hash32((u32)(t*44100.0f)+deg*911u)&255)-128.0f)/128.0f; w += nz*(1.0f-x/0.025f)*0.025f; }
    return w*e;
}
static f32 music_menu_sample(f32 t){
    const f32 beat=0.8333333f, sec=6.666666f;
    f32 sm=0.0f;
    int bar=(int)f_floor(t/sec);
    /* soft minor pad */
    static const int chords[4][3]={{0,3,5},{5,1,3},{3,6,2},{4,0,2}};
    int ci=bar&3;
    f32 local=t-sec*f_floor(t/sec);
    f32 fade=local/1.8f; if(fade>1.0f)fade=1.0f;
    f32 out=(sec-local)/1.8f; if(out<fade)fade=out; if(fade<0.0f)fade=0.0f;
    for(int j=0;j<3;j++){
        f32 f=note_hz(chords[ci][j],1);
        sm += (f_sin(TAU*f*0.997f*t + (f32)(j+1)*1.7f)*0.030f + f_sin(TAU*f*1.004f*t + (f32)(j+3)*2.1f)*0.025f)*fade;
    }
    sm *= 0.75f + 0.25f*f_sin(TAU*0.045f*t);
    /* sparse piano-like motif, check current and previous section for note tails */
    static const f32 off[8]={0.0f,1.5f,3.0f,5.0f,6.0f,8.0f,10.0f,11.5f};
    static const int deg[8]={0,2,3,1,0,5,4,2};
    static const int oct[8]={1,1,1,1,2,1,1,1};
    static const f32 vel[8]={1.0f,0.70f,0.78f,0.50f,0.60f,0.68f,0.52f,0.58f};
    for(int b=-1;b<=1;b++){
        f32 base=(f32)(bar+b)*sec;
        for(int i=0;i<8;i++) sm += music_note_at(t,deg[i],oct[i],base+off[i]*beat,vel[i]);
    }
    /* low bass pulses */
    f32 qsec=beat*4.0f; int qb=(int)f_floor(t/qsec);
    static const f32 boff[3]={0.0f,2.0f,3.25f}; static const int bdeg[3]={0,5,3};
    for(int b=-1;b<=0;b++){
        f32 base=(f32)(qb+b)*qsec;
        for(int i=0;i<3;i++){
            f32 x=t-(base+boff[i]*beat), dur=1.7f;
            f32 e=env_note(x,dur,0.035f,0.18f,0.55f,0.65f);
            if(e>0.0f){ f32 f=note_hz(bdeg[i],-1); sm += e*(f_sin(TAU*f*x)*0.105f + f_sin(TAU*f*2.0f*x)*0.025f); }
        }
    }
    /* restrained heartbeat pulse */
    f32 ph=t-(beat*2.0f)*f_floor(t/(beat*2.0f));
    if(ph<0.18f){ f32 e=1.0f-ph/0.18f; e*=e; sm += f_sin(TAU*(58.0f+25.0f*e)*ph)*e*0.055f; }
    /* moving air */
    f32 nz=((f32)(hash32((u32)(t*44100.0f)*13u+0xBEEFu)&255)-128.0f)/128.0f;
    sm += nz*0.004f*(0.45f+0.55f*f_sin(TAU*0.031f*t));
    return sm;
}

static void audio_cb(void* ud, u8* stream, int len){
    (void)ud;
    int16_t* out=(int16_t*)stream; int n=len/2;
    const f32 dt=0.0000226757f;
    for(int i=0;i<n;i+=2){
        int menu=music_state_on();
        f32 mt=menu?1.0f:0.0f;
        G.music_amp += (mt-G.music_amp)*0.000045f;

        G.audio_phase += 0.012f + G.audio_target_amp*0.020f;
        if(G.audio_phase>TAU) G.audio_phase-=TAU;
        f32 en = G.audio_target_amp;
        f32 nz0=((f32)(hash32(G.audio_n*17u+0x99u)&255)-128.0f)/128.0f;
        f32 hum = f_sin(G.audio_phase)*0.48f + f_sin(G.audio_phase*2.01f)*0.18f + nz0*0.10f*en;
        f32 mono = hum*(0.030f + en*0.30f);
        f32 l=mono, r=mono;

        if(G.music_amp>0.001f){
            f32 dry=music_menu_sample(G.music_t);
            f32 pan=0.18f*f_sin(TAU*0.017f*G.music_t);
            f32 ml=dry*(0.80f-pan), mr=dry*(0.80f+pan);
            f32 dl=music_dl[music_di_l], dr=music_dr[music_di_r];
            music_dl[music_di_l]=ml+dr*0.34f;
            music_dr[music_di_r]=mr+dl*0.31f;
            music_di_l++; if(music_di_l>=MDEL_L) music_di_l=0;
            music_di_r++; if(music_di_r>=MDEL_R) music_di_r=0;
            ml += dl*0.38f; mr += dr*0.38f;
            l += ml*1.55f*G.music_amp;
            r += mr*1.55f*G.music_amp;
            G.music_t += dt;
        } else if(menu) G.music_t += dt;

        G.audio_n++;
        for(int j=0;j<MAXSFX;j++) if(G.sfx[j].t>0){
            Sfx* s=&G.sfx[j];
            f32 x=1.0f - s->t/s->d, env=(1.0f-x); env*=env;
            f32 nz=((f32)(hash32(G.audio_n + j*911u + (u32)(s->p*97.0f))&255)-128.0f)/128.0f;
            f32 fr=s->f, w=0;
            switch(s->type){
                case SFX_LASER:   fr += 1100.0f*(1.0f-x); w=f_sin(s->p)+0.22f*nz; break;
                case SFX_MISSILE: fr += 620.0f*x; w=0.55f*f_sin(s->p)+0.55f*nz; env=1.0f-x*0.65f; break;
                case SFX_HIT:     fr += 120.0f*(1.0f-x); w=0.35f*f_sin(s->p)+0.85f*nz; break;
                case SFX_KILL:    fr += 80.0f*(1.0f-x); w=0.45f*f_sin(s->p)+0.90f*nz; break;
                case SFX_DOCK:    fr += 520.0f*(x>0.55f); w=f_sin(s->p)*(x<0.48f?1.0f:0.55f); break;
                case SFX_LAUNCH:  fr += 260.0f*x; w=0.55f*f_sin(s->p)+0.40f*nz; env=1.0f-x; break;
                case SFX_JUMP:    fr += 900.0f*x; w=0.65f*f_sin(s->p)+0.40f*nz; env=x<0.85f?0.80f:(1.0f-x)*5.0f; break;
                case SFX_BUY:     fr += 260.0f*x; w=f_sin(s->p); break;
                case SFX_WARN:    fr += 90.0f*((G.audio_n>>11)&1); w=f_sin(s->p); env=(x<0.85f)?0.95f:(1.0f-x)*6.0f; break;
                default:          fr += 70.0f*(1.0f-x); w=0.25f*f_sin(s->p)+0.90f*nz; break;
            }
            s->p += fr*0.0001424759f;
            if(s->p>TAU) s->p-=TAU;
            l += w*env*s->a;
            r += w*env*s->a;
            s->t -= dt;
        }
        l = l/(1.0f+f_abs(l)*0.55f);
        r = r/(1.0f+f_abs(r)*0.55f);
        if(l>1) l=1;
        if(l<-1) l=-1;
        if(r>1) r=1;
        if(r<-1) r=-1;
        out[i]=(int16_t)(l*30000.0f); out[i+1]=(int16_t)(r*30000.0f);
    }
}


/* ============================ title screen ================================ */
static void title_start_game(void){
    if(!title_name[0]) strset_lim(title_name,"PINGUY",12);
    strset_lim(cmdr_name,title_name,12);
    u32 adev=G.adev;
    new_game(title_default_seed);
    G.adev=adev;
    p_SDL_SetRelativeMouseMode(0);
}
static void update_title(const u8* ks){
    if(edge(ks,SC_BACKSPACE)) title_back(title_name);
    for(int i=0;i<26;i++) if(edge(ks,4+i)) title_add(title_name,12,(char)('A'+i));
    if(edge(ks,SC_RETURN)) title_start_game();
}
static void draw_title_screen(void){
    f32 W=G.win_w?G.win_w:1280, H=G.win_h?G.win_h:720;
    f32 far=7200.0f;
    for(int i=0;i<G.nb;i++) if(G.body[i].parent<0){ f32 d=vlen(G.body[i].pos); if(d>far) far=d; }
    G.t=(f32)p_SDL_GetTicks()*0.001f;
    f32 a=G.t*0.10f;
    G.ppos=v(f_cos(a)*far*1.05f, far*0.27f, f_sin(a)*far*1.05f);
    orient_player_to(G.star[0].pos);
    set_view();
    draw_starfield();
    draw_world();
    begin2d();
    text(W/2-textw("VOIDRUNNER",4.5f)/2,58,4.5f,"VOIDRUNNER",0.58f,1.0f,0.78f);
    text(W/2-textw("TRADE  FIGHT  RUN  DOCK",1.45f)/2,112,1.45f,"TRADE  FIGHT  RUN  DOCK",0.52f,0.68f,0.74f);
    panel(W/2-245,H-154,490,96,0.08f,0.80f,1.0f);
    text(W/2-208,H-128,1.35f,"COMMANDER NAME",0.45f,0.68f,0.76f);
    text(W/2-208,H-100,2.0f,title_name[0]?title_name:"_",0.78f,1.0f,0.78f);
    text(W/2-textw("BACKSPACE DELETE   ENTER LAUNCH",1.18f)/2,H-36,1.18f,"BACKSPACE DELETE   ENTER LAUNCH",0.55f,0.70f,0.72f);
    end2d();
}

/* ============================ main ======================================== */

static int streq(const char* a,const char* b){ while(*a&&*b&&*a==*b){a++;b++;} return *a==0&&*b==0; }
static u32 parse_u32(const char* s){ u32 v=0; while(*s>='0'&&*s<='9'){ v=v*10+(u32)(*s-'0'); s++; } return v; }
static u32 parse_seed(int argc,char** argv){
    for(int i=1;i+1<argc;i++) if(streq(argv[i],"--seed")) return parse_u32(argv[i+1]);
    return (u32)p_SDL_GetTicks() ^ 0x5EED1234u;
}

int main(int argc, char** argv){
    int miss=0;
    gl_lib  = dlopen("libGL.so.1", RTLD_NOW|RTLD_GLOBAL);
    sdl_lib = dlopen("libSDL2-2.0.so.0", RTLD_NOW|RTLD_GLOBAL);
    if(!gl_lib || !sdl_lib) return 1;

    LSDL(SDL_Init);LSDL(SDL_Quit);LSDL(SDL_GL_SetAttribute);LSDL(SDL_CreateWindow);
    LSDL(SDL_DestroyWindow);LSDL(SDL_GL_CreateContext);LSDL(SDL_GL_DeleteContext);
    LSDL(SDL_GL_SetSwapInterval);LSDL(SDL_GL_SwapWindow);LSDL(SDL_PollEvent);
    LSDL(SDL_SetWindowFullscreen);LSDL(SDL_GetRelativeMouseState);LSDL(SDL_SetRelativeMouseMode);LSDL(SDL_GetKeyboardState);
    LSDL(SDL_GetTicks);LSDL(SDL_Delay);LSDL(SDL_ShowCursor);LSDL(SDL_OpenAudioDevice);LSDL(SDL_PauseAudioDevice);

    LGL(glClearColor);LGL(glClear);LGL(glEnable);LGL(glDisable);LGL(glViewport);
    LGL(glMatrixMode);LGL(glLoadIdentity);LGL(glLoadMatrixf);LGL(glPushMatrix);LGL(glPopMatrix);
    LGL(glTranslatef);LGL(glRotatef);LGL(glScalef);LGL(glFrustum);LGL(glOrtho);
    LGL(glBegin);LGL(glEnd);LGL(glVertex3f);LGL(glVertex2f);LGL(glColor3f);LGL(glColor4f);
    LGL(glNormal3f);LGL(glTexCoord2f);LGL(glGenTextures);LGL(glBindTexture);
    LGL(glTexParameteri);LGL(glTexImage2D);LGL(glPointSize);LGL(glLineWidth);LGL(glDepthFunc);LGL(glDepthMask);
    LGL(glBlendFunc);LGL(glShadeModel);LGL(glHint);LGL(glLightfv);LGL(glLightModelfv);LGL(glColorMaterial);
    if(miss) return 1;

    if(p_SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)!=0) return 1;
    p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
    p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    p_SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    p_SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    p_SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,24);
    p_SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);
    p_SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);
    p_SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);

    void* win=p_SDL_CreateWindow("VOIDRUNNER", SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                                 1280,720, SDL_WINDOW_OPENGL);
    if(!win) return 1;
    void* ctx=p_SDL_GL_CreateContext(win);
    if(!ctx) return 1;
    p_SDL_ShowCursor(0);
    p_SDL_GL_SetSwapInterval(1);

    u32 boot_seed=parse_seed(argc, argv);
    title_default_seed=boot_seed;
    for(int i=1;i+1<argc;i++) if(streq(argv[i],"--name")) strset_lim(title_name,argv[i+1],12);
    strset_lim(cmdr_name,title_name,12);
    new_game(boot_seed);
    G.state=ST_TITLE; G.docked=0; G.ne=0; G.alert_t=0; G.target=-1;

    /* audio: best-effort */
    AudioSpec want; m_set(&want,0,sizeof(want));
    want.freq=44100; want.format=AUDIO_S16SYS; want.channels=2; want.samples=1024;
    want.callback=audio_cb;
    AudioSpec got; m_set(&got,0,sizeof(got));
    G.adev=p_SDL_OpenAudioDevice(0,0,&want,&got,0);
    if(G.adev) p_SDL_PauseAudioDevice(G.adev,0);

    p_SDL_SetRelativeMouseMode(G.state==ST_FLIGHT);
    p_glEnable(GL_DEPTH_TEST); p_glDepthFunc(GL_LEQUAL);
    p_glEnable(GL_LINE_SMOOTH); p_glHint(GL_LINE_SMOOTH_HINT,GL_NICEST);
    p_glViewport(0,0,1280,720);

    u32 last=p_SDL_GetTicks();
    int running=1, fullscreen=0;
    u8 ev[64];
    while(running){
        u32 now=p_SDL_GetTicks(); f32 dt=(now-last)/1000.0f; last=now;
        if(dt>0.05f) dt=0.05f;
        if(dt<=0) dt=0.001f;
        if(G.alert_t>0) G.alert_t-=dt;
        while(p_SDL_PollEvent(ev)){
            u32 et=*(u32*)ev;
            if(et==SDL_QUIT) running=0;
            if(et==SDL_WINDOWEVENT && ev[12]==SDL_WINDOWEVENT_SIZE_CHANGED){
                G.win_w=*(int*)(ev+16); G.win_h=*(int*)(ev+20);
                p_glViewport(0,0,G.win_w,G.win_h);
            }
        }
        const u8* ks=p_SDL_GetKeyboardState(0);
        if(edge(ks,SC_F11)){ fullscreen=!fullscreen; p_SDL_SetWindowFullscreen(win,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0); }
        if(edge(ks,SC_ESC)) running=0;

        switch(G.state){
            case ST_TITLE:  update_title(ks); break;
            case ST_FLIGHT: update(dt,ks); break;
            case ST_MARKET: update_market(ks); break;
            case ST_STATUS: update_status(ks); break;
            case ST_DATA: update_data(ks); break;
            case ST_MANIFEST: update_manifest(ks); break;
            case ST_EQUIP: update_equip(ks); break;
            case ST_MAP:    update_map(ks); break;
            case ST_JUMP:
                G.t += dt;
                if(G.alert_t<=0){ enter_system(G.target,1); if(G.alert_code==2||G.alert_code==3) sfx_play(SFX_WARN); G.state=ST_FLIGHT; p_SDL_SetRelativeMouseMode(1); }
                break;
            case ST_DEAD:
                if(edge(ks,SC_RETURN)){
                    G.hull=G.maxhull; G.shield=G.maxshield; G.wanted=0; G.legal=0;
                    G.credits = G.credits/2; for(int i=0;i<NCOM;i++) G.cargo[i]=0;
                    enter_system(G.cur,0); G.state=ST_STATUS; p_SDL_SetRelativeMouseMode(0);
                }
                break;
        }
        m_cpy(prevkeys,ks,300);

        /* ---- render ---- */
        p_glClearColor(0.015f,0.02f,0.035f,1);
        p_glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        if(G.state==ST_TITLE) draw_title_screen();
        if(G.state==ST_FLIGHT || G.state==ST_DEAD){
            set_view();
            draw_starfield();
            draw_world();
            begin2d(); draw_hud(); end2d();
        }
        if(G.state==ST_MARKET) draw_market();
        if(G.state==ST_STATUS) draw_status_page();
        if(G.state==ST_DATA) draw_data_page();
        if(G.state==ST_MANIFEST) draw_manifest();
        if(G.state==ST_EQUIP) draw_equipment();
        if(G.state==ST_MAP)    draw_map();
        if(G.state==ST_JUMP)   draw_jump_screen();
        if(G.state==ST_DEAD)   draw_dead();
        p_SDL_GL_SwapWindow(win);
    }

    if(G.adev) p_SDL_PauseAudioDevice(G.adev,1);
    p_SDL_GL_DeleteContext(ctx);
    p_SDL_DestroyWindow(win);
    p_SDL_Quit();
    return 0;
}
