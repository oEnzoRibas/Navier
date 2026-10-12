import {it,expect} from 'vitest';
import {readdir,readFile} from 'node:fs/promises';
import path from 'node:path';
import {parseMetadata} from '../src/parsers/metadata';
it('parses all existing project sidecars without specializing solver names',async()=>{const root=path.resolve('..'),names=(await readdir(root)).filter(name=>name.endsWith('.data.json'));for(const layout of ['planar-row-major, index=y*nx+x','planar-row-major, index=(z*ny+y)*nx+x','planar, index=(z*ny+y)*nx+x'])expect(parseMetadata({nx:2,ny:2,fields:['a'],layout}).fields.length).toBe(1);for(const name of names){const raw:unknown=JSON.parse(await readFile(path.join(root,name),'utf8'));const metadata=parseMetadata(raw);expect(metadata.fields.length,name).toBeGreaterThan(0);}});

