"""Extract the exact registered child12 ranking banks and numeric scene tables."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_menus import export_model_menu
CANONICAL='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path);a=p.parse_args()
    b=a.image.read_bytes();assert hashlib.sha256(b).hexdigest()==CANONICAL
    def u(x):return struct.unpack_from('<I',b,x-0xc020000)[0]
    def s(x):return b[x-0xc020000:b.index(0,x-0xc020000)].decode('ascii')
    roots=a.project/'data/original_assets/attract/ranking';roots.mkdir(parents=True,exist_ok=True)
    banks={name:export_model_menu(a.hostfs/'model'/name,roots/name,'twiddled')for name in ['s_ground','v3sT13rankin','v3sT00common']}
    dimensions=[[u(0xc23be08+i*44+j)for j in (0,8)]for i in range(35)]
    source=['#pragma once','#include <array>','namespace idas3::original::ranking_data {',
      'inline constexpr std::array<unsigned,9> courses{'+','.join(str(u(0xc2ef27c+4*i))for i in range(9))+'};',
      'inline constexpr std::array<int,18> conditions{'+','.join(str(struct.unpack_from('<i',b,0xc2ef230-0xc020000+4*i)[0])for i in range(18))+'};',
      'inline constexpr std::array<std::array<unsigned,2>,35> dimensions{{'+','.join('{0x%08xu,0x%08xu}'%tuple(row)for row in dimensions)+'}};',
      'inline constexpr std::array<unsigned,35> heights{'+','.join('0x%08xu'%u(0xc2f4ed8+4*i)for i in range(35))+'};',
      'inline constexpr std::array<unsigned,35> carNameChunks{'+','.join(str(u(0xc2aae80+4*i))for i in range(35))+'};',
      'inline constexpr std::array<unsigned,9> courseChunks{'+','.join(str(u(0xc2aaf0c+4*i))for i in range(9))+'};',
      'inline constexpr std::array<unsigned,18> routeChunks{'+','.join(str(u(0xc2aaf30+4*i))for i in range(18))+'};']
    literals=[0xc1bdbc8,0xc1bdbcc,0xc1bdbd4,0xc1bdbd8,0xc1bdbe0,0xc1be0ac,0xc1be0b0,0xc1be0b4,0xc1be0b8,0xc1be0bc,0xc1be0c0,0xc1be0c8,0xc1be0d4,0xc1be0d8,0xc1be0dc,0xc1be0e0,0xc1be0e4,0xc1be0e8,0xc1be0ec,0xc1be0f0,0xc1be1ec,0xc1be1f0]
    source+=['inline constexpr unsigned lit_%08X=0x%08xu;'%(x,u(x))for x in literals]+['}']
    (a.project/'src/original_ranking_data.h').write_text('\n'.join(source)+'\n')
    report={'schema':'idas3-original-ranking-attract-v1','image_sha256':CANONICAL,'registered_child':12,'constructor':'0C02E8A0','init':'0C02EA60','main':'0C02F1C0','scene_draw':'0C02F8E0','background_draw':'0C02FA60','strings':{f'{x:08X}':s(x)for x in [0xc23bbbc,0xc23bbe4,0xc23bc08,0xc2aaf88,0xc2aafb4,0xc2aafe0,0xc2ab00c]},'banks':{k:{'chunks':v['chunk_count'],'textures':v['texture_count']}for k,v in banks.items()},'remaining_boundary':'Total leaderboard/native fresh records are implemented separately. Detail views1BDC00/1BE200, early car loading/slide phases, enclosing scene projection and reflection material override still gate complete owner integration.'}
    (roots/'provenance.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['banks']))
if __name__=='__main__':main()
