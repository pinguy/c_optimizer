#define main tether_original_main
#include "../examples/TETHER9.c"
#undef main

static int controller(int tick){
 int target=-1;float best=1e6f;
 for(int i=0;i<np;i++)if(!pods[i].state){
  float d=dist(G.x,G.y,pods[i].x,pods[i].y);
  if(d<best){best=d;target=i;}
 }
 float x=WX*.5f,y=WY*.5f;
 if(G.tether<0 && target>=0){x=pods[target].x;y=pods[target].y;}
 float dx=x-G.x,dy=y-G.y,d=length(dx,dy),sp=length(G.vx,G.vy);
 if(G.tether<0 && d<75 && sp<125)toggle_tether();
 if(G.tether<0 && target<0)return 0;
 float goal=G.tether<0?55:4;
 float ddx=dx-G.vx*.65f,ddy=dy-G.vy*.65f;
 float desired=atan2f(ddy,ddx);
 float err=wrapang(desired-G.angle);
 int left=err<-.06f, right=err>.06f;
 int thrust=d>goal+10 && fabsf(err)<.3f && sp<(d>200?125:100);
 int brake=(d<goal+25 && sp>25) || (fabsf(err)>.65f && sp>45) || (sp>160);
 if(G.tether>=0 && d<35){brake=1;thrust=0;}
 float enemy=1e5;for(int i=0;i<nd;i++)if(drones[i].hp){float q=dist(G.x,G.y,drones[i].x,drones[i].y);if(q<enemy)enemy=q;}
 if(enemy<120 && G.energy>40) pulse();
 step(1.0f/60,left,right,thrust,brake,0);
 if(tick%120==0)fprintf(stderr,"tick=%d wave=%d delivered=%d/%d score=%d hull=%.0f tether=%d ship=(%.0f,%.0f) v=%.0f target=(%.0f,%.0f) d=%.0f enemy=%.0f\n",tick,G.wave,G.delivered,G.quota,G.score,G.hp,G.tether,G.x,G.y,sp,x,y,d,enemy);
 return 1;
}
int main(int argc,char**argv){
 int seed=(argc>1)?atoi(argv[1]):12345;new_game(seed);
 for(int j=0;j<15000 && G.state==1 && G.wave==1;j++)controller(j);
 printf("RESULT seed=%d wave=%d delivered=%d score=%d hp=%.1f state=%d\n",seed,G.wave,G.delivered,G.score,G.hp,G.state);
 return G.wave>=2?0:1;
}
