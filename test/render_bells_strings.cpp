// Hell's Bells at Sinister 0 / 50 / 100%, then Twisted Strings at Sinister 0 / 50 / 100%.
#include "DreadEngine.h"
#include <cstdio>
const int SR=48000,B=256; std::vector<float> OL,OR; bool bad=false; float peak=0;
void run(dread::Engine&e,float sec){ std::vector<float> l(B),r(B); for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,72,-1);
  for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; peak=std::max(peak,std::abs(l[i])); OL.push_back(l[i]);OR.push_back(r[i]);} } }
void base(dread::Params&p){ p.droneLevel=p.metalLevel=p.airLevel=0; p.dread=0; p.torment=0; p.wobble=0; p.attack=0.002f; p.sustain=1; p.release=6; p.cutoff=16000; p.envAmount=0; p.reverbMix=0.4f; p.reverbSize=0.85f; p.delayMix=0; }
int main(int argc,char**argv){
  for(float s: {0.0f,0.5f,1.0f}){ dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.bellLevel=1; p.bellSinister=s; if(s>0.9f){ p.dread=0.3f; p.reverbMix=0.55f; }
    e.noteOn(48,0.9f); run(e,1.6f); e.noteOff(48); e.noteOn(48,0.85f); run(e,1.6f); e.noteOff(48); e.noteOn(43,0.95f); run(e,2.6f); e.allNotesOff(); run(e,1.5f); }
  for(float s: {0.0f,0.5f,1.0f}){ dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.symphonyLevel=1; p.symphonyTwist=s; p.attack=0.35f; p.release=1.8f; p.reverbMix=0.45f;
    int prog[3][4]={{45,57,60,64},{43,55,59,62},{41,53,57,60}};
    for(auto&c:prog){ for(int n:c) e.noteOn(n,0.8f); run(e,2.4f); for(int n:c) e.noteOff(n); } run(e,1.6f); }
  printf("seconds=%.1f peak=%.3f finite=%s\n",OL.size()/(float)SR,peak,bad?"NO":"yes");
  FILE*f=fopen(argc>1?argv[1]:"bells_strings.wav","wb");uint32_t n=OL.size(),db=n*4,riff=36+db,fl=16,rate=SR,bps=SR*4;uint16_t fm=1,ch=2,al=4,bits=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
  fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
  for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(OL[i],-1,1)*32767),(int16_t)(dread::clampf(OR[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f); return bad; }
