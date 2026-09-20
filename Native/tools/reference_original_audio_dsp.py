"""Build/run only local Flycast DSP arithmetic in an isolated owned-RAM shim.
The resulting fixture is consumed by portable native tests. No full runtime,
sound CPU, device mapping, instruction-cycle emulator or audio device is used.
"""
from pathlib import Path
import subprocess,hashlib,json
project=Path(__file__).resolve().parents[1];work=project.parents[1]/'work';work.mkdir(exist_ok=True)
reference=Path(r'C:/Users/Developer/Documents/Codex/reference_sources/flycast/core/hw/aica')
source=(reference/'dsp_interp.cpp').read_text();codec=(reference/'dsp.cpp').read_text()
def function(text,signature):
 start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
 while depth:
  if text[end]=='{':depth+=1
  if text[end]=='}':depth-=1
  end+=1
 return text[start:end]
program=function(source,'void runStep()')
pack=function(codec,'u16 DYNACALL PACK').replace('u16 DYNACALL','static u16')
unpack=function(codec,'s32 DYNACALL UNPACK').replace('s32 DYNACALL','static s32')
header='''// Generated numerical-only reference. Original arithmetic source:
// Flycast dsp_interp.cpp (Audio Overload SDK, Copyright2007-2009 R. Belmont,
// Richard Bannister and others) and dsp.cpp (skmp/nullDC/reicast).
// Local source copyright/license remain applicable. Do not use as production code.
#pragma once
#include "original_audio_dsp.h"
#include <algorithm>
#include <cstring>
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;
using s16=std::int16_t;using s32=std::int32_t;using s64=std::int64_t;
struct DspScalarReference{
 struct State{s32 TEMP[128]{},MEMS[32]{},MIXS[16]{};u32 RBL=32767,RBP=0,MDEC_CT=1;bool stopped=false;}state;
 struct Data{u32 COEF[128]{},MADRS[64]{},MPRO[512]{},EXTS[2]{},EFREG[16]{};}data;
 Data*DSPData=&data;std::vector<u8>aica_ram=std::vector<u8>(131072);static constexpr u32 ARAM_MASK=131071;
 explicit DspScalarReference(const idas3::OriginalAudioDspProgram&p){
  state.RBL=p.ringLengthWords-1;
  for(unsigned i=0;i<128;++i){data.COEF[i]=u16(p.coefficients[i]);for(unsigned j=0;j<4;++j)data.MPRO[i*4+j]=p.instructions[i][j];}
  for(unsigned i=0;i<64;++i)data.MADRS[i]=p.memoryAddresses[i];
  for(unsigned i=0;i<65536;++i){u16 v=i<p.initialMemoryWords.size()?p.initialMemoryWords[i]:0x6000;aica_ram[i*2]=u8(v);aica_ram[i*2+1]=u8(v>>8);}
 }
 void frame(const std::array<s32,16>&input){std::copy(input.begin(),input.end(),state.MIXS);runStep();}
'''+pack+'\n'+unpack+'\n'+program+'\n};\n'
(work/'dsp_scalar_reference.generated.h').write_text(header)
out=project/'verification/original-audio-dsp';out.mkdir(parents=True,exist_ok=True)
cmd=work/'build_dsp_reference.cmd'
cmd.write_text('@echo off\ncall "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvarsall.bat" x64 >nul\n'
 f'cl /nologo /std:c++20 /O2 /W4 /EHsc /I "{project / "src"}" /I "{project / "tests"}" /I "{work}" "{project / "tools/reference_original_audio_dsp.cpp"}" "{project / "src/original_audio_dsp_bank.cpp"}" /Fo:"{work}/" /Fe:"{work / "reference_original_audio_dsp.exe"}"\n'
 'if errorlevel 1 exit /b 1\n'
 f'"{work / "reference_original_audio_dsp.exe"}" "{project}" "{out / "scalar_reference.bin"}"\n')
subprocess.run(['cmd','/c',str(cmd)],check=True)
(out/'numerical_reference.json').write_text(json.dumps(dict(scope='Only supplied scalar DSP arithmetic; owned RAM, no hardware or full runtime. Used-program tests cover two-step read latency and odd-only SRAM operation ordering. Next-step write commit equals immediate write for this subset.',
 sources={n:hashlib.sha256((reference/n).read_bytes()).hexdigest() for n in ['dsp_interp.cpp','dsp.cpp']},
 notesSha256=hashlib.sha256((reference.parents[2]/'docs/neil_corlett_aica_notes.txt').read_bytes()).hexdigest(),
 fixtureSha256=hashlib.sha256((out/'scalar_reference.bin').read_bytes()).hexdigest(),
 samplesPerPreset=131072,initialRingPhases=[1,17329],codecUnpackInputs=65536,codecPackInputs=16777216,
excludedInstructions=['NOFL','TABLE','ADRL','ADREB','even-step SRAM operations','IWT without MRD two instructions before','same MEMS read/write bypass']),indent=2))

