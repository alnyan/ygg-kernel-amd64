#pragma once

struct module_desc {
    char name[64];
    int version;
};

#define MODULE_ENTER                    _mod_enter
#define MODULE_EXIT                     _mod_exit
#define MODULE_DESC(_name, _version)    \
    struct module_desc _mod_desc = {    \
        .name = _name,                  \
        .version = _version,            \
    }
