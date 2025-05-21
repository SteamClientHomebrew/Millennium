import { callable } from '@steambrew/client';
import { PluginComponent } from '../types';

export const API_URL = 'https://steambrew.app';
export const PLUGINS_URL = [API_URL, 'plugins'].join('/');
export const THEMES_URL = [API_URL, 'themes'].join('/');
