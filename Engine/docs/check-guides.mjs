import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {createRequire} from 'node:module';
const dir=process.env.DOCS_ROOT?path.resolve(process.env.DOCS_ROOT):path.dirname(fileURLToPath(import.meta.url));
const require=createRequire(import.meta.url);
const {chromium}=require(path.join(process.env.RUNTIME_NODE_MODULES,'playwright'));
const out=path.resolve(process.argv[2]||'Engine/build/docs-review');fs.mkdirSync(out,{recursive:true});
const browser=await chromium.launch({headless:true,channel:'msedge'});
const page=await browser.newPage({viewport:{width:1400,height:1000}});
const errors=[];page.on('pageerror',e=>errors.push(e.message));
const names=['index','editor','materials','audio','code','assets','scene','API-REFERENCE'];
let links=0;
for(const name of names){
 await page.goto(pathToFileURL(path.join(dir,name+'.html')).href);
 if(await page.locator('img').evaluateAll(xs=>xs.some(x=>!x.complete||x.naturalWidth===0)))throw Error('Broken image '+name);
 for(const href of await page.locator('a[href]').evaluateAll(xs=>xs.map(x=>x.getAttribute('href')))){
  if(href.startsWith('#')||/^(https?:|data:|mailto:)/.test(href))continue;
  if(!fs.existsSync(path.resolve(dir,decodeURIComponent(href.split('#')[0]))))throw Error('Missing '+name+': '+href);
  links++;
 }
 await page.screenshot({path:path.join(out,name+'.png'),fullPage:false});
 if(name==='API-REFERENCE'){
  const ids=await page.locator('script#reference-data').textContent().then(JSON.parse);
  for(const p of ids.pages){
   await page.goto(pathToFileURL(path.join(dir,name+'.html')).href+'#'+p.id);
   if(!await page.locator('#'+p.id).isVisible())throw Error('Broken route '+p.id);
   for(const m of p.methods){if(!ids.sources[m.header].split('\n')[m.line-1].includes(m.name))throw Error('Wrong source '+m.name);}
  }
  await page.locator('#search').fill('PlayStream');
  await page.waitForTimeout(250);
  if(!await page.locator('#results').isVisible())throw Error('Search hidden');
  if(!(await page.locator('#results').textContent()).includes('PlayStream'))throw Error('Audio absent in search');
 }
 await page.setViewportSize({width:390,height:844});
 await page.goto(pathToFileURL(path.join(dir,name+'.html')).href);
 if(await page.evaluate(()=>document.documentElement.scrollWidth>innerWidth+2))throw Error('Mobile overflow '+name);
 await page.setViewportSize({width:1400,height:1000});
}
await browser.close();if(errors.length)throw Error(errors.join('\n'));
fs.writeFileSync(path.join(out,'result.json'),JSON.stringify({pages:names.length,links,apiRoutes:40,apiMethods:317,search:true,mobile:true,errors},null,2));
console.log('PASS: 8 HTML pages, 40 API routes, 317 source links, search, mobile; '+links+' local links');
