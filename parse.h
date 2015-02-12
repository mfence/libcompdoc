#ifndef _COMP_DOC_PARSE_H_
#define _COMP_DOC_PARSE_H_
#include <stdint.h>
#include <unistd.h>
#include "compdoc.h"

off_t short_sector_position(comp_doc_file_t *, uint32_t);
off_t sector_position(comp_doc_header_t *, uint32_t);
ssize_t read_exactly(int, void *, ssize_t);
inline void free_header(comp_doc_header_t *);
inline void free_msat(comp_doc_msat_t *);
inline void free_sat(comp_doc_sat_t *);
inline void free_directory(comp_doc_directory_t *);
int parse_msat(int, comp_doc_header_t *, comp_doc_msat_t **);
int parse_sat(int, comp_doc_header_t *, comp_doc_msat_t *, comp_doc_sat_t **);
int parse_ssat(int, comp_doc_header_t *, comp_doc_sat_t *, comp_doc_ssat_t **); 
int parse_directories(int, comp_doc_header_t *, comp_doc_sat_t *, comp_doc_directory_t **, unsigned int *);
int parse_header(int, comp_doc_header_t **);

#define free_ssat free_sat

#endif /* _COMP_DOC_PARSE_H_*/