// Twisted Symphony: strings at Twist 0.1 / 0.6 / 1.0, then brass (Funeral Brass-ish)
#include "DreadEngine.h"
#include <cstdio>
const int SR=48000,B=256; std::vector<float> OL,OR; bool bad=false; float peak=0;
void run(dread::Engine&e,float sec){ std::vector<float> l(B),r(B); for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,80,-1);
  for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; peak=std::max(peak,std::abs(l[i])); OL.push_back(l[i]);OR.push_back(r[i]);} } }
int main(int argc,char**argv){
  int chord[]={45,52,57,60,64};
  for(float tw: {0.1f,0.6f,1.0f}){ dread::Engine e; e.prepare(SR,B); auto&p=e.params;
    p.droneLevel=p.metalLevel=p.airLevel=0; p.symphonyLevel=1; p.symphonyTwist=tw; p.symphonySection=0.25f; p.dread=0.35f; p.attack=0.6f; p.sustain=1; p.release=2.5f;
    p.cutoff=9000; p.envAmount=0; p.wobble=0.25f; p.reverbMix=0.55f; p.reverbSize=0.85f; p.delayMix=0.08f;
    for(int n:chord) e.noteOn(n,0.75f); run(e,4.5f); e.allNotesOff(); run(e,1.5f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params;
    p.droneLevel=0.2f; p.droneSub=0.6f; p.metalLevel=0; p.airLevel=0; p.symphonyLevel=1; p.symphonyTwist=0.4f; p.symphonySection=0.85f; p.dread=0.3f; p.torment=0.25f;
    p.attack=0.25f; p.sustain=1; p.release=2; p.cutoff=7000; p.envAmount=0; p.reverbMix=0.5f; p.reverbSize=0.9f;
    int prog[3][3]={{41,48,53},{40,47,52},{38,45,50}}; for(auto&c:prog){ for(int n:c) e.noteOn(n,0.9f); run(e,1.8f); for(int n:c) e.noteOff(n);} run(e,2.5f); }
  printf("seconds=%.1f peak=%.3f finite=%s\n",OL.size()/(float)SR,peak,bad?"NO":"yes");
  FILE*f=fopen(argc>1?argv[1]:"symphony.wav","wb");uint32_t n=OL.size(),db=n*4,riff=36+db,fl=16,rate=SR,bps=SR*4;uint16_t fm=1,ch=2,al=4,bits=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
  fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
  for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(OL[i],-1,1)*32767),(int16_t)(dread::clampf(OR[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f); return bad; }
