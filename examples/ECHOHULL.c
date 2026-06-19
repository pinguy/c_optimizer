/* ECHOHULL - native Linux 8K sonar salvage horror.
 * Copyright 2026 Antoni Norman.
 * Licensed under the Apache License, Version 2.0; see examples/LICENSE. */
#include <stdint.h>
#include <stddef.h>
#include <dlfcn.h>

typedef uint8_t u8; typedef uint32_t u32; typedef uint64_t u64; typedef int32_t i32; typedef float f32;
static void* m_set(void*d,int c,u64 n){u8*p=d;while(n--)*p++=(u8)c;return d;}
static void* m_cpy(void*d,const void*s,u64 n){u8*a=d;const u8*b=s;while(n--)*a++=*b++;return d;}

#define PI 3.14159265f
#define TAU 6.28318531f
static f32 f_abs(f32 x){return x<0?-x:x;}
static f32 f_floor(f32 x){i32 i=(i32)x;f32 f=(f32)i;return (x<0&&f!=x)?f-1:f;}
static f32 f_sqrt(f32 x){if(x<=0)return 0;f32 r;__asm__("sqrtss %1,%0":"=x"(r):"x"(x));return r;}
static f32 f_sin(f32 x){x-=TAU*f_floor((x+PI)/TAU);f32 s=1.27323954f*x-0.405284735f*x*f_abs(x);return 0.225f*(s*f_abs(s)-s)+s;}
static f32 f_cos(f32 x){return f_sin(x+1.5707963f);}
static f32 f_min(f32 a,f32 b){return a<b?a:b;}
static f32 f_max(f32 a,f32 b){return a>b?a:b;}

typedef struct{f32 x,y;} V2;
static f32 len2(f32 x,f32 y){return f_sqrt(x*x+y*y);}

static u32 rng=0x5eed1234;
static u32 xr(void){u32 x=rng;x^=x<<13;x^=x>>17;x^=x<<5;rng=x;return x;}
static void seed(u32 s){rng=s?s:0xdecafbad;}
static f32 frand(void){return (f32)(xr()&0xffffff)/16777215.0f;}
static u32 hash32(u32 a){a^=a>>16;a*=0x7feb352d;a^=a>>15;a*=0x846ca68b;a^=a>>16;return a;}

#define FN(ret,name,args) typedef ret(*name##_t)args; static name##_t p_##name
FN(void,glClearColor,(f32,f32,f32,f32));FN(void,glClear,(u32));FN(void,glViewport,(i32,i32,i32,i32));
FN(void,glMatrixMode,(u32));FN(void,glLoadIdentity,(void));FN(void,glOrtho,(double,double,double,double,double,double));
FN(void,glBegin,(u32));FN(void,glEnd,(void));FN(void,glVertex2f,(f32,f32));FN(void,glColor3f,(f32,f32,f32));FN(void,glColor4f,(f32,f32,f32,f32));
FN(void,glEnable,(u32));FN(void,glBlendFunc,(u32,u32));FN(void,glLineWidth,(f32));FN(void,glPointSize,(f32));

FN(int,SDL_Init,(u32));FN(void,SDL_Quit,(void));
FN(void*,SDL_CreateWindow,(const char*,int,int,int,int,u32));FN(void,SDL_DestroyWindow,(void*));
FN(void*,SDL_GL_CreateContext,(void*));FN(void,SDL_GL_DeleteContext,(void*));
FN(void,SDL_GL_SwapWindow,(void*));FN(int,SDL_PollEvent,(void*));FN(int,SDL_SetWindowFullscreen,(void*,u32));
FN(const u8*,SDL_GetKeyboardState,(int*));FN(u32,SDL_GetTicks,(void));FN(void,SDL_Delay,(u32));
FN(u32,SDL_OpenAudioDevice,(const char*,int,const void*,void*,int));FN(void,SDL_PauseAudioDevice,(u32,int));

static void* gl_lib; static void* sdl_lib;
#define LGL(n) do{p_##n=(n##_t)dlsym(gl_lib,#n);if(!p_##n)miss++;}while(0)
#define LSDL(n) do{p_##n=(n##_t)dlsym(sdl_lib,#n);if(!p_##n)miss++;}while(0)

