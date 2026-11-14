// v1.7 sounds back to back: Broken Piano (Break 30% then 80%), Screams, Rusty Chains (Rust 80%),
// Air (wind), Demented Organ, Heartbeat, Panic Attack.
#include "DreadEngine.h"
#include <cstdio>
const int SR=48000,B=256; std::vector<float> OL,OR; bool bad=false; float peak=0;
void run(dread::Engine&e,float sec){ std::vector<float> l(B),r(B); for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,80,-1);
  for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; peak=std::max(peak,std::abs(l[i])); OL.push_back(l[i]);OR.push_back(r[i]);} } }
void base(dread::Params&p){ p.droneLevel=p.metalLevel=p.airLevel=0; p.dread=0.2f; p.torment=0; p.wobble=0; p.attack=0.003f; p.sustain=1; p.release=1.0f; p.cutoff=14000; p.envAmount=0; p.reverbMix=0.35f; p.reverbSize=0.6f; p.delayMix=0; }
int main(int argc,char**argv){
  for(float brk: {0.3f,0.8f}){ dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.pianoLevel=1; p.pianoBreak=brk; p.wobble=0.2f;
    int mel[]={57,60,64,63,59,56,52,53}; float d[]={0.5f,0.5f,0.8f,0.45f,0.45f,0.8f,0.5f,1.1f};
    for(int k=0;k<8;k++){ e.noteOn(mel[k],0.75f); if(k%4==0) e.noteOn(33,0.6f); run(e,d[k]); e.noteOff(mel[k]); e.noteOff(33);} run(e,1.2f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.bellLevel=1; p.bellSinister=0.6f; p.attack=0.01f; p.release=0.5f; p.reverbMix=0.45f;
    e.noteOn(67,1.0f); run(e,2.2f); e.noteOff(67); run(e,0.7f); e.noteOn(62,0.9f); e.noteOn(69,1.0f); run(e,2.0f); e.allNotesOff(); run(e,1.2f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.chainLevel=1; p.chainRattle=0.45f; p.chainRust=0.8f; e.noteOn(43,0.9f); run(e,3.5f); e.allNotesOff(); run(e,0.8f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.airLevel=1; p.airMotion=0.5f; p.attack=0.8f; p.reverbMix=0.2f; e.noteOn(55,0.8f); run(e,4.5f); e.allNotesOff(); run(e,1.0f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.organLevel=1; p.organDement=0.6f; p.organStops=0.75f; p.dread=0.25f; p.attack=0.03f; p.release=1.6f; p.wobble=0.15f; p.reverbMix=0.6f; p.reverbSize=1;
    int prog[3][4]={{38,50,53,57},{37,49,52,56},{36,48,51,55}}; for(auto&c:prog){ for(int n:c) e.noteOn(n,0.85f); run(e,2.0f); for(int n:c) e.noteOff(n);} run(e,2.0f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.droneLevel=0.25f; p.droneSub=0.4f; p.heartLevel=1; p.heartRate=62; p.heartPanic=0.1f; p.attack=0.01f; p.cutoff=3000; p.reverbMix=0.2f;
    e.noteOn(36,0.9f); run(e,5.0f); e.allNotesOff(); run(e,0.8f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; base(p); p.droneLevel=0.3f; p.droneSub=0.5f; p.metalLevel=0.2f; p.airLevel=0.3f; p.heartLevel=1; p.heartRate=88; p.heartPanic=0.85f; p.dread=0.6f; p.torment=0.25f; p.cutoff=4000; p.wobble=0.35f;
    e.noteOn(38,1.0f); run(e,7.0f); e.allNotesOff(); run(e,1.2f); }
  printf("seconds=%.1f peak=%.3f finite=%s\n",OL.size()/(float)SR,peak,bad?"NO":"yes");
  FILE*f=fopen(argc>1?argv[1]:"v17.wav","wb");uint32_t n=OL.size(),db=n*4,riff=36+db,fl=16,rate=SR,bps=SR*4;uint16_t fm=1,ch=2,al=4,bits=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
  fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
  for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(OL[i],-1,1)*32767),(int16_t)(dread::clampf(OR[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f); return bad; }
