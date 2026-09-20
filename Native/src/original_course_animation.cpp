#include "original_course_animation.h"

namespace idas3 {
void OriginalCourseAnimation::reset(unsigned courseIndex,bool night,bool wet){
    phase_=0;active_=courseIndex==2;nightConstructor_=night||wet;
}
void OriginalCourseAnimation::advance(){
    if(active_)phase_=std::uint16_t(unsigned(phase_)+(nightConstructor_?1u:273u));
}
original::OriginalMatrix OriginalCourseAnimation::matrix(const original::OriginalFscaTable& table)const{
    auto result=original::originalIdentityMatrix();
    // Literal order at 1A07AC/1A152C is Y,Z,X; preserve the source values.
    original::translateOriginalMatrix(result,{1916.06005859375f,474.1619873046875f,-1793.31005859375f});
    original::rotateOriginalMatrixPhase(result,2,std::uint16_t(0u-unsigned(phase_)),table);
    original::rotateOriginalMatrixPhase(result,1,std::uint16_t(-618),table);
    return result;
}
unsigned OriginalCourseAnimation::apply(NativeAssembly& assembly,const original::OriginalFscaTable& table)const{
    if(!active_)return 0;
    const auto transform=matrix(table);unsigned count=0;
    for(auto& instance:assembly.instances)if(instance.chunk==chunk()){
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            instance.transform[row*4+col]=transform.elements[col*4+row];
        ++count;
    }
    return count;
}
}
