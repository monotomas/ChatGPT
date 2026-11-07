#include "Core/MusicData.h"
#include <iostream>
int main(){for(int t:{50,51})for(int r=0;r<12;++r)for(int d:{0,4,7}){auto m=flysnapper::rotateMask(flysnapper::chordToneMask(t),r);auto q=flysnapper::recogniseChord(m,(r+d)%12);if(q.typeIndex!=t||q.root!=r)std::cout<<"expected="<<t<<" root="<<r<<" bassdegree="<<d<<" actual="<<q.typeIndex<<" actualroot="<<q.root<<" candidates="<<q.candidateCount<<" name="<<flysnapper::chordName(m,(r+d)%12)<<'\n';}}
