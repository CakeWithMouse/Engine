// node Engine/docs/api-reference/build.mjs [--check]
// No npm dependencies. Internal geometry setup is deliberately absent from the reader's API.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {pages, groups} from './content.mjs';
const dir = path.dirname(fileURLToPath(import.meta.url));
const engine = path.resolve(dir, '../..');
const include = path.join(engine, 'Include/Engine');
const version = JSON.parse(fs.readFileSync(path.join(engine, 'Version.json'), 'utf8'));
const read = p => fs.readFileSync(p, 'utf8').replace(/\r\n/g, '\n');
const cover = 'data:image/png;base64,' + fs.readFileSync(path.join(dir,'assets/painengine-cover.png')).toString('base64');
const esc = s => String(s).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const inline = s => esc(s).replace(/`([^`]+)`/g, '<code>$1</code>');
function files(root) { return fs.readdirSync(root, {withFileTypes:true}).sort((a,b)=>a.name.localeCompare(b.name, 'en')).flatMap(e => e.isDirectory() ? files(path.join(root,e.name)) : [path.join(root,e.name)]); }
const publicSnapshot = text => text.replace(/[^\n]*\/\/ @engine-internal-begin[\s\S]*?\/\/ @engine-internal-end[^\n]*/g,
 block=>block.replace(/[^\n]/g, '')); // Preserve original line numbers, omit internal implementation hooks.
const sources = Object.fromEntries(files(include).filter(f=>/\.(h|inl)$/.test(f)).map(f=>[path.relative(include,f).replaceAll('\\','/'), publicSnapshot(read(f))]));
if (/\b(?:RenameComponent|GetSnapshot|GetRevision)\b/.test(sources['Scene/Scene.h']))
 throw Error('Internal Scene bookkeeping leaked into the developer reference');
const digest = crypto.createHash('sha256').update(JSON.stringify(sources)).digest('hex').slice(0,12);
const cpp = read(path.join(dir,'examples/ReferenceExamples.cpp'));
const component = read(path.join(dir,'examples/ReferenceComponent.h'));
const examples = {component:{title:'ReferenceComponent.h · 3 свойства + 3 внутренних поля', code:component, filename:'ReferenceComponent.h'}, all:{title:'ReferenceExamples.cpp · полный файл',code:cpp,filename:'ReferenceExamples.cpp'}};
for (const match of cpp.matchAll(/\/\/ example:(\w+)\n([\s\S]*?)\/\/ endexample/g)) examples[match[1]] = {title:({settings:'Настроить Game и завершить standalone',scene:'Создать компонент',world:'Луч в мировом пространстве',screen:'Пиксель, курсор и отдельный луч',mesh:'Процедурная плоскость',input:'Подписка на действие',ui:'Простой HUD',resources:'Прочитать ресурс из DLL'})[match[1]],code:match[2].trim()};
const ids = new Set(pages.map(p=>p.id));
if(ids.size!==pages.length) throw Error('Duplicate page IDs');
const clean = text => text.replace(/\/\*[\s\S]*?\*\/|\/\/[^\n]*/g,m=>m.replace(/[^\n]/g,' '));
const cleanSources = Object.fromEntries(Object.entries(sources).map(([k,v])=>[k,clean(v).split('\n')]));
let methodCount = 0;
for (const p of pages) {
 if(p.header && !sources[p.header]) throw Error('Missing header '+p.header);
 for(const id of [...(p.related||[]),...(p.cards||[])]) if(!ids.has(id)) throw Error('Invalid related page '+id);
 for(const id of p.examples||[]) if(!examples[id]) throw Error('Unknown example '+id);
 p.methods = p.methods.map(([name,description,header])=>{
  header ||= p.header;
  if(!sources[header]) throw Error('Missing method header '+header);
  const re = new RegExp('\\b'+name.replace(/[.*+?^${}()|[\]\\]/g,'\\$&')+'\\b');
  const line = cleanSources[header].findIndex(l=>re.test(l))+1;
  if(!line) throw Error(`Symbol ${name} missing in ${header}`);
  ++methodCount;
  return {name,description,header,line};
 });
}
const sourceHref = (h,line=1)=>'#source/'+encodeURI(h)+'@'+line;
const pageLink = id => {const p=pages.find(p=>p.id===id);return `<a href="#${id}">${esc(p.title)} <span aria-hidden="true">↗</span></a>`;};
function codeBlock(id) {
 const ex=examples[id];
 return `<section class="example"><div class="code-title"><span>${esc(ex.title)}</span><div>${ex.filename?`<button type="button" data-download="${id}" aria-label="Скачать ${esc(ex.filename)}">Скачать</button>`:''}<button type="button" data-copy="${id}" aria-label="Копировать ${esc(ex.title)}">Копировать</button></div></div><pre tabindex="0"><code>${esc(ex.code)}</code></pre>${ex.filename?'':`<p class="code-note">Фрагмент из ReferenceExamples.cpp. Полный файл с includes — в разделе <a href="#examples">Примеры</a>.</p>`}</section>`;
}
function renderPage(p) {
 return `<article class="page" id="${p.id}" data-page="${p.id}">
 <div class="eyebrow">${esc(groups[p.group])} <span>/</span> ${p.header?'C++ API':'Руководство'}</div>
 <h1 tabindex="-1">${esc(p.title)}</h1><p class="lead">${esc(p.summary)}</p>
 ${p.id==='overview'?`<figure class="engine-cover"><img src="${cover}" width="1672" height="941" alt="Куб, сфера и цилиндр на сетке в синем освещении"><figcaption>Иллюстрация PainEngine · базовые формы игрового мира</figcaption></figure>`:''}
 ${p.header?`<a class="include" href="${sourceHref(p.header)}"><span>#include</span> &lt;Engine/${esc(p.header)}&gt; <b>Открыть исходник ↗</b></a>`:''}
 ${p.id==='overview'?`<div class="hero-map"><span>Game<small>сеанс и сервисы</small></span><i>→</i><span>Scene<small>владелец объектов</small></span><i>→</i><span>GameComponent<small>данные и поведение</small></span></div><div class="stats"><div><strong>${pages.length}</strong><span>разделов</span></div><div><strong>${methodCount}</strong><span>описаний API</span></div><div><strong>${Object.keys(sources).length}</strong><span>заголовков SDK</span></div><div><strong>C++17</strong><span>Windows · D3D11</span></div></div>`:''}
 ${p.id==='overview'?`<p class="muted">Scene JSON v${version.sceneFormat} · Module ABI v${version.moduleApi} · Edit API v${version.editApi} (поддержаны: ${version.supportedEditApis.join(', ')})</p>`:''}
 <div class="prose">${p.paragraphs.map(t=>`<p>${inline(t)}</p>`).join('')}</div>
 ${p.note?`<aside class="note"><strong>Границы API</strong><p>${inline(p.note)}</p></aside>`:''}
 ${p.cards?`<div class="cards">${p.cards.map(id=>{const x=pages.find(p=>p.id===id);return `<a href="#${id}"><span class="card-group">${esc(groups[x.group])}</span><strong>${esc(x.title)} <span>↗</span></strong><p>${esc(x.summary)}</p></a>`;}).join('')}</div>`:''}
 ${p.methods.length?`<h2>Методы и типы <span class="count">${p.methods.length}</span></h2><p class="muted">Имя открывает точное объявление в заголовке. <span>¶</span> — ссылка на описание.</p><div class="table-scroll"><table><thead><tr><th>Имя</th><th>Назначение и контракт</th></tr></thead><tbody>${p.methods.map(m=>`<tr id="${p.id}--${m.name}"><td><a class="symbol" href="${sourceHref(m.header,m.line)}">${esc(m.name)}</a> <a class="permalink" href="#${p.id}--${m.name}" aria-label="Ссылка на ${esc(m.name)}">¶</a></td><td>${inline(m.description)}</td></tr>`).join('')}</tbody></table></div>`:''}
 ${(p.examples||[]).map(codeBlock).join('')}
 ${p.related?`<section class="related"><h2>Связанные разделы</h2><div>${p.related.map(pageLink).join('')}</div></section>`:''}
 <footer>PAINENGINE <span>API Reference · ${esc(version.version)} · снимок ${digest}</span><a href="#overview">К началу ↑</a></footer>
 </article>`;
}
const inventory = `<article class="page" id="headers" data-page="headers"><div class="eyebrow">Служебный API / исходники</div><h1 tabindex="-1">Заголовки SDK</h1><p class="lead">Объявления, перегрузки и комментарии текущего выпуска.</p><p>Это обзор .h/.inl из Include/Engine. Внутренние операции движка исключены; номера строк соответствуют оригиналам SDK. Доступность определяется C++ access specifiers, а не наличием в этом списке. Пользовательские примеры и описания — в тематических разделах.</p><div class="header-list">${Object.entries(sources).map(([h,s])=>`<a href="${sourceHref(h)}"><code>Engine/${esc(h)}</code><span>${s.split('\n').length} строк ↗</span></a>`).join('')}</div></article>`;
const payload = JSON.stringify({pages,groups,sources,examples,version,digest}).replaceAll('<','\\u003c');
const html = `<!doctype html>
<html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><meta name="color-scheme" content="light dark"><meta name="description" content="PainEngine: справочник C++ API для разработчика игры. Классы, методы, свойства, трассировки, примеры и заголовки SDK."><title>PainEngine — API Reference</title><style>${read(path.join(dir,'style.css'))}</style></head>
<body><a class="skip" href="#content">Перейти к содержимому</a>
<aside class="sidebar" id="navigation"><a class="brand" href="#overview"><span class="brand-icon">P<span>∕</span></span><span>PAINENGINE<small>DEVELOPER REFERENCE</small></span></a><div class="release"><span class="dot"></span>${esc(version.version)} <span>C++ SDK</span></div><nav aria-label="Разделы справочника">${groups.map((g,i)=>`<section><h2>${esc(g)}</h2>${pages.filter(p=>p.group===i).map(p=>`<a href="#${p.id}" data-nav="${p.id}">${esc(p.id==='overview'?'Обзор API':p.title)}</a>`).join('')}</section>`).join('')}<section><h2>Исходники</h2><a href="#headers" data-nav="headers">Все заголовки SDK <span>${Object.keys(sources).length}</span></a></section></nav><div class="sidebar-foot">Локальная документация<br><span>Без сервера и внешних зависимостей</span></div></aside>
<div class="workspace"><header class="topbar"><button type="button" id="menu" aria-label="Открыть навигацию" aria-expanded="false" aria-controls="navigation">☰</button><span class="top-label">Документация <span>/</span> API Reference</span><div class="search-wrap"><label class="sr-only" for="search">Поиск по API и исходникам</label><span aria-hidden="true">⌕</span><input id="search" type="search" placeholder="Найти класс, метод, задачу…" autocomplete="off" aria-controls="results"><kbd>Ctrl K</kbd></div><button type="button" id="theme" aria-label="Переключить тему">◐</button><button type="button" id="print" aria-label="Печать текущего раздела">Печать</button></header>
<div class="search-panel" id="results" hidden aria-label="Результаты поиска"><div class="search-summary" role="status" aria-live="polite"></div><div class="search-items"></div></div>
<main id="content">${pages.map(renderPage).join('')}${inventory}<article class="page" id="source" data-page="source"></article><article class="page" id="not-found" data-page="not-found"><h1>Раздел не найден</h1><p>Перейдите в <a href="#overview">обзор API</a> или воспользуйтесь поиском.</p></article></main></div><div class="toast" id="toast" role="status" aria-live="polite"></div><noscript><p class="noscript">JavaScript отключён: ниже доступны все описания и примеры. Поиск, просмотр исходников и переключение разделов требуют JavaScript.</p></noscript>
<script id="reference-data" type="application/json">${payload}</script><script>${read(path.join(dir,'app.js'))}</script></body></html>\n`;
const output = path.join(engine,'docs/API-REFERENCE.html');
if(/\b(?:InitCube|InitSphere|InitSquare|InitSquareWithColor|InitPoints|GetFrameIndex|GetWorldMatrix|SetParentWithoutScale|SetShaderNames|SetReflectionSettings|SetStaticShadowCaster|RemapProjectReferences)\b/.test(html))
 throw Error('An internal engine method leaked into the developer reference');
if(process.argv.includes('--check')) {
 if(!fs.existsSync(output)||read(output)!==html) throw Error('API-REFERENCE.html is stale; run build.mjs');
} else fs.writeFileSync(output, html, 'utf8');
console.log(`${process.argv.includes('--check')?'Checked':'Generated'} ${output}: ${pages.length} pages, ${methodCount} API entries, ${Object.keys(sources).length} headers, ${Object.keys(examples).length} code blocks; source snapshot ${digest}`);
