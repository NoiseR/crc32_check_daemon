#ifndef FILE_REPO_HEADER
#define FILE_REPO_HEADER

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILE_NOT_FOUND,
    ATTR_CHANGED,
    VALID_ATTR,
    CHECK_ERROR
} FileAttrStatus;

// start observing file
// param[in] file_name - uniq file name
// param[in] file_attr - file's attribute for observation
// return operation result: 0 - file was added, 1 - error
int push_file(const char *file_name, uint32_t file_attr);

// check file's attribute
// param[in] file_name - uniq file name
// param[in] file_attr - current file's attribute
// return attribute's status (see typedef)
FileAttrStatus check_file_attr(const char *file_name, uint32_t file_attr);

// get file's attribute
// param[in] file_name - uniq file name
// return file's attribute (0 if file not found)
uint32_t get_file_attr(const char *file_name);

// return unchecked file name until there is no unchecked file (return NULL)
const char *get_next_unchecked_file();

#ifdef __cplusplus
}
#endif

#endif // FILE_REPO_HEADER
