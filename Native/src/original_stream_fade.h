#pragma once
#include <cstdint>
namespace idas3::original {
// SH4 1CFBE0 initializes the stream ramp; 1CFB60 emits the current
// integer level before stepping by one, waiting delay+1 ticks per step.
struct OriginalStreamFade {
    unsigned level=127, next=127, wait=0, delay=0;
    bool active=false;
    void begin(unsigned volume,unsigned stepDelay){level=next=volume&127;wait=0;delay=stepDelay;active=true;}
    unsigned tick(){
        if(!active)return level;
        level=next;
        if(wait){--wait;return level;}
        if(next<=1){level=next=0;active=false;return level;}
        --next;wait=delay;return level;
    }
};
}
