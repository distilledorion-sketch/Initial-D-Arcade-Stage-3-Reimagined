import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInNewContext} from 'node:vm';
const html=readFileSync(new URL('../src/index.html',import.meta.url),'utf8');
const functionText=html.match(/^function updateDirections\(\).*$/m)[0];
test('Odawara labels match the physical routes without moving stable board IDs',()=>{
    for(let course=0;course<18;course++){
        const options=[{value:'0'},{value:'1'}];
        const $=id=>id==='course'?{value:String(course)}:{options};
        runInNewContext(functionText+';updateDirections();',{$});
        const expected=course===17?['Clockwise','Counterclockwise']:course<2?['Counterclockwise','Clockwise']:
            [4,6,7,16].includes(course)?['Outbound','Inbound']:['Downhill',course===5?'Reverse':'Uphill'];
        assert.deepEqual(options.map(o=>o.textContent),expected);
        assert.deepEqual(options.map(o=>o.value),['0','1']);
    }
});
