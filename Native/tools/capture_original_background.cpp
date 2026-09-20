#include "sh4_scalar_reference.h"
#include "akina_background.h"
#include <algorithm>
#include <iomanip>
#include <iostream>

using Matrix=std::array<float,16>; // row-major, column vectors
Matrix viewMatrix(idas3::Vec3 eye,float yaw,float pitch){
    const float cy=std::cos(yaw),sy=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    const idas3::Vec3 right{cy,0,-sy},up{-sy*sp,cp,-cy*sp},forward{sy*cp,sp,cy*cp};
    return {right.x,right.y,right.z,-idas3::dot(right,eye),
            up.x,up.y,up.z,-idas3::dot(up,eye),
            forward.x,forward.y,forward.z,-idas3::dot(forward,eye),0,0,0,1};
}
Matrix multiply(const Matrix& a,const Matrix& b){
    Matrix out{};for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)for(unsigned k=0;k<4;k++)out[r*4+c]+=a[r*4+k]*b[k*4+c];return out;
}
int main(int argc,char**argv){try{
    using namespace idas3::reference;
    if(argc!=3)throw std::runtime_error("canonical-image capture.json required");
    RefMemory mem(argv[1]);
    constexpr unsigned object=0x0d000000,scratch=0x0d001000,stack=0x0d010000,bank=0x0d020000,geometry=0x0d020100;
    std::ofstream out(argv[2]);if(!out)throw std::runtime_error("Cannot open background capture");out<<std::setprecision(9);
    out<<"{\"start_pc\":\"0C19DA74\",\"stop_pc\":\"0C19DAAA\",\"hooks\":[\"1FBD60 read current view\",\"1FC5A0 capture final matrix\",\"05A8E0 capture bank/chunk lookup\",\"1D7120 capture selected draw\"],\"cases\":[";
    std::size_t instructions=0;float maxError=0;
    for(unsigned sample=0;sample<72;sample++){
        mem.clear();mem.zeroRegion(object,0x3000);mem.zeroRegion(stack,0x10000);
        const idas3::Vec3 eye{float(int(sample*97%3901)-1700),550+float(sample*71%700),float(int(sample*173%5101)-2600)};
        const float yaw=float(sample%12)*idas3::pi/6,pitch=float(int(sample/12)-3)*.11f;
        const Matrix view=viewMatrix(eye,yaw,pitch);Matrix captured{};
        mem.write32(object+0x158,bank);mem.write32(scratch+80,scratch+48);mem.write32(scratch+84,scratch+56);
        RefCpu cpu(mem);cpu.r[13]=object;cpu.r[14]=scratch;cpu.r[15]=stack+0xf000;
        unsigned matrices=0,lookups=0,draws=0;
        cpu.callHooks[0x0c1fbd60]=[&](RefCpu& c){if(c.r[4]!=scratch)throw std::runtime_error("Unexpected matrix destination");for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++)mem.writeFloat(scratch+4*(col*4+r),view[r*4+col]);};
        cpu.callHooks[0x0c1fc5a0]=[&](RefCpu& c){++matrices;for(unsigned r=0;r<4;r++)for(unsigned col=0;col<4;col++)captured[r*4+col]=mem.readFloat(c.r[4]+4*(col*4+r));};
        cpu.callHooks[0x0c05a8e0]=[&](RefCpu& c){if(c.r[4]!=bank||c.r[5]!=0)throw std::runtime_error("Background selector changed");++lookups;c.r[0]=geometry;};
        cpu.callHooks[0x0c1d7120]=[&](RefCpu& c){if(c.r[4]!=geometry)throw std::runtime_error("Wrong background draw");++draws;};
        const auto count=cpu.run(0x0c19da74,0x0c19daaa,100000);instructions+=count;
        if(matrices!=1||lookups!=1||draws!=1)throw std::runtime_error("Background draw contract failed");
        const auto instance=idas3::originalAkinaBackgroundInstance(eye);
        const Matrix expected=multiply(view,instance.transform);float error=0;
        for(unsigned i=0;i<16;i++){if(!std::isfinite(captured[i]))throw std::runtime_error("Nonfinite original matrix");error=std::max(error,std::abs(expected[i]-captured[i]));}
        maxError=std::max(maxError,error);if(error>.003f)throw std::runtime_error("Original background anchor differs from rigid-view equivalence: "+std::to_string(error));
        if(sample)out<<',';out<<"{\"sample\":"<<sample<<",\"eye\":["<<eye.x<<','<<eye.y<<','<<eye.z<<"],\"yaw\":"<<yaw<<",\"pitch\":"<<pitch<<",\"original_instructions\":"<<count<<",\"max_matrix_abs_error\":"<<error<<",\"original_matrix\":[";
        for(unsigned i=0;i<16;i++){if(i)out<<',';out<<captured[i];}out<<"]}";
    }
    out<<"],\"total_original_instructions\":"<<instructions<<",\"max_matrix_abs_error\":"<<maxError<<",\"matrix_comparison\":\"Rigid-view mathematical equivalence with measured original F32 inversion rounding, not bit parity\"}\n";
    std::cout<<"PASS72 actual original background selections and double matrix inversions, "<<instructions<<" instructions; max matrix absolute difference "<<maxError<<". No graphics device.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
