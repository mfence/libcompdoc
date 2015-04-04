#ifndef _CFBF_H_
#define _CFBF_H_
#define _DEBUG_
#include <stdint.h>
#include <stdlib.h>
// Currenty, it supports only the little endian format.
#define COMP_DOC_SUPPORT_ONLY_LITTLE_ENDIAN

// I didn't find any reference for ssz max size. This limitation is set by me,
// for security purposes. You may change it, but be cautious for possible integer overflows.
// (ssz stands for sector size, refer in header's struct for more info)
#define COMP_DOC_SSZ_MAX 0x10


#define COMP_DOC_MAGIC "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1"
#define COMP_DOC_BIG_ENDIAN 0xFEFF
#define COMP_DOC_LITTLE_ENDIAN 0xFFFE
#define COMP_DOC_SSZ_MIN 7
#define COMP_DOC_STREAM_MIN_SIZE 0x1000

#define CALC_SECTOR_SIZE(sz) (2 << (sz - 1))
#define CALC_SHORT_SECTOR_SIZE(sz) CALC_SECTOR_SIZE(sz)

#define SECID_FREE 0xFFFFFFFF
#define SECID_END_OF_CHAIN 0xFFFFFFFE
#define SECID_SAT 0xFFFFFFFD
#define SECID_MSAT 0xFFFFFFFC

#define COMP_DOC_DIRECTORY_NAME_SIZE            64

#define COMP_DOC_DIRECTORY_TYPE_EMPTY           0x0
#define COMP_DOC_DIRECTORY_TYPE_USER_STORAGE    0x1
#define COMP_DOC_DIRECTORY_TYPE_USER_STREAM     0x2
#define COMP_DOC_DIRECTORY_TYPE_LOCK_BYTES      0x3
#define COMP_DOC_DIRECTORY_TYPE_PROPERTY        0x4
#define COMP_DOC_DIRECTORY_TYPE_ROOT_STORAGE    0x5

#define COMP_DOC_DIRECTORY_RED                  0x0
#define COMP_DOC_DIRECTORY_BLACK                0x1

#define COMP_DOC_DIRECTORY_NO_NODE              0xFFFFFFFF

#define COMP_DOC_DIRECTORY_UID_SZ               0x10
#define COMP_DOC_DIRECTORY_SZ                   128

#define IS_DIR_EMPTY(dir) (dir->entry_type == COMP_DOC_DIRECTORY_TYPE_EMPTY)
#define IS_DIR_STORAGE(dir) (dir->entry_type == COMP_DOC_DIRECTORY_TYPE_USER_STORAGE)
#define IS_DIR_STREAM(dir) (dir->entry_type == COMP_DOC_DIRECTORY_TYPE_USER_STREAM)
#define IS_DIR_ROOT_ENTRY(dir) (dir->entry_type == COMP_DOC_DIRECTORY_TYPE_ROOT_STORAGE)


typedef struct {
    uint8_t name[COMP_DOC_DIRECTORY_NAME_SIZE];
    uint16_t name_length;
    uint8_t entry_type;
    uint8_t colour;

    uint32_t left_child_dirid;
    uint32_t right_child_dirid;
    uint32_t root_dirid;

    uint8_t uid[COMP_DOC_DIRECTORY_UID_SZ];
    uint32_t flags;

    uint64_t creation_time;
    uint64_t last_modification_time;

    uint32_t first_sector;
    uint32_t size;

    uint32_t unused;  
} __attribute__((packed)) comp_doc_directory_t;

typedef uint32_t comp_doc_secid_value_t;

struct comp_doc_sector_id {
    size_t offset;
    comp_doc_secid_value_t value;
    // if value is positive then next
    // points to the next sector in the chain
    struct comp_doc_sector_id *next;
};

typedef struct comp_doc_sector_id comp_doc_sector_id_t;


typedef struct {
    unsigned int slots;
    comp_doc_sector_id_t *secids;
} comp_doc_sat_t;

#define comp_doc_ssat_t comp_doc_sat_t

typedef struct {
    unsigned int slots;
    uint32_t *secids;
} comp_doc_msat_t;

#define COMP_DOC_HEADER_MSAT_SLOTS 109
#define COMP_DOC_HEADER_SIZE 512

typedef struct {
    uint8_t magic[8];
    /* Unique identifier (may be all 0) */
    uint8_t uid[16];
    /* Most used: 0x3e */
    uint16_t revision;
    /* Most used: 0x3 */
    uint16_t version;
    uint16_t byte_order;
    /* Sector size in power-of-two */
    uint16_t ssz;
    /* Short sector size in power-of-two */
    uint16_t sssz;

    uint8_t not_used[10];

    uint32_t nsat_sectors;
    uint32_t first_dir_sector;

    uint8_t not_used2[4];

    /* 
     * Streams that are smaller from this size are considered 
     * short-sector streams. (Min size: 4096 (0x1000))
     */ 
    uint32_t stream_min_size;

    uint32_t first_ssat_sector;
    uint32_t nssat_sectors;

    uint32_t msat_first_sector;
    uint32_t nmsat_sectors;

    /* Here comes the first part of the MSAT. */

} __attribute__((packed)) comp_doc_header_t;

#define COMP_DOC_PERM_READ          0
#define COMP_DOC_PERM_WRITE         1
#define COMP_DOC_PERM_READ_WRITE    2

typedef struct {
    char *path;
    int perm;
    int fd;
    comp_doc_header_t *hdr;
    comp_doc_msat_t *msat;
    comp_doc_sat_t *sat;
    comp_doc_ssat_t *ssat;
    comp_doc_directory_t *dirs;
    unsigned int ndirs;
} comp_doc_file_t;

#define COMP_DOC_INVALID_SAT        (-9)
#define COMP_DOC_INSANE_HEADER      (-8)
#define COMP_DOC_SEEK_ERR           (-7)
#define COMP_DOC_NO_DIRS            (-6)
#define COMP_DOC_INVALID_MSAT       (-5)
#define COMP_DOC_READ_ERR           (-4)
#define COMP_DOC_NO_MEM             (-3)
#define COMP_DOC_PERM_UNK           (-2)
#define COMP_DOC_NO_SUCH_FILE       (-1)
#define COMP_DOC_SUCCESS            0
#define COMP_DOC_NO_SSAT            1


comp_doc_directory_t ** comp_doc_list_dir(comp_doc_file_t *, comp_doc_directory_t *);
comp_doc_directory_t * comp_doc_get_root_storage(comp_doc_file_t *);
comp_doc_directory_t * comp_doc_get_directory(comp_doc_file_t *, uint32_t);
int comp_doc_open(char *, int, comp_doc_file_t **);    
void comp_doc_close(comp_doc_file_t *);

#include "io.h"
#endif /* _CFBF_H_ */