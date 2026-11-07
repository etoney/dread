// Rusty Chains at slow/medium/frantic rattle, then the 4 sting types (with reverb).
#include "DreadEngine.h"
#include <cstdio>
const int SR=48000,B=256; std::vector<float> OL,OR; bool bad=false; float peak=0;
void run(dread::Engine&e,float sec){ std::vector<float> l(B),r(B); for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,90,-1);
  for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; peak=std::max(peak,std::abs(l[i])); OL.push_back(l[i]);OR.push_back(r[i]);} } }
int main(int argc,char**argv){
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params;
    p.droneLevel=p.metalLevel=p.airLevel=0; p.chainLevel=1; p.chainRust=0.6f; p.dread=0.2f; p.attack=0.05f; p.sustain=1; p.release=1.5f;
    p.cutoff=14000; p.envAmount=0; p.reverbMix=0.45f; p.reverbSize=0.65f; p.delayMix=0.08f;
    for(float rt: {0.1f,0.45f,0.9f}){ p.chainRattle=rt; e.noteOn(43,0.9f); run(e,2.6f); e.noteOff(43); run(e,0.6f); } run(e,1.0f); }
  for(int ty=0;ty<4;ty++){ dread::Engine e; e.prepare(SR,B); auto&p=e.params; p.droneLevel=0.35f; p.metalLevel=0; p.dread=0.4f; p.stingType=ty;
    p.attack=0.5f; e.noteOn(38,0.6f); run(e,0.8f); e.noteOn(90,1.0f); run(e,2.6f); e.allNotesOff(); run(e,0.8f); }
  printf("seconds=%.1f peak=%.3f finite=%s\n",OL.size()/(float)SR,peak,bad?"NO":"yes");
  FILE*f=fopen(argc>1?argv[1]:"chains_stings.wav","wb");uint32_t n=OL.size(),db=n*4,riff=36+db,fl=16,rate=SR,bps=SR*4;uint16_t fm=1,ch=2,al=4,bits=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
  fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
  for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(OL[i],-1,1)*32767),(int16_t)(dread::clampf(OR[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f); return bad; }
