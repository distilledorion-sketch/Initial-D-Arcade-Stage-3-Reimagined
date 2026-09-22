import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';

const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
const declaration=html.match(/^const nameOf=.*;$/m)[0];
const nameOf=runInNewContext(declaration+'\nnameOf;');

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
 assert.equal(nameOf([198,221,221,221,221]),'◇');
});
