#include "reader.h"
#include "book_layout.h"
#include "book_storage.h"
#include "pictures.h"
#include "feedback.h"
#include "version.h"
#include "reader_page.h"
#include "refresh_test.h"
#include "menu_input.h"
#include <SD.h>
#include <Preferences.h>
#include <WiFi.h>
#include <algorithm>

namespace Reader {
namespace {
enum class View { Menu, Library, Reading, Fonts, Sampler, Info, FontMenu, Bookmarks };
bool listView(View v){return v==View::Menu||v==View::Library||v==View::Fonts||v==View::FontMenu||v==View::Bookmarks;}
struct Book { String id,title; uint32_t size; };
struct Progress { uint32_t magic=0x42524b31, offset=0, bookmark=UINT32_MAX; };
struct Command { Action action; char id[9]; uint32_t value; bool wasActive; };
struct Snapshot {
    bool active=false,busy=false;
    char id[9]{},title[65]{},message[128]{};
    uint32_t page=0,pages=0,offset=0,bookmark=UINT32_MAX;
    uint8_t font=0;
    View view=View::Menu;
    unsigned selection=0,count=0;
};
struct FontChoice { const lgfx::IFont* font; const char* label; };
const FontChoice fontChoices[]={
    {&fonts::FreeSerif12pt7b,"Book serif / 12 pt"},
    {&fonts::FreeSerif9pt7b,"Book serif / 9 pt"},
    {&fonts::FreeSerif18pt7b,"Book serif / 18 pt"},
    {&fonts::FreeSans12pt7b,"Clean sans / 12 pt"},
    {&fonts::FreeMono12pt7b,"Typewriter / 12 pt"}
};
constexpr unsigned fontCount=sizeof(fontChoices)/sizeof(fontChoices[0]);
const char* menuItems[]={"Continue reading","Library","Fonts","Bookmarks","Device information"};
Preferences settings;
SemaphoreHandle_t stateMutex;
Snapshot snapshot;
Command pending{};
void (*scheduleScreen)()=nullptr;
bool storage=false,saveAfterDisplay=false;
View view=View::Menu;
unsigned selection=0,fontIndex=0;
unsigned listPage=0,listCount=0;
std::vector<Book> books;
char* text=nullptr;
size_t textSize=0,pageIndex=0;
String currentId,currentTitle,lastId;
Progress progress;
std::vector<uint32_t> pages;
BookLayout::Metrics metrics;
int lineHeight=30;
String message;

Snapshot state(){xSemaphoreTake(stateMutex,portMAX_DELAY);Snapshot copy=snapshot;xSemaphoreGive(stateMutex);return copy;}
void publish(){
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    strlcpy(snapshot.id,currentId.c_str(),sizeof(snapshot.id));
    strlcpy(snapshot.title,currentTitle.c_str(),sizeof(snapshot.title));
    strlcpy(snapshot.message,message.c_str(),sizeof(snapshot.message));
    snapshot.page=pages.empty()?0:pageIndex+1;snapshot.pages=pages.size();
    snapshot.offset=pages.empty()?0:pages[pageIndex];snapshot.bookmark=progress.bookmark;snapshot.font=fontIndex;
    snapshot.view=view;
    snapshot.selection=selection;snapshot.count=listView(view)?listCount:0;
    xSemaphoreGive(stateMutex);
}
String path(const String& id){return "/books/"+id+".txt";}
String escaped(const String& value){
    String out="";out.reserve(value.length()+8);
    for(size_t i=0;i<value.length();++i){char c=value[i];if(c=='"'||c=='\\')out+='\\';if(uint8_t(c)<32)out+=' ';else out+=c;}
    return out;
}
void scanLibrary(){
    books.clear();
    File directory=SD.open("/books");
    if(!directory || !directory.isDirectory())return;
    while(File file=directory.openNextFile()){
        String name=file.name();
        if(file.isDirectory()||name.length()!=12||!name.endsWith(".txt"))continue;
        String id=name.substring(0,8);
        if(!BookLayout::validId(id.c_str()))continue;
        String title=bookStorageTitle(id);
        books.push_back({id,title,uint32_t(file.size())});
        if(books.size()==64)break;
    }
    std::sort(books.begin(),books.end(),[](const Book& a,const Book& b){return a.title.compareTo(b.title)<0;});
}
bool paginate(M5Canvas& canvas){
    canvas.setFont(fontChoices[fontIndex].font);canvas.setTextSize(1);
    lineHeight=canvas.fontHeight()+4;
    metrics.rows=std::max(1,472/lineHeight);
    for(unsigned c=32;c<127;++c){char ch[]={char(c),0};metrics.widths[c]=std::max(1,int(canvas.textWidth(ch)));}
    pages.clear();uint32_t p=0;
    while(p<textSize){
        if(pages.size()>=BookLayout::maxPages){pages.clear();pageIndex=0;message="Too many pages for this layout.";return false;}
        pages.push_back(p);
        uint32_t next=BookLayout::page(text,textSize,p,metrics);
        if(next<=p){pages.clear();pageIndex=0;message="Cannot lay out this book.";return false;}
        p=next;
        if((pages.size()&63)==0)delay(1);
    }
    return !pages.empty();
}
bool loadBook(const String& id,M5Canvas& canvas){
    if(!BookLayout::validId(id.c_str())){message="Choose a book first.";return false;}
    File file=SD.open(path(id));
    if(!file||file.size()==0||file.size()>BookLayout::maxBytes){message="Book missing, empty or too large.";return false;}
    size_t size=file.size();char* next=static_cast<char*>(ps_malloc(size+1));
    if(!next){message="Not enough memory to open this book.";return false;}
    bool ok=file.read(reinterpret_cast<uint8_t*>(next),size)==size&&BookLayout::validText(next,size);
    file.close();next[size]=0;
    if(!ok){free(next);message="Unsupported or damaged text. Import via /books.";return false;}
    free(text);text=next;textSize=size;currentId=id;currentTitle=id;
    currentTitle=bookStorageTitle(id);
    progress=Progress{};
    String key="b"+id;
    if(storage && settings.getBytesLength(key.c_str())==sizeof(Progress))settings.getBytes(key.c_str(),&progress,sizeof(progress));
    if(progress.magic!=0x42524b31 || progress.offset>=textSize)progress=Progress{};
    if(progress.bookmark>=textSize)progress.bookmark=UINT32_MAX;
    if(!paginate(canvas)){pages.clear();return false;}
    pageIndex=BookLayout::pageAt(pages,progress.offset);
    view=View::Reading;lastId=id;
    return true;
}
bool saveProgress(){
    if(!storage||currentId.isEmpty()||pages.empty())return false;
    progress.offset=pages[pageIndex];
    String key="b"+currentId;
    bool ok=settings.putBytes(key.c_str(),&progress,sizeof(progress))==sizeof(progress);
    if(ok)ok=settings.putString("last",currentId)==currentId.length();
    return ok;
}
String fit(M5Canvas& c,String s,int width){while(s.length() && c.textWidth(s.c_str())>width)s.remove(s.length()-1);return s;}
void heading(M5Canvas& c,const char* title){
    c.fillSprite(WHITE);c.setTextColor(BLACK,WHITE);c.setTextSize(1);c.setTextDatum(top_left);
    c.setFont(&fonts::FreeSerif18pt7b);c.drawString(title,24,20);
    c.drawFastHLine(24,70,352,BLACK);
}
void footer(M5Canvas& c,const String& text){c.setFont(&fonts::Font2);c.drawFastHLine(24,551,352,BLACK);c.drawString(text,24,564);}
void drawList(M5Canvas& c,const char* title,const std::vector<String>& rows){
    heading(c,title);c.setFont(&fonts::FreeSans12pt7b);
    unsigned start=rows.size()>5?listPage*4:0;
    unsigned visible=rows.size()>5?std::min(size_t(4),rows.size()-start):rows.size();
    listCount=MenuInput::count(rows.size(),listPage);
    if(rows.empty())c.drawString("No books yet. Open /books",24,112);
    for(unsigned i=0;i<listCount;++i){
        int y=90+i*83;auto bg=MenuInput::colours[i];
        c.fillRoundRect(18,y,364,73,5,bg);c.drawRoundRect(18,y,364,73,5,BLACK);
        c.setTextColor(i==3?WHITE:BLACK,bg);
        c.setFont(&fonts::Font2);c.drawString(String(i+1)+" / "+MenuInput::names[i],30,y+5);
        c.setFont(&fonts::FreeSans12pt7b);
        c.drawString(fit(c,i<visible?rows[start+i]:String("More books (next page)"),338),30,y+31);
    }
    c.setTextColor(BLACK,WHITE);c.setFont(&fonts::Font2);
    c.drawString("LED = selection / A up / B down",24,518);
    footer(c,"AA: choose / BB: back / C: choose");
}
}

void begin(void (*schedule)()){
    stateMutex=xSemaphoreCreateMutex();scheduleScreen=schedule;
    if(!stateMutex){Serial.println("Reader mutex allocation failed");while(true)delay(1000);}
    storage=settings.begin("paper-reader",false);
    if(storage){fontIndex=settings.getUChar("font",0);lastId=settings.getString("last","");}
    if(fontIndex>=fontCount)fontIndex=0;
    snapshot.font=fontIndex;
}
bool active(){return stateMutex && state().active;}
bool busy(){return stateMutex && state().busy;}
int menuSelection(){if(!stateMutex)return -1;auto s=state();return s.active&&!s.busy&&listView(s.view)&&s.count?int(s.selection):-1;}
bool menuPicking(){if(!stateMutex)return false;auto s=state();return s.active&&!s.busy&&listView(s.view);}
void leave(){if(!stateMutex)return;xSemaphoreTake(stateMutex,portMAX_DELAY);snapshot.active=false;xSemaphoreGive(stateMutex);}
void resumeLast(){if(!lastId.isEmpty())request(Action::Resume);}
bool request(Action action,const char* id,uint32_t value){
    if(!stateMutex||!scheduleScreen||RefreshTest::faulted())return false;
    xSemaphoreTake(stateMutex,portMAX_DELAY);
    if(snapshot.busy){xSemaphoreGive(stateMutex);return false;}
    if(snapshot.active&&listView(snapshot.view)&&(action==Action::Previous||action==Action::Next)){
        snapshot.selection=MenuInput::move(snapshot.selection,snapshot.count,action==Action::Next);
        xSemaphoreGive(stateMutex);return true; // No display scheduling or SD work.
    }
    if(action==Action::Select&&listView(snapshot.view))value=snapshot.selection;
    pending={action,{},value,snapshot.active};snapshot.busy=true;snapshot.active=true;strlcpy(pending.id,id,sizeof(pending.id));
    xSemaphoreGive(stateMutex);scheduleScreen();return true;
}

bool render(M5Canvas& canvas,int battery){
    xSemaphoreTake(stateMutex,portMAX_DELAY);Command cmd=pending;
    if(listView(snapshot.view))selection=snapshot.selection;
    xSemaphoreGive(stateMutex);
    if(cmd.action==Action::Select&&listView(view))selection=cmd.value;
    message="";saveAfterDisplay=false;
    auto bookmark=[&]{
        if(pages.empty()){message="Open a book before setting a bookmark.";return;}
        progress.bookmark=pages[pageIndex];
        if(saveProgress()){message="Bookmark saved.";Feedback::play(Feedback::Cue::Saved);}
        else {message="Bookmark could not be saved.";Feedback::play(Feedback::Cue::Error);}
    };
    auto recall=[&]{if(progress.bookmark==UINT32_MAX||pages.empty())message="No bookmark in this book yet.";else{pageIndex=BookLayout::pageAt(pages,progress.bookmark);view=View::Reading;}};
    switch(cmd.action){
        case Action::TestNormal:case Action::TestAccelerated:
            view=View::Info;selection=0;publish();
            RefreshTest::run(canvas,cmd.action==Action::TestAccelerated,cmd.value);return false;
        case Action::Menu:view=View::Menu;selection=listPage=0;break;
        case Action::Back:
            if(view==View::Fonts||view==View::Sampler)view=View::FontMenu;
            else if(view==View::Menu)view=pages.empty()?View::Info:View::Reading;
            else view=View::Menu;
            selection=listPage=0;break;
        case Action::Resume:if(!currentId.isEmpty()&&!pages.empty())view=View::Reading;else if(!loadBook(lastId,canvas))view=View::Menu;break;
        case Action::Open:if(!loadBook(cmd.id,canvas))view=View::Menu;break;
        case Action::Sampler:view=View::Sampler;break;
        case Action::Bookmark:
            bookmark();
            if(cmd.wasActive){publish();return false;}
            view=pages.empty()?View::Menu:View::Reading;break;
        case Action::Recall:recall();break;
        case Action::Jump:if(!pages.empty()&&cmd.value<pages.size()){pageIndex=cmd.value;view=View::Reading;}else message="Page is outside this book.";break;
        case Action::Font:{
            if(cmd.value>=fontCount){message="Unknown font.";break;}
            uint32_t offset=pages.empty()?0:pages[pageIndex];fontIndex=cmd.value;
            if(storage)settings.putUChar("font",fontIndex);
            if(text&&paginate(canvas)){pageIndex=BookLayout::pageAt(pages,offset);view=View::Reading;}
            else view=View::Fonts;
            break;
        }
        case Action::Previous:case Action::Next:{
            int delta=cmd.action==Action::Next?1:-1;
            if(view==View::Reading){
                if((delta<0&&pageIndex==0)||(delta>0&&pageIndex+1>=pages.size())){message=delta<0?"Beginning of book.":"End of book.";publish();Feedback::play(Feedback::Cue::Busy);return false;}
                pageIndex+=delta;
            }else{
                size_t count=view==View::Library?books.size():view==View::Fonts?fontCount:view==View::FontMenu||view==View::Bookmarks?2:5;
                if(view==View::Info||view==View::Sampler){view=View::Menu;selection=0;}
                else if(count)selection=(selection+count+delta)%count;
            }
            break;
        }
        case Action::Select:
            if(view==View::Reading||view==View::Info||view==View::Sampler){view=View::Menu;selection=0;}
            else if(view==View::Library){
                if(books.empty()){view=View::Menu;selection=0;}
                else if(books.size()>5&&selection==listCount-1){listPage=(listPage+1)%((books.size()+3)/4);selection=0;}
                else loadBook(books[(books.size()>5?listPage*4:0)+selection].id,canvas);
            }
            else if(view==View::FontMenu){view=selection==0?View::Fonts:View::Sampler;selection=fontIndex;}
            else if(view==View::Bookmarks){if(selection==0)bookmark();else recall();}
            else if(view==View::Fonts){
                uint32_t offset=pages.empty()?0:pages[pageIndex];fontIndex=selection%fontCount;
                if(storage)settings.putUChar("font",fontIndex);
                if(text&&paginate(canvas)){pageIndex=BookLayout::pageAt(pages,offset);view=View::Reading;}
                else{view=View::Menu;selection=0;}
            }else{
                switch(selection){
                    case 0:if(!currentId.isEmpty()&&!pages.empty())view=View::Reading;else loadBook(lastId,canvas);break;
                    case 1:scanLibrary();view=View::Library;selection=listPage=0;break;
                    case 2:view=View::FontMenu;selection=0;break;
                    case 3:view=View::Bookmarks;selection=0;break;
                    case 4:view=View::Info;break;
                }
            }
            break;
    }
    if(view==View::Reading&&!pages.empty()){
        canvas.fillSprite(WHITE);canvas.setTextColor(BLACK,WHITE);canvas.setTextSize(1);canvas.setTextDatum(top_left);
        canvas.setFont(&fonts::Font2);canvas.drawString(fit(canvas,currentTitle,352),24,20);canvas.drawFastHLine(24,47,352,BLACK);
        canvas.setFont(fontChoices[fontIndex].font);
        std::vector<BookLayout::Line> lines;
        BookLayout::page(text,textSize,pages[pageIndex],metrics,&lines);
        for(size_t i=0;i<lines.size();++i){auto line=lines[i];char saved=text[line.end];text[line.end]=0;canvas.drawString(text+line.begin,24,63+i*lineHeight);text[line.end]=saved;}
        footer(canvas,String(pageIndex+1)+" / "+pages.size()+"     "+(pageIndex+1)*100/pages.size()+"%     "+battery+"% battery");
        saveAfterDisplay=true;
    }else if(view==View::Sampler){
        heading(canvas,"Type on paper");
        for(unsigned i=0;i<fontCount;++i){int y=86+i*89;canvas.setFont(&fonts::Font2);canvas.drawString(fontChoices[i].label,24,y);canvas.setFont(fontChoices[i].font);canvas.drawString("A quiet morning.",24,y+23);}
        footer(canvas,"C: menu / Choose Reading font");
    }else if(view==View::Info){
        heading(canvas,"Paper OS");canvas.setFont(&fonts::FreeSans12pt7b);
        canvas.drawString(String("Version ")+PAPER_OS_VERSION,24,105);canvas.drawString(String("Battery ")+battery+"%",24,156);
        canvas.drawString(WiFi.localIP().toString(),24,210);canvas.drawString("Open /books in your browser",24,261);
        canvas.setFont(&fonts::Font2);canvas.drawString("A: upper-left / B: lower-left / C: top",24,370);
        canvas.drawString("Hold A: bookmark / Hold B: Wi-Fi",24,402);canvas.drawString("Hold C: unlock firmware updates",24,434);
        footer(canvas,"C: menu");
    }else{
        std::vector<String> rows;
        if(view==View::Library){for(const auto& book:books)rows.push_back(book.title);drawList(canvas,"Your library",rows);}
        else if(view==View::Fonts){for(const auto& font:fontChoices)rows.push_back(font.label);drawList(canvas,"Reading font",rows);}
        else if(view==View::FontMenu){rows={"Choose reading font","Font comparison sheet"};drawList(canvas,"Fonts",rows);}
        else if(view==View::Bookmarks){rows={"Bookmark this page","Go to bookmark"};drawList(canvas,"Bookmarks",rows);}
        else{for(const auto& item:menuItems)rows.push_back(item);drawList(canvas,"Paper OS",rows);}
    }
    if(!message.isEmpty()){canvas.fillRect(20,554,360,46,WHITE);canvas.setFont(&fonts::Font2);canvas.setTextColor(BLACK,WHITE);canvas.drawString(fit(canvas,message,352),24,566);}
    publish();return true;
}
void displayed(){
    if(saveAfterDisplay && !saveProgress()){message="Could not save reading position.";Feedback::play(Feedback::Cue::Error);}
    saveAfterDisplay=false;publish();
    xSemaphoreTake(stateMutex,portMAX_DELAY);snapshot.busy=false;xSemaphoreGive(stateMutex);
}

void routes(WebServer& server,const String& token){
    server.on("/books",HTTP_GET,[&server,&token]{String page(READER_HTML);page.replace("{{TOKEN}}",token);server.sendHeader("Cache-Control","no-store");server.sendHeader("Content-Security-Policy","default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'");server.send(200,"text/html; charset=utf-8",page);});
    server.on("/reader.js",HTTP_GET,[&server]{server.send_P(200,"text/javascript",READER_JS);});
    server.on("/api/reader",HTTP_GET,[&server]{
        Snapshot s=state();
        String body=String("{\"active\":")+(s.active?"true":"false")+",\"busy\":"+(s.busy?"true":"false");
        body+=",\"id\":\""+String(s.id)+"\",\"title\":\""+escaped(s.title)+"\",\"message\":\""+escaped(s.message)+"\"";
        body+=",\"page\":"+String(s.page)+",\"pages\":"+String(s.pages)+",\"offset\":"+String(s.offset)+",\"font\":"+String(s.font)+",\"hasBookmark\":"+(s.bookmark==UINT32_MAX?"false":"true")+"}";
        const char* views[]={"menu","library","reading","fonts","sampler","info","font-menu","bookmarks"};
        body.remove(body.length()-1);body+=",\"view\":\""+String(views[unsigned(s.view)])+"\",\"selection\":"+String(s.selection)+",\"menuCount\":"+String(s.count)+"}";
        server.sendHeader("Cache-Control","no-store");server.send(200,"application/json",body);
    });
    server.on("/api/reader/action",HTTP_POST,[&server,&token]{
        auto reject=[&](int code,const char* text){server.send(code,"application/json",String("{\"error\":\"")+text+"\"}");};
        if(server.header("X-Paper-Token")!=token){reject(403,"Reload the page.");return;}
        if(RefreshTest::faulted()){reject(409,"Display fault latched. Restart required before further display commands.");return;}
        String action=server.arg("action"),id=server.arg("id"),value=server.arg("value");
        Action cmd;uint32_t number=0;
        if(action=="open"){if(!BookLayout::validId(id.c_str())){reject(400,"Invalid book ID.");return;}cmd=Action::Open;}
        else if(action=="next")cmd=Action::Next;
        else if(action=="previous")cmd=Action::Previous;
        else if(action=="menu")cmd=Action::Menu;
        else if(action=="select")cmd=Action::Select;
        else if(action=="back")cmd=Action::Back;
        else if(action=="resume")cmd=Action::Resume;
        else if(action=="bookmark")cmd=Action::Bookmark;
        else if(action=="recall")cmd=Action::Recall;
        else if(action=="sampler")cmd=Action::Sampler;
        else if(action=="font"||action=="jump"){
            if(value.isEmpty()||value.length()>5){reject(400,"Invalid number.");return;}
            for(size_t i=0;i<value.length();++i){if(!isDigit(value[i])){reject(400,"Invalid number.");return;}number=number*10+value[i]-'0';}
            if(action=="font"){cmd=Action::Font;if(number>=fontCount){reject(400,"Unknown font.");return;}}
            else{cmd=Action::Jump;Snapshot s=state();if(!number||number>s.pages){reject(400,"Page outside book.");return;}--number;}
        }else{reject(400,"Unknown reader action.");return;}
        if(!request(cmd,id.c_str(),number)){Feedback::play(Feedback::Cue::Busy);reject(409,"Reader is refreshing. Please wait.");return;}
        server.send(202,"application/json","{\"queued\":true}");
    });
}
}
