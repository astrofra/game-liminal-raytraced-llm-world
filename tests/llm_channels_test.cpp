#include "llm_channels.h"
#include <cstdio>
int main() {
    int errors=0;
    auto check=[&](bool value,const char* name) { if(!value) { ++errors; std::fprintf(stderr,"FAIL %s\n",name); } };
    for(const std::string text : {"Une carriere silencieuse.","{\"heat\":0}","2 < 3"})
        check(liminal::GuardFinalChannel(text,true).text==text,"ordinary final text");
    for(const std::string raw : {"<|channel>thought\nprivate<channel|>public","<|tool_call>bad","<|turn>model","<think>private</think>"}) {
        for(size_t length=1;length<=raw.size();++length)
            check(liminal::GuardFinalChannel(raw.substr(0,length),false).text.empty(),"no private fragment leaks");
        check(!liminal::GuardFinalChannel(raw,true).error.empty(),"unexpected channel rejected");
    }
    check(!liminal::GuardFinalChannel("",true).error.empty(),"empty final rejected");
    check(!liminal::GuardFinalChannel("visible<",true).error.empty(),"truncated control prefix rejected");
    check(liminal::GuardFinalChannel("visible<",false).text=="visible","withhold trailing prefix");
    std::printf("Final channel checks: %s\n",errors?"FAILED":"passed");
    return errors?1:0;
}
