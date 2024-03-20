const stylesheets = {
    css: 
    `.settings_item {
        margin-bottom: 0;
        display: flex;
        align-items: center;
        white-space: normal;
        max-width: 180px;
        text-overflow: ellipsis;
        padding: 10px 16px 10px 24px;
        font-size: 14px;
        font-weight: 400;
        color: #b8bcbf;
        text-transform: none;
        cursor: pointer;
        user-select: none
    }
    .settings_item_icon {
        color: #b8bcbf;
        height: 20px;
        width: 20px;
        margin-right: 16px
    }  
    .settings_item_name {
        overflow: hidden;
        text-overflow: ellipsis
    }  
    .settings_item.selected {
        color: #fff;
        background-color: #3d4450
    } 
    .settings_item:not(.selected):hover {
        background-color: #dcdedf;
        color: #3d4450
    }
    .settings_item.selected .settings_item_icon {
        color: #fff
    }
    .settings_item:not(.selected):hover .settings_item_icon {
        color: #3d4450
    }
    input[type=color] {
        -webkit-appearance: none;
        border: none
    }
    input[type=color]::-webkit-color-swatch-wrapper {
        padding: 0
    }
    input[type=color]::-webkit-color-swatch {
        border: none
    }
    input#favcolor {
        width: 35px;
        height: 100%;
        border-radius: 10px !important
    }
    input[type=color i] {
        padding: 0 !important
    }
    .stop-scrolling {
        height: 100%;
        overflow: hidden
    }
    .dialog_overlay {
        position: absolute;
        top: 0;
        left: 0;
        width: 100vw;
        height: 100vh;
        z-index: 99
    }
    .cdiv {
        align-items: center;
        display: flex;
        height: 100%;
        width: 100%;
        flex-direction: column;
        justify-content: center;
    }
    .desc-text-center {
        align-items: center;
        justify-content: center;
        display: flex
    }
    ._1tiuYeMmTc9DMG4mhgaQ5w._DialogInputContainer {
        max-height: 300px !important;
    }`,
    
    insert: () => {
        document.head.appendChild(Object.assign(document.createElement('style'), {
          textContent: stylesheets.css
        }));
    }
}

export { stylesheets }