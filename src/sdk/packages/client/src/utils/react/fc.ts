// Utilities for patching function components
import { createElement, type FC } from 'react';
import { applyHookStubs, removeHookStubs } from './react';
import Logger from '../../logger';

export interface FCTrampoline {
	component: FC;
}

let loggingEnabled = false;

export function setFCTrampolineLoggingEnabled(value: boolean = true) {
	loggingEnabled = value;
}

let logger = new Logger('FCTrampoline');

/**
 * Directly hooks a function component from its reference, redirecting it to a user-patchable wrapper in its returned object.
 * This only works if the original component when called directly returns either nothing, null, or another child element.
 *
 * This works by tricking react into thinking it's a class component by cleverly working around its class component checks,
 * keeping the unmodified function component intact as a mostly working constructor (as it is impossible to direcly modify a function),
 * stubbing out hooks to prevent errors by detecting setter/getter triggers that occur direcly before/after the class component is instantiated by react,
 * and creating a fake class component render method to trampoline out into your own handler.
 *
 * Due to the nature of this method of hooking a component, please only use this where it is *absolutely necessary.*
 * Incorrect hook stubs can cause major instability, be careful when writing them. Refer to fakeRenderComponent for the hook stub implementation.
 * Make sure your hook stubs can handle all the cases they could possibly need to within the component you are injecting into.
 * You do not need to worry about its children, as they are never called due to the first instance never actually rendering.
 */
export function injectFCTrampoline(component: FC, customHooks?: any): FCTrampoline {
	// It needs to be wrapped so React doesn't infinitely call the fake class render method.
	const newComponent = function (this: any, ...args: any) {
		loggingEnabled && logger.debug('new component rendering with props', args);
		return component.apply(this, args);
	};
	const userComponent = { component: newComponent };
	// Create a fake class component render method
	component.prototype.render = function (...args: any[]) {
		loggingEnabled && logger.debug('rendering trampoline', args, this);
		// Pass through rendering via creating the component as a child so React can use function component logic instead of class component logic (setting up the hooks)
		return createElement(userComponent.component, this.props, this.props.children);
	};
	component.prototype.isReactComponent = true;
	let stubsApplied = false;
	let oldCreateElement = window.SP_REACT.createElement;

	const applyStubsIfNeeded = () => {
		if (!stubsApplied) {
			loggingEnabled && logger.debug('applied stubs');
			stubsApplied = true;
			applyHookStubs(customHooks);
			// we have to redirect this to return an object with component's prototype as a constructor returning a value overrides its prototype
			window.SP_REACT.createElement = () => {
				loggingEnabled && logger.debug('createElement hook called');
				loggingEnabled && console.trace('createElement trace');
				return Object.create(component.prototype);
			};
		}
	};

	const removeStubsIfNeeded = () => {
		if (stubsApplied) {
			loggingEnabled && logger.debug('removed stubs');
			stubsApplied = false;
			removeHookStubs();
			window.SP_REACT.createElement = oldCreateElement;
		}
	};

	let renderHookStep = 0;

	if (window.SP_REACTDOM.version.startsWith('19.')) {
		// Accessed two times directly before class instantiation on path A and once on path B
		Object.defineProperty(component, 'contextType', {
			configurable: true,
			get: function () {
				loggingEnabled && logger.debug('get contexttype', this, this._contextType, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('contextType trace');
				if (renderHookStep == 0) {
					renderHookStep = 1;
				}
				if (this._contextType == null) {
					this._contextType = {};
				}
				if (!this._contextType.appliedCurrentValueHook) {
					logger.debug('applied currentvalue hook');
					this._contextType.appliedCurrentValueHook = true;
					Object.defineProperty(this._contextType, '_currentValue', {
						configurable: true,
						get: function () {
							loggingEnabled && logger.debug('get currentValue', this, stubsApplied, renderHookStep);
							loggingEnabled && console.trace('currentValue trace');
							if (renderHookStep == 1) {
								renderHookStep = 2;
								applyStubsIfNeeded();
							}
							return this.__currentValue;
						},
						set: function (value) {
							return (this.__currentValue = value);
						},
					});
				}
				return this._contextType;
			},
			set: function (value) {
				this._contextType = value;
			},
		});

		// Set directly after class is instantiated
		Object.defineProperty(component.prototype, 'updater', {
			configurable: true,
			get: function () {
				return this._updater;
			},
			set: function (value) {
				loggingEnabled && logger.debug('set updater', this, value, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('updater trace');
				if (renderHookStep == 1 || renderHookStep == 2) {
					renderHookStep = 0;
					removeStubsIfNeeded();
				}
				return (this._updater = value);
			},
		});

		// Prevents the second contextType access from leaving its hooks around
		Object.defineProperty(component, 'getDerivedStateFromProps', {
			configurable: true,
			get: function () {
				loggingEnabled && logger.debug('get getDerivedStateFromProps', this, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('getDerivedStateFromProps trace');
				if (renderHookStep == 1 || renderHookStep == 2) {
					renderHookStep = 0;
					removeStubsIfNeeded();
				}
				return this._getDerivedStateFromProps;
			},
			set: function (value) {
				this._getDerivedStateFromProps = value;
			},
		});
	} else if (window.SP_REACTDOM.version.startsWith('18.')) {
		// Accessed two times, once directly before class instantiation, and again in some extra logic we don't need to worry about that we hanlde below just in case.
		Object.defineProperty(component, 'contextType', {
			configurable: true,
			get: function () {
				loggingEnabled && logger.debug('get contexttype', this, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('contextType trace');
				if (renderHookStep == 0) renderHookStep = 1;
				else if (renderHookStep == 3) renderHookStep = 4;
				return this._contextType;
			},
			set: function (value) {
				this._contextType = value;
			},
		});

		// Always accessed directly after contextType for the path we want to catch.
		Object.defineProperty(component, 'contextTypes', {
			configurable: true,
			get: function () {
				loggingEnabled && logger.debug('get contexttypes', this, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('contextTypes trace');
				if (renderHookStep == 1) {
					renderHookStep = 2;
					applyStubsIfNeeded();
				}
				return this._contextTypes;
			},
			set: function (value) {
				this._contextTypes = value;
			},
		});

		// Set directly after class is instantiated
		Object.defineProperty(component.prototype, 'updater', {
			configurable: true,
			get: function () {
				return this._updater;
			},
			set: function (value) {
				loggingEnabled && logger.debug('set updater', this, value, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('updater trace');
				if (renderHookStep == 2) {
					renderHookStep = 0;
					removeStubsIfNeeded();
				}
				return (this._updater = value);
			},
		});

		// Prevents the second contextType+contextTypes access from leaving its hooks around
		Object.defineProperty(component, 'getDerivedStateFromProps', {
			configurable: true,
			get: function () {
				loggingEnabled && logger.debug('get getDerivedStateFromProps', this, stubsApplied, renderHookStep);
				loggingEnabled && console.trace('getDerivedStateFromProps trace');
				if (renderHookStep == 2) {
					renderHookStep = 0;
					removeStubsIfNeeded();
				}
				return this._getDerivedStateFromProps;
			},
			set: function (value) {
				this._getDerivedStateFromProps = value;
			},
		});
	}

	return userComponent;
}
