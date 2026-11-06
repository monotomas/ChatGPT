from pathlib import Path
b=Path(__file__).parent
s=(b/'ablation_probe.cpp').read_text()
a=s.index(' const std::vector<std::vector<int>> chords')
z=s.index('\n auto det=',a)
s=s[:a]+''' const std::vector<std::vector<int>> chords{{40,52,64},{41,48,53,56,60,65},{47,54,59,63,66,71},{64,69,74,79,83,88},{45,52,57,60,64},{43,47,50,55,59,67},{50,57,62,66},{40,52},{45,64},{38,45,50,55,59,64},{28,35,40,45}};'''+s[z:]
s=s.replace('for(int cents: {-40,-20,0,20,40})for(int timbre: {0,1,2})','for(int cents: {-20,0,20})for(int timbre: {0,1})')
s=s.replace('double start=.08+j*.007;','double start=(c==7||c==8)?(j==0?.08:.28):.08+j*.007;')
s=s.replace('double f=440*std::exp2((chords[c][j]-69+cents*.01)/12.);','constexpr std::array<double,6> individual{{-7,3,-2,8,-5,1}};double f=440*std::exp2((chords[c][j]-69+(cents+individual[j])* .01)/12.);')
s=s.replace('std::sin(2*pi*f*h*a+h*.391+j*.76)','std::sin(2*pi*f*h*std::sqrt(1.+(.00002+.00002*j)*h*h)*a+h*.391+j*.76)')
(b/'counter_probe.cpp').write_text(s)