#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_BLEND 0x0BE2
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_POINTS 0x0000
#define GL_LINE_LOOP 0x0002
#define GL_QUADS 0x0007
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define SDL_INIT_VIDEO 0x20u
#define SDL_INIT_AUDIO 0x10u
#define SDL_WINDOW_OPENGL 2u
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x1001u
#define SDL_WINDOWPOS_CENTERED 0x2fff0000
#define SDL_QUIT 0x100
#define SDL_WINDOWEVENT 0x200
#define SDL_WINDOWEVENT_SIZE_CHANGED 5
#define AUDIO_S16SYS 0x8010

#define SC_A 4
#define SC_S 22
#define SC_ESC 41
#define SC_SPACE 44
#define SC_F11 68
#define SC_RIGHT 79
#define SC_LEFT 80
#define SC_DOWN 81
#define SC_UP 82

typedef struct{int freq;uint16_t format;u8 channels,silence;uint16_t samples,padding;u32 size;void(*callback)(void*,u8*,int);void* userdata;} AudioSpec;

#define GW 52
#define GH 32
#define MAXP 8
#define MAXE 14
#define MAXI 24
enum{ST_TITLE,ST_PLAY,ST_DEAD,ST_WIN};

typedef struct{f32 x,y,r,m;u8 live;} Pulse;
typedef struct{f32 x,y,tx,ty,alert,wander;u8 k;} Enemy;
typedef struct{f32 x,y;u8 got,k;} Item;
static struct{
  u8 wall[GW*GH],vis[GW*GH],prev[96];
  Pulse p[MAXP]; Enemy e[MAXE]; Item it[MAXI];
  int ne,ni,state,win,wh,full,relic,salvage,score,gw,gh,floor,pings,ping_bonus;
  f32 px,py,hx,hy,hp,stamina,hit_cd,noise,noise_x,noise_y,t,shake;
  f32 audio_t,ping_a,hunt_a,hurt_a,relic_a,sting_a,warn_a,flash;
  u32 seed,adev;
} G;

static int edge(const u8*ks,int sc){return ks[sc]&&!G.prev[sc];}
static int idx(int x,int y){return y*G.gw+x;}
static int solid(int x,int y){return x<0||y<0||x>=G.gw||y>=G.gh||G.wall[idx(x,y)];}
static void carve(int x,int y){if(x>0&&y>0&&x<G.gw-1&&y<G.gh-1)G.wall[idx(x,y)]=0;}
static void carve_box(int cx,int cy,int w,int h){for(int y=cy-h;y<=cy+h;y++)for(int x=cx-w;x<=cx+w;x++)carve(x,y);}
static void add_pulse(f32 x,f32 y,f32 maxr){
  int slot=0; for(int i=0;i<MAXP;i++)if(!G.p[i].live){slot=i;break;}
  G.p[slot].x=x;G.p[slot].y=y;G.p[slot].r=0;G.p[slot].m=maxr;G.p[slot].live=1;
  G.ping_a=1.0f; G.noise=1.0f; G.noise_x=x; G.noise_y=y;
  for(int i=0;i<G.ne;i++){f32 d=len2(G.e[i].x-x,G.e[i].y-y);if(d<maxr*(G.relic?1.2f:.82f)||G.relic){G.e[i].tx=x;G.e[i].ty=y;G.e[i].alert=2.3f+G.relic*1.5f;}}
}

