/* TETHER/9 — single-source procedural orbital salvage arcade game.
 * SPDX-License-Identifier: MIT. Built for C Optimizer as a standalone example.
 * Build: COPT_EXTRA_LDLIBS='-ldl -lm' sh build_asm_syscall.sh examples/TETHER9.c
 * Linux x86-64 / SDL2 / compatibility OpenGL; no asset files.
 */
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define PI 3.14159265358979323846f
#define TAU 6.28318530717958647692f
#define WX 960.0f
#define WY 600.0f
#define NP 14
#define ND 14
#define FX 192

/* Dynamic entry points keep this code buildable with C Optimizer's tiny SDL headers. */
#define SF(X) \
 X(SDL_Init) X(SDL_Quit) X(SDL_CreateWindow) X(SDL_DestroyWindow) \
 X(SDL_GL_CreateContext) X(SDL_GL_DeleteContext) X(SDL_GL_SwapWindow) \
 X(SDL_GL_GetProcAddress) X(SDL_GL_SetSwapInterval) X(SDL_PollEvent) \
 X(SDL_GetTicks) X(SDL_OpenAudioDevice) X(SDL_PauseAudioDevice) \
 X(SDL_CloseAudioDevice) X(SDL_SetWindowFullscreen)
#define D(X) static typeof(X)* p_##X;
SF(D)
#undef D
#define SDL_Init p_SDL_Init
#define SDL_Quit p_SDL_Quit
#define SDL_CreateWindow p_SDL_CreateWindow
#define SDL_DestroyWindow p_SDL_DestroyWindow
#define SDL_GL_CreateContext p_SDL_GL_CreateContext
#define SDL_GL_DeleteContext p_SDL_GL_DeleteContext
#define SDL_GL_SwapWindow p_SDL_GL_SwapWindow
#define SDL_GL_GetProcAddress p_SDL_GL_GetProcAddress
#define SDL_GL_SetSwapInterval p_SDL_GL_SetSwapInterval
#define SDL_PollEvent p_SDL_PollEvent
#define SDL_GetTicks p_SDL_GetTicks
#define SDL_OpenAudioDevice p_SDL_OpenAudioDevice
#define SDL_PauseAudioDevice p_SDL_PauseAudioDevice
#define SDL_CloseAudioDevice p_SDL_CloseAudioDevice
#define SDL_SetWindowFullscreen p_SDL_SetWindowFullscreen
static const Uint8 *(*p_SDL_GetKeyboardState)(int*);
static Uint32 (*p_SDL_GetQueuedAudioSize)(SDL_AudioDeviceID);
static int (*p_SDL_QueueAudio)(SDL_AudioDeviceID,const void*,Uint32);
static void (*p_SDL_Delay)(Uint32);
void glLineWidth(GLfloat width);
#define GF(X) X(glBegin) X(glEnd) X(glVertex2f) X(glColor4f) X(glClearColor) X(glClear) \
 X(glBlendFunc) X(glEnable) X(glMatrixMode) X(glLoadIdentity) X(glOrtho) \
 X(glViewport) X(glLineWidth) X(glPointSize) X(glReadPixels)
#define D(X) static typeof(X)* p_##X;
GF(D)
#undef D
#define glBegin p_glBegin
#define glEnd p_glEnd
#define glVertex2f p_glVertex2f
#define glColor4f p_glColor4f
#define glClearColor p_glClearColor
#define glClear p_glClear
#define glBlendFunc p_glBlendFunc
#define glEnable p_glEnable
#define glMatrixMode p_glMatrixMode
#define glLoadIdentity p_glLoadIdentity
#define glOrtho p_glOrtho
#define glViewport p_glViewport
#define glLineWidth p_glLineWidth
#define glPointSize p_glPointSize
#define glReadPixels p_glReadPixels

