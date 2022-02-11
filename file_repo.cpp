#include "file_repo.h"
#include <unordered_map>
#include <string>

struct file_info {
    uint32_t file_attr;
    bool     checked;
};
static std::unordered_map<std::string, file_info> check_files;

int push_file(const char *file_name, uint32_t file_attr) {
    try {
        check_files[std::string(file_name)] = {file_attr, false};
    }  catch (...) {
        return 1;
    }
    return 0;
}

FileAttrStatus check_file_attr(const char *file_name, uint32_t file_attr) {
    try {
        std::string fname = std::string(file_name);
        if (check_files.find(fname) == check_files.end())
            return FILE_NOT_FOUND;
        check_files[fname].checked = true;
        if (check_files[fname].file_attr != file_attr)
            return ATTR_CHANGED;
        return VALID_ATTR;
    }  catch (...) {}
    return CHECK_ERROR;
}

uint32_t get_file_attr(const char *file_name) {
    try {
        std::string fname = std::string(file_name);
        if (check_files.find(fname) == check_files.end())
            return 0;
        return check_files[fname].file_attr;
    }  catch (...) {}
    return 0;
}

const char *get_next_unchecked_file() {
    try {
        for (auto &it: check_files) {
            if (!it.second.checked) {
                it.second.checked = true;
                return it.first.c_str();
            }
        }
    }  catch (...) {
        return NULL;
    }
    for (auto &it: check_files) {
        it.second.checked = false;
    }
    return NULL;
}
