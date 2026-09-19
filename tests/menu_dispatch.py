"""Compile the actual reader dispatcher against a tiny host state/scheduler fake."""
from pathlib import Path
import subprocess
import tempfile

reader = Path('src/reader.cpp').read_text()
action = Path('include/reader.h').read_text().split('enum class Action',1)[1].split(';',1)[0]
request = reader.split('bool request(Action action',1)[1].split('\nbool render(',1)[0]
source = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include "menu_input.h"
enum class Action''' + action + r''';
enum class View {Menu,Library,Reading,Fonts,Sampler,Info,FontMenu,Bookmarks,ReaderMenu,PictureMonths,Pictures,Picture};
bool listView(View v){return v==View::Menu||v==View::Library||v==View::Fonts||v==View::FontMenu||v==View::Bookmarks||v==View::ReaderMenu||v==View::PictureMonths||v==View::Pictures;}
struct Snapshot {bool busy=false,active=true;View view=View::Menu;unsigned selection=0,count=5;} snapshot;
struct Command {Action action;char id[9];uint32_t value;bool wasActive;} pending;
int stateMutex=1,portMAX_DELAY=0,scheduled=0;
void xSemaphoreTake(int,int){} void xSemaphoreGive(int){}
void schedule(){++scheduled;} void (*scheduleScreen)()=schedule;
namespace RefreshTest {bool fault=false;bool faulted(){return fault;}}
namespace StatusLight {void menuActivity(){}}
namespace Voice {bool recording=false;bool active(){return recording;}}
bool request(Action action''' + request + r'''
int main(){
  for(auto v:{View::Menu,View::Library,View::Fonts,View::FontMenu,View::Bookmarks,View::ReaderMenu,View::PictureMonths,View::Pictures}){
    snapshot.view=v;snapshot.selection=0;scheduled=0;snapshot.busy=false;
    for(int i=0;i<100;++i){assert(request(Action::Next,"",0));assert(!snapshot.busy);assert(scheduled==0);}
    assert(snapshot.selection==0);assert(request(Action::Previous,"",0));assert(snapshot.selection==4);
    assert(request(Action::Select,"",0));assert(scheduled==1&&snapshot.busy&&pending.value==4);
    assert(!request(Action::Next,"",0));assert(scheduled==1);
  }
  snapshot.busy=false;snapshot.view=View::Reading;scheduled=0;
  assert(request(Action::Next,"",0));assert(scheduled==1); // real page turns still render
  snapshot.busy=false;snapshot.view=View::Menu;snapshot.count=0;scheduled=0;
  assert(request(Action::Next,"",0));assert(!scheduled);
  assert(request(Action::Back,"",0));assert(scheduled==1);
  snapshot.busy=false;Voice::recording=true;assert(!request(Action::Next,"",0));Voice::recording=false;
  snapshot.busy=false;RefreshTest::fault=true;assert(!request(Action::Next,"",0));
  puts("Actual reader dispatcher: menu moves schedule zero screen updates; choose/back/page turns schedule; busy/fault guards passed.");
}
'''
source = '#include <initializer_list>\n' + source
with tempfile.TemporaryDirectory(prefix='paper-dispatch-') as directory:
    executable = str(Path(directory)/'test')
    subprocess.run(['g++','-std=c++11','-fsanitize=address,undefined','-Iinclude','-x','c++','-','-o',executable],input=source,text=True,check=True)
    subprocess.run([executable],check=True)