static int load_api(void){
 void*s=dlopen("libSDL2-2.0.so.0",RTLD_NOW); if(!s)return 0;
 int miss=0;
 #define D(X) do {p_##X=(typeof(p_##X))dlsym(s,#X);if(!p_##X)miss++;}while(0);
 SF(D)
 #undef D
 p_SDL_GetKeyboardState=dlsym(s,"SDL_GetKeyboardState");
 p_SDL_QueueAudio=dlsym(s,"SDL_QueueAudio");
 p_SDL_GetQueuedAudioSize=dlsym(s,"SDL_GetQueuedAudioSize");
 p_SDL_Delay=dlsym(s,"SDL_Delay");
 if(!p_SDL_GetKeyboardState||!p_SDL_QueueAudio||!p_SDL_GetQueuedAudioSize||!p_SDL_Delay)return 0;
 return miss==0;
}
static int load_gl(void){int miss=0;
 #define D(X) do{p_##X=(typeof(p_##X))SDL_GL_GetProcAddress(#X);if(!p_##X)miss++;}while(0);
 GF(D)
 #undef D
 return !miss;
}

static unsigned rseed=0x119e99u;
static unsigned rng(void){unsigned x=rseed; x^=x<<13;x^=x>>17;x^=x<<5;return rseed=x;}
static float rnd(void){return (rng()&65535)*0.000015258789f;}
static float clamp(float v,float a,float b){return v<a?a:v>b?b:v;}
static float length(float x,float y){return sqrtf(x*x+y*y);}
static float dist(float x,float y,float a,float b){return length(x-a,y-b);}
static float wrapang(float a){return a>PI?a-TAU:a<-PI?a+TAU:a;}

typedef struct {float x,y,vx,vy;int state;} Pod;
typedef struct {float x,y,vx,vy,t;int hp;} Drone;
typedef struct {float x,y,vx,vy,life,size;int kind;} Spark;
static Pod pods[NP];static Drone drones[ND];static Spark sparks[FX];
static struct {
 float x,y,vx,vy,angle,energy,pulse,pulse_timer,hp,t,shake,combo,frame;
 int state,wave,delivered,quota,score,tether,full,ss,shot,test_ticks;
 unsigned seed;
} G;
static int particle_at;
static int nd,np;
static int vsw=960,vsh=600;
static SDL_AudioDeviceID audio_dev;
static unsigned long audio_tick;
static int audio_enabled;

