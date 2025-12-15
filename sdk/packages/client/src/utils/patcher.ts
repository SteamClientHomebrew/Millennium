// TODO: implement storing patches as an option so we can offer unpatchAll selectively
// Return this in a replacePatch to call the original method (can still modify args).
export let callOriginal = Symbol('MILLENNIUM_CALL_ORIGINAL');

export interface PatchOptions {
	singleShot?: boolean;
}

// Unused at this point, exported for backwards compatibility
export type GenericPatchHandler = (args: any[], ret?: any) => any;

export interface Patch<
	Object extends Record<PropertyKey, any> = Record<PropertyKey, any>,
	Property extends keyof Object = keyof Object,
	Target extends Object[Property] = Object[Property]
> {
	object: Object;
	property: Property;
	original: Target;

	patchedFunction: Target;
	hasUnpatched: boolean;
	handler: ((args: Parameters<Target>, ret: ReturnType<Target>) => any) | ((args: Parameters<Target>) => any);

	unpatch(): void;
}

// let patches = new Set<Patch>();

export function beforePatch<
	Object extends Record<PropertyKey, any>,
	Property extends keyof Object,
	Target extends Object[Property],
>(
	object: Object,
	property: Property,
	handler: (args: Parameters<Target>) => void,
	options: PatchOptions = {},
): Patch<Object, Property, Target> {
	const orig = object[property];
	object[property] = (function (this: any, ...args: Parameters<Target>) {
		handler.call(this, args);
		const ret = patch.original.call(this, ...args);
		if (options.singleShot) {
			patch.unpatch();
		}
		return ret;
	}) as any;
	const patch = processPatch(object, property, handler, object[property], orig);
	return patch;
}

export function afterPatch<
	Object extends Record<PropertyKey, any>,
	Property extends keyof Object,
	Target extends Object[Property]
>(
	object: Object,
	property: Property,
	handler: (args: Parameters<Target>, ret: ReturnType<Target>) => ReturnType<Target>,
	options: PatchOptions = {},
): Patch<Object, Property, Target> {
	const orig = object[property];
	object[property] = (function (this: any, ...args: Parameters<Target>) {
		let ret = patch.original.call(this, ...args);
		ret = handler.call(this, args, ret);
		if (options.singleShot) {
			patch.unpatch();
		}
		return ret
	}) as any;
	const patch = processPatch(object, property, handler, object[property], orig);
	return patch;
}

export function replacePatch<
	Object extends Record<PropertyKey, any>,
	Property extends keyof Object,
	Target extends Object[Property]
>(
	object: Object,
	property: Property,
	handler: (args: Parameters<Target>) => ReturnType<Target>,
	options: PatchOptions = {},
): Patch<Object, Property, Target> {
	const orig = object[property];
	object[property] = (function (this: any, ...args: Parameters<Target>) {
		const ret = handler.call(this, args);
		// console.debug('[Patcher] replacePatch', patch);

		if (ret == callOriginal) return patch.original.call(this, ...args);
		if (options.singleShot) {
			patch.unpatch();
		}
		return ret;
	}) as any;
	const patch = processPatch(object, property, handler, object[property], orig);
	return patch;
}

function processPatch<
	Object extends Record<PropertyKey, any>,
	Property extends keyof Object,
	Target extends Object[Property],
>(
	object: Object,
	property: Property,
	handler: ((args: Parameters<Target>, ret: ReturnType<Target>) => any) | ((args: Parameters<Target>) => any),
	patchedFunction: Target,
	original: Target,
): Patch<Object, Property, Target> {
	// Assign all props of original function to new one
	Object.assign(object[property], original);
	// Allow toString webpack filters to continue to work
	object[property].toString = () => original.toString();

	// HACK: for compatibility, remove when all plugins are using new patcher
	Object.defineProperty(object[property], '__millenniumOrig', {
		get: () => patch.original,
		set: (val: any) => (patch.original = val),
	});

	// Build a Patch object of this patch
	const patch = {
		object,
		property,
		handler,
		patchedFunction,
		original,
		hasUnpatched: false,
		unpatch: (): void => unpatch(patch),
	};

	object[property].__millenniumPatch = patch;

	return patch;
}

function unpatch<
	Object extends Record<PropertyKey, any>,
>(patch: Patch<Object>): void {
	const { object, property, handler, patchedFunction, original } = patch;
	if (patch.hasUnpatched) throw new Error('Function is already unpatched.');
	let realProp = property;
	let realObject = object;
	console.debug('[Patcher] unpatching', {
		realObject,
		realProp,
		object,
		property,
		handler,
		patchedFunction,
		original,
		isEqual: realObject[realProp] === patchedFunction,
	});

	// If another patch has been applied to this function after this one, move down until we find the correct patch
	while (realObject[realProp] && realObject[realProp] !== patchedFunction) {
		realObject = realObject[realProp].__millenniumPatch;
		realProp = 'original';
		console.debug('[Patcher] moved to next', {
			realObject,
			realProp,
			object,
			property,
			handler,
			patchedFunction,
			original,
			isEqual: realObject[realProp] === patchedFunction,
		});
	}

	realObject[realProp] = realObject[realProp].__millenniumPatch.original;

	patch.hasUnpatched = true;
	console.debug('[Patcher] unpatched', {
		realObject,
		realProp,
		object,
		property,
		handler,
		patchedFunction,
		original,
		isEqual: realObject[realProp] === patchedFunction,
	});
}
