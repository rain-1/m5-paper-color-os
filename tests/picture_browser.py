"""Host-check actual bounded sorting plus preview sampling math."""
from pathlib import Path
import subprocess
import tempfile

source=Path('src/pictures.cpp').read_text()
function='void newest'+source.split('void newest',1)[1].split('\n}\n}\nbool picturesMonths',1)[0]+'\n}'
test=r'''
#include <algorithm>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>
struct String:std::string {
    using std::string::string;
    int compareTo(const String& b)const{return compare(b);}
};
'''+function+r'''
int main(){
    std::vector<String> v;
    for(int i=0;i<250;++i){char s[20];snprintf(s,sizeof(s),"%04d",i);newest(v,s,100);assert(v.size()<=100);}
    assert(v.size()==100&&v.front()=="0249"&&v.back()=="0150");
    assert(std::is_sorted(v.begin(),v.end(),[](const String&a,const String&b){return a>b;}));
    newest(v,"0000",100);assert(v.back()=="0150");
    newest(v,"9999",100);assert(v.front()=="9999"&&v.back()=="0151");
    v.clear();newest(v,"2026-01",120);newest(v,"2025-12",120);newest(v,"2026-09",120);
    assert(v[0]=="2026-09"&&v[2]=="2025-12");
    for(unsigned y=0;y<66;++y)for(unsigned x=0;x<44;++x){
        unsigned sy=(y*600+300)/66,sx=(x*400+200)/44;
        assert(sy<600&&sx<400&&16+(sy*400+sx)/2<120016);
    }
    puts("Picture browser: bounded newest-first selection, month order and every preview sample in bounds passed.");
}
'''
with tempfile.TemporaryDirectory(prefix='paper-picture-browser-') as directory:
    executable=str(Path(directory)/'test')
    subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-x','c++','-','-o',executable],input=test,text=True,check=True)
    subprocess.run([executable],check=True)
