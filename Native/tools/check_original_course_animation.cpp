#include "original_course_animation.h"
#include "sh4_scalar_reference.h"
#include <iostream>
#include <stdexcept>

using namespace idas3;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("image native-root required");
    reference::RefMemory memory(argv[1]);
    constexpr unsigned base=0x0d000000,phaseAddress=base+0x56c;
    memory.zeroRegion(base,0x1000);reference::RefCpu cpu(memory);
    const auto table=original::OriginalFscaTable::load(std::filesystem::path(argv[2])/"data/original_physics/fsca_table.bin");
    unsigned samples=0;
    for(unsigned variant=0;variant<3;++variant){
        OriginalCourseAnimation animation;animation.reset(2,variant==1,variant==2);
        if(!animation.active()||animation.phase()!=0||animation.chunk()!=(variant?84:60))throw std::runtime_error("Original constructor variant mismatch");
        NativeAssembly assembly;assembly.instances.resize(2);assembly.instances[0].chunk=animation.chunk();assembly.instances[1].chunk=7;
        const auto untouched=assembly.instances[1].transform;
        if(animation.apply(assembly,table)!=1)throw std::runtime_error("Original prop instance selection mismatch");
        const auto initial=assembly.instances[0].transform;
        // Execute the canonical phase increment block for an entire wrap.
        for(unsigned i=0;i<65537;++i){
            memory.write32(phaseAddress,animation.phase());cpu.r.fill(0);cpu.r[8]=base+0x53c;
            cpu.run(variant?0x0c1a1488:0x0c1a0708,variant?0x0c1a149a:0x0c1a071c,40);
            animation.advance();
            if(memory.read32(phaseAddress)!=animation.phase())throw std::runtime_error("Original phase increment/wrap mismatch");
            ++samples;
        }
        animation.apply(assembly,table);
        if(assembly.instances[0].transform==initial||assembly.instances[1].transform!=untouched)throw std::runtime_error("Animated transform replacement mismatch");
        const auto& m=assembly.instances[0].transform;
        if(m[3]!=1916.06005859375f||m[7]!=474.1619873046875f||m[11]!=-1793.31005859375f)throw std::runtime_error("Original pivot moved");
    }
    OriginalCourseAnimation inactive;inactive.reset(3,false,false);inactive.advance();
    NativeAssembly empty;if(inactive.active()||inactive.phase()||inactive.apply(empty,table))throw std::runtime_error("Animation leaked to another course");
    std::cout<<"PASS "<<samples<<" canonical Akagi phase/wrap steps; day/night/day-wet constructor selection, original pivot, instance replacement and other-course isolation.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
