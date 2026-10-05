#pragma once
constexpr float stableEcho(const float *samples,int size,int latest) {
  if(size<3 || samples[latest]<2 || samples[latest]>300)return -1;
  float sorted[5]={};int count=0;
  for(int i=0;i<size;i++)if(samples[i]>=2 && samples[i]<=300)sorted[count++]=samples[i];
  if(count<3)return -1;
  for(int i=0;i<count;i++)for(int j=i+1;j<count;j++)if(sorted[j]<sorted[i]){float t=sorted[i];sorted[i]=sorted[j];sorted[j]=t;}
  float center=sorted[count/2];int close=0;
  for(int i=0;i<count;i++){float delta=sorted[i]-center;if(delta>=-8 && delta<=8)close++;}
  return close>=3 ? sorted[0] : -1;
}
