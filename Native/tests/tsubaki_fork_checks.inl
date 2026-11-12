// Independently surveyed RCL2 island boundary, source triangles 24296..24321.
// The old driving-limit ribbon passes through every one of these curb faces.
// This fixture deliberately does not read the converter's boundary metadata.
void checkTsubakiFork(const OriginalCollisionData& collision){
    const std::array<OriginalRacePoint,28> boundary{{
        {1113.699951172f, 368.558013916f, 276.368011475f},
        {1112.780029297f, 368.480010986f, 276.304992676f},
        {1110.829956055f, 368.317993164f, 276.523010254f},
        {1108.920043945f, 368.148986816f, 276.865997314f},
        {1107.089965820f, 367.970001221f, 277.522003174f},
        {1105.410034180f, 367.782989502f, 278.473999023f},
        {1103.900024414f, 367.588989258f, 279.694000244f},
        {1102.619995117f, 367.382995605f, 281.148010254f},
        {1101.599975586f, 367.162994385f, 282.795013428f},
        {1100.670043945f, 366.923004150f, 284.529998779f},
        {1100.349975586f, 366.803009033f, 285.589996338f},
        {1100.430053711f, 366.748992920f, 286.105987549f},
        {1100.760009766f, 366.747009277f, 286.381011963f},
        {1101.280029297f, 366.776000977f, 286.419006348f},
        {1101.699951172f, 366.825012207f, 286.071990967f},
        {1101.880004883f, 366.912994385f, 285.652008057f},
        {1102.640014648f, 367.151000977f, 283.997009277f},
        {1103.790039062f, 367.376007080f, 282.611999512f},
        {1105.099975586f, 367.584991455f, 281.371002197f},
        {1106.530029297f, 367.782012939f, 280.286010742f},
        {1108.079956055f, 367.971008301f, 279.368988037f},
        {1109.719970703f, 368.152008057f, 278.632995605f},
        {1111.430053711f, 368.324005127f, 278.092010498f},
        {1113.219970703f, 368.480010986f, 277.960998535f},
        {1113.750000000f, 368.523010254f, 277.929992676f},
        {1114.270019531f, 368.569000244f, 277.649993896f},
        {1114.430053711f, 368.589996338f, 277.166992188f},
        {1114.189941406f, 368.580993652f, 276.712005615f},
    }};
    int checks=0;
    for(unsigned edge=0;edge<boundary.size();++edge){
        const auto& a=boundary[edge];const auto& b=boundary[(edge+1)%boundary.size()];
        const float dx=b[0]-a[0],dz=b[2]-a[2],length=std::hypot(dx,dz);
        // The source contour is clockwise in X/Z: its left normal is outward.
        const float nx=-dz/length,nz=dx/length;
        for(float along:{.15f,.5f,.85f})for(float offset:{.1f,-.1f,-.5f,-5.f}){
            const float x=a[0]+along*dx,y=a[1]+along*(b[1]-a[1])+1,z=a[2]+along*dz;
            OriginalCollisionQuery q;clearOriginalCollisionQuery(q);
            q.setf(44,x+nx*.5f);q.setf(48,y);q.setf(52,z+nz*.5f);
            q.setf(32,x+nx*offset);q.setf(36,y);q.setf(40,z+nz*offset);
            OriginalTriangleSearchTrace trace;OriginalSurfaceScratch surface;
            require(queryOriginalCollisionSwept(collision,q,trace,surface),"Missing surface beside Tsubaki fork island");
            require(bool(q.u(28)&0x8000u)==(offset<0),"Tsubaki fork collision differs from authored island");
            if(offset<0)require(std::abs(q.f(24)+offset)<.003f,"Tsubaki fork contact depth differs from authored boundary");
            ++checks;
        }
    }
    std::cout<<"PASS "<<checks<<" surveyed fork clearance/contact/anti-tunnelling checks\n";
}
