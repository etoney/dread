// Same riff through every distortion type (clean first), ~2.5 s each.
#include "DreadEngine.h"
#include <cstdio>
int main(int argc,char**argv){
  const int SR=48000,B=256; std::vector<float> OL,OR; bool bad=false;
  for(int t=-1;t<6;t++){ dread::Engine e; e.prepare(SR,B); auto&p=e.params;
    p.attack=0.005f; p.decay=0.3f; p.sustain=0.6f; p.release=0.4f; p.reverbMix=0.2f; p.delayMix=0; p.dread=0.3f;
    p.droneSub=0.5f; p.cutoff=4000; p.distType=std::max(0,t); p.distDrive= t<0?0.0f:0.7f;
    std::vector<float> l(B),r(B); int notes[]={33,33,36,34}; 
    for(int k=0;k<4;k++){ e.noteOn(notes[k],1); e.noteOn(notes[k]+7,0.8f);
      for(int b=0;b<(int)(0.55*SR/B);b++){ e.process(l.data(),r.data(),B,120,-1); for(int i=0;i<B;i++){ if(!std::isfinite(l[i]))bad=true; OL.push_back(l[i]);OR.push_back(r[i]);} }
      e.noteOff(notes[k]); e.noteOff(notes[k]+7); }
    for(int b=0;b<(int)(0.4*SR/B);b++){ e.process(l.data(),r.data(),B,120,-1); for(int i=0;i<B;i++){OL.push_back(l[i]);OR.push_back(r[i]);} } }
  FILE*f=fopen(argc>1?argv[1]:"dist.wav","wb");uint32_t n=OL.size(),db=n*4,riff=36+db,fl=16,rate=SR,bps=SR*4;uint16_t fm=1,ch=2,al=4,bits=16;
  fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&fl,4,1,f);fwrite(&fm,2,1,f);fwrite(&ch,2,1,f);
  fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&al,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&db,4,1,f);
  for(uint32_t i=0;i<n;i++){int16_t s[2]={(int16_t)(dread::clampf(OL[i],-1,1)*32767),(int16_t)(dread::clampf(OR[i],-1,1)*32767)};fwrite(s,2,2,f);}fclose(f);
  printf("finite=%s seconds=%.1f\n",bad?"NO":"yes",n/(float)SR); return bad; }
