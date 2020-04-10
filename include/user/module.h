#pragma once

struct module_desc {
    char name[64];
    int version;
};

#define MODULE_ENTRY                    _mod_entry
#define MODULE_EXIT                     _mod_exit
#define MODULE_DESC(_name, _version)    \
    struct module_desc _mod_desc = {    \
        .name = _name,                  \
        .version = _version,            \
    }
