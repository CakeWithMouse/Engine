(() => {
 'use strict';
 const data = JSON.parse(document.getElementById('reference-data').textContent);
 const root = document.documentElement;
 root.classList.add('js');
 const $ = s => document.querySelector(s);
 const escape = s => String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 const search = $('#search'), results = $('#results');
 const sourceHash = (h,n=1)=>'#source/'+encodeURI(h)+'@'+n;
 let sourceFile = '', toastTimer;
 try { if(localStorage.getItem('painengine-api-theme')==='dark') root.classList.add('dark'); } catch {}
 function toast(message) {const t=$('#toast');t.textContent=message;t.classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>t.classList.remove('show'),2400);}
 function closeSearch(){results.hidden=true;}
 function route() {
  let hash; try{hash=decodeURI(location.hash.slice(1));}catch{hash='invalid';}
  let id = hash.split('--')[0] || 'overview';
  const sourceMatch = /^source\/(.+)@(\d+)$/.exec(hash);
  if(sourceMatch && Object.hasOwn(data.sources,sourceMatch[1])) {
   id='source'; const file=sourceMatch[1], line=Number(sourceMatch[2]);
   if(sourceFile!==file) {
    sourceFile=file;
    $('#source').innerHTML=`<div class="eyebrow">Исходники / снимок SDK ${escape(data.version.version)}</div><h1 tabindex="-1">${escape(file.split('/').pop())}</h1><p class="lead">Engine/${escape(file)}</p><div class="source-head"><a href="#headers">← Все заголовки</a><button type="button" data-source-copy>Копировать выдержку</button></div><p class="muted">Выдержка из SDK: внутренние операции движка исключены. Для сборки используйте оригинальные заголовки SDK. Номер строки — ссылка на объявление.</p><pre class="source-code" tabindex="0"><code>${data.sources[file].split('\n').map((s,i)=>`<span class="line" id="source-line-${i+1}"><a href="${sourceHash(file,i+1)}" aria-label="Строка ${i+1}">${i+1}</a>${escape(s)}</span>`).join('')}</code></pre>`;
   }
   $('#source').querySelectorAll('.target').forEach(e=>e.classList.remove('target'));
   const target=document.getElementById('source-line-'+line);
   if(target){target.classList.add('target');requestAnimationFrame(()=>target.scrollIntoView({block:'center',behavior:'instant'}));}
  } else if(!document.querySelector(`[data-page="${CSS.escape(id)}"]`)) id='not-found';
  document.querySelectorAll('.page').forEach(e=>e.classList.toggle('active',e.dataset.page===id));
  document.querySelectorAll('[data-nav]').forEach(a=>{if(a.dataset.nav===id||(id==='source'&&a.dataset.nav==='headers'))a.setAttribute('aria-current','page');else a.removeAttribute('aria-current');});
  root.classList.remove('nav-open');$('#menu').setAttribute('aria-expanded','false');closeSearch();
  const title=document.querySelector('.page.active h1');document.title=(title?.textContent||'PainEngine')+' — PainEngine API';
  if(!sourceMatch){
   document.querySelectorAll('tr.target').forEach(e=>e.classList.remove('target'));
   const anchor=document.getElementById(hash);
   if(hash.includes('--')&&anchor){anchor.classList.add('target');requestAnimationFrame(()=>anchor.scrollIntoView({block:'center',behavior:'instant'}));}
   else window.scrollTo({top:0,behavior:'instant'});
  }
 }
 function searchNow() {
  const q=search.value.trim().toLocaleLowerCase('ru');
  if(!q){closeSearch();return;}
  const terms=q.split(/\s+/), found=[];
  const matches=s=>terms.every(t=>s.toLocaleLowerCase('ru').includes(t));
  for(const p of data.pages){
   if(matches([p.title,p.summary,...p.paragraphs].join(' ')))found.push({title:p.title,detail:p.summary,url:'#'+p.id,kind:'Раздел',score:p.title.toLowerCase().includes(q)?100:20});
   for(const m of p.methods)if(matches(p.title+' '+m.name+' '+m.description))found.push({title:m.name,detail:p.title+' · '+m.description,url:'#'+p.id+'--'+m.name,kind:'API',score:m.name.toLowerCase()===q?150:m.name.toLowerCase().includes(q)?110:40});
  }
  // One source result per header: the first line containing all query terms, or a filename match.
  for(const [file,text] of Object.entries(data.sources)){
   const lines=text.split('\n'); const n=lines.findIndex(matches);
   if(n>=0||matches(file))found.push({title:file.split('/').pop(),detail:'Engine/'+file+(n>=0?' · '+lines[n].trim():''),url:sourceHash(file,n>=0?n+1:1),kind:'Заголовок',score:matches(file)?30:5});
  }
  found.sort((a,b)=>b.score-a.score||a.title.localeCompare(b.title));
  $('.search-summary').textContent=found.length?`Найдено: ${found.length}${found.length>45?' · показаны первые 45, уточните запрос':''}`:'Ничего не найдено. Попробуйте имя класса, метод или слово «камера».';
  $('.search-items').innerHTML=found.slice(0,45).map(x=>`<a href="${escape(x.url)}"><span class="result-kind">${escape(x.kind)}</span><strong>${escape(x.title)}</strong><small>${escape(x.detail.slice(0,240))}</small></a>`).join('');
  results.hidden=false;
 }
 async function copy(text) {
  try {if(!navigator.clipboard)throw Error('clipboard unavailable');await navigator.clipboard.writeText(text);toast('Скопировано');}
  catch {const t=document.createElement('textarea');t.value=text;t.style.cssText='position:fixed;left:-10000px;top:0';document.body.append(t);t.select();let ok=false;try{ok=document.execCommand('copy');}catch{}t.remove();toast(ok?'Скопировано':'Выделите код и скопируйте Ctrl+C');}
 }
 function download(name,text){const blob=new Blob([text],{type:'text/plain;charset=utf-8'}),url=URL.createObjectURL(blob),a=document.createElement('a');a.href=url;a.download=name;a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);}
 search.addEventListener('input',searchNow); search.addEventListener('focus',()=>{if(search.value.trim())searchNow();});
 search.addEventListener('keydown',e=>{if(e.key==='ArrowDown'){e.preventDefault();$('.search-items a')?.focus();}});
 results.addEventListener('keydown',e=>{if(e.key==='ArrowDown'||e.key==='ArrowUp'){e.preventDefault();const a=[...results.querySelectorAll('a')],i=a.indexOf(document.activeElement);(a[i+(e.key==='ArrowDown'?1:-1)]||search).focus();}});
 document.addEventListener('keydown',e=>{
  if(((e.ctrlKey||e.metaKey)&&e.key.toLowerCase()==='k')||(e.key==='/'&&!/INPUT|TEXTAREA/.test(document.activeElement.tagName))){e.preventDefault();search.focus();search.select();}
  if(e.key==='Escape'){closeSearch();search.blur();root.classList.remove('nav-open');$('#menu').setAttribute('aria-expanded','false');}
 });
 document.addEventListener('click',e=>{
  if(e.target.closest('.skip')){e.preventDefault();document.querySelector('.page.active h1')?.focus();return;}
  if(!e.target.closest('.search-wrap')&&!e.target.closest('#results'))closeSearch();
  const button=e.target.closest('button');
  if(button?.dataset.copy)copy(data.examples[button.dataset.copy].code);
  if(button?.dataset.download){const ex=data.examples[button.dataset.download];download(ex.filename,ex.code);}
  if(button?.hasAttribute('data-source-copy'))copy(data.sources[sourceFile]);
  if(button?.hasAttribute('data-source-download'))download(sourceFile.split('/').pop(),data.sources[sourceFile]);
  if(e.target.closest('#results a'))closeSearch();
  if(e.target.closest('nav a')){root.classList.remove('nav-open');$('#menu').setAttribute('aria-expanded','false');}
 });
 $('#menu').addEventListener('click',()=>{const open=root.classList.toggle('nav-open');$('#menu').setAttribute('aria-expanded',String(open));});
 $('#theme').addEventListener('click',()=>{const dark=root.classList.toggle('dark');try{localStorage.setItem('painengine-api-theme',dark?'dark':'light');}catch{}});
 $('#print').addEventListener('click',()=>window.print());
 window.addEventListener('hashchange',route);
 route();
})();
