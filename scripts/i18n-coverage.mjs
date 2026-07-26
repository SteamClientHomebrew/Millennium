// Computes localization coverage for every locale in src/locales against the
// english.json source of truth and renders it as an SVG bar chart. Two SVGs are
// written (light and dark) so the README can pick per theme with <picture>.
//
// Usage:
//   node scripts/i18n-coverage.mjs                 write both SVGs to .github/assets
//   node scripts/i18n-coverage.mjs --out <dir>     ...to a different directory
//
// Coverage for a language = (keys from english.json that are present and
// non-empty in that language) / (total english.json keys). It is fully
// automatic: add a key to english.json and every language's percentage drops
// until it is translated. No third-party service involved.

import { readFileSync, writeFileSync, readdirSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const LOCALES_DIR = join(ROOT, 'src', 'locales');
const SOURCE = 'english';

/** Human-readable names for Steam's locale codes; falls back to a capitalized filename. */
const DISPLAY_NAMES = {
	brazilian: 'Portuguese (Brazil)',
	bulgarian: 'Bulgarian',
	czech: 'Czech',
	danish: 'Danish',
	dutch: 'Dutch',
	finnish: 'Finnish',
	french: 'French',
	german: 'German',
	greek: 'Greek',
	hungarian: 'Hungarian',
	indonesian: 'Indonesian',
	italian: 'Italian',
	japanese: 'Japanese',
	koreana: 'Korean',
	latam: 'Spanish (Latin America)',
	norwegian: 'Norwegian',
	polish: 'Polish',
	portuguese: 'Portuguese',
	romanian: 'Romanian',
	russian: 'Russian',
	schinese: 'Chinese (Simplified)',
	spanish: 'Spanish',
	swedish: 'Swedish',
	tchinese: 'Chinese (Traditional)',
	thai: 'Thai',
	turkish: 'Turkish',
	ukrainian: 'Ukrainian',
	vietnamese: 'Vietnamese',
};

/** Colors per theme, mirroring GitHub's own light/dark tokens. */
const THEMES = {
	light: { text: '#1f2328', muted: '#656d76', track: '#8b949e', fill: '#2da44e', source: '#0969da' },
	dark: { text: '#e6edf3', muted: '#8b949e', track: '#8b949e', fill: '#3fb950', source: '#388bfd' },
};

const displayName = (code) => DISPLAY_NAMES[code] ?? code.charAt(0).toUpperCase() + code.slice(1);

function readJson(name) {
	return JSON.parse(readFileSync(join(LOCALES_DIR, `${name}.json`), 'utf-8'));
}

function computeCoverage() {
	const source = readJson(SOURCE);
	const sourceKeys = Object.keys(source);
	const total = sourceKeys.length;

	const translations = readdirSync(LOCALES_DIR)
		.filter((f) => f.endsWith('.json') && f !== `${SOURCE}.json`)
		.map((f) => f.slice(0, -5))
		.map((code) => {
			const data = readJson(code);
			let present = 0;
			for (const key of sourceKeys) {
				const value = data[key];
				if (typeof value === 'string' && value.trim() !== '') present++;
			}
			return { name: displayName(code), present, total, pct: (present / total) * 100 };
		})
		.sort((a, b) => b.pct - a.pct || a.name.localeCompare(b.name));

	/** english.json is the reference every language is measured against, so it is always
	 *  100% by definition. Pin it first, labelled as the source, so the list reads as complete. */
	const sourceRow = { name: 'English', present: total, total, pct: 100, source: true };

	return { total, translationCount: translations.length, rows: [sourceRow, ...translations] };
}

/** Rounds to a whole percent, but never shows 100% unless every string is present. */
function pctLabel(row) {
	if (row.present === row.total) return '100%';
	const r = Math.round(row.pct);
	return `${r === 100 ? 99 : r}%`;
}

const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

function renderSvg({ total, translationCount, rows }, c) {
	const rowH = 26;
	const padX = 20;
	const padTop = 58;
	const padBottom = 16;
	const labelW = 160;
	const pctW = 92;
	const width = 720;
	const trackX = padX + labelW;
	const trackW = width - trackX - pctW - padX;
	const height = padTop + rows.length * rowH + padBottom;
	const sans = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif";
	const mono = 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';

	const rowsSvg = rows
		.map((r, i) => {
			const cy = padTop + i * rowH + rowH / 2;
			const full = r.present === r.total;
			const fillW = full ? trackW : Math.max(3, (r.pct / 100) * trackW);
			const name = r.source ? `${r.name} (source)` : r.name;
			const fill = r.source ? c.source : c.fill;
			return [
				`<text x="${trackX - 12}" y="${cy}" text-anchor="end" dominant-baseline="central" font-family="${sans}" font-size="12" fill="${c.text}">${esc(name)}</text>`,
				`<rect x="${trackX}" y="${cy - 5}" width="${trackW}" height="10" rx="5" fill="${c.track}" fill-opacity="0.22"/>`,
				`<rect x="${trackX}" y="${cy - 5}" width="${fillW.toFixed(1)}" height="10" rx="5" fill="${fill}"/>`,
				`<text x="${trackX + trackW + 12}" y="${cy}" dominant-baseline="central" font-family="${mono}" font-size="12" font-weight="600" fill="${c.muted}">${pctLabel(r)}</text>`,
			].join('');
		})
		.join('\n  ');

	return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="Localization coverage: ${translationCount} translations against ${total} strings">
  <text x="${padX}" y="26" font-family="${sans}" font-size="15" font-weight="600" fill="${c.text}">Localization coverage</text>
  <text x="${padX}" y="44" font-family="${sans}" font-size="12" fill="${c.muted}">${translationCount} translations · ${total} strings · source: english.json</text>
  ${rowsSvg}
</svg>
`;
}

// --- CLI ---
const args = process.argv.slice(2);
const outIdx = args.indexOf('--out');
const outDir = join(ROOT, outIdx !== -1 && args[outIdx + 1] ? args[outIdx + 1] : '.github/assets');

const data = computeCoverage();
mkdirSync(outDir, { recursive: true });
for (const [theme, colors] of Object.entries(THEMES)) {
	const file = join(outDir, `i18n-coverage-${theme}.svg`);
	writeFileSync(file, renderSvg(data, colors));
	console.log(`Wrote ${file}`);
}