static void particle(float x,float y,int kind,int count){
 for(int i=0;i<count;i++){
  Spark *s=&sparks[particle_at++%FX]; float a=rnd()*TAU,sp=25+rnd()*180;
  s->x=x;s->y=y;s->vx=cosf(a)*sp;s->vy=sinf(a)*sp;
  s->life=.2f+rnd()*.8f;s->size=1+rnd()*3;s->kind=kind;
 }
}
static void spawn_wave(void){
 G.delivered=0; np=G.quota; nd=2+G.wave; if(nd>ND)nd=ND;
 for(int i=0;i<np;i++){
  float a=TAU*(i+rnd()*.3f)/np,r=174+rnd()*205;
  pods[i]=(Pod){WX*.5f+cosf(a)*r,WY*.5f+sinf(a)*r,0,0,0};
 }
 for(int i=0;i<nd;i++){
  float a=(i+rnd()*.4f)*TAU/nd,r=310+rnd()*170;
  drones[i]=(Drone){WX*.5f+cosf(a)*r,WY*.5f+sinf(a)*r,0,0,rnd()*8,2};
 }
 G.tether=-1;
}
static void new_game(unsigned seed){
 memset(&G,0,sizeof G);memset(sparks,0,sizeof sparks);
 G.seed=seed?seed:0x119e99u; rseed=G.seed;
 G.x=WX*.5f+65;G.y=WY*.5f;G.angle=-PI*.5f;
 G.hp=100;G.energy=100;G.quota=4;G.wave=1;G.tether=-1;
 G.state=1;spawn_wave();
}
/* Returns 1 if a tether was acquired or detached; autonomous nearest-pod selection. */
static int toggle_tether(void){
 if(G.tether>=0){G.tether=-1;return 1;}
 float best=100;int chosen=-1;
 for(int i=0;i<np;i++)if(!pods[i].state){float d=dist(G.x,G.y,pods[i].x,pods[i].y);if(d<best){best=d;chosen=i;}}
 if(chosen<0)return 0;
 G.tether=chosen;G.pulse_timer=.16f;return 1;
}
static int pulse(void){
 if(G.pulse>0||G.energy<32||G.state!=1)return 0;
 G.energy-=32;G.pulse=5.5f;G.pulse_timer=.55f;G.shake=.18f;
 for(int i=0;i<nd;i++)if(drones[i].hp){
  Drone*d=&drones[i];float dx=d->x-G.x,dy=d->y-G.y,r=length(dx,dy);
  if(r<175){float k=330/(r+10);d->vx+=dx*k;d->vy+=dy*k;d->hp--;if(!d->hp)particle(d->x,d->y,2,16);}
 }
 particle(G.x,G.y,1,22);return 1;
}
static void step(float dt,int left,int right,int thrust,int brake,int boost){
 if(G.state!=1)return;
 G.t+=dt;G.frame+=1;
 G.angle=wrapang(G.angle+(right-left)*3.25f*dt);
 float acc=thrust?330:0;
 if(boost&&G.energy>1&&thrust){acc*=2;G.energy-=36*dt;G.shake=.045f;}
 else G.energy=clamp(G.energy+9*dt,0,100);
 G.vx+=cosf(G.angle)*acc*dt;G.vy+=sinf(G.angle)*acc*dt;
 if(brake){G.vx*=1-2.5f*dt;G.vy*=1-2.5f*dt;}
 G.vx*=1-.22f*dt;G.vy*=1-.22f*dt;
 float sp=length(G.vx,G.vy);if(sp>390){G.vx*=390/sp;G.vy*=390/sp;}
 G.x+=G.vx*dt;G.y+=G.vy*dt;
 if(G.x<15||G.x>WX-15){G.x=clamp(G.x,15,WX-15);G.vx*=-.4f;}
 if(G.y<23||G.y>WY-20){G.y=clamp(G.y,23,WY-20);G.vy*=-.4f;}
 if(G.pulse>0)G.pulse-=dt;
 if(G.pulse_timer>0)G.pulse_timer-=dt;
 if(G.shake>0)G.shake-=dt;
 if(G.combo>0)G.combo-=dt;
 /* Spring cable: movement and dragging are physically coupled. */
 if(G.tether>=0){
  Pod*p=&pods[G.tether];float dx=G.x-p->x,dy=G.y-p->y,r=length(dx,dy);
  if(r>215){G.tether=-1;G.pulse_timer=.12f;}
  else if(r>27){
   float tension=(r-27)*4.2f, inv=1/(r+.001f);
   p->vx+=(dx*inv*tension-p->vx*.65f)*dt;
   p->vy+=(dy*inv*tension-p->vy*.65f)*dt;
   G.vx-=dx*inv*tension*.20f*dt;G.vy-=dy*inv*tension*.20f*dt;
  }
 }
 for(int i=0;i<np;i++)if(!pods[i].state){
  Pod*p=&pods[i];p->x+=p->vx*dt;p->y+=p->vy*dt;
  p->vx*=1-.6f*dt;p->vy*=1-.6f*dt;
  p->x=clamp(p->x,12,WX-12);p->y=clamp(p->y,19,WY-19);
  /* Only a tethered pod can enter the orbital station's capture field. */
  if(G.tether==i && dist(p->x,p->y,WX*.5f,WY*.5f)<47){
   p->state=1;G.tether=-1;G.delivered++;G.score+=100+(G.combo>0?75:0)+G.wave*25;
   G.combo=8;G.energy=clamp(G.energy+18,0,100);particle(p->x,p->y,0,28);
  }
 }
 for(int i=0;i<nd;i++)if(drones[i].hp){
  Drone*d=&drones[i];d->t+=dt;
  float dx=G.x-d->x,dy=G.y-d->y,r=length(dx,dy)+.001f;
  float ax=dx/r*88,ay=dy/r*88;
  /* Hunter drones orbit rather than just beelining, then close in. */
  float strafe=sinf(d->t*1.4f+i)*48;
  d->vx+=(ax-dy/r*strafe-d->vx*.50f)*dt;
  d->vy+=(ay+dx/r*strafe-d->vy*.50f)*dt;
  d->x+=d->vx*dt;d->y+=d->vy*dt;
  if(r<25){
   float knock=G.hp>0?19:0;G.hp-=knock*dt;
   G.vx+=dx/r*47*dt;G.vy+=dy/r*47*dt;G.shake=.13f;
  }
 }
 for(int i=0;i<FX;i++)if(sparks[i].life>0){
  Spark*s=&sparks[i];s->x+=s->vx*dt;s->y+=s->vy*dt;s->vx*=1-dt;s->vy*=1-dt;s->life-=dt;
 }
 if(G.delivered>=G.quota){
  G.score+=G.wave*200;G.wave++;G.quota=G.wave+3;if(G.quota>NP)G.quota=NP;
  spawn_wave();G.hp=clamp(G.hp+22,0,100);G.energy=100;
  particle(WX*.5f,WY*.5f,1,60);
 }
 if(G.hp<=0){G.hp=0;G.state=2;G.tether=-1;particle(G.x,G.y,2,65);}
}

