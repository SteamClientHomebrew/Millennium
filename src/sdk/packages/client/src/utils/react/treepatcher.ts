import Logger from '../../logger';
import { GenericPatchHandler, afterPatch } from '../patcher';
import { wrapReactClass, wrapReactType } from './react';

// TODO max size limit? could implement as a class extending map perhaps
type PatchedComponentCache = Map<any, any>;
export type GenericNodeStep = (node: any) => any;
// to patch a specific method of a class component other than render. TODO implement
// export type ClassNodeStep = [finder: GenericNodeStep, method: string];
// export type NodeStep = GenericNodeStep | ClassNodeStep;
export type NodeStep = GenericNodeStep;
export type ReactPatchHandler = GenericPatchHandler;

// These will get *very* spammy.
let loggingEnabled = false;
let perfLoggingEnabled = false;

export function setReactPatcherLoggingEnabled(value: boolean = true) {
	loggingEnabled = value;
}
export function setReactPatcherPerformanceLoggingEnabled(value: boolean = true) {
	perfLoggingEnabled = value;
}

function patchComponent(node: any, handler: GenericPatchHandler, steps: NodeStep[], step: number, caches: PatchedComponentCache[], logger: Logger, prop: string = 'type') {
	loggingEnabled && logger.group('Patching node:', node);
	// We need to take extra care to not mutate the original node.type
	switch (typeof node?.[prop]) {
		case 'function':
			// Function component
			const patch = afterPatch(node, prop, steps[step + 1] ? createStepHandler(handler, steps, step + 1, caches, logger) : handler);
			loggingEnabled && logger.debug('Patched a function component', patch);
			break;
		case 'object':
			if (node[prop]?.prototype?.render) {
				// Class component
				// TODO handle patching custom methods
				wrapReactClass(node);
				const patch = afterPatch(node[prop].prototype, 'render', steps[step + 1] ? createStepHandler(handler, steps, step + 1, caches, logger) : handler);
				loggingEnabled && logger.debug('Patched class component', patch);
			} else {
				loggingEnabled && logger.debug('Patching forwardref/memo');
				wrapReactType(node, prop);
				// Step down the object
				patchComponent(node[prop], handler, steps, step, caches, logger, node[prop]?.render ? 'render' : 'type');
			}
			break;
		default:
			logger.error('Unhandled component type', node);
			break;
	}
	loggingEnabled && logger.groupEnd();
}

function handleStep(tree: any, handler: GenericPatchHandler, steps: NodeStep[], step: number, caches: PatchedComponentCache[], logger: Logger): any {
	const startTime = loggingEnabled || perfLoggingEnabled ? performance.now() : 0;
	const stepHandler = steps[step];
	const cache = caches[step] || (caches[step] = new Map());
	loggingEnabled && logger.debug(`Patch step ${step} running`, { tree, stepHandler, step, caches });

	const node = stepHandler(tree);
	if (node && node.type) {
		loggingEnabled && logger.debug('Found node', node);
	} else if (node) {
		loggingEnabled && logger.error('Found node without type. Something is probably wrong.', node);
		return tree;
	} else {
		loggingEnabled && logger.warn('Found no node. Depending on your usecase, this might be fine.', node);
		return tree;
	}

	let cachedType;
	if ((cachedType = cache.get(node.type))) {
		loggingEnabled && logger.debug('Found cached patched component', node);
		node.type = cachedType;
		(loggingEnabled || perfLoggingEnabled) && logger.debug(`Patch step ${step} took ${performance.now() - startTime}ms with cache`);
		return tree;
	}

	const originalType = node.type;

	patchComponent(node, handler, steps, step, caches, logger);

	cache.set(originalType, node.type);

	(loggingEnabled || perfLoggingEnabled) && logger.debug(`Patch step ${step} took ${performance.now() - startTime}ms`);
	return tree;
}

function createStepHandler(handler: GenericPatchHandler, steps: NodeStep[], step: number, caches: PatchedComponentCache[], logger: Logger) {
	loggingEnabled && logger.debug(`Creating handler for step ${step}`);
	return (_: any, tree: any) => handleStep(tree, handler, steps, step, caches, logger);
}

// TODO handle createReactTreePatcher inside handler and cache it (or warn)
export function createReactTreePatcher(steps: NodeStep[], handler: GenericPatchHandler, debugName: string = 'ReactPatch'): GenericPatchHandler {
	const caches: PatchedComponentCache[] = [];

	const logger = new Logger(`ReactTreePatcher -> ${debugName}`);

	loggingEnabled && logger.debug('Init with options:', steps, debugName);

	return createStepHandler(handler, steps, 0, caches, logger);
}
