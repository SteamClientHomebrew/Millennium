// Usage:
//   node scripts/i18n-coverage.ts
//   node scripts/i18n-coverage.ts --out <dir>

import { readFileSync, writeFileSync, readdirSync, mkdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = join(dirname(fileURLToPath(import.meta.url)), '..');
const LOCALES_DIR = join(ROOT, 'src', 'locales');
const SOURCE = 'english';

interface Row {
	name: string;
	present: number;
	total: number;
	pct: number;
	source?: boolean;
}

interface CoverageData {
	total: number;
	translationCount: number;
	rows: Row[];
}

interface Theme {
	text: string;
	muted: string;
	track: string;
	fill: string;
	source: string;
}

const DISPLAY_NAMES: Record<string, string> = {
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

const THEMES: Record<string, Theme> = {
	light: { text: '#1f2328', muted: '#656d76', track: '#8b949e', fill: '#2da44e', source: '#0969da' },
	dark: { text: '#e6edf3', muted: '#8b949e', track: '#8b949e', fill: '#3fb950', source: '#388bfd' },
};

const displayName = (code: string) => DISPLAY_NAMES[code] ?? code.charAt(0).toUpperCase() + code.slice(1);

function readJson(name: string): Record<string, unknown> {
	return JSON.parse(readFileSync(join(LOCALES_DIR, `${name}.json`), 'utf-8'));
}

function computeCoverage(): CoverageData {
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

	const sourceRow: Row = { name: 'English', present: total, total, pct: 100, source: true };

	return { total, translationCount: translations.length, rows: [sourceRow, ...translations] };
}

function pctLabel(row: Row): string {
	if (row.present === row.total) return '100%';
	const r = Math.round(row.pct);
	return `${r === 100 ? 99 : r}%`;
}

const esc = (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

function renderSvg({ rows }: CoverageData, c: Theme): string {
	const rowH = 26;
	const width = 720;
	const trackW = 420;
	const pctX = trackW + 40;
	const nameX = trackW + 52;
	const height = rows.length * rowH;
	const sans = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, Arial, sans-serif";
	const mono = 'ui-monospace, SFMono-Regular, Menlo, Consolas, monospace';

	const rowsSvg = rows
		.map((r, i) => {
			const cy = i * rowH + rowH / 2;
			const full = r.present === r.total;
			const fillW = full ? trackW : Math.max(3, (r.pct / 100) * trackW);
			const name = r.source ? `${r.name} (source)` : r.name;
			const fill = r.source ? c.source : c.fill;
			return [
				`<rect x="0" y="${cy - 5}" width="${trackW}" height="10" fill="${c.track}" fill-opacity="0.22"/>`,
				`<rect x="0" y="${cy - 5}" width="${fillW.toFixed(1)}" height="10" fill="${fill}"/>`,
				`<text x="${pctX}" y="${cy}" text-anchor="end" dominant-baseline="central" font-family="${mono}" font-size="12" font-weight="600" fill="${c.muted}">${pctLabel(r)}</text>`,
				`<text x="${nameX}" y="${cy}" dominant-baseline="central" font-family="${sans}" font-size="12" fill="${c.text}">${esc(name)}</text>`,
			].join('');
		})
		.join('\n  ');

	return `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}" role="img" aria-label="Localization coverage">
  ${rowsSvg}
</svg>
`;
}

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