/* Bitmap font, columns use bit 0 as the top row; numerals must match. */
static const unsigned char font[][5]={{0x7e,0x11,0x11,0x11,0x7e},
 {0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22},
 {0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41},
 {0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a},
 {0x7f,0x08,0x08,0x08,0x7f},
 {0x00,0x41,0x7f,0x41,0x00},
 {0x20,0x40,0x41,0x3f,0x01},
 {0x7f,0x08,0x14,0x22,0x41},
 {0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f},
 {0x7f,0x04,0x08,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e},
 {0x7f,0x09,0x09,0x09,0x06},
 {0x3e,0x41,0x51,0x21,0x5e},
 {0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31},
 {0x01,0x01,0x7f,0x01,0x01},
 {0x3f,0x40,0x40,0x40,0x3f},
 {0x1f,0x20,0x40,0x20,0x1f},
 {0x7f,0x20,0x18,0x20,0x7f},
 {0x63,0x14,0x08,0x14,0x63},
 {0x03,0x04,0x78,0x04,0x03},
 {0x61,0x51,0x49,0x45,0x43},
 {0x3e,0x51,0x49,0x45,0x3e},
 {0x0,0x42,0x7f,0x40,0x0},
 {0x42,0x61,0x51,0x49,0x46},
 {0x21,0x41,0x45,0x4b,0x31},
 {0x18,0x14,0x12,0x7f,0x10},
 {0x27,0x45,0x45,0x45,0x39},
 {0x3e,0x49,0x49,0x49,0x32},
 {0x1,0x71,0x9,0x5,0x3},
 {0x36,0x49,0x49,0x49,0x36},
 {0x26,0x49,0x49,0x49,0x3e},
 {0,0,0,0,0},
 {0,0x36,0x36,0,0},
 {0x08,0x08,0x08,0x08,0x08},
 {0x20,0x10,0x08,0x04,0x02},
 {0x08,0x08,0x3e,0x08,0x08},
 {0,0x60,0x60,0,0},
 {0x02,0x01,0x51,0x09,0x06}
};
static int glyph(int c){
 if(c>='A'&&c<='Z')return c-'A';
 if(c>='a'&&c<='z')return c-'a';
 if(c>='0'&&c<='9')return 26+c-'0';
 const char*sp=" :-/+.?";for(int i=0;sp[i];i++)if(c==sp[i])return 36+i;
 return 36;
}
static void col(float r,float g,float b,float a){glColor4f(r,g,b,a);}
static void v(float x,float y){glVertex2f(x,y);}
static void line(float x,float y,float a,float b){glBegin(1);v(x,y);v(a,b);glEnd();}
static void box(float x,float y,float w,float h){glBegin(GL_QUADS);v(x,y);v(x+w,y);v(x+w,y+h);v(x,y+h);glEnd();}
static void ring(float x,float y,float r,int seg){
 glBegin(2);for(int i=0;i<seg;i++){float a=i*TAU/seg;v(x+cosf(a)*r,y+sinf(a)*r);}glEnd();
}
static void text(float x,float y,float size,const char*str){
 glBegin(GL_QUADS);
 for(;*str;str++,x+=size*6){const unsigned char *b=font[glyph(*str)];
  for(int i=0;i<5;i++)for(int j=0;j<7;j++)if((b[i]>>j)&1){float xx=x+i*size, yy=y+j*size;
   v(xx,yy);v(xx+size,yy);v(xx+size,yy+size);v(xx,yy+size);
  }
 }
 glEnd();
}
static void number(float x,float y,int n,float z){char buf[24];snprintf(buf,sizeof buf,"%d",n);text(x,y,z,buf);}
static void hud_bar(float x,float y,float val,int k){
 col(.11f,.18f,.24f,1);box(x,y,112,8);
 if(k)col(.96f,.49f,.20f,1);else col(.25f,.86f,.94f,1);
 box(x,y,112*clamp(val/100,0,1),8);
}
static void starfield(void){
 for(int i=0;i<160;i++){
  unsigned h=(unsigned)i*2654435761u+G.seed;h^=h>>16;h*=2246822519u;h^=h>>13;
  float x=(h&1023)*WX/1024,y=((h>>10)&1023)*WY/1024;
  float t=.3f+.15f*sinf(G.t*.4f+i*.8f);
  col(.3f,.64f,.75f,t);box(x,y,(i%7==0?2:1),(i%7==0?2:1));
 }
}
static void render(void){
 int vpw=vsw,vph=vsh,vpx=0,vpy=0;
 if(vsw*600>vsh*960){vpw=vsh*960/600;vpx=(vsw-vpw)/2;}
 else {vph=vsw*600/960;vpy=(vsh-vph)/2;}
 glViewport(vpx,vpy,vpw,vph);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,WX,WY,0,-1,1);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glClearColor(.018f,.027f,.065f,1);glClear(GL_COLOR_BUFFER_BIT);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 starfield();
 /* Grid + orbital cues. */
 col(.11f,.23f,.38f,.27f);for(int i=0;i<=16;i++)line(i*60,0,i*60,WY);
 for(int i=0;i<=10;i++)line(0,i*60,WX,i*60);
 col(.1f,.55f,.67f,.18f);ring(WX*.5f,WY*.5f,180,100);ring(WX*.5f,WY*.5f,300,120);
 /* The station: capture ring, rotating arms, pulsing centre. */
 float cx=WX*.5f,cy=WY*.5f;
 col(.12f,.7f,.88f,.12f);ring(cx,cy,59,60);ring(cx,cy,58,60);
 col(.2f,.80f,.95f,.62f);ring(cx,cy,45,56);ring(cx,cy,33,48);
 for(int j=0;j<4;j++){
  float a=G.t*.37f+j*PI*.5f;
  col(.18f,.9f,1,.7f);line(cx+cosf(a)*36,cy+sinf(a)*36,cx+cosf(a)*49,cy+sinf(a)*49);
 }
 col(.13f,.7f,.85f,.9f);ring(cx,cy,18+2*sinf(G.t*2),36);
 col(.3f,.9f,.95f,1);text(cx-27,cy-4,1.5f,"DOCK");
 for(int i=0;i<np;i++)if(!pods[i].state){
  Pod *p=&pods[i];float flick=.7f+.3f*sinf(G.t*4+i*2);
  col(.9f,.62f,.17f,.15f*flick);ring(p->x,p->y,15,20);
  col(1,.7f,.26f,1);box(p->x-7,p->y-7,14,14);
  col(.11f,.17f,.27f,1);box(p->x-4,p->y-4,8,8);
  col(1,.8f,.52f,1);line(p->x-10,p->y,p->x-15,p->y);line(p->x+10,p->y,p->x+15,p->y);
 }
 for(int i=0;i<nd;i++)if(drones[i].hp){
  Drone*d=&drones[i];float a=atan2f(G.y-d->y,G.x-d->x);
  col(.99f,.32f,.39f,.15f);ring(d->x,d->y,17,22);
  col(1,.38f,.37f,1);glBegin(3);
  for(int k=0;k<4;k++){float z=a+k*TAU/3;v(d->x+cosf(z)*10,d->y+sinf(z)*10);}glEnd();
  col(1,.8f,.44f,1);line(d->x-3,d->y,d->x+3,d->y);
 }
 if(G.tether>=0){
  Pod*p=&pods[G.tether];float r=dist(G.x,G.y,p->x,p->y),tension=clamp(r/215,0,1);
  col(1,.8f-.3f*tension,.29f,.9f);glLineWidth(2);line(G.x,G.y,p->x,p->y);glLineWidth(1);
  col(1,.75f,.3f,.35f);ring(p->x,p->y,19,22);
 }
 /* Ship + exhaust + wake. */
 if(G.state!=2){
  float a=G.angle,c=cosf(a),s=sinf(a);
  if(length(G.vx,G.vy)>10){
   col(.2f,.75f,.95f,.12f);line(G.x-G.vx*.28f,G.y-G.vy*.28f,G.x,G.y);
  }
  col(.10f,.7f,1,.16f);ring(G.x,G.y,17,26);
  col(.42f,.92f,1,1);glBegin(3);
  v(G.x+c*15,G.y+s*15);
  v(G.x-c*10-s*8,G.y-s*10+c*8);
  v(G.x-c*7,G.y-s*7);
  v(G.x-c*10+s*8,G.y-s*10-c*8);
  v(G.x+c*15,G.y+s*15);glEnd();
  col(.99f,.99f,1,1);box(G.x-2,G.y-2,4,4);
 }
 if(G.pulse_timer>0){
  float r=(.55f-G.pulse_timer)*320;col(.27f,.94f,1,clamp(G.pulse_timer*1.2f,0,.8f));glLineWidth(3);ring(G.x,G.y,r,70);glLineWidth(1);
 }
 for(int i=0;i<FX;i++)if(sparks[i].life>0){
  Spark *s=&sparks[i];float al=clamp(s->life,0,1);
  if(s->kind==2)col(1,.43f,.30f,al);else if(s->kind==1)col(.4f,.85f,1,al);else col(1,.77f,.39f,al);
  box(s->x,s->y,s->size,s->size);
 }
 /* HUD, not colour-only: all resources have labels. */
 col(.015f,.035f,.07f,.9f);box(0,0,WX,67);box(0,WY-39,WX,39);
 col(.33f,.94f,1,1);text(22,14,3,"TETHER/9");
 col(.68f,.8f,.9f,1);text(240,10,1.9f,"WAVE");number(240,30,G.wave,2.5f);
 text(325,10,1.9f,"SALVAGE");number(325,30,G.delivered,2.5f);
 col(.96f,.76f,.27f,1);text(356,30,1.6f,"/");number(374,30,G.quota,2.5f);
 col(.73f,.85f,1,1);text(440,10,1.9f,"SCORE");number(440,30,G.score,2.5f);
 col(.78f,.9f,1,1);text(603,10,1.7f,"HULL");hud_bar(603,33,G.hp,1);
 text(738,10,1.7f,"CHARGE");hud_bar(738,33,G.energy,0);
 col(.59f,.72f,.83f,1);text(22,WY-27,1.7f,"W THRUST  A/D TURN  S BRAKE  SPACE TETHER  E PULSE  SHIFT BOOST");
 if(G.state==0||G.state==2){
  col(.01f,.018f,.042f,.91f);box(135,176,690,254);
  col(.2f,.76f,.94f,.85f);ring(480,300,214,96);
  col(1,.77f,.39f,1);
  text(216,217,5,G.state==0?"TETHER/9":"SIGNAL LOST");
  col(.71f,.89f,1,1);
  if(G.state==0){
   text(258,286,2.25f,"RECOVER CARGO. EVADE HUNTERS.");
   text(236,316,2,"DRAG ORANGE PODS INTO THE DOCK.");
   text(318,368,2.3f,"PRESS ENTER TO LAUNCH");
  }else{
   text(330,296,2.7f,"FINAL SCORE");number(452,335,G.score,3.2f);
   text(306,385,2.4f,"PRESS R TO RETRY");
  }
 }
}

