#include <SDL.h>
#define private public
#include "../../src/runtime/host_hud.h"
#undef private
#include <cassert>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>
#include <string>

static void write_atomic(const std::string& path,const char *contents) {
 std::string tmp=path+".new"; FILE *f=fopen(tmp.c_str(),"w");assert(f);fputs(contents,f);fclose(f);assert(!rename(tmp.c_str(),path.c_str()));
}
static PidosHud::Snapshot snapshot(PidosHud& h) {
 SDL_LockMutex(h.mutex_); auto s=h.snapshot_; SDL_UnlockMutex(h.mutex_); return s;
}
static PidosHud::Snapshot wait_seq(PidosHud& h,unsigned min) {
 for(int i=0;i<5000;i++){auto s=snapshot(h);if(s.sequence>=min)return s;SDL_Delay(1);} assert(false);return {};
}
int main() {
 assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
 char path[]="/tmp/pidos-hud-test-XXXXXX";
 char *dir=mkdtemp(path);assert(dir);std::string root(dir),z0=root+"/thermal_zone0",z1=root+"/thermal_zone1",proc=root+"/stat";
 assert(!mkdir(z0.c_str(),0700));assert(!mkdir(z1.c_str(),0700));
 write_atomic(z0+"/type","gpu-thermal\n");write_atomic(z0+"/temp","99000\n");
 write_atomic(z1+"/type","cpu-thermal\n");write_atomic(z1+"/temp","42000\n");
 write_atomic(proc,"cpu 0 0 0 0\ncpu0 100 0 0 900 0 0 0 0 99 99\ncpu1 100 0 0 900 0 0 0 0\ncpu2 100 0 0 900 0 0 0 0\ncpu3 100 0 0 900 0 0 0 0\n");
 PidosHud hud(proc.c_str(),root.c_str()); assert(hud.start());
 auto a=wait_seq(hud,1); assert(a.temp_valid && a.temp_c==42);for(int i=0;i<4;i++)assert(a.cores[i].present&&!a.cores[i].valid); assert(!a.extra);
 write_atomic(z1+"/temp","51000\n");
 write_atomic(proc,"cpu 0 0 0 0\ncpu0 180 0 0 920 0 0 0 0 999 999\ncpu1 100 0 0 1000 0 0 0 0\ncpu2 200 0 0 900 0 0 0 0\ncpu3 150 0 0 950 0 0 0 0\n");
 auto b=wait_seq(hud,2); assert(b.temp_valid && b.temp_c==51);
 const int expected[]={80,0,100,50}; for(int i=0;i<4;i++)assert(b.cores[i].valid && b.cores[i].percent==expected[i]);
 SDL_Window *w=SDL_CreateWindow("hud",0,0,640,480,0);assert(w);SDL_Surface *screen=SDL_GetWindowSurface(w);assert(screen);
 Uint32 red=SDL_MapRGB(screen->format,255,0,0);assert(!SDL_FillRect(screen,nullptr,red));
 assert(hud.draw(screen)); assert(hud.image_ && hud.image_->w<130 && hud.image_->h<=30);
 SDL_Surface *ref=SDL_ConvertSurface(screen,screen->format,0);assert(ref);
 int left=screen->w-hud.image_->w-3; int white=0, black=0;
 for(int y=0;y<screen->h;y++)for(int x=0;x<screen->w;x++){
  const int bp=screen->format->BytesPerPixel; Uint32 v=0;memcpy(&v,(uint8_t*)screen->pixels+y*screen->pitch+x*bp,bp);
  Uint8 r,g,bl;SDL_GetRGB(v,screen->format,&r,&g,&bl);
  bool inside=x>=left && x<left+hud.image_->w && y>=3 && y<3+hud.image_->h;
  if(!inside)assert(r==255 && g==0 && bl==0); else {if(r==255&&g==255&&bl==255)white++;if(r==0&&g==0&&bl==0)black++;}
 }
 assert(white>60 && black>60); assert(!SDL_UpdateWindowSurface(w));
 assert(!hud.pulse_due());SDL_Delay(1100);assert(hud.pulse_due());
 write_atomic(z1+"/temp","500000\n");auto c=wait_seq(hud,3);if(c.temp_valid)c=wait_seq(hud,4);assert(!c.temp_valid);
 hud.stop();
 /* Missing /proc and non-CPU thermal must be shown as unavailable. */
 write_atomic(z1+"/type","gpu-thermal\n");
 PidosHud missing("/definitely/not/a/proc/stat",root.c_str());assert(missing.start());
 auto d=wait_seq(missing,1);assert(!d.temp_valid);for(int i=0;i<8;i++)assert(!d.cores[i].present);
 assert(missing.draw(screen));missing.stop();
 /* Repeated mode-change lifetimes, including immediate cancellation. */
 for(int i=0;i<100;i++){assert(missing.start());if(i%10==0)SDL_Delay(2);missing.stop();}
 SDL_FreeSurface(ref);SDL_DestroyWindow(w);SDL_Quit();
 unlink((z0+"/type").c_str());unlink((z0+"/temp").c_str());unlink((z1+"/type").c_str());unlink((z1+"/temp").c_str());unlink(proc.c_str());rmdir(z0.c_str());rmdir(z1.c_str());rmdir(root.c_str());
 puts("HUD sampling, idle percentages, CPU thermal selection, invalid temp, viewport, idle pulse, missing files and 100 restarts OK");
}
