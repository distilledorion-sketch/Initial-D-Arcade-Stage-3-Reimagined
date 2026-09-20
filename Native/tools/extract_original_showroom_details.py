"""Export the source-selected car showroom shadow without substitute geometry."""
from pathlib import Path
import argparse, hashlib, json, struct
from extract_original_models import parse_model, write_binary

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('hostfs',type=Path); ap.add_argument('image',type=Path); ap.add_argument('game',type=Path)
    args=ap.parse_args()
    source=args.hostfs/'model/selcrs'
    _,chunks,files,payload=parse_model(source)
    shadow=chunks[4]
    assert len(chunks)==5 and len(shadow['batches'])==2
    assert all(b['material'][9]==0xffffffff for b in shadow['batches'])
    out=args.game/'data/original_models/showroom';out.mkdir(parents=True,exist_ok=True)
    write_binary(out/'selcrs.idasmesh',chunks)
    image=args.image.read_bytes()
    assert len(image)==4194304
    strings={hex(a):image[a-0xc020000:].split(b'\0',1)[0].decode('ascii') for a in (0xc266d4c,0xc266d6c)}
    assert strings['0xc266d4c']=='/driveA/model/selcrs/selcrs_pol'
    dimensions=[struct.unpack_from('<2I',image,0xc265af4-0xc020000+44*i)[0:1]+struct.unpack_from('<I',image,0xc265afc-0xc020000+44*i) for i in range(35)]
    manifest={'schema':'idas3-original-showroom-shadow-v1','source_image_sha256':hashlib.sha256(image).hexdigest(),
      'source_files':{k:{'path':str(v),'sha256':hashlib.sha256(v.read_bytes()).hexdigest()} for k,v in files.items()},
      'loader_strings':strings,'bank_binding':'11D940 -> owner+120 -> 12D1E0 stack[1] -> selector+436 -> 12DC60/05A8E0 chunk4 -> 10F040 display+16 -> 10FA00',
      'selected_chunk':4,'draw_count':2,'textures_required':False,
      'material_handling':'Keep original VUR alpha gradient, GMP and ICH. No UI color-buffer override.',
      'other_chunks':'Preserved to retain original indices; not selected by this shadow helper.',
      'dimension_words':dimensions,'shadow_vertices':sum(len(b['vertices']) for b in shadow['batches']),
      'shadow_triangles':sum(len(b['indices'])//3 for b in shadow['batches']),
      'model_sha256':hashlib.sha256((out/'selcrs.idasmesh').read_bytes()).hexdigest()}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    header='''#pragma once
#include "native_assets.h"
#include "original_matrix.h"
#include <bit>
#include <stdexcept>

namespace idas3 {
// Source 10FA00. The model is data/original_models/showroom/selcrs.idasmesh.
// Its shadow chunk4 is untextured, retaining the authored alpha gradient.
struct OriginalShowroomShadow {
    static constexpr std::uint32_t chunk=4;
    static Vec3 scale(unsigned car) {
        if(car>=dimensions.size())throw std::out_of_range("Original showroom car ID");
        float x=std::bit_cast<float>(dimensions[car][0]);
        float z=std::bit_cast<float>(dimensions[car][1]);
        x*=1.6f; x*=.9f; z*=1.9f; z*=.9f;
        return {x,1,z};
    }
    static NativeAssembly assembly(unsigned car,float yaw,const original::OriginalFscaTable& table) {
        auto matrix=original::originalActorMatrix({0,.01f,0},{0,yaw,0},table);
        auto s=scale(car);
        for(unsigned i=0;i<4;++i) {matrix.elements[i]*=s.x;matrix.elements[8+i]*=s.z;}
        NativeAssembly result;
        auto emit=[&](){NativeModelInstance instance;instance.chunk=chunk;
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                instance.transform[row*4+col]=matrix.elements[col*4+row];
            result.instances.push_back(instance);};
        emit();original::translateOriginalMatrix(matrix,{0,.02f,0});emit();
        return result;
    }
private:
    static constexpr std::array<std::array<std::uint32_t,2>,35> dimensions={{DIMENSIONS}};
};
}
'''.replace('DIMENSIONS',','.join('{0x%08xu,0x%08xu}'%pair for pair in dimensions))
    (args.game/'src/original_showroom_details.h').write_text(header)
    print(f'Exported source bank5 chunks; selected shadow4: {manifest["shadow_vertices"]} vertices, {manifest["shadow_triangles"]} triangles; no textures.')

if __name__=='__main__':main()