static void new_game(u32 sd){
  int kscore=G.score; int kfloor=(G.state==ST_WIN)?G.floor+1:1;
  int kpb=G.ping_bonus; u32 keep=G.adev; int ww=G.win?G.win:1280, wh=G.wh?G.wh:720;
  m_set(&G,0,sizeof(G)); G.adev=keep; G.win=ww; G.wh=wh;
  G.score=kscore; G.floor=kfloor; G.ping_bonus=kpb; G.seed=sd; seed(sd);
  int gw=32,gh=20;
  for(int f=1;f<kfloor;f++){
    f32 ratio=(f32)gw/(f32)gh;
    int grow_w=(ratio>1.85f)?0:(ratio<1.45f)?1:(int)(hash32((u32)f*1337u)&1);
    if(grow_w){if(gw+2<=GW)gw+=2;else if(gh+2<=GH)gh+=2;}
    else{if(gh+2<=GH)gh+=2;else if(gw+2<=GW)gw+=2;}
  }
  G.gw=gw; G.gh=gh;
  for(int i=0;i<GW*GH;i++)G.wall[i]=1;
  int path[GW]; int y=G.gh/2;
  for(int x=2;x<G.gw-2;x++){ if((xr()&3)==0)y+=(int)(xr()%3)-1; if(y<5)y=5;if(y>G.gh-6)y=G.gh-6; path[x]=y; carve_box(x,y,1,1); }
  int nrooms=28*G.gw*G.gh/(GW*GH); if(nrooms<8)nrooms=8;
  for(int r=0;r<nrooms;r++){
    int x=4+(xr()%(G.gw-8)), cy=4+(xr()%(G.gh-8)), rw=2+(xr()%4), rh=1+(xr()%3);
    carve_box(x,cy,rw,rh); int py=path[x]; for(int yy=(int)f_min(cy,py);yy<=(int)f_max(cy,py);yy++)carve(x,yy);
    for(int xx=(int)f_min(x,x+rw);xx<=(int)f_max(x,x+rw);xx++)carve(xx,py);
  }
  G.px=G.hx=3.5f; G.py=G.hy=(f32)path[3]+0.5f; G.hp=100; G.stamina=100; G.state=ST_PLAY; carve_box(3,path[3],2,2);
  G.pings=3+G.ping_bonus;
  /* relic */
  { int ox=G.gw/2+3+(int)(xr()%(G.gw/2-6)); G.it[0].x=(f32)ox+0.5f; G.it[0].y=(f32)path[ox]+0.5f; G.it[0].k=0; } G.ni=1;
  /* salvage */
  for(int i=0;i<15&&G.ni<MAXI-5;i++){
    for(int t=0;t<200;t++){int x=3+(xr()%(G.gw-6)),yy=3+(xr()%(G.gh-6)); if(!solid(x,yy)&&x>5){G.it[G.ni].x=x+0.5f;G.it[G.ni].y=yy+0.5f;G.it[G.ni].k=1;G.ni++;break;}}
  }
  /* regular ping pickups: start floor 10, +1 every 5 floors, max 4 */
  int npings=G.floor>=10?1+(G.floor-10)/5:0; if(npings>4)npings=4;
  for(int i=0;i<npings&&G.ni<MAXI-1;i++){
    for(int t=0;t<200;t++){int x=G.gw/3+(xr()%(G.gw*2/3-4)),yy=3+(xr()%(G.gh-6)); if(!solid(x,yy)){G.it[G.ni].x=x+0.5f;G.it[G.ni].y=yy+0.5f;G.it[G.ni].k=2;G.ni++;break;}}
  }
  /* rare ping: floor 10+, random corner, carved so it's reachable */
  if(G.floor>=10&&G.ni<MAXI){
    int corner=(int)(hash32(G.seed*31337u)%4);
    int cx=(corner&1)?G.gw-4:3, cy=(corner&2)?G.gh-4:3;
    carve_box(cx,cy,1,1);
    for(int yy=cy;yy!=path[cx];yy+=yy<path[cx]?1:-1)carve(cx,yy);
    G.it[G.ni].x=(f32)cx+0.5f; G.it[G.ni].y=(f32)cy+0.5f; G.it[G.ni].k=3; G.ni++;
  }
  for(int i=0;i<MAXE;i++){
    for(int t=0;t<200;t++){int x=8+(xr()%(G.gw-10)),yy=3+(xr()%(G.gh-6)); if(!solid(x,yy)){Enemy*e=&G.e[G.ne++];e->x=x+0.5f;e->y=yy+0.5f;e->tx=e->x;e->ty=e->y;e->k=(i%5)==0?2:((i%4)==0);break;}}
  }
  add_pulse(G.px,G.py,8);
}

static int try_move(f32*px,f32*py,f32 dx,f32 dy){
  f32 nx=*px+dx,ny=*py+dy,r=.28f; int ok=1;
  if(solid((int)(nx-r),(int)(*py-r))||solid((int)(nx+r),(int)(*py-r))||solid((int)(nx-r),(int)(*py+r))||solid((int)(nx+r),(int)(*py+r)))ok=0; else *px=nx;
  nx=*px; if(solid((int)(nx-r),(int)(ny-r))||solid((int)(nx+r),(int)(ny-r))||solid((int)(nx-r),(int)(ny+r))||solid((int)(nx+r),(int)(ny+r)))ok=0; else *py=ny;
  return ok;
}

