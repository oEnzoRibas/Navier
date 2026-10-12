import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const sandbox={console,Float64Array,Math,Map,WeakMap,devicePixelRatio:1};
vm.createContext(sandbox);
vm.runInContext(fs.readFileSync(new URL('./volume3d.js',import.meta.url),'utf8')+';globalThis.engine=Volume3D;',sandbox);
const engine=sandbox.engine,n=8,cells=n*n*n,k=2*Math.PI/n;
const metadata={nx:n,ny:n,nz:n,fields:['density','velocity_x','velocity_y','velocity_z']};
const dataset={metadata,velocity:true};
const values=new Float64Array(4*cells);
for(let z=0;z<n;z++)for(let y=0;y<n;y++)for(let x=0;x<n;x++){
    const i=(z*n+y)*n+x;values[i]=1;
    values[cells+i]=.035*(Math.sin(k*z)+Math.cos(k*y));
    values[2*cells+i]=.035*(Math.sin(k*x)+Math.cos(k*z));
    values[3*cells+i]=.035*(Math.sin(k*y)+Math.cos(k*x));
}
const frame={values},speed=engine.scalar(dataset,frame,'__speed__'),curl=engine.scalar(dataset,frame,'__vorticity__');
for(let i=0;i<cells;i++){
    assert.ok(Math.abs(speed[i]-Math.hypot(values[cells+i],values[2*cells+i],values[3*cells+i]))<1e-14);
    // The discrete periodic curl of the ABC field is sin(k) * velocity.
    assert.ok(Math.abs(curl[i]-Math.sin(k)*speed[i])<1e-14);
}
assert.throws(()=>engine.scalar(dataset,frame,'missing'));
const counters={};
const context=new Proxy({}, {get:(_,key)=> typeof key==='string'? (...args)=>{counters[key]=(counters[key]||0)+1}:undefined,set:()=>true});
const events={};const canvas={clientWidth:600,clientHeight:380,getContext:()=>context,addEventListener:(name,fn)=>events[name]=fn,setPointerCapture:()=>{}};
const palette=[[0,0,0],[255,255,255]],color=t=>[t*255,t*255,t*255];
for(const mode of ['lines','cloud','slice'])engine.render(canvas,dataset,frame,'__speed__',speed,0,.13,palette,color,mode,4,false,true);
assert.ok(counters.lineTo>100);assert.ok(counters.arc>100);assert.ok(counters.fill>60);
let redraws=0;engine.bind(canvas,()=>redraws++,()=>true);
events.pointerdown({clientX:0,clientY:0,button:0,shiftKey:false,pointerId:1,preventDefault(){}});
events.pointermove({clientX:40,clientY:20});events.pointerup();
events.wheel({deltaY:-100,preventDefault(){}});engine.reset();
assert.equal(redraws,3);
console.log('3D speed/curl, periodic derivatives, volume/slice/streamlines, rotation/zoom/reset: passed');
