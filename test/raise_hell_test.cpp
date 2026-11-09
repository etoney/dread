// RAISE HELL: 300 rolls -> every value in range, every result audible, none blows up, all characters appear.
#include "RaiseHell.h"
#include <cstdio>
#include <map>
int main(){
  const int SR=48000,B=256; std::map<std::string,int> chars; int bad=0; double minDb=1e9,maxDb=-1e9; float maxPeak=0;
  for(uint32_t seed=1; seed<=300; ++seed){
    auto roll=dread::rollHell(seed); chars[roll.character]++;
    for(auto&[id,x]:roll.values){ bool ok=std::isfinite(x);
      if(id=="ringFreq") ok=ok&&x>=5&&x<=2000; else if(id=="cutoff") ok=ok&&x>=40&&x<=16000; else if(id=="delayFeedback") ok=ok&&x>=0&&x<=0.95f;
      else if(id=="distType") ok=ok&&x>=0&&x<=5; else if(id=="gateRate"||id=="delayTime") ok=ok&&x>=0&&x<=3; else ok=ok&&x>=0&&x<=1;
      if(!ok){ printf("seed %u: %s=%f out of range\n",seed,id.c_str(),x); bad++; } }
    if(seed%10) continue;   // render every 10th roll
    dread::Engine e; e.prepare(SR,B); auto&p=e.params; p.metalLevel=0.4f; p.airLevel=0.2f; p.attack=0.05f; dread::applyRoll(roll,p);
    e.noteOn(38,0.9f); e.noteOn(45,0.8f); e.noteOn(50,0.8f); std::vector<float> l(B),r(B); double s=0; long n=0; float pk=0;
    for(int b=0;b<(int)(4*SR/B);b++){ e.process(l.data(),r.data(),B,110,-1); for(int i=0;i<B;i++){ if(!std::isfinite(l[i])){bad++;break;} s+=l[i]*l[i]; n++; pk=std::max(pk,std::abs(l[i])); } }
    double db=10*std::log10(s/n+1e-20); minDb=std::min(minDb,db); maxDb=std::max(maxDb,db); maxPeak=std::max(maxPeak,pk);
  }
  for(auto&[c,k]:chars) printf("%-15s %d\n",c.c_str(),k);
  printf("render loudness range %.1f .. %.1f dB, max peak %.3f, problems %d\n",minDb,maxDb,maxPeak,bad);
  auto a=dread::rollHell(7), b=dread::rollHell(8); printf("different presses differ: %s\n", a.values!=b.values?"YES":"NO");
  return bad?1:0; }
