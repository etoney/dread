// Renders the Broken Piano, Demonic Whispers, Screams and Haunted Parlor presets back to back.
#include "DreadEngine.h"
#include <cstdio>
static void wav(const char* path,const std::vector<float>&L,const std::vector<float>&R,int sr){
 FILE*f=fopen(path,"wb");uint32_t n=L.size(),db=n*4,riff=36+db,fl=16,rate=sr,bps=sr*4;uint16_t fm=1,ch=2,al=4,bits=16;
 fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
 fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
 for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(L[i],-1,1)*32767),(int16_t)(dread::clampf(R[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f);}
const int SR=48000,B=256; std::vector<float> OL,OR; float peak=0; bool bad=false;
void run(dread::Engine&e,float sec){ std::vector<float> l(B),r(B); for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,80,-1);
  for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; peak=std::max(peak,std::abs(l[i])); OL.push_back(l[i]);OR.push_back(r[i]);} } }
int main(int argc,char**argv){
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; // Broken Piano
    p.droneLevel=p.metalLevel=p.airLevel=0; p.pianoLevel=1; p.pianoBreak=0.65f; p.dread=0.25f; p.attack=0.002f; p.sustain=1; p.release=1.2f;
    p.cutoff=9000; p.envAmount=0; p.wobble=0.3f; p.reverbMix=0.4f; p.reverbSize=0.6f; p.delayMix=0.12f;
    int mel[]={69,72,76,75,71,68,64,65}; float dur[]={0.6f,0.6f,0.9f,0.5f,0.5f,0.9f,0.6f,1.2f};
    for(int k=0;k<8;k++){ e.noteOn(mel[k],0.7f); if(k%3==0) e.noteOn(45,0.6f); run(e,dur[k]); e.noteOff(mel[k]); e.noteOff(45);} run(e,2.0f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; // Demonic Whispers
    p.droneLevel=0; p.metalLevel=0.15f; p.airLevel=0; p.whisperLevel=1; p.whisperDemon=0.75f; p.airMotion=0.6f; p.dread=0.5f;
    p.attack=0.3f; p.sustain=1; p.release=2.5f; p.cutoff=12000; p.envAmount=0; p.reverbMix=0.45f; p.delayMix=0.25f; p.wobble=0.2f;
    e.noteOn(40,0.9f); e.noteOn(52,0.7f); run(e,6.0f); e.allNotesOff(); run(e,2.0f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; // Screams
    p.droneLevel=p.metalLevel=p.airLevel=0; p.bellLevel=1; p.bellSinister=0.5f; p.dread=0.2f; p.torment=0.2f; p.attack=0.04f; p.sustain=1;
    p.release=0.6f; p.cutoff=14000; p.envAmount=0; p.reverbMix=0.5f; p.reverbSize=0.8f; p.delayMix=0.1f;
    e.noteOn(72,1.0f); run(e,1.6f); e.noteOff(72); run(e,0.8f);
    e.noteOn(67,0.9f); e.noteOn(74,0.9f); e.noteOn(79,1.0f); run(e,2.2f); e.allNotesOff(); run(e,2.5f); }
  { dread::Engine e; e.prepare(SR,B); auto&p=e.params; // Haunted Parlor
    p.droneLevel=0.15f; p.metalLevel=0; p.airLevel=0; p.pianoLevel=0.9f; p.pianoBreak=0.5f; p.whisperLevel=0.5f; p.whisperDemon=0.5f;
    p.dread=0.4f; p.attack=0.002f; p.sustain=1; p.release=2; p.cutoff=8000; p.envAmount=0; p.wobble=0.35f; p.reverbMix=0.5f;
    int ch[3][3]={{45,52,60},{44,51,59},{41,48,56}}; for(auto&c:ch){ for(int n:c) e.noteOn(n,0.65f); run(e,2.0f); for(int n:c) e.noteOff(n);} run(e,3.0f); }
  printf("seconds=%.1f peak=%.3f finite=%s\n",OL.size()/(float)SR,peak,bad?"NO":"yes"); wav(argc>1?argv[1]:"new_sounds.wav",OL,OR,SR); return bad;
}
