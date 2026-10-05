#pragma once
constexpr bool validEcho(float value){return value>=2 && value<=300;}
constexpr bool confirmedNearEcho(const float *samples,int size,int latest,float threshold) {
  if(size<2)return false;
  float a=samples[latest],b=samples[latest ? latest-1 : size-1];
  float delta=a-b;
  return validEcho(a) && validEcho(b) && a<threshold && b<threshold && delta>=-8 && delta<=8;
}
constexpr float stableEcho(const float *samples,int size,int latest) {
  if(size<3 || samples[latest]<2 || samples[latest]>300)return -1;
  float sorted[5]={};int count=0;
  for(int i=0;i<size;i++)if(samples[i]>=2 && samples[i]<=300)sorted[count++]=samples[i];
  if(count<3)return -1;
  for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(sorted[j]<sorted[i]){float t=sorted[i];sorted[i]=sorted[j];sorted[j]=t;}
  float center=sorted[count/2];float cluster[5]={};int close=0;
  for(int i=0;i<count;i++){float delta=sorted[i]-center;if(delta>=-8 && delta<=8)cluster[close++]=sorted[i];}
  // Two consecutive matching closer echoes beat the older far cluster. One spike only brakes provisionally.
  if(samples[latest]<center-8 && confirmedNearEcho(samples,size,latest,center))return samples[latest]<samples[latest ? latest-1 : size-1] ? samples[latest] : samples[latest ? latest-1 : size-1];
  return close>=3 ? cluster[close/2] : -1;
}