static void update_game(f32 dt,const u8*ks){
  int sprinting=ks[SC_A]&&G.stamina>1.0f; int slow=!sprinting; f32 mx=(f32)ks[SC_RIGHT]-(f32)ks[SC_LEFT], my=(f32)ks[SC_DOWN]-(f32)ks[SC_UP];
  f32 l=len2(mx,my); if(l>0){f32 sp=(slow?2.0f:4.2f)*dt/l; try_move(&G.px,&G.py,mx*sp,my*sp); if(!slow){G.noise_x=G.px;G.noise_y=G.py;G.noise=f_max(G.noise,.18f);}}
  if(sprinting&&l>0){G.stamina-=dt*30;if(G.stamina<0)G.stamina=0;}else if(!ks[SC_A]){G.stamina+=dt*20;if(G.stamina>100)G.stamina=100;}
  if(ks[SC_A]&&G.stamina<=1.0f)G.warn_a=.75f;
  if(edge(ks,SC_S)&&G.pings>0){G.pings--;add_pulse(G.px,G.py,18);}
  if(G.relic&&len2(G.px-G.hx,G.py-G.hy)<0.9f){G.score+=G.salvage*100+(int)G.hp*5+(int)G.stamina;G.state=ST_WIN;return;}

  for(int i=0;i<G.gw*G.gh;i++){int v=G.vis[i]-(int)(dt*52);G.vis[i]=v>0?v:0;}
  for(int i=0;i<MAXP;i++)if(G.p[i].live){
    Pulse*p=&G.p[i]; p->r+=dt*18.0f; if(p->r>p->m+1)p->live=0;
    int xmin=(int)(p->x-p->r-2),xmax=(int)(p->x+p->r+2),ymin=(int)(p->y-p->r-2),ymax=(int)(p->y+p->r+2);
    if(xmin<1)xmin=1;
    if(ymin<1)ymin=1;
    if(xmax>G.gw-2)xmax=G.gw-2;
    if(ymax>G.gh-2)ymax=G.gh-2;
    for(int y=ymin;y<=ymax;y++)for(int x=xmin;x<=xmax;x++){f32 d=len2(x+.5f-p->x,y+.5f-p->y); if(f_abs(d-p->r)<1.4f||d<2.0f){int n=idx(x,y); if(G.vis[n]<230)G.vis[n]=230;}}
  }
  for(int i=0;i<G.ni;i++)if(!G.it[i].got&&len2(G.px-G.it[i].x,G.py-G.it[i].y)<.65f){
    G.it[i].got=1; if(G.it[i].k==0){G.relic=1;G.relic_a=1;G.sting_a=1.0f;G.flash=1.0f;add_pulse(G.px,G.py,24);} else if(G.it[i].k==1){G.salvage++;G.score+=100;G.ping_a=.7f;} else if(G.it[i].k==2){if(G.pings<20)G.pings++;G.ping_a=.7f;} else{G.ping_bonus++;G.pings++;G.ping_a=.7f;}
  }

  if(G.noise>0)G.noise-=dt*.8f;
  if(G.hurt_a>0)G.hurt_a-=dt*.6f;
  if(G.hit_cd>0)G.hit_cd-=dt;
  G.hunt_a=0;
  for(int i=0;i<G.ne;i++){
    Enemy*e=&G.e[i]; f32 dx=G.px-e->x,dy=G.py-e->y,d=len2(dx,dy);
    if(G.relic){e->tx=G.px;e->ty=G.py;e->alert=f_max(e->alert,1.2f);}
    if(G.noise>0&&d<6.0f+G.noise*9){e->tx=G.noise_x;e->ty=G.noise_y;e->alert=f_max(e->alert,1.6f+G.noise*1.4f);}
    if(e->alert>0){e->alert-=dt;G.hunt_a=.9f;dx=e->tx-e->x;dy=e->ty-e->y;d=len2(dx,dy);if(d>.05f&&e->k!=2){f32 sp=(e->k?2.65f:1.35f)+(G.relic?.55f:0);sp*=dt/d;try_move(&e->x,&e->y,dx*sp,dy*sp);} if(e->k==2&&d<6.0f&&G.noise>.7f&&G.hit_cd<=0){G.hp-=4;G.hit_cd=.6f;G.hurt_a=1;G.shake=.18f;}}
    else if(e->k!=2){e->wander-=dt;if(e->wander<=0){e->wander=1+frand()*3;e->tx=e->x+(frand()*2-1)*5;e->ty=e->y+(frand()*2-1)*5;}dx=e->tx-e->x;dy=e->ty-e->y;d=len2(dx,dy);if(d>.2f){f32 sp=.55f*dt/d;try_move(&e->x,&e->y,dx*sp,dy*sp);}}
    dx=G.px-e->x;dy=G.py-e->y;d=len2(dx,dy); if(d<.62f&&G.hit_cd<=0){G.hp-=e->k?9:5;G.hit_cd=.75f;G.hurt_a=1;G.shake=.35f;if(d>.01f)try_move(&G.px,&G.py,dx/d*.75f,dy/d*.75f);if(G.hp<=0){G.state=ST_DEAD;return;}}
  }
}

