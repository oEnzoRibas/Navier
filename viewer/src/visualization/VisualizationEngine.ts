import * as THREE from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import type {RenderFieldData} from '../data/types';
import type {ScientificGeometry} from '../analysis/geometry';
import type {VisualizationSettings,Probe,CameraSnapshot} from './settings';
import {effectiveRange,colorBytes} from './colormap/colormaps';
import {scalarSlice,type SliceResult} from './scalar/scalarSlice';
import {geometryObject} from './vector/geometryObject';
import {isosurfaceObject} from './scalar/isosurfaceObject';
import {rayVolume} from './volume/rayVolume';
export interface RenderedField {
  field:RenderFieldData; settings:VisualizationSettings; range:readonly [number,number]; spacing:readonly [number,number,number];
  primaryGeometry?:ScientificGeometry; glyphs?:ScientificGeometry; streamlines?:ScientificGeometry;
}
export class VisualizationEngine {
  readonly renderer:THREE.WebGLRenderer;
  private scene=new THREE.Scene();private content=new THREE.Group();
  private camera:THREE.PerspectiveCamera|THREE.OrthographicCamera;
  private controls:OrbitControls;private observer:ResizeObserver;
  private textures:THREE.Texture[]=[];private current:RenderedField|null=null;private slice:SliceResult|null=null;
  private frame=0;private dirty=true;private disposed=false;private is3d=false;private sourceId='';private projection='orthographic';
  private onProbe:(probe:Probe|undefined)=>void=()=>{};
  private renderMs=0;private uploadMs=0;
  constructor(private canvas:HTMLCanvasElement){
    try{this.renderer=new THREE.WebGLRenderer({canvas,antialias:true,alpha:false});}catch{throw new Error('WebGL2 is unavailable. Enable GPU acceleration or open in a WebGL2-capable browser.');}
    this.renderer.setPixelRatio(Math.min(devicePixelRatio,2));this.renderer.setClearColor(0x101419);this.renderer.outputColorSpace=THREE.LinearSRGBColorSpace;
    this.camera=new THREE.OrthographicCamera(-.7,.7,.7,-.7,.01,100);this.camera.position.set(0,0,2);
    this.controls=new OrbitControls(this.camera,this.canvas);this.controls.enableDamping=true;this.controls.enableRotate=false;this.controls.mouseButtons.LEFT=THREE.MOUSE.PAN;this.controls.addEventListener('change',()=>this.dirty=true);
    this.scene.add(new THREE.AmbientLight(0xffffff,1.4));const light=new THREE.DirectionalLight(0xffffff,2);light.position.set(2,2,4);this.scene.add(light);
    this.scene.add(this.content);this.observer=new ResizeObserver(()=>this.resize());this.observer.observe(canvas.parentElement??canvas);
    this.canvas.addEventListener('pointermove',this.probe);this.resize();this.animate();
  }
  setProbeCallback(callback:(probe:Probe|undefined)=>void):void{this.onProbe=callback;}
  get limits(){const gl=this.renderer.getContext() as WebGL2RenderingContext;return {texture2d:this.renderer.capabilities.maxTextureSize,texture3d:Number(gl.getParameter(gl.MAX_3D_TEXTURE_SIZE))};}
  show(sourceId:string,frame:RenderedField):void{
    const is3d=frame.field.sourceDimensions[2]>1,projection=is3d?frame.settings.projection:'orthographic';
    const changedSource=this.sourceId!==sourceId;this.sourceId=sourceId;
    if(this.is3d!==is3d||this.projection!==projection)this.setCamera(is3d,projection);
    const start=performance.now();this.clearContent();this.current=frame;this.slice=null;
    const range=effectiveRange(frame.range[0],frame.range[1],frame.settings.colors),s=frame.settings;
    const maxTexture=this.limits.texture2d;
    if(s.mode==='scalar'||s.mode==='slice'||(!is3d&&(s.mode==='glyphs'||s.mode==='streamlines'))){
      if(Math.max(...frame.field.dimensions)>maxTexture)throw new Error(`GPU texture limit exceeded (${maxTexture}). Reduce visualization quality.`);
      this.slice=scalarSlice(frame.field,s,range);this.content.add(this.slice.mesh);this.textures.push(...this.slice.textures);
    }
    if(s.mode==='volume'){
      if(Math.max(...frame.field.dimensions)>this.limits.texture3d)throw new Error(`GPU 3D texture limit exceeded (${this.limits.texture3d}). Choose a slice or lower quality.`);
      const volume=rayVolume(frame.field,s,range);
      if(!this.renderer.extensions.has('OES_texture_float_linear'))for(const texture of volume.textures){if(texture instanceof THREE.Data3DTexture){texture.minFilter=THREE.NearestFilter;texture.magFilter=THREE.NearestFilter;}}
      this.content.add(volume.object);this.textures.push(...volume.textures);
    }
    if(s.mode==='isosurface'&&frame.primaryGeometry)this.content.add(isosurfaceObject(frame.primaryGeometry,s,range));
    else if(frame.primaryGeometry){const points=['cloud','sparse','wavefront'].includes(s.mode);const rendered=geometryObject(frame.primaryGeometry,s,range,points);this.content.add(rendered.object);this.textures.push(...rendered.textures);}
    for(const data of [frame.glyphs,frame.streamlines]){if(!data)continue;const rendered=geometryObject(data,{...s,colors:{...s.colors,opacity:1}},range,false);if(!is3d)rendered.object.position.z=.002;this.content.add(rendered.object);this.textures.push(...rendered.textures);}
    if(is3d){
      const outline=new THREE.LineSegments(new THREE.EdgesGeometry(new THREE.BoxGeometry(1,1,1)),new THREE.LineBasicMaterial({color:0x586470,transparent:true,opacity:.6}));this.content.add(outline);
      const axes=new THREE.AxesHelper(.65);axes.position.set(-.5,-.5,-.5);this.content.add(axes);
    }
    if(is3d){const extent=frame.field.sourceDimensions.map((n,i)=>n*frame.spacing[i]!);const longest=Math.max(...extent);this.content.scale.set(extent[0]!/longest,extent[1]!/longest,extent[2]!/longest);}else this.content.scale.set(1,1,1);
    if(changedSource)this.fit();
    this.uploadMs=performance.now()-start;this.dirty=true;this.renderNow();
  }
  /** Uniform/camera changes never request data or rebuild worker geometry. Reuse equal-sized GPU buffers between states. */
  updateAppearance(sourceId:string,data:Pick<RenderedField,'field'|'primaryGeometry'|'glyphs'|'streamlines'>,settings:VisualizationSettings,range:readonly [number,number]):boolean{
    const previous=this.current;
    if(!previous||this.sourceId!==sourceId||previous.settings.mode!==settings.mode||settings.vectorOverlay||settings.streamlineOverlay||['glyphs','streamlines','isosurface'].includes(settings.mode)||previous.field.dimensions.some((n,i)=>n!==data.field.dimensions[i]))return false;
    if(this.slice&&settings.sliceAxis!==previous.settings.sliceAxis)return false;
    const changedField=previous.field!==data.field;
    if(['cloud','sparse','wavefront'].includes(settings.mode)&&changedField){
      const points=this.content.children.find(o=>o instanceof THREE.Points);
      if(!(points instanceof THREE.Points)||!data.primaryGeometry)return false;
      const positions=points.geometry.getAttribute('position'),scalars=points.geometry.getAttribute('scalar');
      if(positions.array.length!==data.primaryGeometry.positions.length||scalars.array.length!==data.primaryGeometry.scalars.length)return false;
      positions.array=data.primaryGeometry.positions;positions.needsUpdate=true;scalars.array=data.primaryGeometry.scalars;scalars.needsUpdate=true;points.geometry.computeBoundingSphere();
    }
    const start=performance.now();
    if(this.slice&&(changedField||settings.slicePosition!==previous.settings.slicePosition)){
      const [nx,ny,nz]=data.field.dimensions,axis=this.slice.axis,width=this.slice.planeDimensions[0],height=this.slice.planeDimensions[1],size=axis==='x'?nx:axis==='y'?ny:nz;
      const location=Math.round(settings.slicePosition*(size-1));this.slice.location=location;
      const material=this.slice.mesh.material as THREE.ShaderMaterial,texture=material.uniforms.uField?.value as THREE.DataTexture;
      const values=texture.image.data as Float32Array;
      for(let y=0;y<height;y++)for(let x=0;x<width;x++){const ix=axis==='x'?location:x,iy=axis==='x'?x:axis==='y'?location:y,iz=axis==='z'?location:y;values[y*width+x]=data.field.renderValues[(iz*ny+iy)*nx+ix]??NaN;}
      texture.needsUpdate=true;if(nz>1)this.slice.mesh.position[axis]=location/Math.max(1,size-1)-.5;
    }
    if(settings.mode==='volume'&&changedField){const texture=this.textures.find(t=>t instanceof THREE.Data3DTexture);if(texture instanceof THREE.Data3DTexture){texture.image.data=data.field.renderValues;texture.needsUpdate=true;}}
    const paletteChanged=previous.settings.colors.palette!==settings.colors.palette;
    this.content.traverse(object=>{
      if(!(object instanceof THREE.Mesh||object instanceof THREE.Points))return;
      const material=object.material;if(Array.isArray(material)||!(material instanceof THREE.ShaderMaterial))return;
      const uniforms=material.uniforms,c=settings.colors;
      const values:Record<string,number|boolean>={uMin:range[0],uMax:range[1],uLog:c.logarithmic,uLow:Number.isFinite(c.thresholdLow)?c.thresholdLow:-1e30,uHigh:Number.isFinite(c.thresholdHigh)?c.thresholdHigh:1e30,uAbsolute:c.absoluteThreshold,uOpacity:c.opacity,uSize:settings.pointSize*Math.min(devicePixelRatio,2)};
      for(const [name,value] of Object.entries(values))if(uniforms[name])uniforms[name].value=value;
      if(paletteChanged&&uniforms.uPalette){const palette=uniforms.uPalette.value as THREE.DataTexture;palette.image.data=colorBytes(c.palette);palette.needsUpdate=true;}
    });
    if(this.is3d&&this.projection!==settings.projection)this.setCamera(true,settings.projection);
    this.current={...previous,...data,settings,range};this.uploadMs=performance.now()-start;this.dirty=true;return true;
  }
  private setCamera(is3d:boolean,projection:string):void{
    const previousPosition=this.camera.position.clone(),target=this.controls.target.clone();this.controls.dispose();this.is3d=is3d;this.projection=projection;
    this.camera=projection==='perspective'?new THREE.PerspectiveCamera(42,1,.01,100):new THREE.OrthographicCamera(-.8,.8,.8,-.8,.01,100);
    this.camera.position.copy(previousPosition);this.controls=new OrbitControls(this.camera,this.canvas);this.controls.target.copy(target);this.controls.enableDamping=true;this.controls.enableRotate=is3d;
    this.controls.mouseButtons.LEFT=is3d?THREE.MOUSE.ROTATE:THREE.MOUSE.PAN;this.controls.addEventListener('change',()=>this.dirty=true);this.resize();
  }
  fit():void{this.controls.target.set(0,0,0);this.camera.position.set(...(this.is3d?[1.65,1.2,1.85]:[0,0,2]) as [number,number,number]);this.camera.zoom=1;this.camera.updateProjectionMatrix();this.controls.update();this.dirty=true;}
  cameraSnapshot():CameraSnapshot{return {position:this.camera.position.toArray(),target:this.controls.target.toArray(),projection:this.projection as CameraSnapshot['projection']};}
  restoreCamera(snapshot:CameraSnapshot):void{if(snapshot.position.length!==3||snapshot.target.length!==3)return;this.camera.position.fromArray(snapshot.position);this.controls.target.fromArray(snapshot.target);this.controls.update();this.dirty=true;}
  private resize():void{
    const parent=this.canvas.parentElement;if(!parent)return;const width=Math.max(1,parent.clientWidth),height=Math.max(1,parent.clientHeight);
    this.renderer.setSize(width,height,false);const aspect=width/height;
    if(this.camera instanceof THREE.PerspectiveCamera)this.camera.aspect=aspect;
    else{this.camera.left=-.7*aspect;this.camera.right=.7*aspect;this.camera.top=.7;this.camera.bottom=-.7;}
    this.camera.updateProjectionMatrix();this.dirty=true;
  }
  private animate=():void=>{if(this.disposed)return;this.frame=requestAnimationFrame(this.animate);this.controls.update();if(this.dirty)this.renderNow();};
  private renderNow():void{const start=performance.now();this.renderer.render(this.scene,this.camera);this.renderMs=performance.now()-start;this.dirty=false;}
  exportPNG():string{this.renderNow();return this.canvas.toDataURL('image/png');}
  diagnostics(){return {uploadMs:this.uploadMs,renderMs:this.renderMs,textures:this.renderer.info.memory.textures,geometries:this.renderer.info.memory.geometries,calls:this.renderer.info.render.calls};}
  private probe=(event:PointerEvent):void=>{
    if(!this.current||!this.slice){this.onProbe(undefined);return;}
    const rect=this.canvas.getBoundingClientRect(),pointer=new THREE.Vector2((event.clientX-rect.left)/rect.width*2-1,-(event.clientY-rect.top)/rect.height*2+1),ray=new THREE.Raycaster();ray.setFromCamera(pointer,this.camera);
    const hit=ray.intersectObject(this.slice.mesh)[0];if(!hit?.uv){this.onProbe(undefined);return;}
    const field=this.current.field,[nx,ny,nz]=field.dimensions,[width,height]=this.slice.planeDimensions,px=Math.min(width-1,Math.floor(hit.uv.x*width)),py=Math.min(height-1,Math.floor(hit.uv.y*height));
    const axis=this.slice.axis,x=axis==='x'?this.slice.location:px,y=axis==='x'?px:axis==='y'?this.slice.location:py,z=axis==='z'?this.slice.location:py,index=(z*ny+y)*nx+x;
    if(x<0||y<0||z<0||z>=nz)return;
    this.onProbe({x:x*field.stride,y:y*field.stride,z:z*field.stride,value:field.values[index]??NaN,field:field.field.id});
  };
  clearFrame():void{this.clearContent();this.slice=null;this.dirty=true;this.renderNow();}
  private clearContent():void{
    this.content.traverse(object=>{if(object instanceof THREE.Mesh||object instanceof THREE.Line||object instanceof THREE.Points){object.geometry.dispose();const materials=Array.isArray(object.material)?object.material:[object.material];for(const material of materials)material.dispose();}});
    this.content.clear();for(const texture of this.textures)texture.dispose();this.textures=[];this.current=null;
  }
  dispose():void{this.disposed=true;cancelAnimationFrame(this.frame);this.observer.disconnect();this.controls.dispose();this.canvas.removeEventListener('pointermove',this.probe);this.clearContent();this.renderer.dispose();}
}
