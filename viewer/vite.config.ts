import {defineConfig,type Plugin} from 'vite';
import react from '@vitejs/plugin-react';
import {createReadStream} from 'node:fs';
import {readdir,readFile,stat} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import type {IncomingMessage,ServerResponse} from 'node:http';
const project=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
interface CatalogItem {id:string;name:string;size:number;url:string;metadata:unknown;path:string}
async function catalog():Promise<CatalogItem[]>{
  const files:string[]=[];
  async function walk(directory:string,depth:number){
    for(const entry of await readdir(directory,{withFileTypes:true})){
      if(entry.isFile()&&entry.name.endsWith('.data'))files.push(path.join(directory,entry.name));
      else if(entry.isDirectory()&&depth>0&&entry.name!=='node_modules'&&!entry.name.startsWith('build')&&!entry.name.startsWith('.'))await walk(path.join(directory,entry.name),depth-1);
    }
  }
  await walk(project,2);const result:CatalogItem[]=[];
  for(const file of files){try{const metaStat=await stat(file+'.json');if(metaStat.size>1024*1024)continue;const metadata:unknown=JSON.parse(await readFile(file+'.json','utf8'));const info=await stat(file);const id=String(result.length);result.push({id,name:path.relative(project,file).replaceAll('\\','/'),path:file,size:info.size,url:`/__navier/files/${id}`,metadata});}catch{/* Files without valid sidecars are not offered as examples. */}}
  return result;
}
function localRanges():Plugin{
  const attach=(middlewares:{use:(fn:(req:IncomingMessage,res:ServerResponse,next:()=>void)=>void)=>void})=>{
    let listed:CatalogItem[]=[];
    middlewares.use((req,res,next)=>{
      const url=req.url?.split('?')[0];if(!url?.startsWith('/__navier/'))return next();
      void (async()=>{
        if(url==='/__navier/catalog'){listed=await catalog();res.setHeader('Content-Type','application/json');res.end(JSON.stringify(listed.map(({path:_path,...item})=>item)));return;}
        const id=/^\/__navier\/files\/(\d+)$/.exec(url)?.[1];const item=listed.find(f=>f.id===id);
        if(!item){res.statusCode=404;res.end('Dataset not found in local catalog');return;}
        const range=/^bytes=(\d+)-(\d+)$/.exec(req.headers.range??'');
        if(!range){res.statusCode=416;res.end('Explicit byte range required');return;}
        const begin=Number(range[1]),end=Number(range[2]);
        if(!Number.isSafeInteger(begin)||!Number.isSafeInteger(end)||begin<0||end<begin||end>=item.size){res.statusCode=416;res.end('Range outside file');return;}
        res.statusCode=206;res.setHeader('Content-Range',`bytes ${begin}-${end}/${item.size}`);res.setHeader('Content-Length',end-begin+1);res.setHeader('Accept-Ranges','bytes');res.setHeader('Content-Type','application/octet-stream');res.setHeader('Cache-Control','no-store');
        const stream=createReadStream(item.path,{start:begin,end});res.on('close',()=>stream.destroy());stream.on('error',()=>res.destroy());stream.pipe(res);
      })().catch((error:unknown)=>{res.statusCode=500;res.end(error instanceof Error?error.message:'Local file server error');});
    });
  };
  return {name:'local-checkpoint-ranges',configureServer(server){attach(server.middlewares);},configurePreviewServer(server){attach(server.middlewares);}};
}
export default defineConfig({plugins:[react(),localRanges()],server:{port:5173,strictPort:true,host:'127.0.0.1'},preview:{port:4173,strictPort:true,host:'127.0.0.1'},worker:{format:'es'},build:{target:'es2022',chunkSizeWarningLimit:900}});