static void begin2d(void){p_glMatrixMode(GL_PROJECTION);p_glLoadIdentity();p_glOrtho(0,G.win,G.wh,0,-1,1);p_glMatrixMode(GL_MODELVIEW);p_glLoadIdentity();}
static void rect(f32 x,f32 y,f32 w,f32 h,f32 r,f32 g,f32 b,f32 a){p_glColor4f(r,g,b,a);p_glBegin(GL_QUADS);p_glVertex2f(x,y);p_glVertex2f(x+w,y);p_glVertex2f(x+w,y+h);p_glVertex2f(x,y+h);p_glEnd();}
static void circle(f32 cx,f32 cy,f32 rr,int n,f32 r,f32 g,f32 b,f32 a){
  p_glColor4f(r,g,b,a);p_glBegin(GL_LINE_LOOP);for(int i=0;i<n;i++){f32 t=TAU*(f32)i/(f32)n;p_glVertex2f(cx+f_cos(t)*rr,cy+f_sin(t)*rr);}p_glEnd();
}

static const u8 font[39][7]={
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},{17,17,17,17,10,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{0,0,0,31,0,0,0},{0,12,12,0,12,12,0},{0,0,0,0,0,12,12}};
static int glyph(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='Z')return 10+c-'A';if(c>='a'&&c<='z')return 10+c-'a';if(c=='-')return 36;if(c==':')return 37;if(c=='.')return 38;return -1;}
static f32 textw(const char*s,f32 sc){f32 w=0;while(*s){w+=(*s++==' ')?4*sc:6*sc;}return w;}
static void text(f32 x,f32 y,f32 sc,const char*s,f32 r,f32 g,f32 b){
  p_glColor3f(r,g,b);p_glBegin(GL_QUADS);
  for(;*s;s++,x+=6*sc){if(*s==' '){x-=2*sc;continue;}int gi=glyph(*s);if(gi<0)continue;for(int yy=0;yy<7;yy++){u8 row=font[gi][yy];for(int xx=0;xx<5;xx++)if(row&(1<<(4-xx))){f32 ax=x+xx*sc,ay=y+yy*sc,e=sc*.92f;p_glVertex2f(ax,ay);p_glVertex2f(ax+e,ay);p_glVertex2f(ax+e,ay+e);p_glVertex2f(ax,ay+e);}}}
  p_glEnd();
}
static char numbuf[16]; static char* itos(i32 n){char t[12];int i=0;if(!n)t[i++]='0';while(n){t[i++]='0'+n%10;n/=10;}int j=0;while(i)numbuf[j++]=t[--i];numbuf[j]=0;return numbuf;}