/* Queued audio is synthesised on the main thread: no shared callback data races. */
static void sound_update(void){
 if(!audio_enabled || p_SDL_GetQueuedAudioSize(audio_dev)>8192)return;
 int16_t buf[1024];
 for(int i=0;i<1024;i++,audio_tick++){
  float t=(float)audio_tick/44100.f;
  float beat=fmodf(t*2.2f,1.f);
  float kick=expf(-beat*28)*sinf(TAU*(61-35*beat)*t)*.13f;
  float melody=sinf(TAU*(82.4f+((int)(t*.27f)%4)*13.7f)*t)*.035f;
  float tone=sinf(TAU*41.2f*t)*.04f+sinf(TAU*40.6f*t)*.028f;
  float alert=G.pulse_timer>0?sinf(TAU*660*t)*G.pulse_timer*.12f:0;
  float thrust=(G.state==1&&length(G.vx,G.vy)>70)?sinf(TAU*113*t)*.018f:0;
  float out=clamp((kick+melody+tone+alert+thrust)*.57f,-.8f,.8f);
  buf[i]=(int16_t)(out*32767);
 }
 p_SDL_QueueAudio(audio_dev,buf,sizeof buf);
}
#ifdef TEST_BUILD
static int failures;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
static int tests(void){
 new_game(12345);CHECK(G.wave==1&&G.quota==4&&G.hp==100&&np==4);
 Pod save[NP];memcpy(save,pods,sizeof save);new_game(12345);
 CHECK(memcmp(save,pods,sizeof save)==0);
 /* Tether acquisition, retention and capture. */
 pods[0].x=G.x+35;pods[0].y=G.y;CHECK(toggle_tether()==1&&G.tether==0);
 CHECK(toggle_tether()==1&&G.tether==-1);
 CHECK(toggle_tether()==1&&G.tether==0);
 G.x=WX*.5f+30;G.y=WY*.5f;pods[0].x=WX*.5f+25;pods[0].y=WY*.5f;
 step(.016f,0,0,0,0,0);CHECK(G.delivered==1&&G.score>0&&G.tether==-1);
 G.energy=100;CHECK(pulse()==1);CHECK(G.energy==68&&pulse()==0);
 G.pulse=0;G.energy=0;CHECK(pulse()==0);
 G.delivered=G.quota;step(.016f,0,0,0,0,0);CHECK(G.wave==2&&G.delivered==0&&G.quota==5);
 /* Physical integration: accelerate west while towing, then brake near base. */
 new_game(90);G.x=WX*.5f+100;G.y=WY*.5f;G.angle=PI;
 pods[0].x=G.x+29;pods[0].y=G.y;
 CHECK(toggle_tether()==1 && G.tether==0);
 for(int j=0;j<260 && !G.delivered;j++)step(.016f,0,0,j<65,j>=65,0);
 CHECK(G.delivered>=1);

 new_game(14);G.hp=1;drones[0].x=G.x;drones[0].y=G.y;
 step(.1f,0,0,0,0,0);CHECK(G.state==2);
 new_game(16);G.x=20;G.y=30;G.vx=-300;G.vy=-200;
 step(.1f,0,0,0,0,0);CHECK(G.x>=15&&G.y>=23);
 if(!failures)puts("PASS: deterministic map, tether, docking, EMP, death, bounds (14 checks)");
 return failures?1:0;
}
#endif

