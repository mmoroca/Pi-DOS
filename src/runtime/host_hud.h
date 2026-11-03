/* Pi-DOS optional LCD host-CPU/temperature overlay. GPL-2.0-or-later.
 * Linux files are read only by the monitor thread; window drawing stays on
 * the SDL owner. Data are snapshots, not emulated-CPU utilisation.
 */
#ifndef PIDOS_HUD_H
#define PIDOS_HUD_H
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

class PidosHud {
public:
    enum { MAX_CORES=8 };
    struct Core { bool present,valid; int percent; uint64_t total,idle; };
    struct Snapshot { Core cores[MAX_CORES]; bool extra, temp_valid; int temp_c; uint32_t sequence; };
    explicit PidosHud(const char *proc="/proc/stat",const char *thermal="/sys/class/thermal") :
        proc_(proc),thermal_(thermal),mutex_(nullptr),cond_(nullptr),thread_(nullptr),stop_(false),
        image_(nullptr),image_seq_(0),image_scale_(0),last_present_(0) {
        memset(&snapshot_,0,sizeof(snapshot_));
    }
    ~PidosHud(){ stop(); }
    PidosHud(const PidosHud&)=delete; PidosHud& operator=(const PidosHud&)=delete;
    bool start() {
        if (mutex_ || thread_) return false;
        mutex_=SDL_CreateMutex(); cond_=SDL_CreateCond();
        if (!mutex_ || !cond_) { stop(); return false; }
        stop_=false; thread_=SDL_CreateThread(entry,"PiDosHud",this);
        if (!thread_) { stop(); return false; }
        return true;
    }
    void stop() {
        if (thread_) {
            SDL_LockMutex(mutex_); stop_=true; SDL_CondSignal(cond_); SDL_UnlockMutex(mutex_);
            SDL_WaitThread(thread_,nullptr); thread_=nullptr;
        }
        if (cond_) SDL_DestroyCond(cond_);
        if (mutex_) SDL_DestroyMutex(mutex_);
        cond_=nullptr; mutex_=nullptr; stop_=false;
        SDL_FreeSurface(image_); image_=nullptr; image_seq_=0; image_scale_=0; last_present_=0;
    }
    /* Call from SDL owner only, after the frame is blitted and before update. */
    bool draw(SDL_Surface *screen) {
        if (!thread_ || !screen || !screen->format || screen->w<80 || screen->h<40) return false;
        Snapshot snap;
        SDL_LockMutex(mutex_); snap=snapshot_; SDL_UnlockMutex(mutex_);
        const int scale=screen->w>=1280 ? 2 : 1;
        if (!image_ || image_seq_!=snap.sequence || image_scale_!=scale) {
            if (!render(snap,scale)) return false;
            image_seq_=snap.sequence; image_scale_=scale;
        }
        if (image_->w+3>screen->w || image_->h+3>screen->h) return false;
        SDL_Rect dst={screen->w-image_->w-3,3,image_->w,image_->h};
        if (SDL_BlitSurface(image_,nullptr,screen,&dst)!=0) return false;
        last_present_=SDL_GetTicks();
        return true;
    }
    bool pulse_due() const { return thread_ && (Uint32)(SDL_GetTicks()-last_present_)>=1000u; }
private:
    const char *proc_, *thermal_;
    SDL_mutex *mutex_; SDL_cond *cond_; SDL_Thread *thread_; bool stop_;
    Snapshot snapshot_;
    SDL_Surface *image_; uint32_t image_seq_; int image_scale_; Uint32 last_present_;
    static int entry(void *p) { return ((PidosHud*)p)->run(); }
    static void read_cpus(const char *path,Snapshot& now) {
        FILE *f=fopen(path,"r"); if(!f) return;
        char line[512];
        while(fgets(line,sizeof(line),f)) {
            if(strncmp(line,"cpu",3)!=0 || !isdigit((unsigned char)line[3])) continue;
            int id=-1; unsigned long long u=0,n=0,s=0,i=0,w=0,irq=0,soft=0,steal=0;
            int k=sscanf(line,"cpu%d %llu %llu %llu %llu %llu %llu %llu %llu",&id,&u,&n,&s,&i,&w,&irq,&soft,&steal);
            if(k<5 || id<0) continue;
            if(id>=MAX_CORES){now.extra=true; continue;}
            Core &c=now.cores[id]; c.present=true;
            c.idle=(uint64_t)i+w; c.total=(uint64_t)u+n+s+i+w+irq+soft+steal;
        }
        fclose(f);
    }
    static bool read_int(const char *path,int &value) {
        FILE *f=fopen(path,"r"); if(!f) return false;
        long v=0; int ok=fscanf(f,"%ld",&v); fclose(f);
        if(ok!=1 || v < -40000 || v>150000) return false;
        value=(int)(v/1000); return true;
    }
    bool find_temp(char *dest,size_t size) const {
        char path[512], line[128];
        for(int pass=0;pass<2;pass++) for(int i=0;i<32;i++) {
            int n=snprintf(path,sizeof(path),"%s/thermal_zone%d/type",thermal_,i);
            if(n<0 || (size_t)n>=sizeof(path)) continue;
            FILE *f=fopen(path,"r"); if(!f) continue;
            bool got=fgets(line,sizeof(line),f)!=nullptr; fclose(f); if(!got) continue;
            for(char *p=line;*p;p++) *p=(char)tolower((unsigned char)*p);
            bool match=pass==0 ? strstr(line,"cpu")!=nullptr : strstr(line,"soc")!=nullptr;
            if(match) {n=snprintf(dest,size,"%s/thermal_zone%d/temp",thermal_,i);return n>=0&&(size_t)n<size;}
        }
        return false;
    }
    int run() {
        Snapshot prev={}; char temp_path[512]={}; unsigned tick=0;
        for(;;) {
            Snapshot next={}; read_cpus(proc_,next);
            for(int i=0;i<MAX_CORES;i++) {
                Core &c=next.cores[i]; const Core &p=prev.cores[i];
                if(c.present && p.present && c.total>p.total && c.idle>=p.idle) {
                    uint64_t total=c.total-p.total,idle=c.idle-p.idle;
                    if(idle<=total){c.valid=true;c.percent=(int)(((total-idle)*100ull+total/2)/total);}
                }
            }
            if(!temp_path[0] && tick%5u==0) find_temp(temp_path,sizeof(temp_path));
            if(temp_path[0]) {next.temp_valid=read_int(temp_path,next.temp_c); if(!next.temp_valid) temp_path[0]=0;}
            prev=next; ++tick;
            SDL_LockMutex(mutex_);
            next.sequence=snapshot_.sequence+1u; snapshot_=next;
            if(!stop_) SDL_CondWaitTimeout(cond_,mutex_,1000);
            const bool done=stop_; SDL_UnlockMutex(mutex_);
            if(done) return 0;
        }
    }
    static const uint8_t *glyph(char c) {
        /* Five low bits of each row, 5x7 pixel glyphs. */
        static const uint8_t digits[10][7]={
          {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
          {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
          {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
          {14,17,17,15,1,1,14}};
        static const uint8_t C[7]={14,17,16,16,16,17,14},T[7]={31,4,4,4,4,4,4};
        static const uint8_t colon[7]={0,4,4,0,4,4,0},dash[7]={0,0,0,31,0,0,0};
        static const uint8_t percent[7]={17,2,4,8,17,0,0},plus[7]={0,4,4,31,4,4,0};
        static const uint8_t blank[7]={0,0,0,0,0,0,0};
        if(c>='0'&&c<='9') return digits[c-'0'];
        switch(c){case 'C':return C; case 'T':return T; case ':':return colon;
            case '-':return dash; case '%':return percent; case '+':return plus; default:return blank;}
    }
    bool render(const Snapshot &s,int scale) {
        char lines[6][40]={}; int n=0,columns=0;
        for(int id=0;id<MAX_CORES;id++) if(s.cores[id].present) {
            char item[16];
            if(s.cores[id].valid) snprintf(item,sizeof(item),"C%d:%3d%%",id,s.cores[id].percent);
            else snprintf(item,sizeof(item),"C%d: --%%",id);
            if(columns==0){snprintf(lines[n],sizeof(lines[n]),"%s",item);columns=1;}
            else {size_t used=strlen(lines[n]); snprintf(lines[n]+used,sizeof(lines[n])-used," %s",item);n++;columns=0;}
        }
        if(columns) n++;
        if(n==0) snprintf(lines[n++],sizeof(lines[0]),"C: --%%");
        if(s.extra && n<5) snprintf(lines[n++],sizeof(lines[0]),"+");
        if(s.temp_valid) snprintf(lines[n++],sizeof(lines[0]),"T:%dC",s.temp_c);
        else snprintf(lines[n++],sizeof(lines[0]),"T:--C");
        size_t chars=0; for(int i=0;i<n;i++) {size_t len=strlen(lines[i]);if(len>chars) chars=len;}
        int w=(int)((chars*6+4)*(size_t)scale),h=(n*8+4)*scale;
        if(!image_ || image_->w!=w || image_->h!=h) {
            SDL_FreeSurface(image_);image_=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGB888);
            if(!image_) return false;
            if(SDL_SetSurfaceBlendMode(image_,SDL_BLENDMODE_NONE)!=0) {
                SDL_FreeSurface(image_); image_=nullptr; return false;
            }
        }
        if(SDL_FillRect(image_,nullptr,SDL_MapRGB(image_->format,0,0,0))!=0) return false;
        if(SDL_MUSTLOCK(image_) && SDL_LockSurface(image_)!=0) return false;
        const uint32_t white=SDL_MapRGB(image_->format,255,255,255);
        for(int y=0;y<n;y++) for(size_t x=0;lines[y][x];x++) {
            const uint8_t *g=glyph(lines[y][x]);
            for(int gy=0;gy<7;gy++) for(int gx=0;gx<5;gx++) if(g[gy]&(1u<<(4-gx)))
                for(int sy=0;sy<scale;sy++) for(int sx=0;sx<scale;sx++) {
                    int px=(2+(int)x*6+gx)*scale+sx,py=(2+y*8+gy)*scale+sy;
                    uint32_t *row=(uint32_t*)((uint8_t*)image_->pixels+py*image_->pitch);row[px]=white;
                }
        }
        if(SDL_MUSTLOCK(image_)) SDL_UnlockSurface(image_);
        return true;
    }
};
#endif