static void star5(f32 cx,f32 cy,f32 r,f32 rc,f32 gc,f32 bc,f32 a){
  f32 r2=r*0.42f; p_glColor4f(rc,gc,bc,a); p_glBegin(GL_LINE_LOOP);
  for(int i=0;i<5;i++){f32 ao=TAU*(f32)i/5.0f-PI/2.0f,ai=ao+TAU/10.0f;p_glVertex2f(cx+f_cos(ao)*r,cy+f_sin(ao)*r);p_glVertex2f(cx+f_cos(ai)*r2,cy+f_sin(ai)*r2);}
  p_glEnd();
}
static void draw_game(void){
  begin2d(); rect(0,0,G.win,G.wh,.005f,.012f,.018f,1);
  f32 cs=f_min((f32)G.win/G.gw,((f32)G.wh-84)/G.gh), ox=((f32)G.win-G.gw*cs)*.5f, oy=54+((f32)G.wh-84-G.gh*cs)*.5f;
  if(G.shake>0){ox+=(frand()*2-1)*G.shake*8;oy+=(frand()*2-1)*G.shake*8;G.shake-=.02f;}
  p_glEnable(GL_BLEND);p_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  p_glPointSize(2);p_glBegin(GL_POINTS);for(int i=0;i<140;i++){u32 h=hash32(i*977+G.seed);f32 x=(h&1023)*(f32)G.win/1024.0f,y=((h>>10)&1023)*(f32)G.wh/1024.0f;p_glColor4f(.18f,.45f,.48f,.10f+.10f*f_sin(G.t+i));p_glVertex2f(x,y);}p_glEnd();
  for(int y=0;y<G.gh;y++)for(int x=0;x<G.gw;x++){
    int n=idx(x,y),v=G.vis[n]; if(len2(G.px-(x+.5f),G.py-(y+.5f))<2.2f)v=180; if(!v)continue;
    f32 a=(f32)v/255.0f, sx=ox+x*cs, sy=oy+y*cs;
    if(G.wall[n]){int near=0;for(int yy=-1;yy<=1;yy++)for(int xx=-1;xx<=1;xx++)if(!solid(x+xx,y+yy)&&G.vis[idx(x+xx,y+yy)])near=1; if(near)rect(sx,sy,cs,cs,.03f,.32f,.38f,a*.55f);}
    else rect(sx+1,sy+1,cs-2,cs-2,.02f,.08f,.10f,a*.35f);
  }
  p_glLineWidth(2);
  for(int i=0;i<G.ni;i++)if(!G.it[i].got){int n=idx((int)G.it[i].x,(int)G.it[i].y);f32 a=G.vis[n]/255.0f;if(a>0){f32 sx=ox+G.it[i].x*cs,sy=oy+G.it[i].y*cs;if(G.it[i].k==0)circle(sx,sy,cs*.55f,24,.45f,1,.80f,a);else if(G.it[i].k==1)circle(sx,sy,cs*.25f,12,1,.74f,.22f,a);else{star5(sx,sy,cs*.28f,.28f,1.f,.75f,a);if(G.it[i].k==3){f32 fl=f_sin(G.t*6.0f);if(fl>0)circle(sx,sy,cs*.48f,20,.28f,1.f,.75f,fl*a*.9f);}}}}
  rect(ox+(G.hx-.8f)*cs,oy+(G.hy-.8f)*cs,cs*1.6f,cs*1.6f,.05f,.28f,.32f,.65f);circle(ox+G.hx*cs,oy+G.hy*cs,cs*.75f,4,.45f,.9f,1,1);
  for(int i=0;i<G.ne;i++){Enemy*e=&G.e[i];int n=idx((int)e->x,(int)e->y);f32 a=G.vis[n]/255.0f;if(e->alert>0)a=f_max(a,.18f);if(a>0){f32 sx=ox+e->x*cs,sy=oy+e->y*cs;circle(sx,sy,cs*(e->k==2?.42f:(e->k?.34f:.28f)),e->k==2?4:(e->k?3:10),1,e->k==2?.55f:.16f,.08f,a);}}
  for(int i=0;i<MAXP;i++)if(G.p[i].live)circle(ox+G.p[i].x*cs,oy+G.p[i].y*cs,G.p[i].r*cs,56,.28f,1,.75f,.55f*(1-G.p[i].r/G.p[i].m));
  f32 px=ox+G.px*cs,py=oy+G.py*cs;circle(px,py,cs*.35f,16,.70f,1,.86f,1);rect(px-2,py-cs*.45f,4,cs*.9f,.8f,1,.9f,.7f);
  rect(22,18,G.hp*2.2f,10,.9f,.18f,.12f,.9f);rect(22,34,G.stamina*2.2f,10,.18f,.8f,1,.8f);
  text(22,52,1.5f,"HULL",.7f,.8f,.8f);text(104,52,1.5f,itos((int)G.hp),.9f,.9f,.8f);text(178,52,1.5f,"STAM",.7f,.8f,.8f);
  text(G.win-260,8,1.5f,"FLOOR",.5f,.7f,.65f);text(G.win-138,8,1.5f,itos(G.floor),.8f,.9f,.7f);
  text(G.win-260,24,1.5f,"PINGS",.4f,.85f,.7f);text(G.win-138,24,1.5f,itos(G.pings),.7f,1.f,.8f);
  text(G.win-260,40,1.6f,"SALVAGE",.65f,.8f,.72f);text(G.win-98,40,1.6f,itos(G.salvage),.95f,.8f,.35f);
  text(G.win-260,58,1.5f,"SCORE",.55f,.70f,.68f);text(G.win-138,58,1.5f,itos(G.score),.85f,.85f,.62f);
  if(G.relic)text(G.win/2-textw("RELIC HOT - ENTER THE HATCH",1.8f)/2,20,1.8f,"RELIC HOT - ENTER THE HATCH",.8f,1,.55f);
  if(G.flash>0){rect(0,0,G.win,G.wh,.1f,.9f,.4f,G.flash*.45f);if(G.flash>.01f)text(G.win/2-textw("RELIC HOT",6)/2,G.wh/2-40,6,"RELIC HOT",.6f,1,.5f);G.flash*=.93f;if(G.flash<.01f)G.flash=0;}
  else if(!G.relic)text(G.win/2-textw("A SPRINT  S PING  FIND CORE",1.5f)/2,20,1.5f,"A SPRINT  S PING  FIND CORE",.45f,.75f,.78f);
}

