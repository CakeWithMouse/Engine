// Read-only freshness check. A changed hash requests review, not automatic regeneration.
import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
const root = path.dirname(fileURLToPath(import.meta.url));
const repo = path.resolve(root, '../../..');
const current = JSON.parse(await fs.readFile(path.join(root,'current.json'),'utf8'));
const version = JSON.parse(await fs.readFile(path.join(repo,'Engine/Version.json'),'utf8'));
const changed=[];
for (const source of current.sources) {
  try {
    const actual=createHash('sha256').update(await fs.readFile(path.join(repo,source.path))).digest('hex');
    if(actual!==source.sha256) changed.push(source.path);
  } catch { changed.push(source.path+' (missing)'); }
}
const presentation=path.join(root,current.presentation);
const exists=await fs.access(presentation).then(()=>true,()=>false);
console.log(JSON.stringify({presentation,exists,revision:current.revision,slideCount:current.slideCount,
  tutorialVersion:current.engineVersion,currentEngineVersion:version.version,changedSources:changed,
  sourceSnapshotMatches:exists && version.version===current.engineVersion && !changed.length},null,2));
