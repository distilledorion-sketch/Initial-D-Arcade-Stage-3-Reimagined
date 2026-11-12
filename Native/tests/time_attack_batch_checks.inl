#include <algorithm>
#include <random>

namespace {
// Frozen single-insertion behavior, independent of the new batch helper.
// A retained top-ten/model-best row can only be displaced by a later, faster
// row; stable ties keep earlier input first, including metadata differences.
void legacyInsert(std::vector<TimeAttackEntry>& rows,TimeAttackEntry e){
    rows.push_back(e);
    std::stable_sort(rows.begin(),rows.end(),[](auto a,auto b){return a.ticks6000<b.ticks6000;});
    std::array<unsigned,supportedConditionCount*2> count{};
    std::array<std::array<bool,35>,supportedConditionCount*2> models{};
    std::erase_if(rows,[&](auto row){auto key=row.condition*2+row.weather;
        bool keep=count[key]<10||!models[key][row.car];++count[key];models[key][row.car]=true;return !keep;});
}
bool sameRow(const TimeAttackEntry& a,const TimeAttackEntry& b){
    return a.condition==b.condition&&a.weather==b.weather&&a.car==b.car&&a.ticks6000==b.ticks6000&&
        a.nameGlyphs==b.nameGlyphs&&a.manual==b.manual&&a.night==b.night&&
        a.intermediate6000==b.intermediate6000&&a.metadataUnknown==b.metadataUnknown;
}
void batchChecks(){
    unsigned comparisons=0;
    auto exact=[&](const auto& actual,const auto& expected){
        require(actual.size()==expected.size(),"Batch changed the number of retained records");
        for(std::size_t i=0;i<actual.size();++i){
            require(sameRow(actual[i],expected[i]),"Batch changed row order or metadata");++comparisons;
        }
    };
    std::mt19937 random(0x1da53);
    for(unsigned trial=0;trial<24;++trial){
        std::vector<TimeAttackEntry> input,expected;
        for(unsigned i=0;i<4800;++i){
            TimeAttackEntry e{random()%supportedConditionCount,random()%2,random()%35,
                100000u+(trial%3==0?random()%7:random()%1000000)};
            for(auto& glyph:e.nameGlyphs)glyph=std::uint8_t(random()%222);
            e.manual=(random()%2)!=0;e.night=(random()%2)!=0;e.metadataUnknown=(random()%3)==0;
            if(random()%2)e.intermediate6000={e.ticks6000/4,e.ticks6000/2,e.ticks6000*3/4};
            input.push_back(e);legacyInsert(expected,e);
        }
        TimeAttackRecords whole;whole.recordBatch(input);exact(whole.entries(),expected);
        TimeAttackRecords chunks;
        for(std::size_t begin=0;begin<input.size();){
            const auto count=std::min<std::size_t>(1+random()%511,input.size()-begin);
            chunks.recordBatch(std::span(input).subspan(begin,count));begin+=count;
        }
        exact(chunks.entries(),expected);
        // Interleave ordinary new finishes with snapshot-sized batches.
        TimeAttackRecords mixed;
        for(unsigned i=0;i<100;++i)mixed.record(input[i]);
        mixed.recordBatch(std::span(input).subspan(100));exact(mixed.entries(),expected);
    }
    std::vector<TimeAttackEntry> ties,expected;
    for(unsigned i=0;i<14;++i){TimeAttackEntry e{0,0,i<12?0u:1u,100000};
        e.nameGlyphs[0]=std::uint8_t(i);e.manual=(i%2)!=0;ties.push_back(e);legacyInsert(expected,e);}
    TimeAttackRecords tied;tied.recordBatch(ties);exact(tied.entries(),expected);
    require(tied.entries().size()==11&&tied.entries()[9].nameGlyphs[0]==9&&tied.entries()[10].nameGlyphs[0]==12,
        "Stable tied top-ten/model winner changed");
    // An aliased input range remains valid while staged output is assembled.
    auto duplicate=tied.entries();for(auto e:duplicate)legacyInsert(expected,e);
    tied.recordBatch(tied.entries());exact(tied.entries(),expected);
    tied.recordBatch({});exact(tied.entries(),expected);

    TimeAttackEntry validRow{0,0,0,200000};validRow.intermediate6000={40000,80000,120000};
    for(unsigned field=0;field<10;++field){
        auto invalid=validRow;
        switch(field){
        case 0:invalid.condition=supportedConditionCount;break;
        case 1:invalid.weather=2;break;
        case 2:invalid.car=35;break;
        case 3:invalid.ticks6000=0;break;
        case 4:invalid.ticks6000=10800000;break;
        case 5:invalid.nameGlyphs[4]=222;break;
        case 6:invalid.intermediate6000[0]=0;break;
        case 7:invalid.intermediate6000[1]=40000;break;
        case 8:invalid.intermediate6000[2]=80000;break;
        case 9:invalid.intermediate6000[2]=200000;break;
        }
        std::array input{validRow,invalid};bool rejected=false;
        try{tied.recordBatch(input);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Invalid final batch row was accepted");exact(tied.entries(),expected);
    }
    std::cout<<"PASS batch ranking: 24 deterministic 4800-row streams, ties, chunking, individual finishes, aliasing and 10 atomic rejections; "
        <<comparisons<<" exact row comparisons\n";
}
}
