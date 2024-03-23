// post-processor that interfaces with the rolled up JS and converts to c-bytes to use in millennium
// reduces overhead massively

const fs = require('fs');
const path = require('path');

// hard coded, no reason for it not to be
const out_file = "../settings.cpp"

const var_name = `millennium_modal`
let file = 
`// this file was auto generated in ./interop/vendor/byte.js
#include "settings.hpp"

std::string ui_interface::settings_page_renderer()
{ 
    %{}

    return std::string(${var_name}, ${var_name} + sizeof(${var_name}) / sizeof(${var_name}[0]));
}`

const removeFolderRecursive = folderPath => {
    fs.existsSync(folderPath) && fs.readdirSync(folderPath).forEach(file => {
        const curPath = path.join(folderPath, file);
        fs.lstatSync(curPath).isDirectory() ? removeFolderRecursive(curPath) : fs.unlinkSync(curPath);
    });
    fs.rmdirSync(folderPath);
};

fs.readFile('vendor/out/bundle.js', (err, data) => {
    if (err) {
        console.error('Error reading file:', err);
        return;
    }
    let bytes = 'unsigned char millennium_modal[] = {\n\t\t';
    for (let i = 0; i < data.length; i++) {
        const hex = data[i].toString(16).padStart(2, '0');
        bytes += `0x${hex}`;
        if (i < data.length - 1) {
            bytes += ', ';
            if ((i + 1) % 16 === 0) {
                bytes += '\n\t\t';
            }
        }
    }
    bytes += '\n\t};';
    file = file.replace(/%{}/g, bytes)
    fs.writeFile(out_file, file, (err) => {
        if (err) throw err;
        console.log('\x1b[32m%s\x1b[0m', '[millennium] bytes written to → output.cpp');
    });

    const removeFolderRecursive = folderPath => {
        fs.existsSync(folderPath) && fs.readdirSync(folderPath).forEach(file => {
            const curPath = path.join(folderPath, file);
            fs.lstatSync(curPath).isDirectory() ? removeFolderRecursive(curPath) : fs.unlinkSync(curPath);
        });
        fs.rmdirSync(folderPath);
    };
    removeFolderRecursive("vendor/out/");
});