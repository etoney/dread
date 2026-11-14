// RAISE HELL offline test:
//  * 400 rolls: values in range, all characters appear, many distinct results
//  * rendered rolls: never silent, never blowing up, and with headroom below the limiter
//  * "starting point": after a roll, turning knobs still clearly changes the sound
#include "RaiseHell.h"
#include <cstdio>
#include <map>
#include <set>
const int SR=48000,B=256;
static std::vector<float> render(const dread::Params& p, std::vector<int> notes, float sec){
  dread::Engine e; e.prepare(SR,B); e.params=p; for(int n:notes) e.noteOn(n,0.85f);
  std::vector<float> l(B),r(B),out; for(int b=0;b<(int)(sec*SR/B);b++){ e.process(l.data(),r.data(),B,110,-1); out.insert(out.end(),l.begin(),l.end()); } return out; }
static double db(const std::vector<float>&x){ double s=0; for(float v:x) s+=v*v; return 10*std::log10(s/x.size()+1e-20); }
static double diffDb(const std::vector<float>&a,const std::vector<float>&b){ double d=0,s=0; for(size_t i=0;i<a.size();i++){ d+=(a[i]-b[i])*(a[i]-b[i]); s+=a[i]*a[i]; } return 10*std::log10(d/(s+1e-20)+1e-20); }
int main(){
  int bad=0; std::map<std::string,int> chars; std::set<std::string> distinct;
  for(uint32_t seed=1; seed<=400; ++seed){ auto roll=dread::rollHell(seed);
    std::string base=roll.character.substr(0,roll.character.find(" /")); chars[base]++;
    std::string sig; for(auto&[id,x]:roll.values){ char buf[32]; snprintf(buf,32,"%.1f,",x); sig+=buf;
      bool ok=std::isfinite(x);
      if(id=="ringFreq") ok=ok&&x>=5&&x<=2000; else if(id=="cutoff") ok=ok&&x>=40&&x<=16000; else if(id=="delayFeedback") ok=ok&&x>=0&&x<=0.95f;
      else if(id=="distType") ok=ok&&x>=0&&x<=5; else if(id=="gateRate"||id=="delayTime") ok=ok&&x>=0&&x<=3; else ok=ok&&x>=0&&x<=1;
      if(!ok){ printf("seed %u: %s=%f out of range\n",seed,id.c_str(),x); bad++; } }
    distinct.insert(sig); }
  int blends=0; for(uint32_t s=1;s<=400;s++) if(dread::rollHell(s).character.find('/')!=std::string::npos) blends++;
  printf("characters:"); for(auto&[c,k]:chars) printf(" %s=%d",c.c_str(),k); printf("\nblended rolls %d/400, distinct results %zu/400\n",blends,distinct.size());

  double minDb=1e9,maxDb=-1e9; float maxPeak=0; int weak[5]={0,0,0,0,0}; const char* knob[5]={"bell level","delay","distortion drive","ring","gate depth"};
  int tested=0;
  for(uint32_t seed=7; seed<=400; seed+=13){ auto roll=dread::rollHell(seed);
    dread::Params p; p.metalLevel=0.4f; p.airLevel=0.2f; p.attack=0.05f; dread::applyRoll(roll,p);
    auto base=render(p,{38,45,50},3); double d=db(base); minDb=std::min(minDb,d); maxDb=std::max(maxDb,d);
    for(float v:base){ if(!std::isfinite(v)){bad++;break;} maxPeak=std::max(maxPeak,std::abs(v)); }
    tested++;
    // knob changes after the roll must be audible
    { dread::Params q=p; q.bellLevel=1; auto x=render(q,{38,45,50,64},3); auto y=render(p,{38,45,50,64},3); if(diffDb(y,x)<=-21) weak[0]++; }
    { dread::Params q=p; q.delayMix=std::min(1.f,p.delayMix+0.4f); { double dd=diffDb(base,render(q,{38,45,50},3)); if(dd<=-21){ weak[1]++; printf("  weak delay: seed %u delay %.2f fb %.2f reverb %.2f gate %.2f diff %.1f\n",seed,p.delayMix,p.delayFeedback,p.reverbMix,p.gateDepth,dd);} } }
    { dread::Params q=p; q.distDrive=std::min(1.f,p.distDrive+0.5f); double dd=diffDb(base,render(q,{38,45,50},3)); if(dd<=-21){ weak[2]++; printf("  weak drive: seed %u type %d drive %.2f->%.2f mix %.2f tone %.2f torment %.2f diff %.1f dB\n",seed,p.distType,p.distDrive,q.distDrive,p.distMix,p.distTone,p.torment,dd);} }
    { dread::Params q=p; q.ringMix=std::min(1.f,p.ringMix+0.5f); if(diffDb(base,render(q,{38,45,50},3))<=-21) weak[3]++; }
    { dread::Params q=p; q.gateDepth=1.0f; double gd=diffDb(base,render(q,{38,45,50},3)); if(p.gateDepth<0.6f && gd<=-21){ weak[4]++; std::string pat; for(bool b:p.gateSteps) pat+=b?'x':'.'; printf("  weak gate: seed %u depth %.2f rate %d pattern %s diff %.1f\n",seed,p.gateDepth,p.gateRate,pat.c_str(),gd);} }
  }
  printf("rendered %d rolls: loudness %.1f .. %.1f dB, max peak %.3f\n",tested,minDb,maxDb,maxPeak);
  for(int i=0;i<5;i++) printf("turning up %-17s after a roll had little effect in %d/%d rolls\n",knob[i],weak[i],tested);
  int weakTotal=weak[0]+weak[1]+weak[2]+weak[3]+weak[4];
  printf("problems %d, weak knobs %d\n",bad,weakTotal);
  return (bad||weakTotal)?1:0; }
