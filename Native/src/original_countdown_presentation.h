#pragma once
#include <algorithm>
#include <array>
namespace idas3::original {
struct CountdownPresentation {float scale,opacity;};
inline CountdownPresentation originalCountdownPresentation(unsigned elapsedTicks){
    // 0C0C8082 initializes the scale filter at 1 with response 60.
    // 0C0C8A80 resets it at ticks 60/120 and sets GO to 2 at 180;
    // 0C1AE560 approaches 2 between those resets. Evaluate by source age,
    // never by render calls, so pause/high refresh/rollback cannot speed it up.
    static const auto scales=[] {
        std::array<float,241> values{};float previousInput=1,output=1;
        values[0]=1;
        for(unsigned tick=1;tick<values.size();++tick){
            if(tick==60||tick==120){previousInput=output=1;}
            else if(tick==180){previousInput=output=2;}
            else {output=(previousInput+2.f-(-59.f*output))*(1.f/61.f);previousInput=2;}
            values[tick]=output;
        }
        return values;
    }();
    const auto age=std::min(elapsedTicks,240u);
    const unsigned alpha=age>210?unsigned((1.f-float(age-210)/30.f)*256.f):255u;
    return {scales[age],float(alpha)/255.f};
}
}
