#include "book_storage.h"
#include "book_layout.h"
#include "pictures.h"
#include "reader.h"
#include "feedback.h"
#include <SD.h>
#include <esp_system.h>

namespace {
uint8_t* buffer=nullptr;
size_t length=0;
bool complete=false,failed=false;
String title;
uint32_t touched=0;
struct Lock {
    bool held=xSemaphoreTake(pictureBus,0)==pdTRUE;
    ~Lock(){if(held)xSemaphoreGive(pictureBus);}
};
void reset(){free(buffer);buffer=nullptr;length=0;complete=false;}
String quote(const String& value){String s="\"";for(size_t i=0;i<value.length();++i){char c=value[i];if(c=='"'||c=='\\')s+='\\';s+=uint8_t(c)<32?' ':c;}return s+'"';}
void error(WebServer& s,int code,const char* msg){s.send(code,"application/json",String("{\"error\":\"")+msg+"\"}");}
bool validTitle(const String& s){if(s.isEmpty()||s.length()>64)return false;for(size_t i=0;i<s.length();++i)if(s[i]<32||s[i]>126)return false;return true;}
}
String bookStorageTitle(const String& id){
    File meta=SD.open("/books/"+id+".title");
    if(!meta)return id;
    char value[65]{};size_t n=meta.read(reinterpret_cast<uint8_t*>(value),64);
    for(size_t i=0;i<n;++i)if(value[i]<32||value[i]>126)value[i]=' ';
    return n?String(value):id;
}
void bookStorageRoutes(WebServer& s,const String& token){
    s.on("/api/books",HTTP_GET,[&s]{
        Lock lock;if(!lock.held){error(s,409,"Screen is refreshing. Try again shortly.");return;}
        if(SD.cardType()==CARD_NONE){error(s,503,"SD card unavailable. Insert a FAT32 card and restart.");return;}
        String body="{\"books\":[";unsigned count=0;
        File dir=SD.open("/books");
        if(dir&&dir.isDirectory())while(File file=dir.openNextFile()){
            String name=file.name(),id=name.substring(0,8);
            if(file.isDirectory()||name.length()!=12||!name.endsWith(".txt")||!BookLayout::validId(id.c_str()))continue;
            String title=bookStorageTitle(id);
            if(count++)body+=',';
            body+="{\"id\":"+quote(id)+",\"title\":"+quote(title)+",\"bytes\":"+String(file.size())+"}";
            if(count==64)break;
        }
        body+="],\"limit\":64}";s.sendHeader("Cache-Control","no-store");s.send(200,"application/json",body);
    });
    s.on("/api/books/download",HTTP_GET,[&s]{
        String id=s.arg("id");if(!BookLayout::validId(id.c_str())){error(s,400,"Invalid book ID.");return;}
        Lock lock;if(!lock.held){error(s,409,"Screen is refreshing. Try again shortly.");return;}
        File file=SD.open("/books/"+id+".txt");if(!file){error(s,404,"Book not found.");return;}
        s.sendHeader("Content-Disposition","attachment; filename=book-"+id+".txt");s.streamFile(file,"text/plain; charset=utf-8");
    });
    s.on("/api/books/upload",HTTP_POST,[&s,&token]{
        if(s.header("X-Paper-Token")!=token){reset();error(s,403,"Reload the page.");return;}
        if(failed||!complete||!buffer||!BookLayout::validText(reinterpret_cast<char*>(buffer),length)){
            reset();error(s,400,"Expected one non-empty normalized TXT book, up to 1 MiB. Import through the Books page.");return;
        }
        Lock lock;if(!lock.held){reset();error(s,409,"Screen is refreshing. Try uploading again shortly.");return;}
        if(!SD.exists("/books")&&!SD.mkdir("/books")){reset();error(s,503,"Could not create the books directory. Check the SD card.");return;}
        unsigned count=0;File dir=SD.open("/books");while(File file=dir.openNextFile())if(!file.isDirectory()&&String(file.name()).endsWith(".txt"))++count;dir.close();
        if(count>=64){reset();error(s,409,"Library limit is 64 books. Archive older books from the SD card first.");return;}
        char name[9];snprintf(name,sizeof(name),"%08lx",(unsigned long)esp_random());String id=name;
        String path="/books/"+id+".txt",meta="/books/"+id+".title",temp=path+".tmp",metaTemp=meta+".tmp";
        if(SD.exists(path)||SD.exists(meta)||SD.exists(temp)||SD.exists(metaTemp)){reset();error(s,409,"Name collision; please retry.");return;}
        File file=SD.open(temp,FILE_WRITE);bool ok=file&&file.write(buffer,length)==length;if(file){file.flush();file.close();}
        if(ok){file=SD.open(temp);ok=file&&file.size()==length;uint8_t chunk[512];size_t p=0;while(ok&&p<length){size_t n=std::min(sizeof(chunk),length-p);ok=file.read(chunk,n)==n&&memcmp(chunk,buffer+p,n)==0;p+=n;}file.close();}
        if(ok){file=SD.open(metaTemp,FILE_WRITE);ok=file&&file.print(title)==title.length();if(file){file.flush();file.close();}}
        if(ok)ok=SD.rename(metaTemp,meta);
        if(ok)ok=SD.rename(temp,path);
        if(!ok){SD.remove(temp);SD.remove(metaTemp);SD.remove(meta);}
        reset();
        if(!ok){error(s,507,"Book write failed. Check the card and free space.");return;}
        Feedback::play(Feedback::Cue::Saved);s.send(201,"application/json","{\"id\":"+quote(id)+",\"title\":"+quote(title)+"}");
    },[&s,&token]{
        if(!s.header("Content-Type").startsWith("multipart/")){
            reset();failed=true;if(s.raw().status==RAW_START){error(s,400,"Multipart TXT upload required.");s.client().stop();}return;
        }
        auto& u=s.upload();
        if(u.status==UPLOAD_FILE_START){
            bool duplicate=buffer&&millis()-touched<15000;reset();failed=false;title=s.arg("title");
            if(duplicate||s.header("X-Paper-Token")!=token||!validTitle(title))failed=true;
            else buffer=static_cast<uint8_t*>(ps_malloc(BookLayout::maxBytes));
            if(!buffer)failed=true;
        }else if(u.status==UPLOAD_FILE_WRITE&&buffer&&!failed){
            if(u.currentSize>BookLayout::maxBytes-length)failed=true;
            else{memcpy(buffer+length,u.buf,u.currentSize);length+=u.currentSize;}
        }else if(u.status==UPLOAD_FILE_END)complete=true;
        else if(u.status==UPLOAD_FILE_ABORTED){reset();failed=true;}
        touched=millis();
    });
}
