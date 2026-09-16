#include "book_layout.h"
#include <cassert>
#include <iostream>
#include <random>
#include <string>

int main() {
    using namespace BookLayout;
    assert(validId("1234abcd"));
    for (auto id : {"", "a", "../x.txt", "123456789", "ABCDEF12"}) assert(!validId(id));
    assert(!validId(nullptr));
    assert(validText("A book\n",7));
    for (auto s : {"", " \n ", "bad\r", "bad\t", "caf\xc3\xa9"}) assert(!validText(s,std::strlen(s)));
    assert(!validText("a", maxBytes+1));
    Metrics m; m.width=5; m.rows=2;
    for (auto& width : m.widths) width=1;
    std::string s="one two three four";
    std::vector<Line> lines;
    auto end=page(s.data(),s.size(),0,m,&lines);
    assert(lines.size()==2 && s.substr(lines[0].begin,lines[0].end-lines[0].begin)=="one");
    assert(s.substr(lines[1].begin,lines[1].end-lines[1].begin)=="two" && end==8);
    lines.clear();s="a\n\nb";end=page(s.data(),s.size(),0,m,&lines);
    assert(end==3 && lines.size()==2 && lines[1].begin==lines[1].end);
    s="abcdefghijk";end=page(s.data(),s.size(),0,m);assert(end==10);
    m.width=0;assert(page(s.data(),s.size(),0,m)==2); // always progresses
    assert(pageAt({0,10,20},0)==0 && pageAt({0,10,20},19)==1 && pageAt({0,10,20},20)==2);
    assert(pageAt({},100)==0);
    // Randomized pagination must preserve every non-whitespace character,
    // terminate, and keep lines within the available width.
    std::mt19937 random(123);
    for (unsigned trial=0;trial<500;++trial) {
        m.width=1+random()%60;m.rows=1+random()%20;s.clear();
        for(unsigned i=0;i<2000;++i){unsigned n=random()%32;s+=n<26?char('a'+n):n==26?'\n':' ';}
        uint32_t p=0;std::string actual,expected;
        for(char c:s)if(c!=' '&&c!='\n')expected+=c;
        while(p<s.size()) {
            lines.clear();auto next=page(s.data(),s.size(),p,m,&lines);
            assert(next>p&&next<=s.size()&&lines.size()<=m.rows);
            for(auto line:lines){assert(line.end>=line.begin&&line.end-line.begin<=m.width);for(auto i=line.begin;i<line.end;++i)if(s[i]!=' ')actual+=s[i];}
            p=next;
        }
        assert(actual==expected);
    }
    std::cout << "Book layout: validation, wrapping, offsets and 500 randomized books passed.\n";
}
