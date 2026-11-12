import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
const declaration=html.match(/^const (?:nameGlyphs|nameOf)=.*;$/gm).join('\n');
const nameOf=runInNewContext(declaration+'\nnameOf;');

test('website names preserve every glyph in the original name-entry table',()=>{
 const source=readFileSync(new URL('../../Native/data/original_frontend/name_entry.bin',import.meta.url));
 assert.equal(source.readUInt32LE(0),0x454e4449);
 assert.equal(source.readUInt32LE(4),1);
 assert.equal(source.readUInt32LE(8),221);
 const decoder=new TextDecoder('euc-jp',{fatal:true}),mismatches=[];
 for(let glyph=0;glyph<221;glyph++){
  const bytes=source.subarray(20+glyph*2,22+glyph*2);
  // WHATWG uses the Windows mapping for A1C1; the game's exported JIS
  // Unicode table uses U+301C (wave dash), not U+FF5E (fullwidth tilde).
  let expected=bytes[0]===0xa1&&bytes[1]===0xc1?'\u301c':decoder.decode(bytes);
  // Match OriginalVsBanner::decodeName's existing readable ASCII display.
  expected=expected.replace(/[\uff01-\uff5e]/g,c=>String.fromCharCode(c.charCodeAt(0)-0xfee0)).replace(/\u3000/g,' ');
  const actual=nameOf([glyph,221,221,221,221]);
  if(actual!==expected)mismatches.push({glyph,expected,actual});
 }
 assert.deepEqual(mismatches,[]);
});

test('website name decoder matches all original numeric glyphs',()=>{
 // Original name_entry.bin EUC-JP glyph records: A3B1..A3B9, then A3B0.
 const digits=[[197,'0'],[188,'1'],[189,'2'],[190,'3'],[191,'4'],
               [192,'5'],[193,'6'],[194,'7'],[195,'8'],[196,'9']];
 for(const [glyph,digit] of digits)assert.equal(nameOf([glyph,221,221,221,221]),digit);
});

test('mixed driver names preserve zero, padding, letters and spaces',()=>{
 assert.equal(nameOf([164,179,197,184,221]),'CR0W');
 assert.equal(nameOf([162,188,196,197,187]),'A190Z');
 assert.equal(nameOf([162,220,197,221,221]),'A 0');
 assert.equal(nameOf([221,221,221,221,221]),'DRIVER');
 assert.equal(nameOf([198,221,221,221,221]),'<');
});

test('Japanese, symbol-only and mixed names remain distinct',()=>{
 assert.equal(nameOf([0,6,20,221,221]),'アキナ');
 assert.equal(nameOf([81,87,101,221,221]),'あきな');
 assert.equal(nameOf([78,119,143,55,221]),'プっじー');
 assert.equal(nameOf([198,200,211,212,199]),'<+×○>');
 assert.equal(nameOf([179,177,174,210,180]),'RPM・S');
 assert.equal(nameOf([219,220,213,214,221]),'〜 !?');
 const glyphs=[162,220,197,221,221],before=[...glyphs];
 nameOf(glyphs);
 assert.deepEqual(glyphs,before);
});
