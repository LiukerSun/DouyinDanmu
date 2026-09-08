// Cache the public Douyin emoji catalog locally so rendering needs no CDN request.
import { mkdir, writeFile, rename } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import path from 'node:path';
const base = fileURLToPath(new URL('../', import.meta.url));
const endpoint = 'https://www.douyin.com/aweme/v1/web/emoji/list?aid=6383&device_platform=webapp';
const response = await fetch(endpoint, {headers:{'User-Agent':'Mozilla/5.0'}, signal:AbortSignal.timeout(15000)});
if (!response.ok) throw new Error(`Emoji catalog HTTP ${response.status}`);
const catalog = await response.json();
if (catalog.status_code !== 0 || !catalog.emoji_list?.length) throw new Error('Invalid emoji catalog');
await mkdir(path.join(base, 'public/emojis'), {recursive:true});
await mkdir(path.join(base, 'src/data'), {recursive:true});
const entries = [...catalog.emoji_list], images = {};
let completed = 0;
async function worker() {
  while (entries.length) {
    const entry = entries.shift();
    if (!/^\[[^\[\]]+\]$/.test(entry.display_name)) continue;
    let saved = false;
    for (const candidate of entry.emoji_url?.url_list || []) {
      const url = new URL(candidate);
      if (url.protocol !== 'https:' || !url.hostname.endsWith('.douyinpic.com')) continue;
      try {
        const result = await fetch(url, {signal:AbortSignal.timeout(15000)});
        if (!result.ok) throw new Error(`HTTP ${result.status}`);
        const data = Buffer.from(await result.arrayBuffer());
        if (data.length > 1024 * 1024) throw new Error('Emoji image exceeds 1 MiB');
        const extension = data.subarray(0,8).equals(Buffer.from('89504e470d0a1a0a','hex')) ? 'png'
          : data.subarray(0,3).toString() === 'GIF' ? 'gif'
          : data.subarray(0,2).equals(Buffer.from('ffd8','hex')) ? 'jpg'
          : data.subarray(8,12).toString() === 'WEBP' ? 'webp' : null;
        if (!extension) throw new Error('Unsupported image response');
        const name = createHash('sha256').update(data).digest('hex').slice(0,24) + '.' + extension;
        await writeFile(path.join(base,'public/emojis',name),data);
        images[entry.display_name] = '/emojis/' + name;
        saved = true; completed++; break;
      } catch { /* Try the next official CDN candidate. */ }
    }
    if (!saved) throw new Error(`Could not cache ${entry.display_name}`);
  }
}
await Promise.all(Array.from({length:6},worker));
const target = path.join(base,'src/data/douyin-emojis.json');
await writeFile(target + '.tmp', JSON.stringify(Object.fromEntries(Object.entries(images).sort()),null,2) + '\n');
await rename(target + '.tmp',target);
await writeFile(path.join(base,'public/emojis/SOURCE.md'),`Source: ${endpoint}\nCatalog version: ${catalog.version}\nUpdated: ${new Date().toISOString()}\nImages and emoji names belong to their respective rights holders.\n`);
console.log(`Cached ${completed} Douyin emoji images`);