static void draw_title(void){
  begin2d(); rect(0,0,G.win,G.wh,.002f,.008f,.014f,1); p_glEnable(GL_BLEND);p_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  for(int i=0;i<5;i++)circle(G.win*.5f,G.wh*.55f,(G.t*60+i*90)-f_floor((G.t*60+i*90)/500)*500,96,.18f,.9f,.75f,.14f);
  text(G.win/2-textw("ECHOHULL",7)/2,150,7,"ECHOHULL",.55f,1,.82f);
  text(G.win/2-textw("SONAR SALVAGE HORROR",2.2f)/2,235,2.2f,"SONAR SALVAGE HORROR",.55f,.72f,.76f);
  text(G.win/2-textw("PING AND SOMETHING ANSWERS",1.6f)/2,330,1.6f,"PING AND SOMETHING ANSWERS",.72f,.86f,.78f);
  text(G.win/2-textw("SPACE START   F11 FULLSCREEN   ESC QUIT",1.6f)/2,G.wh-96,1.6f,"SPACE START   F11 FULLSCREEN   ESC QUIT",.55f,.70f,.72f);
}
static void draw_end(const char*s,f32 r,f32 g,f32 b){
  draw_game(); rect(0,0,G.win,G.wh,0,0,0,.55f); text(G.win/2-textw(s,5)/2,G.wh/2-60,5,s,r,g,b);
  text(G.win/2-textw("SPACE NEW DIVE   ESC QUIT",1.8f)/2,G.wh/2+24,1.8f,"SPACE NEW DIVE   ESC QUIT",.75f,.82f,.78f);
}

static void audio_cb(void*ud,u8*stream,int len){
  (void)ud; int16_t*out=(int16_t*)stream; int n=len/2;
  for(int i=0;i<n;i+=2){
    f32 t=G.audio_t, sm=.05f*f_sin(TAU*43*t+.7f*f_sin(TAU*.07f*t))+.025f*f_sin(TAU*86.2f*t);
    sm+=G.relic_a*.07f*f_sin(TAU*61*t)*(.6f+.4f*f_sin(TAU*3*t));
    sm+=G.sting_a*.35f*f_sin(TAU*(320+680*G.sting_a)*t)*(G.sting_a>.1f?1:0);
    sm+=G.hunt_a*.08f*f_sin(TAU*(92+18*f_sin(TAU*1.7f*t))*t);
    sm+=G.warn_a*.18f*f_sin(TAU*880*t);
    sm+=G.ping_a*.45f*f_sin(TAU*(520+360*G.ping_a)*t);
    sm+=G.hurt_a*.22f*f_sin(TAU*(70+120*G.hurt_a)*t);
    if(sm>1)sm=1;
    if(sm<-1)sm=-1;
    int16_t v=(int16_t)(sm*26000); out[i]=v; out[i+1]=v;
    G.audio_t+=1.0f/44100.0f; G.ping_a*=.99935f; G.hunt_a*=.99985f; G.hurt_a*=.9995f; G.relic_a*=.99998f; G.sting_a*=.9992f; G.warn_a*=.9991f;
  }
}

