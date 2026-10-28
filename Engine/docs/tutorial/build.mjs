import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { createHash } from 'node:crypto';

const workspaceDir = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(workspaceDir, '../../..');
const runtimeModules = process.env.RUNTIME_NODE_MODULES;
const python = process.env.RUNTIME_PYTHON;
const skill = process.env.PRESENTATIONS_SKILL;
if (!runtimeModules || !python || !skill) throw new Error('Set RUNTIME_NODE_MODULES, RUNTIME_PYTHON and PRESENTATIONS_SKILL from load_workspace_dependencies and the installed presentation skill.');
const { Presentation, PresentationFile } = await import(pathToFileURL(path.join(runtimeModules, '@oai/artifact-tool/dist/artifact_tool.mjs')).href);
const { finalizePresentation } = await import(pathToFileURL(path.join(skill, 'container_tools/artifact_tool_utils.mjs')).href);
const data = JSON.parse(await fs.readFile(path.join(workspaceDir, 'content.json'), 'utf8'));
const stem = `PainEngine-Tutorial-${data.date}-r${data.revision}`;
const buildDir = path.join(workspaceDir, '.build', stem);
const output = path.join(workspaceDir, 'releases', stem + '-final.pptx');
await fs.mkdir(buildDir, { recursive: true });
await fs.mkdir(path.dirname(output), { recursive: true });
const deck = Presentation.create({ slideSize: { width: 1280, height: 720 } });
const font = 'Arial';
function text(slide, value, x, y, w, h, size=26, color='#243646', bold=false, face=font) {
  const t = slide.shapes.add({ geometry: 'textbox', position: { left:x, top:y, width:w, height:h }, fill:'none', line:{ fill:'none', width:0 } });
  t.text = value;
  t.text.style = { typeface:face, fontSize:size, color, bold };
  return t;
}
async function picture(slide, file, position, fit='contain') {
  const b=await fs.readFile(path.join(workspaceDir,'assets',file));
  slide.images.add({blob:b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength),contentType:'image/png',alt:'Скриншот PainEngine: '+file,position,fit});
}
function block(slide,x,y,w,h,color) {
  slide.shapes.add({geometry:'rect',position:{left:x,top:y,width:w,height:h},fill:color,line:{fill:'none',width:0}});
}
const tableSlides=[];
for (const [index,d] of data.slides.entries()) {
  const slide=deck.slides.add();
  const dark=d.type==='cover'||d.type==='code';
  slide.background.fill=dark?'#101E2D':'#E7EFF2';
  const titleColor=dark?'#F5F9FA':'#102737';
  if(d.type==='cover') {
    block(slide,0,0,480,720,'#133C4C');
    text(slide,d.title,48,90,405,280,52,'#FFFFFF',true);
    text(slide,d.subtitle,48,430,390,110,29,'#BFE0E5');
    await picture(slide,d.image,{left:510,top:130,width:730,height:440});
    text(slide,`${data.engineVersion}  /  28.10.2026`,48,628,650,38,22,'#A6C5D1');
  } else {
    block(slide,0,0,1280,124,dark?'#183449':'#D1E3E8');
    text(slide,d.title,48,35,1184,80,40,titleColor,true);
    if(d.type==='table') {
      tableSlides.push(index+1);
      const cols=d.headers.length, rows=d.rows.length+1;
      const widths=cols===2?[430,722]:[270,400,482];
      const table=slide.tables.add({rows,columns:cols,left:64,top:154,width:1152,height:rows*64,columnWidths:widths,values:[d.headers,...d.rows]});
      table.borders.assign({fill:'#D9E1E5',width:1,style:'solid'});
      table.cells.block({row:0,column:0,rowCount:rows,columnCount:cols}).assign({
        fill:'#EEF4F5',textStyle:{typeface:font,fontSize:24,color:'#243646'},margins:{left:14,right:14,top:12,bottom:10}
      });
      table.cells.block({row:0,column:0,rowCount:1,columnCount:cols}).assign({fill:'#13384A',textStyle:{typeface:font,fontSize:24,bold:true,color:'#FFFFFF'}});
    } else if(d.type==='code') {
      block(slide,40,148,840,475,'#0A1420');
      text(slide,d.code,56,166,812,440,21,'#BCE8ED',false,'Consolas');
      text(slide,d.aside,914,164,318,450,25,'#E1E9EF');
    } else if(d.type==='shot') {
      block(slide,40,148,390,475,'#D7E6EA');
      await picture(slide,d.image,{left:452,top:148,width:780,height:475});
      const step=d.items.length===2?220:151;
      for(const [i,item] of d.items.entries()) {
        text(slide,item[0],60,165+i*step,350,44,28,'#066D80',true);
        text(slide,item[1],60,210+i*step,350,step-48,24);
      }
    } else if(d.type==='screenshot') {
      await picture(slide,'editor-panels.png',{left:64,top:158,width:640,height:442});
      for(const [i,item] of d.items.entries()) {
        text(slide,item[0],756,175+i*143,460,42,29,'#08778E',true);
        text(slide,item[1],756,222+i*143,460,94,25);
      }
    } else {
      for(const [i,item] of d.items.entries()) {
        text(slide,item[0],64,175+i*140,340,100,30,'#08778E',true);
        text(slide,item[1],440,178+i*140,776,106,28);
      }
    }
    if(d.foot) text(slide,d.foot,48,640,1138,55,20,dark?'#B1C5D2':'#4D6272');
    text(slide,String(index+1).padStart(2,'0'),1180,684,40,24,16,dark?'#9BB3C4':'#66808F');
  }
  slide.speakerNotes.text=d.notes+'\n\nИсточники в репозитории O:/Engine/Engine:\n'+d.sources.join('\n');
  await fs.writeFile(path.join(buildDir,`slide-${String(index+1).padStart(2,'0')}.png`),new Uint8Array(await (await deck.export({slide,format:'png',scale:1})).arrayBuffer()));
  console.log(`Rendered slide ${index+1}/${data.slides.length}`);
}
const candidate=path.join(buildDir,'candidate.pptx');
await (await PresentationFile.exportPptx(deck)).save(candidate);
await finalizePresentation({
  workspaceDir,candidatePath:candidate,finalPath:output,pythonExecutable:python,
  integrityValidatorPath:path.join(skill,'container_tools/inspect_presentation_package_integrity.py'),
  layoutValidatorPath:path.join(skill,'container_tools/inspect_presentation_layout_geometry.py'),
  layoutArgs:['--expected-slide-size-emu','12192000,6858000','--validate-heading-fit',...tableSlides.flatMap(n=>['--require-native-table-slide',String(n)])],
  requiredNativeTableOwnerSlides:tableSlides,fontPolicy:{basis:'design',families:[font,'Consolas']},
  verifyArtifactToolImport:true,receiptPath:path.join(buildDir,'final-validation.json')
});
const pdfPath=output.replace(/\.pptx$/,'.pdf');
await fs.writeFile(pdfPath,new Uint8Array(await (await deck.export({format:'pdf'})).arrayBuffer()));
// A source manifest makes updates detect code changes even without a version bump.
const sourcePaths=[...new Set(data.slides.flatMap(s=>s.sources))].filter(s=>!s.endsWith('/current.json'));
const sources=[];
for(const relative of sourcePaths) {
  const bytes=await fs.readFile(path.join(repo,relative));
  sources.push({path:relative,sha256:createHash('sha256').update(bytes).digest('hex')});
}
await fs.writeFile(path.join(buildDir,'pending-current.json'),JSON.stringify({
  title:data.title.replace('\n',' — '),engineVersion:data.engineVersion,date:data.date,revision:data.revision,
  slideCount:data.slides.length,presentation:path.relative(workspaceDir,output).replaceAll('\\','/'),
  pdf:path.relative(workspaceDir,pdfPath).replaceAll('\\','/'),content:'content.json',builder:'build.mjs',editorialPolicy:'editorial-policy.json',sources
},null,2));
console.log(`Finalized ${output}. Review every rendered slide, then publish pending-current.json as current.json.`);
