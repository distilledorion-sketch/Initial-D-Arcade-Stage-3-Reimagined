import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
const declarations=['cars','carGroups'].map(name=>html.match(new RegExp(`^const ${name}=.*;$`,'m'))[0]);
const {cars,carGroups}=runInNewContext(declarations.join('\n')+'\n({cars,carGroups});');
test('make grouping retains every stable leaderboard car ID exactly once',()=>{
    const ids=Array.from(carGroups.flatMap(([,ids])=>ids)).sort((a,b)=>a-b);
    assert.deepEqual(ids,Array.from({length:35},(_,id)=>id));
    assert.equal(cars.length,35);
    assert.equal(new Set(cars).size,35);
    assert.equal(cars[13],'RPS13 180SX');assert.equal(cars[14],'RPS13 SILEIGHTY');
    for(const [id,label] of [[10,"S14 SILVIA Q'S"],[11,"S14 SILVIA K'S"],[22,'FD3S RX-7 TYPE RS'],[23,'FD3S RX-7 TYPE R'],[27,'GC8 IMPREZA TYPE R STi VI'],[29,'GC8 IMPREZA TYPE R STi V'],[31,'ER34 SKYLINE 25GT TURBO']])assert.equal(cars[id],label);
});