/* Automated attract movement is optional for smoke tests and screenshots. */
int main(int argc,char**argv){
#ifdef TEST_BUILD
 if(argc>1 && !strcmp(argv[1],"--selftest"))return tests();
#endif
 int demo=0, frames=0;unsigned seed=0x119e99u;
#ifdef CAPTURE_BUILD
 const char*capture=0;
#endif
 for(int i=1;i<argc;i++){
  if(!strcmp(argv[i],"--demo"))demo=1;
  if(!strcmp(argv[i],"--frames")&&i+1<argc)frames=atoi(argv[++i]);
  if(!strcmp(argv[i],"--seed")&&i+1<argc)seed=(unsigned)strtoul(argv[++i],0,0);
#ifdef CAPTURE_BUILD
  if(!strcmp(argv[i],"--capture")&&i+1<argc)capture=argv[++i];
#endif
 }
 if(!load_api()){fprintf(stderr,"Missing SDL2 runtime symbols\n");return 2;}
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)){fprintf(stderr,"SDL init failed\n");return 2;}
 SDL_Window *win=SDL_CreateWindow("TETHER/9 - orbital salvage",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,600,SDL_WINDOW_OPENGL);
 if(!win){SDL_Quit();return 3;}
 SDL_GLContext ctx=SDL_GL_CreateContext(win);if(!ctx||!load_gl()){fprintf(stderr,"OpenGL init failed\n");return 4;}
 SDL_GL_SetSwapInterval(1);
 new_game(seed);if(!demo)G.state=0;
 SDL_AudioSpec want={0};want.freq=44100;want.channels=1;want.format=0x8010;want.samples=1024;
 audio_dev=SDL_OpenAudioDevice(0,0,&want,0,0);
 if(audio_dev){audio_enabled=1;SDL_PauseAudioDevice(audio_dev,0);}
 unsigned last=SDL_GetTicks();int running=1,kcount=0,prevspace=0,preve=0;
 while(running){
  SDL_Event ev;while(SDL_PollEvent(&ev)){
   if(ev.type==SDL_QUIT)running=0;
   if(ev.type==SDL_WINDOWEVENT&&ev.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){vsw=ev.window.data1;vsh=ev.window.data2;}
   if(ev.type==SDL_KEYDOWN&&!ev.key.repeat){
    int k=ev.key.keysym.scancode;
    if(k==41)running=0;
    if(k==68){G.full^=1;SDL_SetWindowFullscreen(win,G.full?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
    if((k==40||k==44)&&G.state==0){new_game(seed);}
    if(k==21&&G.state==2){new_game(seed);}
   }
  }
  const Uint8*ks=p_SDL_GetKeyboardState(0);
  int left=ks[4]||ks[80],right=ks[7]||ks[79],up=ks[26]||ks[82];
  int brake=ks[22]||ks[81],boost=ks[225]||ks[229];
  int space=ks[44],e=ks[8];
  if(demo){up=1;left=((kcount/100)%3==0);right=((kcount/100)%3==1);}
  if(G.state==1){
   if((space&&!prevspace)||(demo&&kcount%110==0))toggle_tether();
   if((e&&!preve)||(demo&&kcount%270==0))pulse();
  }
  prevspace=space;preve=e;
  unsigned now=SDL_GetTicks();float dt=(now-last)*.001f;last=now;
  dt=clamp(dt,.001f,.035f);
  if(demo)dt=1.0f/60.0f;
  step(dt,left,right,up,brake,boost);
  if(G.state==0)G.t+=dt;
  render();sound_update();
#ifdef CAPTURE_BUILD
  if(capture && frames>0 && kcount+1>=frames){
   unsigned char *rgb=malloc((size_t)vsw*vsh*3);
   if(rgb){glReadPixels(0,0,vsw,vsh,GL_RGB,GL_UNSIGNED_BYTE,rgb);
    FILE*f=fopen(capture,"wb");if(f){fprintf(f,"P6\n%d %d\n255\n",vsw,vsh);
      for(int y=vsh-1;y>=0;y--)fwrite(rgb+(size_t)y*vsw*3,1,(size_t)vsw*3,f);
      fclose(f);}
    free(rgb);
   }
  }
#endif
  SDL_GL_SwapWindow(win);
  kcount++;if(frames>0&&kcount>=frames)running=0;
  if(demo)p_SDL_Delay(8);
 }
 if(audio_enabled)SDL_CloseAudioDevice(audio_dev);
 SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(win);SDL_Quit();return 0;
}