int main(void){
  int miss=0; gl_lib=dlopen("libGL.so.1",2|256); sdl_lib=dlopen("libSDL2-2.0.so.0",2|256); if(!gl_lib||!sdl_lib)return 1;
  LSDL(SDL_Init);LSDL(SDL_Quit);LSDL(SDL_CreateWindow);LSDL(SDL_DestroyWindow);LSDL(SDL_GL_CreateContext);LSDL(SDL_GL_DeleteContext);LSDL(SDL_GL_SwapWindow);LSDL(SDL_PollEvent);LSDL(SDL_SetWindowFullscreen);LSDL(SDL_GetKeyboardState);LSDL(SDL_GetTicks);LSDL(SDL_Delay);LSDL(SDL_OpenAudioDevice);LSDL(SDL_PauseAudioDevice);
  LGL(glClearColor);LGL(glClear);LGL(glViewport);LGL(glMatrixMode);LGL(glLoadIdentity);LGL(glOrtho);LGL(glBegin);LGL(glEnd);LGL(glVertex2f);LGL(glColor3f);LGL(glColor4f);LGL(glEnable);LGL(glBlendFunc);LGL(glLineWidth);LGL(glPointSize); if(miss)return 1;
  if(p_SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO))return 1;
  G.win=1280;G.wh=720; void*win=p_SDL_CreateWindow("ECHOHULL",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,G.win,G.wh,SDL_WINDOW_OPENGL); if(!win)return 1; void*ctx=p_SDL_GL_CreateContext(win); if(!ctx)return 1;
  G.seed=(u32)p_SDL_GetTicks()^0xEC0011; G.state=ST_TITLE;
  AudioSpec want; m_set(&want,0,sizeof(want)); want.freq=44100; want.format=AUDIO_S16SYS; want.channels=2; want.samples=1024; want.callback=audio_cb; AudioSpec got; m_set(&got,0,sizeof(got)); G.adev=p_SDL_OpenAudioDevice(0,0,&want,&got,0); if(G.adev)p_SDL_PauseAudioDevice(G.adev,0);
  p_glViewport(0,0,G.win,G.wh); u32 last=p_SDL_GetTicks(); int running=1; u8 ev[64];
  while(running){
    u32 now=p_SDL_GetTicks(); f32 dt=(now-last)/1000.0f; last=now; if(dt>.05f)dt=.05f; if(dt<=0)dt=.001f; G.t+=dt;
    while(p_SDL_PollEvent(ev)){u32 et=*(u32*)ev;if(et==SDL_QUIT)running=0;if(et==SDL_WINDOWEVENT&&ev[12]==SDL_WINDOWEVENT_SIZE_CHANGED){G.win=*(int*)(ev+16);G.wh=*(int*)(ev+20);p_glViewport(0,0,G.win,G.wh);}}
    const u8*ks=p_SDL_GetKeyboardState(0); if(edge(ks,SC_ESC))running=0; if(edge(ks,SC_F11)){G.full^=1;p_SDL_SetWindowFullscreen(win,G.full?SDL_WINDOW_FULLSCREEN_DESKTOP:0);}
    if(G.state==ST_TITLE&&edge(ks,SC_SPACE))new_game(G.seed);
    else if((G.state==ST_DEAD||G.state==ST_WIN)&&edge(ks,SC_SPACE))new_game(hash32(G.seed+(u32)now));
    else if(G.state==ST_PLAY)update_game(dt,ks);
    m_cpy(G.prev,ks,96);
    p_glClearColor(0,0,0,1);p_glClear(GL_COLOR_BUFFER_BIT);
    if(G.state==ST_TITLE)draw_title(); else if(G.state==ST_PLAY)draw_game(); else if(G.state==ST_DEAD)draw_end("SUIT BREACHED",1,.2,.16); else draw_end("RELIC RECOVERED",.55f,1,.72f);
    p_SDL_GL_SwapWindow(win); p_SDL_Delay(1);
  }
  if(G.adev)p_SDL_PauseAudioDevice(G.adev,1);
  p_SDL_GL_DeleteContext(ctx); p_SDL_DestroyWindow(win); p_SDL_Quit(); return 0;
}
