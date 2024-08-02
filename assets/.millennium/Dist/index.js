const pluginName = "core";
function InitializePlugins() {
    /**
     * This function is called n times depending on n plugin count,
     * Create the plugin list if it wasn't already created
     */
    !window.PLUGIN_LIST && (window.PLUGIN_LIST = {});
    // initialize a container for the plugin
    if (!window.PLUGIN_LIST[pluginName]) {
        window.PLUGIN_LIST[pluginName] = {};
    }
}
InitializePlugins()
async function wrappedCallServerMethod(methodName, kwargs) {
    return new Promise((resolve, reject) => {
        // @ts-ignore
        Millennium.callServerMethod(pluginName, methodName, kwargs).then((result) => {
            resolve(result);
        }).catch((error) => {
            reject(error);
        });
    });
}
var millennium_main = (function (exports, React, ReactDOM) {
    'use strict';

    const bgStyle1 = 'background: #8a16a2; color: black;';
    const log = (name, ...args) => {
        console.log(`%c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #b11cce; color: black;', 'background: transparent;', ...args);
    };
    const group = (name, ...args) => {
        console.group(`%c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #b11cce; color: black;', 'background: transparent;', ...args);
    };
    const groupEnd = (name, ...args) => {
        console.groupEnd();
        if (args?.length > 0)
            console.log(`^ %c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #b11cce; color: black;', 'background: transparent;', ...args);
    };
    const debug = (name, ...args) => {
        console.debug(`%c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #1abc9c; color: black;', 'color: blue;', ...args);
    };
    const warn = (name, ...args) => {
        console.warn(`%c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #ffbb00; color: black;', 'color: blue;', ...args);
    };
    const error = (name, ...args) => {
        console.error(`%c @millennium/ui %c ${name} %c`, bgStyle1, 'background: #FF0000;', 'background: transparent;', ...args);
    };
    let Logger$1 = class Logger {
        constructor(name) {
            this.name = name;
            this.name = name;
        }
        log(...args) {
            log(this.name, ...args);
        }
        debug(...args) {
            debug(this.name, ...args);
        }
        warn(...args) {
            warn(this.name, ...args);
        }
        error(...args) {
            error(this.name, ...args);
        }
        group(...args) {
            group(this.name, ...args);
        }
        groupEnd(...args) {
            groupEnd(this.name, ...args);
        }
    };

    const logger = new Logger$1('Webpack');
    let modules = [];
    function initModuleCache() {
        const startTime = performance.now();
        logger.group('Webpack Module Init');
        // Webpack 5, currently on beta
        // Generate a fake module ID
        const id = Math.random(); // really should be an int and not a float but who cares
        let webpackRequire;
        // Insert our module in a new chunk.
        // The module will then be called with webpack's internal require function as its first argument
        window.webpackChunksteamui.push([
            [id],
            {},
            (r) => {
                webpackRequire = r;
            },
        ]);
        logger.log('Initializing all modules. Errors here likely do not matter, as they are usually just failing module side effects.');
        // Loop over every module ID
        for (let i of Object.keys(webpackRequire.m)) {
            try {
                const module = webpackRequire(i);
                if (module) {
                    modules.push(module);
                }
            }
            catch (e) {
                logger.debug('Ignoring require error for module', i, e);
            }
        }
        logger.groupEnd(`Modules initialized in ${performance.now() - startTime}ms...`);
    }
    initModuleCache();
    const findModule = (filter) => {
        for (const m of modules) {
            if (m.default && filter(m.default))
                return m.default;
            if (filter(m))
                return m;
        }
    };
    const findModuleDetailsByExport = (filter, minExports) => {
        for (const m of modules) {
            if (!m)
                continue;
            for (const mod of [m.default, m]) {
                if (typeof mod !== 'object')
                    continue;
                for (let exportName in mod) {
                    if (mod?.[exportName]) {
                        const filterRes = filter(mod[exportName], exportName);
                        if (filterRes) {
                            return [mod, mod[exportName], exportName];
                        }
                        else {
                            continue;
                        }
                    }
                }
            }
        }
        return [undefined, undefined, undefined];
    };
    const findModuleByExport = (filter, minExports) => {
        return findModuleDetailsByExport(filter)?.[0];
    };
    const findModuleExport = (filter, minExports) => {
        return findModuleDetailsByExport(filter)?.[1];
    };
    /**
     * @deprecated use findModuleExport instead
     */
    const findModuleChild = (filter) => {
        for (const m of modules) {
            for (const mod of [m.default, m]) {
                const filterRes = filter(mod);
                if (filterRes) {
                    return filterRes;
                }
                else {
                    continue;
                }
            }
        }
    };
    const findAllModules = (filter) => {
        const out = [];
        for (const m of modules) {
            if (m.default && filter(m.default))
                out.push(m.default);
            if (filter(m))
                out.push(m);
        }
        return out;
    };
    const CommonUIModule = modules.find((m) => {
        if (typeof m !== 'object')
            return false;
        for (let prop in m) {
            if (m[prop]?.contextType?._currentValue && Object.keys(m).length > 60)
                return true;
        }
        return false;
    });
    const IconsModule = findModuleByExport((e) => e?.toString && /Spinner\)}\)?,.\.createElement\(\"path\",{d:\"M18 /.test(e.toString()));
    const ReactRouter = findModuleByExport((e) => e.computeRootMatch);

    const CommonDialogDivs = Object.values(CommonUIModule).filter((m) => typeof m === 'object' && m?.render?.toString().includes('createElement("div",{...') ||
        m?.render?.toString().includes('createElement("div",Object.assign({},'));
    const MappedDialogDivs = new Map(Object.values(CommonDialogDivs).map((m) => {
        try {
            const renderedDiv = m.render({});
            // Take only the first class name segment as it identifies the element we want
            return [renderedDiv.props.className.split(' ')[0], m];
        }
        catch (e) {
            console.error("[DFL:Dialog]: failed to render common dialog component", e);
            return [null, null];
        }
    }));
    const DialogHeader = MappedDialogDivs.get('DialogHeader');
    const DialogSubHeader = MappedDialogDivs.get('DialogSubHeader');
    MappedDialogDivs.get('DialogFooter');
    MappedDialogDivs.get('DialogLabel');
    const DialogBodyText = MappedDialogDivs.get('DialogBodyText');
    const DialogBody = MappedDialogDivs.get('DialogBody');
    const DialogControlsSection = MappedDialogDivs.get('DialogControlsSection');
    MappedDialogDivs.get('DialogControlsSectionHeader');
    Object.values(CommonUIModule).find((mod) => mod?.render?.toString()?.includes('"DialogButton","_DialogLayout","Primary"'));
    const DialogButtonSecondary = Object.values(CommonUIModule).find((mod) => mod?.render?.toString()?.includes('"DialogButton","_DialogLayout","Secondary"'));
    // This is the "main" button. The Primary can act as a submit button,
    // therefore secondary is chosen (also for backwards comp. reasons)
    const DialogButton = DialogButtonSecondary;

    // Button isn't exported, so call DialogButton to grab it
    DialogButton?.render({}).type;

    /**
     * Create a Regular Expression to search for a React component that uses certain props in order.
     *
     * @export
     * @param {string[]} propList Ordererd list of properties to search for
     * @returns {RegExp} RegEx to call .test(component.toString()) on
     */
    function createPropListRegex(propList, fromStart = true) {
        let regexString = fromStart ? "const\{" : "";
        propList.forEach((prop, propIdx) => {
            regexString += `"?${prop}"?:[a-zA-Z_$]{1,2}`;
            if (propIdx < propList.length - 1) {
                regexString += ",";
            }
        });
        // TODO provide a way to enable this
        // console.debug(`[DFL:Utils] createPropListRegex generated regex "${regexString}" for props`, propList);
        return new RegExp(regexString);
    }

    const classMapList = findAllModules((m) => {
        if (typeof m == 'object' && !m.__esModule) {
            const keys = Object.keys(m);
            // special case some libraries
            if (keys.length == 1 && m.version)
                return false;
            // special case localization
            if (keys.length > 1000 && m.AboutSettings)
                return false;
            return keys.length > 0 && keys.every((k) => !Object.getOwnPropertyDescriptor(m, k)?.get && typeof m[k] == 'string');
        }
        return false;
    });
    const classMap = Object.assign({}, ...classMapList.map(obj => Object.fromEntries(Object.entries(obj).map(([key, value]) => [key, value]))));
    function findClass(name) {
        return classMapList.find((m) => m?.[name])?.[name];
    }
    const findClassHandler = {
        get: (target, prop) => {
            if (typeof prop === "string") {
                return target(prop);
            }
        }
    };
    const Classes = new Proxy(findClass, findClassHandler);
    function findClassModule(filter) {
        return classMapList.find((m) => filter(m));
    }

    findClassModule((m) => m.Title && m.QuickAccessMenu && m.BatteryDetailsLabels);
    findClassModule((m) => m.ScrollPanel);
    findClassModule((m) => m.GamepadDialogContent && !m.BindingButtons);
    findClassModule((m) => m.BatteryPercentageLabel && m.PanelSection && !m['vr-dashboard-bar-height'] && !m.QuickAccessMenu && !m.QuickAccess && !m.PerfProfileInfo);
    findClassModule((m) => m.OOBEUpdateStatusContainer);
    findClassModule((m) => m.PlayBarDetailLabel);
    findClassModule((m) => m.SliderControlPanelGroup);
    findClassModule((m) => m.TopCapsule);
    findClassModule((m) => m.HeaderLoaded);
    findClassModule((m) => m.BasicUiRoot);
    findClassModule((m) => m.GamepadTabbedPage);
    findClassModule((m) => m.BasicContextMenuModal);
    findClassModule((m) => m.AchievementListItemBase && !m.Page);
    findClassModule((m) => m.AchievementListItemBase && m.Page);
    findClassModule((m) => m.AppRunningControls && m.OverlayAchievements);
    findClassModule((m) => m.AppDetailsRoot);
    findClassModule(m => m.SpinnerLoaderContainer);
    findClassModule(m => m.QuickAccessFooter);
    findClassModule(m => m.PlayButtonContainer);
    findClassModule(m => m.LongTitles && m.GreyBackground);
    findClassModule(m => m.GamepadLibrary);
    findClassModule(m => m.FocusRingRoot);
    findClassModule(m => m.SearchAndTitleContainer);
    findClassModule(m => m.MainBrowserContainer);

    /**
     * Finds the SP window, since it is a render target as of 10-19-2022's beta
     */
    function findSP() {
        // old (SP as host)
        if (document.title == 'SP')
            return window;
        // new (SP as popup)
        const navTrees = getGamepadNavigationTrees();
        return navTrees?.find((x) => x.m_ID == 'root_1_').Root.Element.ownerDocument.defaultView;
    }
    /**
     * Gets the correct FocusNavController, as the Feb 22 2023 beta has two for some reason.
     */
    function getFocusNavController() {
        return window.GamepadNavTree?.m_context?.m_controller || window.FocusNavController;
    }
    /**
     * Gets the gamepad navigation trees as Valve seems to be moving them.
     */
    function getGamepadNavigationTrees() {
        const focusNav = getFocusNavController();
        const context = focusNav.m_ActiveContext || focusNav.m_LastActiveContext;
        return context?.m_rgGamepadNavigationTrees;
    }

    const buttonItemRegex = createPropListRegex(["highlightOnFocus", "childrenContainerWidth"], false);
    Object.values(CommonUIModule).find((mod) => (mod?.render?.toString && buttonItemRegex.test(mod.render.toString())) ||
        mod?.render?.toString?.().includes('childrenContainerWidth:"min"'));

    findModuleExport((e) => e.render?.toString().includes('setFocusedColumn:'));

    findModuleExport((e) => e?.toString && e.toString().includes('().ControlsListChild') && e.toString().includes('().ControlsListOuterPanel'));

    Object.values(findModule((m) => {
        if (typeof m !== 'object')
            return false;
        for (const prop in m) {
            if (m[prop]?.prototype?.GetPanelElementProps)
                return true;
        }
        return false;
    })).find((m) => m.contextType &&
        m.prototype?.render.toString().includes('fallback:') &&
        m?.prototype?.SetChecked &&
        m?.prototype?.Toggle &&
        m?.prototype?.GetPanelElementProps);

    const Dropdown = Object.values(CommonUIModule).find((mod) => mod?.prototype?.SetSelectedOption && mod?.prototype?.BuildMenu);
    const dropdownItemRegex = createPropListRegex(["dropDownControlRef", "description"], false);
    Object.values(CommonUIModule).find((mod) => mod?.toString && dropdownItemRegex.test(mod.toString()));

    findModuleExport((e) => e?.render?.toString().includes('"shift-children-below"'));

    const focusableRegex = createPropListRegex(["flow-children", "onActivate", "onCancel", "focusClassName", "focusWithinClassName"]);
    findModuleExport((e) => e?.render?.toString && focusableRegex.test(e.render.toString()));

    findModuleExport((e) => e?.toString()?.includes('.GetShowDebugFocusRing())'));

    var GamepadButton;
    (function (GamepadButton) {
        GamepadButton[GamepadButton["INVALID"] = 0] = "INVALID";
        GamepadButton[GamepadButton["OK"] = 1] = "OK";
        GamepadButton[GamepadButton["CANCEL"] = 2] = "CANCEL";
        GamepadButton[GamepadButton["SECONDARY"] = 3] = "SECONDARY";
        GamepadButton[GamepadButton["OPTIONS"] = 4] = "OPTIONS";
        GamepadButton[GamepadButton["BUMPER_LEFT"] = 5] = "BUMPER_LEFT";
        GamepadButton[GamepadButton["BUMPER_RIGHT"] = 6] = "BUMPER_RIGHT";
        GamepadButton[GamepadButton["TRIGGER_LEFT"] = 7] = "TRIGGER_LEFT";
        GamepadButton[GamepadButton["TRIGGER_RIGHT"] = 8] = "TRIGGER_RIGHT";
        GamepadButton[GamepadButton["DIR_UP"] = 9] = "DIR_UP";
        GamepadButton[GamepadButton["DIR_DOWN"] = 10] = "DIR_DOWN";
        GamepadButton[GamepadButton["DIR_LEFT"] = 11] = "DIR_LEFT";
        GamepadButton[GamepadButton["DIR_RIGHT"] = 12] = "DIR_RIGHT";
        GamepadButton[GamepadButton["SELECT"] = 13] = "SELECT";
        GamepadButton[GamepadButton["START"] = 14] = "START";
        GamepadButton[GamepadButton["LSTICK_CLICK"] = 15] = "LSTICK_CLICK";
        GamepadButton[GamepadButton["RSTICK_CLICK"] = 16] = "RSTICK_CLICK";
        GamepadButton[GamepadButton["LSTICK_TOUCH"] = 17] = "LSTICK_TOUCH";
        GamepadButton[GamepadButton["RSTICK_TOUCH"] = 18] = "RSTICK_TOUCH";
        GamepadButton[GamepadButton["LPAD_TOUCH"] = 19] = "LPAD_TOUCH";
        GamepadButton[GamepadButton["LPAD_CLICK"] = 20] = "LPAD_CLICK";
        GamepadButton[GamepadButton["RPAD_TOUCH"] = 21] = "RPAD_TOUCH";
        GamepadButton[GamepadButton["RPAD_CLICK"] = 22] = "RPAD_CLICK";
        GamepadButton[GamepadButton["REAR_LEFT_UPPER"] = 23] = "REAR_LEFT_UPPER";
        GamepadButton[GamepadButton["REAR_LEFT_LOWER"] = 24] = "REAR_LEFT_LOWER";
        GamepadButton[GamepadButton["REAR_RIGHT_UPPER"] = 25] = "REAR_RIGHT_UPPER";
        GamepadButton[GamepadButton["REAR_RIGHT_LOWER"] = 26] = "REAR_RIGHT_LOWER";
        GamepadButton[GamepadButton["STEAM_GUIDE"] = 27] = "STEAM_GUIDE";
        GamepadButton[GamepadButton["STEAM_QUICK_MENU"] = 28] = "STEAM_QUICK_MENU";
    })(GamepadButton || (GamepadButton = {}));

    findModuleExport((e) => e?.toString && e.toString().includes('.Marquee') && e.toString().includes('--fade-length'));

    // import { fakeRenderComponent } from '../utils';
    findModuleExport((e) => typeof e === 'function' && e.toString().includes('GetContextMenuManagerFromWindow(')
        && e.toString().includes('.CreateContextMenuInstance('));
    findModuleExport((e) => e?.prototype?.HideIfSubmenu && e?.prototype?.HideMenu);
    findModuleExport((e) => e?.render?.toString()?.includes('bPlayAudio:') || (e?.prototype?.OnOKButton && e?.prototype?.OnMouseEnter));
    /*
    all().map(m => {
    if (typeof m !== "object") return undefined;
    for (let prop in m) { if (m[prop]?.prototype?.OK && m[prop]?.prototype?.Cancel && m[prop]?.prototype?.render) return m[prop]}
    }).find(x => x)
    */

    const showModalRaw = findModuleChild((m) => {
        if (typeof m !== 'object')
            return undefined;
        for (let prop in m) {
            if (typeof m[prop] === 'function' &&
                m[prop].toString().includes('props.bDisableBackgroundDismiss') &&
                !m[prop]?.prototype?.Cancel) {
                return m[prop];
            }
        }
    });
    const oldShowModalRaw = findModuleChild((m) => {
        if (typeof m !== 'object')
            return undefined;
        for (let prop in m) {
            if (typeof m[prop] === 'function' && m[prop].toString().includes('bHideMainWindowForPopouts:!0')) {
                return m[prop];
            }
        }
    });
    const showModal = (modal, parent, props = {
        strTitle: 'Decky Dialog',
        bHideMainWindowForPopouts: false,
    }) => {
        if (showModalRaw) {
            return showModalRaw(modal, parent || findSP(), props.strTitle, props, undefined, {
                bHideActions: props.bHideActionIcons,
            });
        }
        else if (oldShowModalRaw) {
            return oldShowModalRaw(modal, parent || findSP(), props);
        }
        else {
            throw new Error('[DFL:Modals]: Cannot find showModal function');
        }
    };
    const ConfirmModal = findModuleChild((m) => {
        if (typeof m !== 'object')
            return undefined;
        for (let prop in m) {
            if (!m[prop]?.prototype?.OK && m[prop]?.prototype?.Cancel && m[prop]?.prototype?.render) {
                return m[prop];
            }
        }
    });
    // new as of december 2022 on beta
    (Object.values(findModule((m) => {
        if (typeof m !== 'object')
            return false;
        for (let prop in m) {
            if (m[prop]?.m_mapModalManager && Object.values(m)?.find((x) => x?.type)) {
                return true;
            }
        }
        return false;
    }) || {})?.find((x) => x?.type?.toString()?.includes('((function(){')) ||
        // before december 2022 beta
        Object.values(findModule((m) => {
            if (typeof m !== 'object')
                return false;
            for (let prop in m) {
                if (m[prop]?.toString()?.includes('"ModalManager","DialogWrapper"')) {
                    return true;
                }
            }
            return false;
        }) || {})?.find((x) => x?.type?.toString()?.includes('((function(){')) ||
        // old
        findModuleChild((m) => {
            if (typeof m !== 'object')
                return undefined;
            for (let prop in m) {
                if (m[prop]?.prototype?.OK && m[prop]?.prototype?.Cancel && m[prop]?.prototype?.render) {
                    return m[prop];
                }
            }
        }));
    const ModalModule = findModule((mod) => {
        if (typeof mod !== 'object')
            return false;
        for (let prop in mod) {
            if (Object.keys(mod).length > 4 && mod[prop]?.toString().includes('.ModalPosition,fallback:'))
                return true;
        }
        return false;
    });
    const ModalModuleProps = ModalModule ? Object.values(ModalModule) : [];
    ModalModuleProps.find(prop => {
        const string = prop?.toString();
        return string?.includes(".ShowPortalModal()") && string?.includes(".OnElementReadyCallbacks.Register(");
    });
    ModalModuleProps.find(prop => prop?.toString().includes(".ModalPosition,fallback:"));
    var MessageBoxResult;
    (function (MessageBoxResult) {
        MessageBoxResult[MessageBoxResult["close"] = 0] = "close";
        MessageBoxResult[MessageBoxResult["okay"] = 1] = "okay";
    })(MessageBoxResult || (MessageBoxResult = {}));

    const [mod, panelSection] = findModuleDetailsByExport((e) => e.toString()?.includes('.PanelSection'));
    Object.values(mod).filter((exp) => !exp?.toString()?.includes('.PanelSection'))[0];

    findModuleExport((e) => e?.toString()?.includes('.ProgressBar,"standard"=='));
    findModuleExport((e) => e?.toString()?.includes('.ProgressBarFieldStatus},'));
    const progressBarItemRegex = createPropListRegex(["indeterminate", "nTransitionSec", "nProgress"]);
    findModuleExport((e) => e?.toString && progressBarItemRegex.test(e.toString()));

    const sidebarNavigationRegex = createPropListRegex(["pages", "fnSetNavigateToPage", "disableRouteReporting"]);
    findModuleExport((e) => e?.toString && sidebarNavigationRegex.test(e.toString()));

    Object.values(CommonUIModule).find((mod) => mod?.toString()?.includes('SliderField,fallback'));

    // TODO type this and other icons?
    Object.values(IconsModule)?.find((mod) => mod?.toString && /Spinner\)}\)?,.\.createElement\(\"path\",{d:\"M18 /.test(mod.toString()));

    findModuleExport((e) => e?.toString?.()?.includes('Steam Spinner') && e?.toString?.()?.includes('src'));

    let oldTabs;
    try {
        const oldTabsModule = findModuleByExport((e) => e.Unbleed);
        if (oldTabsModule)
            oldTabs = Object.values(oldTabsModule).find((x) => x?.type?.toString()?.includes('((function(){'));
    }
    catch (e) {
        console.error('Error finding oldTabs:', e);
    }

    Object.values(CommonUIModule).find((mod) => mod?.validateUrl && mod?.validateEmail);

    const Toggle = Object.values(CommonUIModule).find((mod) => mod?.render?.toString()?.includes('.ToggleOff)'));

    Object.values(CommonUIModule).find((mod) => mod?.render?.toString()?.includes('ToggleField,fallback'));

    const ScrollingModule = findModuleByExport((e) => e?.render?.toString?.().includes('{case"x":'));
    const ScrollingModuleProps = ScrollingModule ? Object.values(ScrollingModule) : [];
    ScrollingModuleProps.find((prop) => prop?.render?.toString?.().includes('{case"x":'));
    findModuleExport((e) => e?.render?.toString().includes('.FocusVisibleChild()),[])'));

    /**
     * Get the current params from ReactRouter
     *
     * @returns an object with the current ReactRouter params
     *
     * @example
     * import { useParams } from "decky-frontend-lib";
     *
     * const { appid } = useParams<{ appid: string }>()
     */
    Object.values(ReactRouter).find((val) => /return (\w)\?\1\.params:{}/.test(`${val}`));

    // import { sleep } from '../utils';
    var SideMenu;
    (function (SideMenu) {
        SideMenu[SideMenu["None"] = 0] = "None";
        SideMenu[SideMenu["Main"] = 1] = "Main";
        SideMenu[SideMenu["QuickAccess"] = 2] = "QuickAccess";
    })(SideMenu || (SideMenu = {}));
    var QuickAccessTab;
    (function (QuickAccessTab) {
        QuickAccessTab[QuickAccessTab["Notifications"] = 0] = "Notifications";
        QuickAccessTab[QuickAccessTab["RemotePlayTogetherControls"] = 1] = "RemotePlayTogetherControls";
        QuickAccessTab[QuickAccessTab["VoiceChat"] = 2] = "VoiceChat";
        QuickAccessTab[QuickAccessTab["Friends"] = 3] = "Friends";
        QuickAccessTab[QuickAccessTab["Settings"] = 4] = "Settings";
        QuickAccessTab[QuickAccessTab["Perf"] = 5] = "Perf";
        QuickAccessTab[QuickAccessTab["Help"] = 6] = "Help";
        QuickAccessTab[QuickAccessTab["Music"] = 7] = "Music";
        QuickAccessTab[QuickAccessTab["Decky"] = 999] = "Decky";
    })(QuickAccessTab || (QuickAccessTab = {}));
    var DisplayStatus;
    (function (DisplayStatus) {
        DisplayStatus[DisplayStatus["Invalid"] = 0] = "Invalid";
        DisplayStatus[DisplayStatus["Launching"] = 1] = "Launching";
        DisplayStatus[DisplayStatus["Uninstalling"] = 2] = "Uninstalling";
        DisplayStatus[DisplayStatus["Installing"] = 3] = "Installing";
        DisplayStatus[DisplayStatus["Running"] = 4] = "Running";
        DisplayStatus[DisplayStatus["Validating"] = 5] = "Validating";
        DisplayStatus[DisplayStatus["Updating"] = 6] = "Updating";
        DisplayStatus[DisplayStatus["Downloading"] = 7] = "Downloading";
        DisplayStatus[DisplayStatus["Synchronizing"] = 8] = "Synchronizing";
        DisplayStatus[DisplayStatus["ReadyToInstall"] = 9] = "ReadyToInstall";
        DisplayStatus[DisplayStatus["ReadyToPreload"] = 10] = "ReadyToPreload";
        DisplayStatus[DisplayStatus["ReadyToLaunch"] = 11] = "ReadyToLaunch";
        DisplayStatus[DisplayStatus["RegionRestricted"] = 12] = "RegionRestricted";
        DisplayStatus[DisplayStatus["PresaleOnly"] = 13] = "PresaleOnly";
        DisplayStatus[DisplayStatus["InvalidPlatform"] = 14] = "InvalidPlatform";
        DisplayStatus[DisplayStatus["PreloadComplete"] = 16] = "PreloadComplete";
        DisplayStatus[DisplayStatus["BorrowerLocked"] = 17] = "BorrowerLocked";
        DisplayStatus[DisplayStatus["UpdatePaused"] = 18] = "UpdatePaused";
        DisplayStatus[DisplayStatus["UpdateQueued"] = 19] = "UpdateQueued";
        DisplayStatus[DisplayStatus["UpdateRequired"] = 20] = "UpdateRequired";
        DisplayStatus[DisplayStatus["UpdateDisabled"] = 21] = "UpdateDisabled";
        DisplayStatus[DisplayStatus["DownloadPaused"] = 22] = "DownloadPaused";
        DisplayStatus[DisplayStatus["DownloadQueued"] = 23] = "DownloadQueued";
        DisplayStatus[DisplayStatus["DownloadRequired"] = 24] = "DownloadRequired";
        DisplayStatus[DisplayStatus["DownloadDisabled"] = 25] = "DownloadDisabled";
        DisplayStatus[DisplayStatus["LicensePending"] = 26] = "LicensePending";
        DisplayStatus[DisplayStatus["LicenseExpired"] = 27] = "LicenseExpired";
        DisplayStatus[DisplayStatus["AvailForFree"] = 28] = "AvailForFree";
        DisplayStatus[DisplayStatus["AvailToBorrow"] = 29] = "AvailToBorrow";
        DisplayStatus[DisplayStatus["AvailGuestPass"] = 30] = "AvailGuestPass";
        DisplayStatus[DisplayStatus["Purchase"] = 31] = "Purchase";
        DisplayStatus[DisplayStatus["Unavailable"] = 32] = "Unavailable";
        DisplayStatus[DisplayStatus["NotLaunchable"] = 33] = "NotLaunchable";
        DisplayStatus[DisplayStatus["CloudError"] = 34] = "CloudError";
        DisplayStatus[DisplayStatus["CloudOutOfDate"] = 35] = "CloudOutOfDate";
        DisplayStatus[DisplayStatus["Terminating"] = 36] = "Terminating";
    })(DisplayStatus || (DisplayStatus = {}));
    findModuleExport((e) => e.Navigate && e.NavigationManager);
    // try {
    //   (async () => {
    //     let InternalNavigators: any = {};
    //     if (!Router.NavigateToAppProperties || (Router as unknown as any).deckyShim) {
    //       function initInternalNavigators() {
    //         try {
    //           InternalNavigators = findModuleExport((e: Export) => e.GetNavigator && e.SetNavigator)?.GetNavigator();
    //         } catch (e) {
    //           console.error('[DFL:Router]: Failed to init internal navigators, trying again');
    //         }
    //       }
    //       initInternalNavigators();
    //       while (!InternalNavigators?.AppProperties) {
    //         console.log('[DFL:Router]: Trying to init internal navigators again');
    //         await sleep(2000);
    //         initInternalNavigators();
    //       }
    //     }
    //     const newNavigation = {
    //       Navigate: Router.Navigate?.bind(Router),
    //       NavigateBack: Router.WindowStore?.GamepadUIMainWindowInstance?.NavigateBack?.bind(
    //         Router.WindowStore.GamepadUIMainWindowInstance,
    //       ),
    //       NavigateToAppProperties: InternalNavigators?.AppProperties || Router.NavigateToAppProperties?.bind(Router),
    //       NavigateToExternalWeb: InternalNavigators?.ExternalWeb || Router.NavigateToExternalWeb?.bind(Router),
    //       NavigateToInvites: InternalNavigators?.Invites || Router.NavigateToInvites?.bind(Router),
    //       NavigateToChat: InternalNavigators?.Chat || Router.NavigateToChat?.bind(Router),
    //       NavigateToLibraryTab: InternalNavigators?.LibraryTab || Router.NavigateToLibraryTab?.bind(Router),
    //       NavigateToLayoutPreview: Router.NavigateToLayoutPreview?.bind(Router),
    //       NavigateToSteamWeb: Router.WindowStore?.GamepadUIMainWindowInstance?.NavigateToSteamWeb?.bind(
    //         Router.WindowStore.GamepadUIMainWindowInstance,
    //       ),
    //       OpenSideMenu: Router.WindowStore?.GamepadUIMainWindowInstance?.MenuStore.OpenSideMenu?.bind(
    //         Router.WindowStore.GamepadUIMainWindowInstance.MenuStore,
    //       ),
    //       OpenQuickAccessMenu: Router.WindowStore?.GamepadUIMainWindowInstance?.MenuStore.OpenQuickAccessMenu?.bind(
    //         Router.WindowStore.GamepadUIMainWindowInstance.MenuStore,
    //       ),
    //       OpenMainMenu: Router.WindowStore?.GamepadUIMainWindowInstance?.MenuStore.OpenMainMenu?.bind(
    //         Router.WindowStore.GamepadUIMainWindowInstance.MenuStore,
    //       ),
    //       CloseSideMenus: Router.CloseSideMenus?.bind(Router),
    //       OpenPowerMenu: Router.OpenPowerMenu?.bind(Router),
    //     } as Navigation;
    //     Object.assign(Navigation, newNavigation);
    //   })();
    // } catch (e) {
    //   console.error('[DFL:Router]: Error initializing Navigation interface', e);
    // }

    const IPCMain = {
        postMessage: (messageId, contents) => {
            return new Promise((resolve) => {
                const message = { id: messageId, iteration: window.CURRENT_IPC_CALL_COUNT++, data: contents };
                const messageHandler = function (data) {
                    const json = JSON.parse(data.data);
                    /**
                     * wait to receive the correct message id from the backend
                     */
                    if (json.id != message.iteration)
                        return;
                    resolve(json);
                    window.MILLENNIUM_IPC_SOCKET.removeEventListener('message', messageHandler);
                };
                window.MILLENNIUM_IPC_SOCKET.addEventListener('message', messageHandler);
                window.MILLENNIUM_IPC_SOCKET.send(JSON.stringify(message));
            });
        }
    };
    window.MILLENNIUM_BACKEND_IPC = IPCMain;
    window.Millennium = {
        // @ts-ignore (ignore overloaded function)
        callServerMethod: (pluginName, methodName, kwargs) => {
            return new Promise((resolve, reject) => {
                const query = {
                    pluginName,
                    methodName,
                    ...(kwargs && { argumentList: kwargs })
                };
                /* call handled from "src\core\ipc\pipe.cpp @ L:67" */
                window.MILLENNIUM_BACKEND_IPC.postMessage(0, query).then((response) => {
                    if (response?.failedRequest) {
                        reject(` IPC call from [name: ${pluginName}, method: ${methodName}] failed on exception -> ${response.failMessage}`);
                    }
                    const responseStream = response.returnValue;
                    // FFI backend encodes string responses in base64 to avoid encoding issues
                    resolve(typeof responseStream === 'string' ? atob(responseStream) : responseStream);
                });
            });
        },
        AddWindowCreateHook: (callback) => {
            // used to have extended functionality but removed since it was shotty
            g_PopupManager.AddPopupCreatedCallback((e) => {
                callback(e);
            });
        },
        findElement: (privateDocument, querySelector, timeout) => {
            return new Promise((resolve, reject) => {
                const matchedElements = privateDocument.querySelectorAll(querySelector);
                /**
                 * node is already in DOM and doesn't require watchdog
                 */
                if (matchedElements.length) {
                    resolve(matchedElements);
                }
                let timer = null;
                const observer = new MutationObserver(() => {
                    const matchedElements = privateDocument.querySelectorAll(querySelector);
                    if (matchedElements.length) {
                        if (timer)
                            clearTimeout(timer);
                        observer.disconnect();
                        resolve(matchedElements);
                    }
                });
                /** observe the document body for item changes, assuming we are waiting for target element */
                observer.observe(privateDocument.body, {
                    childList: true,
                    subtree: true
                });
                if (timeout) {
                    timer = setTimeout(() => {
                        observer.disconnect();
                        reject();
                    }, timeout);
                }
            });
        },
        exposeObj: function (obj) {
            for (const key in obj) {
                exports[key] = obj[key];
            }
        }
    };
    /**
     * @brief
     * pluginSelf is a sandbox for data specific to your plugin.
     * You can't access other plugins sandboxes and they can't access yours
     *
     * @example
     * | pluginSelf.var = "Hello"
     * | console.log(pluginSelf.var) -> Hello
     */
    const pluginSelf = window.PLUGIN_LIST[pluginName];
    const Millennium = window.Millennium;

    const CommonPatchTypes = [
        "TargetCss", "TargetJs"
    ];
    var ConditionalControlFlowType;
    (function (ConditionalControlFlowType) {
        ConditionalControlFlowType[ConditionalControlFlowType["TargetCss"] = 0] = "TargetCss";
        ConditionalControlFlowType[ConditionalControlFlowType["TargetJs"] = 1] = "TargetJs";
    })(ConditionalControlFlowType || (ConditionalControlFlowType = {}));

    const DOMModifier = {
        /**
         * Append a StyleSheet to DOM from raw text
         * @param document Target document to append StyleSheet to
         * @param innerStyle string encoded CSS
         * @param id HTMLElement id
         */
        AddStyleSheetFromText: (document, innerStyle, id) => {
            if (document.querySelectorAll(`style[id='${id}']`).length)
                return;
            document.head.appendChild(Object.assign(document.createElement('style'), { id: id })).innerText = innerStyle;
        },
        /**
         * Append a StyleSheet to DOM from loopbackhost or absolute URI
         * @param document Target document to append StyleSheet to
         * @param localPath relative/absolute path to CSS module
         */
        AddStyleSheet: (document, localPath) => {
            if (!pluginSelf.stylesAllowed)
                return;
            if (document.querySelectorAll(`link[href='${localPath}']`).length)
                return;
            document.head.appendChild(Object.assign(document.createElement('link'), {
                href: localPath,
                rel: 'stylesheet', id: 'millennium-injected'
            }));
        },
        /**
         * Append a JavaScript module to DOM from loopbackhost or absolute URI
         * @param document Target document to append JavaScript to
         * @param localPath relative/absolute path to CSS module
         */
        AddJavaScript: (document, localPath) => {
            if (!pluginSelf.scriptsAllowed)
                return;
            if (document.querySelectorAll(`script[src='${localPath}'][type='module']`).length)
                return;
            document.head.appendChild(Object.assign(document.createElement('script'), {
                src: localPath,
                type: 'module', id: 'millennium-injected'
            }));
        }
    };
    function constructThemePath(nativeName, relativePath) {
        return ['skins', nativeName, relativePath].join('/');
    }
    const classListMatch = (classList, affectee) => {
        for (const classItem in classList) {
            if (classList[classItem].includes(affectee)) {
                return true;
            }
        }
        return false;
    };
    const EvaluatePatch = (type, modulePatch, documentTitle, classList, document) => {
        if (modulePatch?.[CommonPatchTypes?.[type]] === undefined) {
            return;
        }
        modulePatch[CommonPatchTypes[type]].affects.forEach((affectee) => {
            if (!documentTitle.match(affectee) && !classListMatch(classList, affectee)) {
                return;
            }
            switch (type) {
                case ConditionalControlFlowType.TargetCss: {
                    DOMModifier.AddStyleSheet(document, constructThemePath(pluginSelf.activeTheme.native, modulePatch[CommonPatchTypes[type]].src));
                }
                case ConditionalControlFlowType.TargetJs: {
                    DOMModifier.AddJavaScript(document, constructThemePath(pluginSelf.activeTheme.native, modulePatch[CommonPatchTypes[type]].src));
                }
            }
        });
    };

    const EvaluateConditions = (theme, title, classes, document) => {
        const themeConditions = theme.data.Conditions;
        const savedConditions = pluginSelf.conditionals[theme.native];
        for (const condition in themeConditions) {
            if (!themeConditions.hasOwnProperty(condition)) {
                return;
            }
            if (condition in savedConditions) {
                const patch = themeConditions[condition].values[savedConditions[condition]];
                EvaluatePatch(ConditionalControlFlowType.TargetCss, patch, title, classes, document);
                EvaluatePatch(ConditionalControlFlowType.TargetJs, patch, title, classes, document);
            }
        }
    };

    /**
     * @deprecated this entire module is deprecated and is only here to support Millennium <= 1.1.5
     *
     * @note this module does not provide interfaces to edit the deprecated conditions,
     * it serves only to allow old ones to still work until they are properly updated by the developer.
     */
    var ConfigurationItemType;
    (function (ConfigurationItemType) {
        ConfigurationItemType["ComboBox"] = "ComboBox";
        ConfigurationItemType["CheckBox"] = "CheckBox";
    })(ConfigurationItemType || (ConfigurationItemType = {}));
    const GetFromConfigurationStore = (configName) => {
        const activeTheme = pluginSelf.activeTheme.data;
        for (const configItem of activeTheme.Configuration) {
            if (configItem.Name === configName) {
                return configItem;
            }
        }
        return undefined;
    };
    const InsertModule = (target, document) => {
        const activeTheme = pluginSelf.activeTheme;
        target?.TargetCss && DOMModifier.AddStyleSheet(document, constructThemePath(activeTheme.native, target.TargetCss));
        target?.TargetJs && DOMModifier.AddJavaScript(document, constructThemePath(activeTheme.native, target.TargetJs));
    };
    const EvaluateComboBox = (statement, currentValue, document) => {
        statement.Combo.forEach((comboItem) => {
            if (comboItem.Equals === currentValue) {
                InsertModule(comboItem?.True, document);
            }
            else {
                InsertModule(comboItem?.False, document);
            }
        });
    };
    const EvaluateCheckBox = (statement, currentValue, document) => {
        if (statement.Equals === currentValue) {
            InsertModule(statement?.True, document);
        }
        else {
            InsertModule(statement?.False, document);
        }
    };
    const EvaluateType = (statement) => {
        return statement.Combo !== undefined ? ConfigurationItemType.ComboBox : ConfigurationItemType.CheckBox;
    };
    const EvaluateStatement = (statement, document) => {
        const statementId = statement.If;
        const statementStore = GetFromConfigurationStore(statementId);
        const storedStatementValue = statementStore.Value;
        const statementType = EvaluateType(statement);
        switch (statementType) {
            case ConfigurationItemType.CheckBox: {
                EvaluateCheckBox(statement, storedStatementValue, document);
                break;
            }
            case ConfigurationItemType.ComboBox: {
                EvaluateComboBox(statement, storedStatementValue, document);
                break;
            }
        }
    };
    const EvaluateStatements = (patchItem, document) => {
        if (Array.isArray(patchItem.Statement)) {
            patchItem.Statement.forEach(statement => {
                EvaluateStatement(statement, document);
            });
        }
        else {
            EvaluateStatement(patchItem.Statement, document);
        }
    };

    const EvaluateModule = (module, type, document) => {
        const activeTheme = pluginSelf.activeTheme;
        switch (type) {
            case ConditionalControlFlowType.TargetCss:
                DOMModifier.AddStyleSheet(document, constructThemePath(activeTheme.native, module));
                break;
            case ConditionalControlFlowType.TargetJs:
                DOMModifier.AddJavaScript(document, constructThemePath(activeTheme.native, module));
                break;
        }
    };
    /**
     * @brief evaluates list of; or single module
     *
     * @param module module(s) to be injected into the frame
     * @param type the type of the module
     * @returns null
     */
    const SanitizeTargetModule = (module, type, document) => {
        if (module === undefined) {
            return;
        }
        else if (typeof module === 'string') {
            EvaluateModule(module, type, document);
        }
        else if (Array.isArray(module)) {
            module.forEach((node) => EvaluateModule(node, type, document));
        }
    };
    const EvaluatePatches = (activeTheme, documentTitle, classList, document, context) => {
        activeTheme.data.Patches.forEach((patch) => {
            const match = patch.MatchRegexString;
            context.m_popup.window.HAS_INJECTED_THEME = true;
            if (!documentTitle.match(match) && !classListMatch(classList, match)) {
                return;
            }
            SanitizeTargetModule(patch?.TargetCss, ConditionalControlFlowType.TargetCss, document);
            SanitizeTargetModule(patch?.TargetJs, ConditionalControlFlowType.TargetJs, document);
            // backwards compatability with old millennium versions. 
            const PatchV1 = patch;
            if (pluginSelf.conditionVersion == 1 && PatchV1?.Statement !== undefined) {
                EvaluateStatements(PatchV1, document);
            }
        });
    };
    /**
     * parses all classnames from a window and concatenates into one list
     * @param context window context from g_popupManager
     * @returns
     */
    const getDocumentClassList = (context) => {
        const bodyClass = context?.m_rgParams?.body_class ?? String();
        const htmlClass = context?.m_rgParams?.html_class ?? String();
        return (`${bodyClass} ${htmlClass}`).split(' ').map((className) => '.' + className);
    };
    function patchDocumentContext(windowContext) {
        if (pluginSelf.isDefaultTheme) {
            return;
        }
        const activeTheme = pluginSelf.activeTheme;
        const document = windowContext.m_popup.document;
        const classList = getDocumentClassList(windowContext);
        const documentTitle = windowContext.m_strTitle;
        // Append System Accent Colors to global document (publically shared)
        DOMModifier.AddStyleSheetFromText(document, pluginSelf.systemColor, "SystemAccentColorInject");
        // Append old global colors struct to DOM
        pluginSelf?.GlobalsColors && DOMModifier.AddStyleSheetFromText(document, pluginSelf.GlobalsColors, "GlobalColors");
        if (activeTheme?.data?.Conditions) {
            pluginSelf.conditionVersion = 2;
            EvaluateConditions(activeTheme, documentTitle, classList, document);
        }
        else {
            pluginSelf.conditionVersion = 1;
        }
        activeTheme?.data?.hasOwnProperty("Patches") && EvaluatePatches(activeTheme, documentTitle, classList, document, windowContext);
        if (activeTheme?.data?.hasOwnProperty("RootColors")) {
            wrappedCallServerMethod("cfg.get_colors").then((colors) => {
                DOMModifier.AddStyleSheetFromText(document, colors, "RootColors");
            });
        }
    }

    var settingsPanelPlugins$5 = "Plugins";
    var settingsPanelThemes$5 = "Themes";
    var settingsPanelUpdates$5 = "Updates";
    var itemNoDescription$6 = "No description yet.";
    var themePanelClientTheme$5 = "Client Theme";
    var themePanelThemeTooltip$6 = "Select the theme you want Steam to use (requires reload)";
    var themePanelGetMoreThemes$6 = "Get more themes";
    var themePanelInjectJavascript$6 = "Inject JavaScript";
    var themePanelInjectJavascriptToolTip$6 = "Decide whether themes are allowed to insert JavaScript into Steam. Disabling JavaScript may break Steam interface as a byproduct (requires reload)";
    var themePanelInjectCSS$6 = "Inject StyleSheets";
    var themePanelInjectCSSToolTip$6 = "Decide whether themes are allowed to insert stylesheets into Steam. (requires reload)";
    var updatePanelHasUpdates$6 = "Updates Available!";
    var updatePanelHasUpdatesSub$6 = "Millennium found the following updates for your themes.";
    var updatePanelReleasedTag$6 = "Released:";
    var updatePanelReleasePatchNotes$6 = "Patch Notes:";
    var updatePanelIsUpdating$5 = "Updating...";
    var updatePanelUpdate$5 = "Update";
    var updatePanelNoUpdatesFound$6 = "No updates found. You're good to go!";
    var ViewMore$6 = "View More";
    var aboutThemeAnonymous$6 = "Anonymous";
    var aboutThemeTitle$6 = "About";
    var aboutThemeVerifiedDev$6 = "Verified Developer";
    var viewSourceCode$6 = "View Source Code";
    var showInFolder$6 = "Show in Folder";
    var uninstall$6 = "Uninstall";
    var optionReloadNow$6 = "Reload Now";
    var optionReloadLater$6 = "Reload Later";
    var updatePanelUpdateNotifications$2 = "Push Notifications";
    var updatePanelUpdateNotificationsTooltip$2 = "Get Millennium to give you a reminder when a item in your library has an update!";
    var customThemeSettingsColorsHeader = "Color Options";
    var customThemeSettingsColorsDescription = "Customize the colors of your theme.";
    var customThemeSettingsConfigHeader = "Custom Settings";
    var customThemeSettingsConfigDescription = "Customize the settings of your theme.";
    var english = {
    	settingsPanelPlugins: settingsPanelPlugins$5,
    	settingsPanelThemes: settingsPanelThemes$5,
    	settingsPanelUpdates: settingsPanelUpdates$5,
    	itemNoDescription: itemNoDescription$6,
    	themePanelClientTheme: themePanelClientTheme$5,
    	themePanelThemeTooltip: themePanelThemeTooltip$6,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$6,
    	themePanelInjectJavascript: themePanelInjectJavascript$6,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$6,
    	themePanelInjectCSS: themePanelInjectCSS$6,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$6,
    	updatePanelHasUpdates: updatePanelHasUpdates$6,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$6,
    	updatePanelReleasedTag: updatePanelReleasedTag$6,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$6,
    	updatePanelIsUpdating: updatePanelIsUpdating$5,
    	updatePanelUpdate: updatePanelUpdate$5,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$6,
    	ViewMore: ViewMore$6,
    	aboutThemeAnonymous: aboutThemeAnonymous$6,
    	aboutThemeTitle: aboutThemeTitle$6,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$6,
    	viewSourceCode: viewSourceCode$6,
    	showInFolder: showInFolder$6,
    	uninstall: uninstall$6,
    	optionReloadNow: optionReloadNow$6,
    	optionReloadLater: optionReloadLater$6,
    	updatePanelUpdateNotifications: updatePanelUpdateNotifications$2,
    	updatePanelUpdateNotificationsTooltip: updatePanelUpdateNotificationsTooltip$2,
    	customThemeSettingsColorsHeader: customThemeSettingsColorsHeader,
    	customThemeSettingsColorsDescription: customThemeSettingsColorsDescription,
    	customThemeSettingsConfigHeader: customThemeSettingsConfigHeader,
    	customThemeSettingsConfigDescription: customThemeSettingsConfigDescription
    };

    var settingsPanelPlugins$4 = "Wtyczki";
    var settingsPanelThemes$4 = "Motywy";
    var settingsPanelUpdates$4 = "Aktualizacje";
    var itemNoDescription$5 = "Brak opisu.";
    var themePanelClientTheme$4 = "Motyw klienta";
    var themePanelThemeTooltip$5 = "Wybierz motyw, którego ma używać Steam (wymaga ponownego załadowania)";
    var themePanelGetMoreThemes$5 = "Pobierz więcej motywów";
    var themePanelInjectJavascript$5 = "Wstrzyknij JavaScript";
    var themePanelInjectJavascriptToolTip$5 = "Zdecyduj, czy motywy mogą wstrzykiwać JavaScript do Steam. Wyłączenie JavaScriptu może spowodować problemy z interfejsem Steam (wymaga ponownego załadowania)";
    var themePanelInjectCSS$5 = "Wstrzyknij arkusze stylów";
    var themePanelInjectCSSToolTip$5 = "Zdecyduj, czy motywy mogą wstrzykiwać arkusze stylów do Steam. (wymaga ponownego załadowania)";
    var updatePanelHasUpdates$5 = "Dostępne aktualizacje!";
    var updatePanelHasUpdatesSub$5 = "Millennium znalazł następujące aktualizacje dla Twoich motywów.";
    var updatePanelReleasedTag$5 = "Wydano:";
    var updatePanelReleasePatchNotes$5 = "Uwagi do wydania:";
    var updatePanelIsUpdating$4 = "Aktualizowanie...";
    var updatePanelUpdate$4 = "Aktualizuj";
    var updatePanelNoUpdatesFound$5 = "Brak dostępnych aktualizacji. Wszystko jest aktualne!";
    var ViewMore$5 = "Zobacz więcej";
    var aboutThemeAnonymous$5 = "Anonimowy";
    var aboutThemeTitle$5 = "O motywie";
    var aboutThemeVerifiedDev$5 = "Zweryfikowany deweloper";
    var viewSourceCode$5 = "Zobacz kod źródłowy";
    var showInFolder$5 = "Pokaż w folderze";
    var uninstall$5 = "Odinstaluj";
    var optionReloadNow$5 = "Załaduj ponownie teraz";
    var optionReloadLater$5 = "Załaduj ponownie później";
    var polish = {
    	settingsPanelPlugins: settingsPanelPlugins$4,
    	settingsPanelThemes: settingsPanelThemes$4,
    	settingsPanelUpdates: settingsPanelUpdates$4,
    	itemNoDescription: itemNoDescription$5,
    	themePanelClientTheme: themePanelClientTheme$4,
    	themePanelThemeTooltip: themePanelThemeTooltip$5,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$5,
    	themePanelInjectJavascript: themePanelInjectJavascript$5,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$5,
    	themePanelInjectCSS: themePanelInjectCSS$5,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$5,
    	updatePanelHasUpdates: updatePanelHasUpdates$5,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$5,
    	updatePanelReleasedTag: updatePanelReleasedTag$5,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$5,
    	updatePanelIsUpdating: updatePanelIsUpdating$4,
    	updatePanelUpdate: updatePanelUpdate$4,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$5,
    	ViewMore: ViewMore$5,
    	aboutThemeAnonymous: aboutThemeAnonymous$5,
    	aboutThemeTitle: aboutThemeTitle$5,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$5,
    	viewSourceCode: viewSourceCode$5,
    	showInFolder: showInFolder$5,
    	uninstall: uninstall$5,
    	optionReloadNow: optionReloadNow$5,
    	optionReloadLater: optionReloadLater$5
    };

    var settingsPanelPlugins$3 = "Complementos";
    var settingsPanelThemes$3 = "Temas";
    var settingsPanelUpdates$3 = "Actualizaciones";
    var itemNoDescription$4 = "Sin Descripción.";
    var themePanelClientTheme$3 = "Tema del Cliente";
    var themePanelThemeTooltip$4 = "Selecciona el tema que quieres que Steam use (requiere reiniciar)";
    var themePanelGetMoreThemes$4 = "Conseguir más temas";
    var themePanelInjectJavascript$4 = "Inyectar JavaScript";
    var themePanelInjectJavascriptToolTip$4 = "Decidir que temas tienen permiso para insertar JavaScript en Steam. Deshabilitar JavaScript puede romper la interfaz de Steam como consecuencia (requiere reiniciar)";
    var themePanelInjectCSS$4 = "Inyectar StyleSheets";
    var themePanelInjectCSSToolTip$4 = "Decidir que temas tienen permiso para insertar stylesheets en Steam. (requiere recargar)";
    var updatePanelHasUpdates$4 = "¡Actualizaciones Disponibles!";
    var updatePanelHasUpdatesSub$4 = "Millennium ha encontrado las siguientes actualizaciones para tus temas.";
    var updatePanelReleasedTag$4 = "Publicado:";
    var updatePanelReleasePatchNotes$4 = "Notas de Parche:";
    var updatePanelIsUpdating$3 = "Actualizando...";
    var updatePanelUpdate$3 = "Actualizar";
    var updatePanelNoUpdatesFound$4 = "No se han encontrado actualizaciones. ¡Todo listo!";
    var ViewMore$4 = "Ver más";
    var aboutThemeAnonymous$4 = "Anónimo";
    var aboutThemeTitle$4 = "Sobre";
    var aboutThemeVerifiedDev$4 = "Desarrollador Verificado";
    var viewSourceCode$4 = "Ver Código Fuente";
    var showInFolder$4 = "Mostrar en Carpeta";
    var uninstall$4 = "Desinstalar";
    var optionReloadNow$4 = "Reiniciar Ahora";
    var optionReloadLater$4 = "Reiniciar Después";
    var spanish = {
    	settingsPanelPlugins: settingsPanelPlugins$3,
    	settingsPanelThemes: settingsPanelThemes$3,
    	settingsPanelUpdates: settingsPanelUpdates$3,
    	itemNoDescription: itemNoDescription$4,
    	themePanelClientTheme: themePanelClientTheme$3,
    	themePanelThemeTooltip: themePanelThemeTooltip$4,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$4,
    	themePanelInjectJavascript: themePanelInjectJavascript$4,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$4,
    	themePanelInjectCSS: themePanelInjectCSS$4,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$4,
    	updatePanelHasUpdates: updatePanelHasUpdates$4,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$4,
    	updatePanelReleasedTag: updatePanelReleasedTag$4,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$4,
    	updatePanelIsUpdating: updatePanelIsUpdating$3,
    	updatePanelUpdate: updatePanelUpdate$3,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$4,
    	ViewMore: ViewMore$4,
    	aboutThemeAnonymous: aboutThemeAnonymous$4,
    	aboutThemeTitle: aboutThemeTitle$4,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$4,
    	viewSourceCode: viewSourceCode$4,
    	showInFolder: showInFolder$4,
    	uninstall: uninstall$4,
    	optionReloadNow: optionReloadNow$4,
    	optionReloadLater: optionReloadLater$4
    };

    var settingsPanelPlugins$2 = "Plugins";
    var settingsPanelThemes$2 = "Tema";
    var settingsPanelUpdates$2 = "Pembaruan";
    var itemNoDescription$3 = "Tidak ada deskripsi.";
    var themePanelClientTheme$2 = "Tema Klien";
    var themePanelThemeTooltip$3 = "Pilih tema yang akan digunakan Steam (diperlukan muat ulang)";
    var themePanelGetMoreThemes$3 = "Lebih banyak tema";
    var themePanelInjectJavascript$3 = "Gunakan JavaScript";
    var themePanelInjectJavascriptToolTip$3 = "Mengizinkan tema untuk memasukkan JavaScript ke Steam. Menonaktifkan JavaScript dapat merusak antarmuka Steam (diperlukan muat ulang)";
    var themePanelInjectCSS$3 = "Gunakan StyleSheets";
    var themePanelInjectCSSToolTip$3 = "Mengizinkan tema untuk memasukkan stylesheets ke Steam.";
    var updatePanelHasUpdates$3 = "Pembaruan tersedia!";
    var updatePanelHasUpdatesSub$3 = "Millenium menemukan pembaruan berikut ini untuk tema Anda.";
    var updatePanelReleasedTag$3 = "Dirilis:";
    var updatePanelReleasePatchNotes$3 = "Catatan Pembaruan:";
    var updatePanelIsUpdating$2 = "Melakukan Pembaruan...";
    var updatePanelUpdate$2 = "Pembaruan";
    var updatePanelNoUpdatesFound$3 = "Tidak ada pembaruan tersedia. Anda sudah siap!";
    var ViewMore$3 = "Lihat Lebih Banyak";
    var aboutThemeAnonymous$3 = "Anonim";
    var aboutThemeTitle$3 = "Tentang";
    var aboutThemeVerifiedDev$3 = "Pengembang Terverifikasi";
    var viewSourceCode$3 = "Lihat Source Code";
    var showInFolder$3 = "Tampilkan di Folder";
    var uninstall$3 = "Hapus Instalasi";
    var optionReloadNow$3 = "Muat Ulang Sekarang";
    var optionReloadLater$3 = "Muat Ulang Nanti";
    var indonesian = {
    	settingsPanelPlugins: settingsPanelPlugins$2,
    	settingsPanelThemes: settingsPanelThemes$2,
    	settingsPanelUpdates: settingsPanelUpdates$2,
    	itemNoDescription: itemNoDescription$3,
    	themePanelClientTheme: themePanelClientTheme$2,
    	themePanelThemeTooltip: themePanelThemeTooltip$3,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$3,
    	themePanelInjectJavascript: themePanelInjectJavascript$3,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$3,
    	themePanelInjectCSS: themePanelInjectCSS$3,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$3,
    	updatePanelHasUpdates: updatePanelHasUpdates$3,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$3,
    	updatePanelReleasedTag: updatePanelReleasedTag$3,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$3,
    	updatePanelIsUpdating: updatePanelIsUpdating$2,
    	updatePanelUpdate: updatePanelUpdate$2,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$3,
    	ViewMore: ViewMore$3,
    	aboutThemeAnonymous: aboutThemeAnonymous$3,
    	aboutThemeTitle: aboutThemeTitle$3,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$3,
    	viewSourceCode: viewSourceCode$3,
    	showInFolder: showInFolder$3,
    	uninstall: uninstall$3,
    	optionReloadNow: optionReloadNow$3,
    	optionReloadLater: optionReloadLater$3
    };

    var settingsPanelPlugins$1 = "插件";
    var settingsPanelThemes$1 = "主题";
    var settingsPanelUpdates$1 = "更新";
    var itemNoDescription$2 = "暂无描述";
    var themePanelClientTheme$1 = "客户端主题";
    var themePanelThemeTooltip$2 = "选择 Steam 主题（需要重新加载）";
    var themePanelGetMoreThemes$2 = "获取更多主题";
    var themePanelInjectJavascript$2 = "注入 JavaScript";
    var themePanelInjectJavascriptToolTip$2 = "是否允许主题在 Steam 中插入 JavaScript。禁用 JavaScript 可能会导致 Steam 界面出现问题。（需要重新加载）";
    var themePanelInjectCSS$2 = "注入 CSS";
    var themePanelInjectCSSToolTip$2 = "是否允许主题在 Steam 中插入 CSS。(需要重新加载）";
    var updatePanelHasUpdates$2 = "发现新版本！";
    var updatePanelHasUpdatesSub$2 = "Millennium 找到了以下主题的更新。";
    var updatePanelReleasedTag$2 = "发布：";
    var updatePanelReleasePatchNotes$2 = "补丁说明：";
    var updatePanelIsUpdating$1 = "更新中...";
    var updatePanelUpdate$1 = "更新";
    var updatePanelNoUpdatesFound$2 = "已经是最新版本！";
    var ViewMore$2 = "查看更多";
    var aboutThemeAnonymous$2 = "匿名";
    var aboutThemeTitle$2 = "关于";
    var aboutThemeVerifiedDev$2 = "经过验证的开发者";
    var viewSourceCode$2 = "查看源码";
    var showInFolder$2 = "打开文件位置";
    var uninstall$2 = "卸载";
    var optionReloadNow$2 = "立即重新加载";
    var optionReloadLater$2 = "稍后重新加载";
    var schinese = {
    	settingsPanelPlugins: settingsPanelPlugins$1,
    	settingsPanelThemes: settingsPanelThemes$1,
    	settingsPanelUpdates: settingsPanelUpdates$1,
    	itemNoDescription: itemNoDescription$2,
    	themePanelClientTheme: themePanelClientTheme$1,
    	themePanelThemeTooltip: themePanelThemeTooltip$2,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$2,
    	themePanelInjectJavascript: themePanelInjectJavascript$2,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$2,
    	themePanelInjectCSS: themePanelInjectCSS$2,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$2,
    	updatePanelHasUpdates: updatePanelHasUpdates$2,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$2,
    	updatePanelReleasedTag: updatePanelReleasedTag$2,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$2,
    	updatePanelIsUpdating: updatePanelIsUpdating$1,
    	updatePanelUpdate: updatePanelUpdate$1,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$2,
    	ViewMore: ViewMore$2,
    	aboutThemeAnonymous: aboutThemeAnonymous$2,
    	aboutThemeTitle: aboutThemeTitle$2,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$2,
    	viewSourceCode: viewSourceCode$2,
    	showInFolder: showInFolder$2,
    	uninstall: uninstall$2,
    	optionReloadNow: optionReloadNow$2,
    	optionReloadLater: optionReloadLater$2
    };

    var itemNoDescription$1 = "Noch keine Beschreibung.";
    var themePanelThemeTooltip$1 = "Wählen Sie das Theme aus, das Steam benutzen soll. (Neustart erforderlich)";
    var themePanelGetMoreThemes$1 = "Weitere Themes";
    var themePanelInjectJavascript$1 = "JavaScript Injizieren";
    var themePanelInjectJavascriptToolTip$1 = "Entscheiden Sie, ob Themes JavaScript in Steam einbinden dürfen. Das Deaktivieren von JavaScript kann die Steam-Schnittstelle als Nebenprodukt zerstören. (Neustart erforderlich)";
    var themePanelInjectCSS$1 = "StyleSheets Injizieren";
    var themePanelInjectCSSToolTip$1 = "Entscheiden Sie, ob Themes StyleSheets in Steam einbinden dürfen. (Neustart erforderlich)";
    var updatePanelHasUpdates$1 = "Updates verfügbar!";
    var updatePanelHasUpdatesSub$1 = "Millennium hat die folgenden Updates für Ihre Themes gefunden.";
    var updatePanelReleasedTag$1 = "Veröffentlicht:";
    var updatePanelReleasePatchNotes$1 = "Patch Notes:";
    var updatePanelNoUpdatesFound$1 = "Keine Updates gefunden. Sie sind startklar!";
    var ViewMore$1 = "Mehr anzeigen";
    var aboutThemeAnonymous$1 = "Anonym";
    var aboutThemeTitle$1 = "Über";
    var aboutThemeVerifiedDev$1 = "Verifizierter Entwickler";
    var viewSourceCode$1 = "Quellcode anzeigen";
    var showInFolder$1 = "Im Ordner anzeigen";
    var uninstall$1 = "Deinstallieren";
    var optionReloadNow$1 = "Jetzt Neustarten";
    var optionReloadLater$1 = "Später Neustarten";
    var updatePanelUpdateNotifications$1 = "Push-Benachrichtigungen";
    var updatePanelUpdateNotificationsTooltip$1 = "Lassen Sie sich von Millennium daran erinnern, wenn ein Element in Ihrer Bibliothek ein Update erhalten hat!";
    var german = {
    	itemNoDescription: itemNoDescription$1,
    	themePanelThemeTooltip: themePanelThemeTooltip$1,
    	themePanelGetMoreThemes: themePanelGetMoreThemes$1,
    	themePanelInjectJavascript: themePanelInjectJavascript$1,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip$1,
    	themePanelInjectCSS: themePanelInjectCSS$1,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip$1,
    	updatePanelHasUpdates: updatePanelHasUpdates$1,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub$1,
    	updatePanelReleasedTag: updatePanelReleasedTag$1,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes$1,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound$1,
    	ViewMore: ViewMore$1,
    	aboutThemeAnonymous: aboutThemeAnonymous$1,
    	aboutThemeTitle: aboutThemeTitle$1,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev$1,
    	viewSourceCode: viewSourceCode$1,
    	showInFolder: showInFolder$1,
    	uninstall: uninstall$1,
    	optionReloadNow: optionReloadNow$1,
    	optionReloadLater: optionReloadLater$1,
    	updatePanelUpdateNotifications: updatePanelUpdateNotifications$1,
    	updatePanelUpdateNotificationsTooltip: updatePanelUpdateNotificationsTooltip$1
    };

    var settingsPanelPlugins = "Плагины";
    var settingsPanelThemes = "Темы";
    var settingsPanelUpdates = "Обновления";
    var itemNoDescription = "Без описания.";
    var themePanelClientTheme = "Тема клиента";
    var themePanelThemeTooltip = "Выберите тему для клиента Steam (требуется переинициализация интерфейса)";
    var themePanelGetMoreThemes = "Загрузить темы";
    var themePanelInjectJavascript = "Выполнение JavaScript";
    var themePanelInjectJavascriptToolTip = "Разрешить темам выполнять JavaScript в Steam. Выключение JavaScript может поломать интерфейс Steam (требуется переинициализация интерфейса)";
    var themePanelInjectCSS = "Встраивание каскадных таблиц стилей (CSS)";
    var themePanelInjectCSSToolTip = "Разрешить темам встраивание таблиц в Steam (требуется переинициализация интерфейса)";
    var updatePanelHasUpdates = "Доступны обновления";
    var updatePanelHasUpdatesSub = "Нашлись обновления для следующих тем:";
    var updatePanelReleasedTag = "Выпуск:";
    var updatePanelReleasePatchNotes = "Заметки:";
    var updatePanelIsUpdating = "Обновление...";
    var updatePanelUpdate = "Обновление";
    var updatePanelNoUpdatesFound = "Обновлять нечего. У вас всё самое свежее!";
    var ViewMore = "Подробнее";
    var aboutThemeAnonymous = "Анонимус";
    var aboutThemeTitle = "О теме";
    var aboutThemeVerifiedDev = "Проверенный разработчик";
    var viewSourceCode = "Посмотреть исходный код";
    var showInFolder = "Показать в папке";
    var uninstall = "Удалить";
    var optionReloadNow = "Сейчас";
    var optionReloadLater = "Потом";
    var updatePanelUpdateNotifications = "Всплывающие уведомления";
    var updatePanelUpdateNotificationsTooltip = "Millennium будет уведомлять об обновлениях тем из вашей библиотеки";
    var russian = {
    	settingsPanelPlugins: settingsPanelPlugins,
    	settingsPanelThemes: settingsPanelThemes,
    	settingsPanelUpdates: settingsPanelUpdates,
    	itemNoDescription: itemNoDescription,
    	themePanelClientTheme: themePanelClientTheme,
    	themePanelThemeTooltip: themePanelThemeTooltip,
    	themePanelGetMoreThemes: themePanelGetMoreThemes,
    	themePanelInjectJavascript: themePanelInjectJavascript,
    	themePanelInjectJavascriptToolTip: themePanelInjectJavascriptToolTip,
    	themePanelInjectCSS: themePanelInjectCSS,
    	themePanelInjectCSSToolTip: themePanelInjectCSSToolTip,
    	updatePanelHasUpdates: updatePanelHasUpdates,
    	updatePanelHasUpdatesSub: updatePanelHasUpdatesSub,
    	updatePanelReleasedTag: updatePanelReleasedTag,
    	updatePanelReleasePatchNotes: updatePanelReleasePatchNotes,
    	updatePanelIsUpdating: updatePanelIsUpdating,
    	updatePanelUpdate: updatePanelUpdate,
    	updatePanelNoUpdatesFound: updatePanelNoUpdatesFound,
    	ViewMore: ViewMore,
    	aboutThemeAnonymous: aboutThemeAnonymous,
    	aboutThemeTitle: aboutThemeTitle,
    	aboutThemeVerifiedDev: aboutThemeVerifiedDev,
    	viewSourceCode: viewSourceCode,
    	showInFolder: showInFolder,
    	uninstall: uninstall,
    	optionReloadNow: optionReloadNow,
    	optionReloadLater: optionReloadLater,
    	updatePanelUpdateNotifications: updatePanelUpdateNotifications,
    	updatePanelUpdateNotificationsTooltip: updatePanelUpdateNotificationsTooltip
    };

    const Logger = {
        Error: (...message) => {
            console.error('%c Millennium ', 'background: red; color: white', ...message);
        },
        Log: (...message) => {
            console.log('%c Millennium ', 'background: purple; color: white', ...message);
        },
        Warn: (...message) => {
            console.warn('%c Millennium ', 'background: orange; color: white', ...message);
        }
    };

    const handler = {
        get: function (target, property) {
            if (property in target) {
                return target[property];
            }
            else {
                try {
                    // fallback to english if the target string wasn't found
                    return english?.[property];
                }
                catch (exception) {
                    return "unknown translation key";
                }
            }
        }
    };
    let locale = new Proxy(english, handler);
    const localizationFiles = {
        english,
        polish,
        spanish,
        indonesian,
        schinese,
        german,
        russian
        // Add other languages here
    };
    const GetLocalization = async () => {
        const language = await SteamClient.Settings.GetCurrentLanguage();
        Logger.Log(`loading locales ${language} ${localizationFiles?.[language]}`);
        if (localizationFiles.hasOwnProperty(language)) {
            locale = new Proxy(localizationFiles[language], handler);
        }
        else {
            Logger.Warn(`Localization for language ${language} not found, defaulting to English.`);
        }
    };
    // setup locales on startup
    GetLocalization();

    const ConnectionFailed = () => {
        const OpenLogsFolder = () => {
            const logsPath = [pluginSelf.steamPath, "ext", "data", "logs"].join("/");
            SteamClient.System.OpenLocalDirectoryInSystemExplorer(logsPath);
        };
        return (window.SP_REACT.createElement("div", { className: "__up-to-date-container", style: {
                display: "flex", flexDirection: "column", alignItems: "center", height: "100%", justifyContent: "center"
            } },
            window.SP_REACT.createElement(IconsModule.Caution, { width: "64" }),
            window.SP_REACT.createElement("div", { className: "__up-to-date-header", style: { marginTop: "20px", color: "white", fontWeight: "500", fontSize: "15px" } }, "Failed to connect to Millennium!"),
            window.SP_REACT.createElement("p", { style: { fontSize: "12px", color: "grey", textAlign: "center", maxWidth: "76%" } }, "This issue isn't network related, you're most likely missing a file millennium needs, or are experiencing an unexpected bug."),
            window.SP_REACT.createElement(DialogButton, { onClick: OpenLogsFolder, style: { width: "50%", marginTop: "20px" } }, "Open Logs Folder")));
    };

    const containerClasses = [
        Classes.Field,
        Classes.WithFirstRow,
        Classes.VerticalAlignCenter,
        Classes.WithDescription,
        Classes.WithBottomSeparatorStandard,
        Classes.ChildrenWidthFixed,
        Classes.ExtraPaddingOnChildrenBelow,
        Classes.StandardPadding,
        Classes.HighlightOnFocus,
        "Panel",
    ].join(" ");
    const fieldClasses = findClassModule((m) => m.FieldLabel &&
        !m.GyroButtonPickerDialog &&
        !m.ControllerOutline &&
        !m.AwaitingEmailConfIcon);
    /**
     * @note
     * Use this instead of the `@millennium/ui` one to prevent the
     * `Assertion failed: Trying to use ConfigContext without a provider!  Add ConfigContextRoot to application.`
     * error.
     */
    const Field = ({ children, description, label, }) => (window.SP_REACT.createElement("div", { className: containerClasses },
        window.SP_REACT.createElement("div", { className: fieldClasses.FieldLabelRow },
            window.SP_REACT.createElement("div", { className: fieldClasses.FieldLabel }, label),
            window.SP_REACT.createElement("div", { className: classMap.FieldChildrenWithIcon }, children)),
        window.SP_REACT.createElement("div", { className: classMap.FieldDescription }, description)));

    const isEditablePlugin = (plugin_name) => {
        return window.PLUGIN_LIST && window.PLUGIN_LIST[plugin_name]
            && typeof window.PLUGIN_LIST[plugin_name].renderPluginSettings === 'function' ? true : false;
    };
    const EditPlugin = ({ plugin }) => {
        if (!isEditablePlugin(plugin?.data?.name)) {
            return window.SP_REACT.createElement(window.SP_REACT.Fragment, null);
        }
        return (window.SP_REACT.createElement(DialogButton, { className: "_3epr8QYWw_FqFgMx38YEEm", style: { marginTop: 0, marginLeft: 0, marginRight: 15 } },
            window.SP_REACT.createElement(IconsModule.Settings, null)));
    };
    const PluginViewModal = () => {
        const [plugins, setPlugins] = React.useState([]);
        const [checkedItems, setCheckedItems] = React.useState({});
        React.useEffect(() => {
            wrappedCallServerMethod("find_all_plugins").then((value) => {
                const json = JSON.parse(value);
                setCheckedItems(json.map((plugin, index) => ({ plugin, index })).filter(({ plugin }) => plugin.enabled)
                    .reduce((acc, { index }) => ({ ...acc, [index]: true }), {}));
                setPlugins(json);
            })
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            })
                .catch((_) => pluginSelf.connectionFailed = true);
        }, []);
        const handleCheckboxChange = (index) => {
            /* Prevent users from disabling this plugin, as its vital */
            const updated = !checkedItems[index] || plugins[index]?.data?.name === "core";
            setCheckedItems({ ...checkedItems, [index]: updated });
            wrappedCallServerMethod("update_plugin_status", { plugin_name: plugins[index]?.data?.name, enabled: updated })
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            });
        };
        if (pluginSelf.connectionFailed) {
            return window.SP_REACT.createElement(ConnectionFailed, null);
        }
        return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
            window.SP_REACT.createElement(DialogHeader, null, locale.settingsPanelPlugins),
            window.SP_REACT.createElement(DialogBody, { className: classMap.SettingsDialogBodyFade }, plugins.map((plugin, index) => (window.SP_REACT.createElement(Field, { key: index, label: plugin?.data?.common_name, description: plugin?.data?.description ?? locale.itemNoDescription },
                window.SP_REACT.createElement(EditPlugin, { plugin: plugin }),
                window.SP_REACT.createElement(Toggle, { disabled: plugin?.data?.name == "core", value: checkedItems[index], onChange: (_checked) => handleCheckboxChange(index) })))))));
    };

    const Folder = () => (window.SP_REACT.createElement("svg", { xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 48 48", width: "16" },
        window.SP_REACT.createElement("path", { fill: "currentColor", d: "M 8.5 8 C 6.019 8 4 10.019 4 12.5 L 4 18 L 16.052734 18 C 16.636734 18 17.202344 17.793922 17.652344 17.419922 L 23.5 12.546875 L 19.572266 9.2734375 C 18.586266 8.4524375 17.336734 8 16.052734 8 L 8.5 8 z M 27.644531 13 L 19.572266 19.724609 C 18.585266 20.546609 17.336734 21 16.052734 21 L 4 21 L 4 35.5 C 4 37.981 6.019 40 8.5 40 L 39.5 40 C 41.981 40 44 37.981 44 35.5 L 44 17.5 C 44 15.019 41.981 13 39.5 13 L 27.644531 13 z" })));
    const Plugins = () => (window.SP_REACT.createElement("svg", { xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 32 32" },
        window.SP_REACT.createElement("path", { d: "M18.3,17.3L15,20.6L11.4,17l3.3-3.3c0.4-0.4,0.4-1,0-1.4s-1-0.4-1.4,0L10,15.6l-1.3-1.3c-0.4-0.4-1-0.4-1.4,0s-0.4,1,0,1.4 L7.6,16l-2.8,2.8C3.6,19.9,3,21.4,3,23c0,1.3,0.4,2.4,1.1,3.5l-2.8,2.8c-0.4,0.4-0.4,1,0,1.4C1.5,30.9,1.7,31,2,31s0.5-0.1,0.7-0.3 l2.8-2.8C6.5,28.6,7.7,29,9,29c1.6,0,3.1-0.6,4.2-1.7l2.8-2.8l0.3,0.3c0.2,0.2,0.5,0.3,0.7,0.3s0.5-0.1,0.7-0.3 c0.4-0.4,0.4-1,0-1.4L16.4,22l3.3-3.3c0.4-0.4,0.4-1,0-1.4S18.7,16.9,18.3,17.3z", fill: "currentColor" }),
        window.SP_REACT.createElement("path", { d: "M30.7,1.3c-0.4-0.4-1-0.4-1.4,0l-2.8,2.8C25.5,3.4,24.3,3,23,3c-1.6,0-3.1,0.6-4.2,1.7l-3.5,3.5c-0.4,0.4-0.4,1,0,1.4l7,7 c0.2,0.2,0.5,0.3,0.7,0.3s0.5-0.1,0.7-0.3l3.5-3.5C28.4,12.1,29,10.6,29,9c0-1.3-0.4-2.4-1.1-3.5l2.8-2.8 C31.1,2.3,31.1,1.7,30.7,1.3z", fill: "currentColor" })));
    const Themes = () => (window.SP_REACT.createElement("svg", { xmlns: "http://www.w3.org/2000/svg", viewBox: "0 0 48 48" },
        window.SP_REACT.createElement("path", { d: "M45.936,18.9a23.027,23.027,0,0,0-1.082-2.1L39.748,30.67a4.783,4.783,0,0,1-.837,1.42,8.943,8.943,0,0,0,7.464-12.115C46.239,19.609,46.093,19.253,45.936,18.9Z", fill: "currentColor" }),
        window.SP_REACT.createElement("path", { d: "M16.63,6.4A23.508,23.508,0,0,0,2.683,37.268c.031.063.052.125.083.188a8.935,8.935,0,0,0,15.662,1.526A16.713,16.713,0,0,1,26.165,32.7c.1-.04.2-.07.3-.107a6.186,6.186,0,0,1,3.859-3.453,4.865,4.865,0,0,1,.451-2.184l7.9-17.107A23.554,23.554,0,0,0,16.63,6.4ZM10.5,32.5a4,4,0,1,1,4-4A4,4,0,0,1,10.5,32.5Zm5-11.5a4,4,0,1,1,4-4A4,4,0,0,1,15.5,21Zm12-3.5a4,4,0,1,1,4-4A4,4,0,0,1,27.5,17.5Z", fill: "currentColor" }),
        window.SP_REACT.createElement("path", { d: "M45.478,4.151a1.858,1.858,0,0,0-2.4.938L32.594,27.794a2.857,2.857,0,0,0,.535,3.18,4.224,4.224,0,0,0-4.865,2.491c-1.619,3.91.942,5.625-.678,9.535a10.526,10.526,0,0,0,8.5-6.3,4.219,4.219,0,0,0-1.25-4.887,2.85,2.85,0,0,0,3.037-1.837l8.64-23.471A1.859,1.859,0,0,0,45.478,4.151Z", fill: "currentColor" })));

    const BBCodeParser = findModuleExport((m) => typeof m === "function" && m.toString().includes("this.ElementAccumulator"));

    const devClasses = findClassModule(m => m.richPresenceLabel && m.blocked);
    const pagedSettingsClasses = findClassModule(m => m.PagedSettingsDialog_PageList);
    const settingsClasses = findClassModule(m => m.SettingsTitleBar && m.SettingsDialogButton);
    const notificationClasses = findClassModule(m => m.GroupMessageTitle && !m.ShortTemplate && !m.TwoLine && !m.FriendIndicator && !m.AchievementIcon);

    const SettingsDialogSubHeader = ({ children }) => window.SP_REACT.createElement("div", { className: "SettingsDialogSubHeader" }, children);

    var ConditionType;
    (function (ConditionType) {
        ConditionType[ConditionType["Dropdown"] = 0] = "Dropdown";
        ConditionType[ConditionType["Toggle"] = 1] = "Toggle";
    })(ConditionType || (ConditionType = {}));
    var ColorTypes;
    (function (ColorTypes) {
        ColorTypes[ColorTypes["RawRGB"] = 1] = "RawRGB";
        ColorTypes[ColorTypes["RGB"] = 2] = "RGB";
        ColorTypes[ColorTypes["RawRGBA"] = 3] = "RawRGBA";
        ColorTypes[ColorTypes["RGBA"] = 4] = "RGBA";
        ColorTypes[ColorTypes["Hex"] = 5] = "Hex";
        ColorTypes[ColorTypes["Unknown"] = 6] = "Unknown";
    })(ColorTypes || (ColorTypes = {}));
    class RenderThemeEditor extends React.Component {
        constructor() {
            super(...arguments);
            this.GetConditionType = (value) => {
                if (Object.keys(value).every((element) => element === 'yes' || element === 'no')) {
                    return ConditionType.Toggle;
                }
                else {
                    return ConditionType.Dropdown;
                }
            };
            this.UpdateLocalCondition = (conditionName, newData) => {
                const activeTheme = pluginSelf.activeTheme;
                return new Promise((resolve) => {
                    wrappedCallServerMethod("cfg.change_condition", {
                        theme: activeTheme.native, newData: newData, condition: conditionName
                    })
                        .then((result) => {
                        pluginSelf.connectionFailed = false;
                        return result;
                    })
                        .then((response) => {
                        const success = JSON.parse(response)?.success ?? false;
                        success && (pluginSelf.ConditionConfigHasChanged = true);
                        resolve(success);
                    });
                });
            };
            this.RenderComponentInterface = ({ conditionType, values, store, conditionName }) => {
                /** Dropdown items if given that the component is a dropdown */
                const items = values.map((value, index) => ({ label: value, data: "componentId" + index }));
                /** Checked status if given that the component is a toggle */
                const [checked, setChecked] = React.useState(store?.[conditionName] == "yes" ? true : false);
                // const [isHovered, setIsHovered] = useState({state: false, target: null});
                const onCheckChange = (enabled) => {
                    this.UpdateLocalCondition(conditionName, enabled ? "yes" : "no").then((success) => {
                        success && setChecked(enabled);
                    });
                };
                const onDropdownChange = (data) => {
                    this.UpdateLocalCondition(conditionName, data.label);
                };
                switch (conditionType) {
                    case ConditionType.Dropdown:
                        // @ts-ignore
                        return window.SP_REACT.createElement(Dropdown, { contextMenuPositionOptions: { bMatchWidth: false }, onChange: onDropdownChange, rgOptions: items, selectedOption: 1, strDefaultLabel: store[conditionName] });
                    case ConditionType.Toggle:
                        return window.SP_REACT.createElement(Toggle, { value: checked, onChange: onCheckChange });
                }
            };
            this.RenderComponent = ({ condition, value, store }) => {
                const conditionType = this.GetConditionType(value.values);
                return (window.SP_REACT.createElement(Field, { label: condition, description: window.SP_REACT.createElement(BBCodeParser, { text: value?.description ?? "No description yet." }) },
                    window.SP_REACT.createElement(this.RenderComponentInterface, { conditionType: conditionType, store: store, conditionName: condition, values: Object.keys(value?.values) })));
            };
            this.RenderColorComponent = ({ color, index }) => {
                const [colorState, setColorState] = React.useState(color?.hex ?? "#000000");
                window.lastColorChangeTime = performance.now();
                const UpdateColor = (hexColor) => {
                    if (performance.now() - window.lastColorChangeTime < 5) {
                        return;
                    }
                    setColorState(hexColor);
                    wrappedCallServerMethod("cfg.change_color", { color_name: color.color, new_color: hexColor, type: color.type })
                        .then((result) => {
                        // @ts-ignore
                        g_PopupManager.m_mapPopups.data_.forEach((element) => {
                            var rootColors = element.value_.m_popup.window.document.getElementById("RootColors");
                            rootColors.innerHTML = rootColors.innerHTML.replace(new RegExp(`${color.color}:.*?;`, 'g'), `${color.color}: ${result};`);
                        });
                    });
                };
                const ResetColor = () => {
                    UpdateColor(color.defaultColor);
                };
                return (window.SP_REACT.createElement(Field, { key: index, label: color?.name ?? color?.color, description: window.SP_REACT.createElement(BBCodeParser, { text: color?.description ?? "No description yet." }) },
                    colorState != color.defaultColor && window.SP_REACT.createElement(DialogButton, { className: settingsClasses.SettingsDialogButton, onClick: ResetColor }, "Reset"),
                    window.SP_REACT.createElement("input", { type: "color", className: "colorPicker", name: "colorPicker", value: colorState, onChange: (event) => UpdateColor(event.target.value) })));
            };
            this.RenderColorsOpts = () => {
                const activeTheme = pluginSelf.activeTheme;
                const [themeColors, setThemeColors] = React.useState();
                React.useEffect(() => {
                    if (activeTheme?.data?.RootColors) {
                        wrappedCallServerMethod("cfg.get_color_opts")
                            .then((result) => {
                            console.log(JSON.parse(result));
                            setThemeColors(JSON.parse(result));
                        });
                    }
                }, []);
                return themeColors && window.SP_REACT.createElement(DialogControlsSection, null,
                    window.SP_REACT.createElement(SettingsDialogSubHeader, null, locale.customThemeSettingsColorsHeader),
                    window.SP_REACT.createElement(DialogBodyText, { className: '_3fPiC9QRyT5oJ6xePCVYz8' }, locale.customThemeSettingsColorsDescription),
                    themeColors?.map((color, index) => window.SP_REACT.createElement(this.RenderColorComponent, { color: color, index: index })));
            };
        }
        render() {
            const activeTheme = pluginSelf.activeTheme;
            const themeConditions = activeTheme.data.Conditions;
            const savedConditions = pluginSelf?.conditionals?.[activeTheme.native];
            return (window.SP_REACT.createElement("div", { className: "ModalPosition", tabIndex: 0 },
                window.SP_REACT.createElement("style", null, `.DialogBody.${Classes.SettingsDialogBodyFade}:last-child { padding-bottom: 65px; }
                        input.colorPicker { margin-left: 10px !important; border: unset !important; min-width: 38px; width: 38px !important; height: 38px; !important; background: transparent; padding: unset !important; }`),
                window.SP_REACT.createElement("div", { className: "ModalPosition_Content", style: { width: "100vw", height: "100vh" } },
                    window.SP_REACT.createElement("div", { className: `${Classes.PagedSettingsDialog} ${Classes.SettingsModal} ${Classes.DesktopPopup} Panel` },
                        window.SP_REACT.createElement("div", { className: "DialogContentTransition Panel", style: { minWidth: "100vw" } },
                            window.SP_REACT.createElement("div", { className: `DialogContent _DialogLayout ${Classes.PagedSettingsDialog_PageContent} ` },
                                window.SP_REACT.createElement("div", { className: "DialogContent_InnerWidth" },
                                    window.SP_REACT.createElement(DialogHeader, null,
                                        "Editing ",
                                        activeTheme?.data?.name ?? activeTheme.native),
                                    window.SP_REACT.createElement(DialogBody, { className: Classes.SettingsDialogBodyFade },
                                        themeConditions && window.SP_REACT.createElement(DialogControlsSection, null,
                                            window.SP_REACT.createElement(SettingsDialogSubHeader, null, locale.customThemeSettingsConfigHeader),
                                            window.SP_REACT.createElement(DialogBodyText, { className: '_3fPiC9QRyT5oJ6xePCVYz8' }, locale.customThemeSettingsConfigDescription),
                                            Object.entries(themeConditions).map(([key, value]) => window.SP_REACT.createElement(this.RenderComponent, { condition: key, store: savedConditions, value: value }))),
                                        window.SP_REACT.createElement(this.RenderColorsOpts, null)))))))));
        }
    }

    const TitleBar = findModuleExport((m) => typeof m === "function" &&
        m.toString().includes('className:"title-area-highlight"'));

    const CreatePopupBase = findModuleExport((m) => typeof m === "function" &&
        m?.toString().includes("CreatePopup(this.m_strName"));

    class CreatePopup extends CreatePopupBase {
        constructor(component, strPopupName, options) {
            super(strPopupName, options);
            this.component = component;
        }
        Show() {
            super.Show();
            const RenderComponent = ({ _window }) => {
                return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                    window.SP_REACT.createElement("div", { className: "PopupFullWindow", onContextMenu: ((_e) => {
                            // console.log('CONTEXT MENU OPEN')
                            // _e.preventDefault()
                            // this.contextMenuHandler.CreateContextMenuInstance(_e)
                        }) },
                        window.SP_REACT.createElement(TitleBar, { popup: _window, hideMin: false, hideMax: false, hideActions: false }),
                        window.SP_REACT.createElement(this.component, null))));
            };
            console.log(super.root_element);
            ReactDOM.render(window.SP_REACT.createElement(RenderComponent, { _window: super.window }), super.root_element);
        }
        SetTitle() {
            console.log("[internal] setting title ->", this);
            if (this.m_popup && this.m_popup.document) {
                this.m_popup.document.title = "WINDOW";
            }
        }
        Render(_window, _element) { }
        OnClose() { }
        OnLoad() {
            const element = this.m_popup.document.querySelector(".DialogContent_InnerWidth");
            const height = element?.getBoundingClientRect()?.height;
            this.m_popup.SteamClient?.Window?.ResizeTo(450, height + 48, true);
            this.m_popup.SteamClient.Window.ShowWindow();
        }
    }

    /**
     * @todo Get & use the webpack module for this
     */
    const FakeFriend = ({ eStatus, strAvatarURL, strGameName, strPlayerName, onClick, }) => (window.SP_REACT.createElement("div", { className: `${Classes.FakeFriend} ${eStatus}`, onClick: onClick },
        window.SP_REACT.createElement("div", { className: `${Classes.avatarHolder} avatarHolder no-drag Medium ${eStatus}` },
            window.SP_REACT.createElement("div", { className: `${Classes.avatarStatus} avatarStatus right` }),
            window.SP_REACT.createElement("img", { src: strAvatarURL, className: `${Classes.avatar} avatar`, draggable: "false" })),
        window.SP_REACT.createElement("div", { className: `${eStatus} ${Classes.noContextMenu} ${Classes.twoLine}` },
            window.SP_REACT.createElement("div", { className: Classes.statusAndName },
                window.SP_REACT.createElement("div", { className: Classes.playerName }, strPlayerName)),
            window.SP_REACT.createElement("div", { className: Classes.richPresenceContainer, style: { width: "100%" } },
                window.SP_REACT.createElement("div", { className: `${Classes.gameName} ${Classes.richPresenceLabel} no-drag` }, strGameName)))));

    class AboutThemeRenderer extends React.Component {
        constructor(props) {
            super(props);
            this.RenderDeveloperProfile = () => {
                const OpenDeveloperProfile = () => {
                    this.activeTheme?.data?.github?.owner
                        && SteamClient.System.OpenInSystemBrowser(`https://github.com/${this.activeTheme?.data?.github?.owner}/`);
                };
                return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                    window.SP_REACT.createElement("style", null, `
                .${Classes.FakeFriend}.online:hover {
                    cursor: pointer !important;
                }
                
                .${Classes.avatarHolder}.avatarHolder.no-drag.Medium.online,
                .online.${devClasses.noContextMenu}.${devClasses.twoLine} {
                    pointer-events: none;
                }`),
                    window.SP_REACT.createElement(FakeFriend, { eStatus: "online", strAvatarURL: this.activeTheme?.data?.github?.owner
                            ? `https://github.com/${this.activeTheme?.data?.github?.owner}.png`
                            : "https://i.pinimg.com/736x/98/1d/6b/981d6b2e0ccb5e968a0618c8d47671da.jpg", strGameName: `✅ ${locale.aboutThemeVerifiedDev}`, strPlayerName: this.activeTheme?.data?.github?.owner ??
                            this.activeTheme?.data?.author ??
                            locale.aboutThemeAnonymous, onClick: OpenDeveloperProfile })));
            };
            this.RenderDescription = () => {
                return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                    window.SP_REACT.createElement(DialogSubHeader, { className: settingsClasses.SettingsDialogSubHeader }, locale.aboutThemeTitle),
                    window.SP_REACT.createElement(DialogBodyText, { className: Classes.FriendsDescription }, this.activeTheme?.data?.description ?? locale.itemNoDescription)));
            };
            this.RenderInfoRow = () => {
                const themeOwner = this.activeTheme?.data?.github?.owner;
                const themeRepo = this.activeTheme?.data?.github?.repo_name;
                this.activeTheme?.data?.funding?.kofi;
                const ShowSource = () => {
                    SteamClient.System.OpenInSystemBrowser(`https://github.com/${themeOwner}/${themeRepo}`);
                };
                const ShowInFolder = () => {
                    wrappedCallServerMethod("Millennium.steam_path")
                        .then((result) => {
                        pluginSelf.connectionFailed = false;
                        return result;
                    })
                        .then((path) => {
                        console.log(path);
                        SteamClient.System.OpenLocalDirectoryInSystemExplorer(`${path}/steamui/skins/${this.activeTheme.native}`);
                    });
                };
                const UninstallTheme = () => {
                    wrappedCallServerMethod("uninstall_theme", {
                        owner: this.activeTheme?.data?.github?.owner,
                        repo: this.activeTheme?.data?.github?.repo_name
                    })
                        .then((result) => {
                        pluginSelf.connectionFailed = false;
                        return result;
                    })
                        .then((raw) => {
                        const message = JSON.parse(raw);
                        console.log(message);
                        SteamClient.Browser.RestartJSContext();
                    });
                };
                return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                    themeOwner && themeRepo && window.SP_REACT.createElement(DialogButton, { style: { width: "unset" }, className: settingsClasses.SettingsDialogButton, onClick: ShowSource }, locale.viewSourceCode),
                    window.SP_REACT.createElement("div", { className: ".flex-btn-container", style: { display: "flex", gap: "5px" } },
                        window.SP_REACT.createElement(DialogButton, { style: { width: "50%", }, className: settingsClasses.SettingsDialogButton, onClick: ShowInFolder }, locale.showInFolder),
                        window.SP_REACT.createElement(DialogButton, { style: { width: "50%" }, className: settingsClasses.SettingsDialogButton, onClick: UninstallTheme }, locale.uninstall))));
            };
            this.CreateModalBody = () => {
                return (window.SP_REACT.createElement("div", { className: "ModalPosition", tabIndex: 0 },
                    window.SP_REACT.createElement("div", { className: "ModalPosition_Content", style: { width: "100vw", height: "100vh" } },
                        window.SP_REACT.createElement("div", { className: "DialogContent _DialogLayout GenericConfirmDialog _DialogCenterVertically" },
                            window.SP_REACT.createElement("div", { className: "DialogContent_InnerWidth", style: { flex: "unset" } },
                                window.SP_REACT.createElement(DialogHeader, null, this.activeTheme?.data?.name ?? this.activeTheme?.native),
                                window.SP_REACT.createElement(DialogBody, { style: { flex: "unset" } },
                                    window.SP_REACT.createElement(this.RenderDeveloperProfile, null),
                                    window.SP_REACT.createElement(this.RenderDescription, null),
                                    window.SP_REACT.createElement(this.RenderInfoRow, null)))))));
            };
            this.activeTheme = pluginSelf.activeTheme;
        }
        render() {
            return (window.SP_REACT.createElement(this.CreateModalBody, null));
        }
    }
    const SetupAboutRenderer = (active) => {
        const params = {
            title: locale.aboutThemeTitle + " " + active,
            popup_class: "fullheight",
            body_class: "fullheight ModalDialogBody DesktopUI ",
            html_class: "client_chat_frame fullheight ModalDialogPopup ",
            eCreationFlags: 274,
            window_opener_id: 1,
            dimensions: { width: 450, height: 375 },
            replace_existing_popup: false,
        };
        const popupWND = new CreatePopup(AboutThemeRenderer, "about_theme", params);
        popupWND.Show();
    };

    const Localize = (token) => 
    // @ts-ignore
    LocalizationManager.LocalizeString(token);
    /**
     * @note
     * There is a specific webpack module for that,
     * but it restarts Steam instead of reloading the UI.
     */
    const PromptReload = (onOK) => showModal(window.SP_REACT.createElement(ConfirmModal, { strTitle: Localize("#Settings_RestartRequired_Title"), strDescription: Localize("#Settings_RestartRequired_Description"), strOKButtonText: Localize("#Settings_RestartNow_ButtonText"), onOK: onOK }), pluginSelf.settingsWnd, {
        bNeverPopOut: true,
    });
    const ThemeSettings = (activeTheme) => showModal(window.SP_REACT.createElement(RenderThemeEditor, null), pluginSelf.settingsWnd, {
        strTitle: "Editing " + activeTheme,
        popupHeight: 675,
        popupWidth: 850,
        fnOnClose: () => {
            if (!pluginSelf.ConditionConfigHasChanged) {
                return;
            }
            PromptReload(() => {
                SteamClient.Browser.RestartJSContext();
            });
            pluginSelf.ConditionConfigHasChanged = false;
        },
    });
    /**
     * Display the edit theme button on a theme if applicable
     * @param active the common name of a theme
     * @returns react component
     */
    const RenderEditTheme = ({ active }) => {
        const Theme = pluginSelf.activeTheme;
        /** Current theme is not editable */
        if (pluginSelf?.isDefaultTheme || (Theme?.data?.Conditions === undefined && Theme?.data?.RootColors === undefined)) {
            return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null));
        }
        return (window.SP_REACT.createElement(DialogButton, { onClick: () => ThemeSettings(active), style: { margin: "0", padding: "0px 10px", marginRight: "10px" }, className: "_3epr8QYWw_FqFgMx38YEEm millenniumIconButton" },
            window.SP_REACT.createElement(IconsModule.Settings, { height: "16" })));
    };
    const findAllThemes = async () => {
        const activeTheme = await wrappedCallServerMethod("cfg.get_active_theme")
            .then((result) => {
            pluginSelf.connectionFailed = false;
            return result;
        });
        return new Promise(async (resolve) => {
            let buffer = JSON.parse(await wrappedCallServerMethod("find_all_themes")
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            }))
                /** Prevent the selected theme from appearing in combo box */
                .filter((theme) => !pluginSelf.isDefaultTheme ? theme.native !== activeTheme.native : true)
                .map((theme, index) => ({
                label: theme?.data?.name ?? theme.native, theme: theme, data: "theme" + index
            }));
            /** Add the default theme to list */
            !pluginSelf.isDefaultTheme && buffer.unshift({ label: "< Default >", data: "default", theme: null });
            resolve(buffer);
        });
    };
    const ThemeViewModal = () => {
        const [themes, setThemes] = React.useState();
        const [active, setActive] = React.useState();
        const [jsState, setJsState] = React.useState(undefined);
        const [cssState, setCssState] = React.useState(undefined);
        React.useEffect(() => {
            const activeTheme = pluginSelf.activeTheme;
            pluginSelf.isDefaultTheme ? setActive("Default") : setActive(activeTheme?.data?.name ?? activeTheme?.native);
            findAllThemes().then((result) => setThemes(result));
            setJsState(pluginSelf.scriptsAllowed);
            setCssState(pluginSelf.stylesAllowed);
        }, []);
        const onScriptToggle = (enabled) => {
            setJsState(enabled);
            PromptReload(() => {
                wrappedCallServerMethod("cfg.set_config_keypair", { key: "scripts", value: enabled })
                    .then((result) => {
                    pluginSelf.connectionFailed = false;
                    return result;
                })
                    .catch((_) => {
                    console.error("Failed to update settings");
                    pluginSelf.connectionFailed = true;
                });
                SteamClient.Browser.RestartJSContext();
            });
        };
        const onStyleToggle = (enabled) => {
            setCssState(enabled);
            PromptReload(() => {
                wrappedCallServerMethod("cfg.set_config_keypair", { key: "styles", value: enabled })
                    .then((result) => {
                    pluginSelf.connectionFailed = false;
                    return result;
                })
                    .catch((_) => {
                    console.error("Failed to update settings");
                    pluginSelf.connectionFailed = true;
                });
                SteamClient.Browser.RestartJSContext();
            });
        };
        const updateThemeCallback = (item) => {
            const themeName = item.data === "default" ? "__default__" : item.theme.native;
            wrappedCallServerMethod("cfg.change_theme", { theme_name: themeName }).then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            });
            findAllThemes().then((result) => setThemes(result));
            PromptReload(() => {
                SteamClient.Browser.RestartJSContext();
            });
        };
        if (pluginSelf.connectionFailed) {
            return window.SP_REACT.createElement(ConnectionFailed, null);
        }
        const OpenThemesFolder = () => {
            const themesPath = [pluginSelf.steamPath, "steamui", "skins"].join("/");
            SteamClient.System.OpenLocalDirectoryInSystemExplorer(themesPath);
        };
        const GetMoreThemes = () => {
            SteamClient.System.OpenInSystemBrowser("https://steambrew.app/themes");
        };
        return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
            window.SP_REACT.createElement("style", null, `.DialogDropDown._DialogInputContainer.Panel.Focusable { min-width: max-content !important; }`),
            window.SP_REACT.createElement(DialogHeader, null, locale.settingsPanelThemes),
            window.SP_REACT.createElement(DialogBody, { className: classMap.SettingsDialogBodyFade },
                window.SP_REACT.createElement(Field, { label: locale.themePanelClientTheme, description: window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                        locale.themePanelThemeTooltip,
                        ". ",
                        window.SP_REACT.createElement("a", { href: "#", onClick: GetMoreThemes }, locale.themePanelGetMoreThemes)) },
                    window.SP_REACT.createElement(RenderEditTheme, { active: active }),
                    !pluginSelf.isDefaultTheme && (window.SP_REACT.createElement(DialogButton, { onClick: () => SetupAboutRenderer(active), style: { margin: "0", padding: "0px 10px", marginRight: "10px" }, className: "_3epr8QYWw_FqFgMx38YEEm millenniumIconButton" },
                        window.SP_REACT.createElement(IconsModule.Information, { height: "16" }))),
                    window.SP_REACT.createElement(DialogButton, { onClick: OpenThemesFolder, style: { margin: "0", padding: "0px 10px", marginRight: "10px" }, className: "_3epr8QYWw_FqFgMx38YEEm millenniumIconButton" },
                        window.SP_REACT.createElement(Folder, null)),
                    window.SP_REACT.createElement(Dropdown, { contextMenuPositionOptions: { bMatchWidth: false }, rgOptions: themes, selectedOption: 1, strDefaultLabel: active, onChange: updateThemeCallback })),
                window.SP_REACT.createElement(Field, { label: locale.themePanelInjectJavascript, description: locale.themePanelInjectCSSToolTip }, jsState !== undefined && (window.SP_REACT.createElement(Toggle, { value: jsState, onChange: onScriptToggle }))),
                window.SP_REACT.createElement(Field, { label: locale.themePanelInjectCSS, description: locale.themePanelInjectCSSToolTip }, cssState !== undefined && (window.SP_REACT.createElement(Toggle, { value: cssState, onChange: onStyleToggle }))))));
    };

    pluginSelf.SettingsStore;
    const Settings = {
        FetchAllSettings: () => {
            return new Promise(async (resolve, _reject) => {
                const settingsStore = JSON.parse(await wrappedCallServerMethod("get_load_config")
                    .then((result) => {
                    pluginSelf.connectionFailed = false;
                    return result;
                })
                    .catch((_) => {
                    console.error("Failed to fetch settings");
                    pluginSelf.connectionFailed = true;
                }));
                resolve(settingsStore);
            });
        }
    };

    const UpToDateModal = () => {
        return (window.SP_REACT.createElement("div", { className: "__up-to-date-container", style: {
                display: "flex", flexDirection: "column", alignItems: "center", gap: "20px", height: "100%", justifyContent: "center"
            } },
            window.SP_REACT.createElement(IconsModule.Checkmark, { width: "64" }),
            window.SP_REACT.createElement("div", { className: "__up-to-date-header", style: { marginTop: "-120px", color: "white", fontWeight: "500", fontSize: "15px" } }, locale.updatePanelNoUpdatesFound)));
    };
    const RenderAvailableUpdates = ({ updates, setUpdates }) => {
        const [updating, setUpdating] = React.useState([]);
        const viewMoreClick = (props) => SteamClient.System.OpenInSystemBrowser(props?.commit);
        const updateItemMessage = (updateObject, index) => {
            setUpdating({ ...updating, [index]: true });
            wrappedCallServerMethod("updater.update_theme", { native: updateObject.native })
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            })
                .then((success) => {
                /** @todo: prompt user an error occured. */
                if (!success)
                    return;
                const activeTheme = pluginSelf.activeTheme;
                // the current theme was just updated, so reload SteamUI
                if (activeTheme?.native === updateObject?.native) {
                    SteamClient.Browser.RestartJSContext();
                }
                wrappedCallServerMethod("updater.get_update_list")
                    .then((result) => {
                    pluginSelf.connectionFailed = false;
                    return result;
                })
                    .then((result) => {
                    setUpdates(JSON.parse(result).updates);
                });
            });
        };
        const fieldButtonsStyles = {
            display: "flex",
            gap: "8px",
        };
        const updateButtonStyles = {
            minWidth: "80px",
        };
        const updateDescriptionStyles = {
            display: "flex",
            flexDirection: "column",
        };
        const updateLabelStyles = {
            display: "flex",
            alignItems: "center",
            gap: "8px",
        };
        return (window.SP_REACT.createElement(DialogControlsSection, null,
            window.SP_REACT.createElement(SettingsDialogSubHeader, null, locale.updatePanelHasUpdates),
            window.SP_REACT.createElement(DialogBodyText, { className: '_3fPiC9QRyT5oJ6xePCVYz8' }, locale.updatePanelHasUpdatesSub),
            updates.map((update, index) => (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
                window.SP_REACT.createElement(Field, { key: index, label: window.SP_REACT.createElement("div", { style: updateLabelStyles },
                        window.SP_REACT.createElement("div", { className: "update-item-type", style: { color: "white", fontSize: "12px", padding: "4px", background: "#007eff", borderRadius: "6px" } }, "Theme"),
                        update.name), description: window.SP_REACT.createElement("div", { style: updateDescriptionStyles },
                        window.SP_REACT.createElement("div", null,
                            window.SP_REACT.createElement("b", null, locale.updatePanelReleasedTag),
                            " ",
                            update?.date),
                        window.SP_REACT.createElement("div", null,
                            window.SP_REACT.createElement("b", null, locale.updatePanelReleasePatchNotes),
                            " ",
                            update?.message)) },
                    window.SP_REACT.createElement("div", { style: fieldButtonsStyles },
                        window.SP_REACT.createElement(DialogButton, { onClick: () => viewMoreClick(update), style: updateButtonStyles, className: "_3epr8QYWw_FqFgMx38YEEm" }, locale.ViewMore),
                        window.SP_REACT.createElement(DialogButton, { onClick: () => updateItemMessage(update, index), style: updateButtonStyles, className: "_3epr8QYWw_FqFgMx38YEEm" }, updating[index] ? locale.updatePanelIsUpdating : locale.updatePanelUpdate))))))));
    };
    const UpdatesViewModal = () => {
        const [updates, setUpdates] = React.useState(null);
        const [checkingForUpdates, setCheckingForUpdates] = React.useState(false);
        const [showUpdateNotifications, setNotifications] = React.useState(undefined);
        React.useEffect(() => {
            wrappedCallServerMethod("updater.get_update_list")
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            })
                .then((result) => {
                const updates = JSON.parse(result);
                console.log(updates);
                setUpdates(updates.updates);
                setNotifications(updates.notifications ?? false);
            })
                .catch((_) => {
                console.error("Failed to fetch updates");
                pluginSelf.connectionFailed = true;
            });
        }, []);
        const checkForUpdates = async () => {
            if (checkingForUpdates)
                return;
            setCheckingForUpdates(true);
            await wrappedCallServerMethod("updater.re_initialize");
            wrappedCallServerMethod("updater.get_update_list")
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            })
                .then((result) => {
                setUpdates(JSON.parse(result).updates);
                setCheckingForUpdates(false);
            })
                .catch((_) => {
                console.error("Failed to fetch updates");
                pluginSelf.connectionFailed = true;
            });
        };
        const DialogHeaderStyles = {
            display: "flex", alignItems: "center", gap: "15px"
        };
        const OnNotificationsChange = (enabled) => {
            wrappedCallServerMethod("updater.set_update_notifs_status", { status: enabled })
                .then((result) => {
                pluginSelf.connectionFailed = false;
                return result;
            })
                .then((success) => {
                if (success) {
                    setNotifications(enabled);
                    Settings.FetchAllSettings();
                }
            });
        };
        if (pluginSelf.connectionFailed) {
            return window.SP_REACT.createElement(ConnectionFailed, null);
        }
        return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
            window.SP_REACT.createElement(DialogHeader, { style: DialogHeaderStyles },
                locale.settingsPanelUpdates,
                !checkingForUpdates &&
                    window.SP_REACT.createElement(DialogButton, { onClick: checkForUpdates, className: "_3epr8QYWw_FqFgMx38YEEm", style: { width: "16px", "-webkit-app-region": "no-drag", zIndex: "9999", padding: "4px 4px", display: "flex" } },
                        window.SP_REACT.createElement(IconsModule.Update, null))),
            window.SP_REACT.createElement(DialogBody, { className: classMap.SettingsDialogBodyFade },
                window.SP_REACT.createElement(Field, { label: locale.updatePanelUpdateNotifications, description: locale.updatePanelUpdateNotificationsTooltip }, showUpdateNotifications !== undefined && window.SP_REACT.createElement(Toggle, { value: showUpdateNotifications, onChange: OnNotificationsChange })),
                updates && (!updates.length ? window.SP_REACT.createElement(UpToDateModal, null) : window.SP_REACT.createElement(RenderAvailableUpdates, { updates: updates, setUpdates: setUpdates })))));
    };

    const active = pagedSettingsClasses.Active;
    const Item = ({ bSelected, icon, title, onClick, }) => (window.SP_REACT.createElement("div", { className: `MillenniumTab ${Classes.PagedSettingsDialog_PageListItem} ${bSelected && active}`, onClick: onClick },
        window.SP_REACT.createElement("div", { className: Classes.PageListItem_Icon }, icon),
        window.SP_REACT.createElement("div", { className: Classes.PageListItem_Title }, title)));
    const Separator = () => (window.SP_REACT.createElement("div", { className: Classes.PageListSeparator }));

    const activeClassName = pagedSettingsClasses.Active;
    var Renderer;
    (function (Renderer) {
        Renderer[Renderer["Plugins"] = 0] = "Plugins";
        Renderer[Renderer["Themes"] = 1] = "Themes";
        Renderer[Renderer["Updates"] = 2] = "Updates";
        Renderer[Renderer["Unset"] = 3] = "Unset";
    })(Renderer || (Renderer = {}));
    const RenderViewComponent = (componentType) => {
        Millennium.findElement(pluginSelf.settingsDoc, ".DialogContent_InnerWidth").then((element) => {
            switch (componentType) {
                case Renderer.Plugins:
                    ReactDOM.render(window.SP_REACT.createElement(PluginViewModal, null), element[0]);
                    break;
                case Renderer.Themes:
                    ReactDOM.render(window.SP_REACT.createElement(ThemeViewModal, null), element[0]);
                    break;
                case Renderer.Updates:
                    ReactDOM.render(window.SP_REACT.createElement(UpdatesViewModal, null), element[0]);
                    break;
            }
        });
    };
    const PluginComponent = () => {
        const [selected, setSelected] = React.useState();
        const nativeTabs = pluginSelf.settingsDoc.querySelectorAll(`.${Classes.PagedSettingsDialog_PageListItem}:not(.MillenniumTab)`);
        for (const tab of nativeTabs) {
            tab.addEventListener("click", () => {
                setSelected(Renderer.Unset);
            });
        }
        const componentUpdate = (type) => {
            RenderViewComponent(type);
            setSelected(type);
            nativeTabs.forEach((element) => {
                element.classList.remove(activeClassName);
            });
        };
        React.useEffect(() => {
            Millennium.findElement(pluginSelf.settingsDoc, ".DialogBody").then(_ => {
                if (pluginSelf?.OpenOnUpdatesPanel ?? false) {
                    componentUpdate(Renderer.Updates);
                    pluginSelf.OpenOnUpdatesPanel = false;
                }
            });
        }, []);
        return (window.SP_REACT.createElement(window.SP_REACT.Fragment, null,
            window.SP_REACT.createElement(Item, { bSelected: selected === Renderer.Plugins, icon: window.SP_REACT.createElement(Plugins, null), title: locale.settingsPanelPlugins, onClick: () => {
                    componentUpdate(Renderer.Plugins);
                } }),
            window.SP_REACT.createElement(Item, { bSelected: selected === Renderer.Themes, icon: window.SP_REACT.createElement(Themes, null), title: locale.settingsPanelThemes, onClick: () => {
                    componentUpdate(Renderer.Themes);
                } }),
            window.SP_REACT.createElement(Item, { bSelected: selected === Renderer.Updates, icon: window.SP_REACT.createElement(IconsModule.Update, null), title: locale.settingsPanelUpdates, onClick: () => {
                    componentUpdate(Renderer.Updates);
                } }),
            window.SP_REACT.createElement(Separator, null)));
    };
    /**
     * Hooks settings tabs components, and forces active overlayed panels to re-render
     * @todo A better, more integrated way of doing this, that doesn't involve runtime patching.
     */
    const hookSettingsComponent = () => {
        let processingItem = false;
        const elements = pluginSelf.settingsDoc.querySelectorAll(`.${Classes.PagedSettingsDialog_PageListItem}:not(.MillenniumTab)`);
        elements.forEach((element, index) => {
            element.addEventListener('click', function (_) {
                if (processingItem)
                    return;
                console.log(pluginSelf.settingsDoc.querySelectorAll('.' + Classes.PageListSeparator));
                pluginSelf.settingsDoc.querySelectorAll('.' + Classes.PageListSeparator).forEach((element) => element.classList.remove(Classes.Transparent));
                const click = new MouseEvent("click", { view: pluginSelf.settingsWnd, bubbles: true, cancelable: true });
                try {
                    processingItem = true;
                    if (index + 1 <= elements.length)
                        elements[index + 1].dispatchEvent(click);
                    else
                        elements[index - 2].dispatchEvent(click);
                    elements[index].dispatchEvent(click);
                    processingItem = false;
                }
                catch (error) {
                    console.log(error);
                }
            });
        });
    };
    function RenderSettingsModal(_context) {
        pluginSelf.settingsDoc = _context.m_popup.document;
        pluginSelf.settingsWnd = _context.m_popup.window;
        Millennium.findElement(_context.m_popup.document, "." + Classes.PagedSettingsDialog_PageList).then(element => {
            hookSettingsComponent();
            // Create a new div element
            var bufferDiv = document.createElement("div");
            bufferDiv.classList.add("millennium-tabs-list");
            element[0].prepend(bufferDiv);
            ReactDOM.render(window.SP_REACT.createElement(PluginComponent, null), bufferDiv);
        });
    }

    /**
     * appends a virtual CSS script into self module
     * @param systemColors SystemAccentColors
     */
    const DispatchSystemColors = (systemColors) => {
        pluginSelf.systemColor = `
    :root {
        --SystemAccentColor: ${systemColors.accent}; 
        --SystemAccentColor-RGB: ${systemColors.accentRgb}; 
        --SystemAccentColorLight1: ${systemColors.light1}; 
        --SystemAccentColorLight1-RGB: ${systemColors.light1Rgb}; 
        --SystemAccentColorLight2: ${systemColors.light2}; 
        --SystemAccentColorLight2-RGB: ${systemColors.light2Rgb}; 
        --SystemAccentColorLight3: ${systemColors.light3};
        --SystemAccentColorLight3-RGB: ${systemColors.light3Rgb};
        --SystemAccentColorDark1: ${systemColors.dark1};
        --SystemAccentColorDark1-RGB: ${systemColors.dark1Rgb};
        --SystemAccentColorDark2: ${systemColors.dark2};
        --SystemAccentColorDark2-RGB: ${systemColors.dark2Rgb};
         --SystemAccentColorDark3: ${systemColors.dark3};
         --SystemAccentColorDark3-RGB: ${systemColors.dark3Rgb};
    }`;
    };

    /**
     * Interpolates and overrides default patches on a theme.
     * @param incomingPatches Preprocessed list of patches from a specific theme
     * @returns Processed patches, interpolated with default patches
     */
    function parseTheme(incomingPatches) {
        let patches = {
            Patches: [
                { MatchRegexString: "^Steam$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^OverlayBrowser_Browser$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^SP Overlay:", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "Menu$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "Supernav$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^notificationtoasts_", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^SteamBrowser_Find$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^OverlayTab\\d+_Find$", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: "^Steam Big Picture Mode$", TargetCss: "bigpicture.custom.css", TargetJs: "bigpicture.custom.js" },
                { MatchRegexString: "^QuickAccess_", TargetCss: "bigpicture.custom.css", TargetJs: "bigpicture.custom.js" },
                { MatchRegexString: "^MainMenu_", TargetCss: "bigpicture.custom.css", TargetJs: "bigpicture.custom.js" },
                { MatchRegexString: ".friendsui-container", TargetCss: "friends.custom.css", TargetJs: "friends.custom.js" },
                { MatchRegexString: ".ModalDialogPopup", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" },
                { MatchRegexString: ".FullModalOverlay", TargetCss: "libraryroot.custom.css", TargetJs: "libraryroot.custom.js" }
            ]
        };
        let newMatchRegexStrings = new Set(incomingPatches.map((patch) => patch.MatchRegexString));
        let filteredPatches = patches.Patches.filter((patch) => !newMatchRegexStrings.has(patch.MatchRegexString));
        return filteredPatches.concat(incomingPatches);
    }
    /**
     * parses a theme after it has been received from the backend.
     * - checks for failure in theme parse
     * - calculates what patches should be used relative to UseDefaultPatches
     * @param theme ThemeItem
     * @returns void
     */
    const ParseLocalTheme = (theme) => {
        if (theme?.failed) {
            pluginSelf.isDefaultTheme = true;
            return;
        }
        theme?.data?.UseDefaultPatches && (theme.data.Patches = parseTheme(theme?.data?.Patches ?? []));
        pluginSelf.activeTheme = theme;
    };

    /**
     * @todo use builtin notification components instead of altering
     * SteamClient.ClientNotifications.DisplayClientNotification
     * @param doc document of notification
     */
    const RemoveAllListeners = (doc) => {
        var bodyClass = [...doc.getElementsByClassName(notificationClasses.DesktopToastTemplate)];
        Array.from(bodyClass).forEach(function (element) {
            var newElement = element.cloneNode(true);
            element.parentNode.replaceChild(newElement, element);
        });
    };
    const SetClickListener = (doc) => {
        var bodyClass = [...doc.getElementsByClassName(notificationClasses.DesktopToastTemplate)][0];
        bodyClass.addEventListener("click", () => {
            console.log("clicked notif!");
            pluginSelf.OpenOnUpdatesPanel = true;
            /** Open the settings window */
            window.open("steam://open/settings", "_blank");
        });
    };
    const PatchNotification = (doc) => {
        try {
            Millennium.findElement(doc, "." + notificationClasses.GroupMessageTitle).then(async (elements) => {
                const header = elements[0].innerText;
                if (header == "Updates Available") {
                    (await Millennium.findElement(doc, "." + notificationClasses.StandardLogoDimensions))?.[0]?.remove();
                    (await Millennium.findElement(doc, "." + notificationClasses.AvatarStatus))?.[0]?.remove();
                    (await Millennium.findElement(doc, "." + notificationClasses.Icon))?.[0]?.remove();
                }
                RemoveAllListeners(doc);
                SetClickListener(doc);
            });
        }
        catch (error) {
            console.error(error);
        }
    };

    /**
     * appends a virtual CSS script into self module
     * @param globalColors V1 Global Colors struct
     */
    const DispatchGlobalColors = (globalColors) => {
        pluginSelf.GlobalsColors = `
    :root {
        ${globalColors.map((color) => `${color.ColorName}: ${color.HexColorCode};`).join(" ")}
    }`;
    };

    const WatchDog = {
        startReload: () => {
            SteamClient.Browser.RestartJSContext();
        },
        startRestart: () => {
            SteamClient.User.StartRestart(false);
        },
        startRestartForce: () => {
            SteamClient.User.StartRestart(true);
        }
    };

    const PatchMissedDocuments = () => {
        // @ts-ignore
        g_PopupManager?.m_mapPopups?.data_?.forEach((element) => {
            if (element?.value_?.m_popup?.window?.HAS_INJECTED_THEME === undefined) {
                patchDocumentContext(element?.value_);
            }
        });
        // @ts-ignore
        if (g_PopupManager?.m_mapPopups?.data_?.length === 0) {
            Logger.Warn("windowCreated callback called, but no popups found...");
        }
    };
    const windowCreated = (windowContext) => {
        switch (windowContext.m_strTitle) {
            /** @ts-ignore */
            case LocalizationManager.LocalizeString("#Steam_Platform"):        /** @ts-ignore */
            case LocalizationManager.LocalizeString("#Settings_Title"): {
                pluginSelf.useInterface && RenderSettingsModal(windowContext);
            }
        }
        if (windowContext.m_strTitle.includes("notificationtoasts")) {
            PatchNotification(windowContext.m_popup.document);
        }
        PatchMissedDocuments();
        patchDocumentContext(windowContext);
    };
    const InitializePatcher = (startTime, result) => {
        Logger.Log(`Received props in [${(performance.now() - startTime).toFixed(3)}ms]`, result);
        const theme = result.active_theme;
        const systemColors = result.accent_color;
        ParseLocalTheme(theme);
        DispatchSystemColors(systemColors);
        const themeV1 = result?.active_theme?.data;
        if (themeV1?.GlobalsColors) {
            DispatchGlobalColors(themeV1?.GlobalsColors);
        }
        pluginSelf.conditionals = result?.conditions;
        pluginSelf.scriptsAllowed = result?.settings?.scripts ?? true;
        pluginSelf.stylesAllowed = result?.settings?.styles ?? true;
        pluginSelf.steamPath = result?.steamPath;
        pluginSelf.useInterface = result?.useInterface ?? true;
        // @ts-ignore
        if (g_PopupManager?.m_mapPopups?.size > 0) {
            // check if RestartJSContext exists
            if (SteamClient?.Browser?.RestartJSContext) {
                SteamClient.Browser.RestartJSContext();
            }
            else {
                Logger.Warn("SteamClient.Browser.RestartJSContext is not available");
            }
        }
        PatchMissedDocuments();
    };
    // Entry point on the front end of your plugin
    async function PluginMain() {
        const startTime = performance.now();
        pluginSelf.WatchDog = WatchDog; // Expose WatchDog to the global scope
        Settings.FetchAllSettings().then((result) => InitializePatcher(startTime, result));
        // @todo: fix notificaitons modal
        // wrappedCallServerMethod("updater.get_update_list")
        //     .then((result: any) => {
        //         pluginSelf.connectionFailed = false
        //         return result
        //     })
        //     .then((result : any)          => JSON.parse(result).updates)
        //     .then((updates: UpdateItem[]) => ProcessUpdates(updates))
        //     .catch((_: any)               => {
        //         console.error("Failed to fetch updates")
        //         pluginSelf.connectionFailed = true
        //     })
        Millennium.AddWindowCreateHook(windowCreated);
    }

    exports.default = PluginMain;

    Object.defineProperty(exports, '__esModule', { value: true });

    return exports;

})({}, window.SP_REACT, window.SP_REACTDOM);

function ExecutePluginModule() {
    // Assign the plugin on plugin list. 
    Object.assign(window.PLUGIN_LIST[pluginName], millennium_main);
    // Run the rolled up plugins default exported function 
    millennium_main["default"]();
    MILLENNIUM_BACKEND_IPC.postMessage(1, { pluginName: pluginName });
}
ExecutePluginModule()