import {defineConfig,type Plugin} from 'vite';
import react from '@vitejs/plugin-react';
import {createReadStream} from 'node:fs';
import {readdir,readFile,stat,open} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import type {IncomingMessage,ServerResponse} from 'node:http';
import {coreCommand} from './coreTransport';
const project=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
interface CatalogItem {id:string;name:string;size:number;url:string;kind:'core'|'legacy';metadata:unknown;path:string}
async function catalog():Promise<CatalogItem[]>{
  const files:string[]=[];
  async function walk(directory:string,depth:number){for(const entry of await readdir(directory,{withFileTypes:true})){if(entry.isFile()&&entry.name.endsWith('.data'))files.push(path.join(directory,entry.name));else if(entry.isDirectory()&&depth>0&&entry.name!=='node_modules'&&!entry.name.startsWith('build')&&!entry.name.startsWith('.'))await walk(path.join(directory,entry.name),depth-1);}}
  await walk(project,2);const result:CatalogItem[]=[];
  for(const file of files){try{const info=await stat(file),name=path.relative(project,file).replaceAll('\\','/'),id=createHash('sha256').update(name).digest('hex').slice(0,16),prefix=Buffer.alloc(8),handle=await open(file,'r');try{await handle.read(prefix,0,8,0);}finally{await handle.close();}
    const core=prefix.toString('ascii')==='NAVIER02';let metadata:unknown={};if(!core){if(prefix.toString('ascii')!=='NAVIER01')continue;const metaStat=await stat(file+'.json');if(metaStat.size>1048576)continue;metadata=JSON.parse(await readFile(file+'.json','utf8'));}
    result.push({id,name,path:file,size:info.size,url:core?`/__navier/core/${id}`:`/__navier/files/${id}`,kind:core?'core':'legacy',metadata});
  }catch{/* Invalid/incomplete metadata is not offered as a legacy example. */}}
  return result;
}
function localTransport():Plugin{
  const attach=(middlewares:{use:(fn:(req:IncomingMessage,res:ServerResponse,next:()=>void)=>void)=>void})=>{
    let listed:CatalogItem[]=[];
    middlewares.use((req,res,next)=>{const url=req.url?.split('?')[0];if(!url?.startsWith('/__navier/'))return next();
      void (async()=>{
        if(req.method!=='GET'){res.statusCode=405;res.end('Read-only transport');return;}
        if(url==='/__navier/catalog'){listed=await catalog();res.setHeader('Content-Type','application/json');res.end(JSON.stringify(listed.map(({path:_path,...item})=>item)));return;}
        const core=/^\/__navier\/core\/([a-f0-9]{16})\/(.+)$/.exec(url);
        if(core){const item=listed.find(f=>f.id===core[1]&&f.kind==='core');if(!item){res.statusCode=404;res.end('Core dataset not in catalog');return;}const endpoint=core[2]??'',list=/^list\/(\d+)\/(\d+)$/.exec(endpoint),checkpoint=/^checkpoint\/(\d+)$/.exec(endpoint),field=/^field\/(\d+)\/([^/]+)$/.exec(endpoint);let args:string[],binary=false;
          if(endpoint==='info')args=['info-json',item.path];else if(list){if(Number(list[2])>1000)throw new Error('Core transport pagination limit is 1000');args=['list-json',item.path,list[1]??'0','18446744073709551615',list[2]??'1000'];}else if(checkpoint)args=['checkpoint-json',item.path,checkpoint[1]??'0'];else if(field){const name=decodeURIComponent(field[2]??'');if(Buffer.byteLength(name)>255||name.includes('\0'))throw new Error('Invalid field name');args=['extract',item.path,field[1]??'0',name];binary=true;}else{res.statusCode=404;res.end('Unknown core operation');return;}
          const controller=new AbortController();res.on('close',()=>controller.abort());const output=await coreCommand(project,args,binary,controller.signal);if(res.destroyed)return;res.setHeader('Content-Type',binary?'application/octet-stream':'application/json');res.setHeader('Content-Length',Buffer.byteLength(output));res.setHeader('Cache-Control','no-store');res.end(output);return;
        }
        const id=/^\/__navier\/files\/([a-f0-9]{16})$/.exec(url)?.[1],item=listed.find(f=>f.id===id&&f.kind==='legacy');if(!item){res.statusCode=404;res.end('Dataset not found in catalog');return;}
        const range=/^bytes=(\d+)-(\d+)$/.exec(req.headers.range??'');if(!range){res.statusCode=416;res.end('Explicit byte range required');return;}const begin=Number(range[1]),end=Number(range[2]);if(!Number.isSafeInteger(begin)||!Number.isSafeInteger(end)||begin<0||end<begin||end>=item.size){res.statusCode=416;res.end('Range outside file');return;}
        res.statusCode=206;res.setHeader('Content-Range',`bytes ${begin}-${end}/${item.size}`);res.setHeader('Content-Length',end-begin+1);res.setHeader('Accept-Ranges','bytes');res.setHeader('Content-Type','application/octet-stream');res.setHeader('Cache-Control','no-store');const stream=createReadStream(item.path,{start:begin,end});res.on('close',()=>stream.destroy());stream.on('error',()=>res.destroy());stream.pipe(res);
      })().catch((error:unknown)=>{if(res.destroyed)return;if(res.headersSent){res.destroy();return;}res.statusCode=500;res.end(error instanceof Error?error.message:'Local transport error');});
    });
  };
  return {name:'navier-local-transport',configureServer(server){attach(server.middlewares);},configurePreviewServer(server){attach(server.middlewares);}};
}
export default defineConfig({plugins:[react(),localTransport()],server:{port:5173,strictPort:true,host:'127.0.0.1'},preview:{port:4173,strictPort:true,host:'127.0.0.1'},worker:{format:'es'},build:{target:'es2022',chunkSizeWarningLimit:900}});
