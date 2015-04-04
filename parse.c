#include "parse.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

off_t
short_sector_position(comp_doc_file_t *file, uint32_t ssecid)
{
    // XXX: assertion first directory entry _MUST_ be the root storage
    uint32_t shortsec_container_id = file->dirs[0].first_sector;
    uint32_t max_shortsec = CALC_SECTOR_SIZE(file->hdr->ssz) / CALC_SHORT_SECTOR_SIZE(file->hdr->sssz);
    uint32_t secid = shortsec_container_id;
    uint32_t offset;

    offset = 0;
    // XXX: sanity check: short container 
    comp_doc_sector_id_t *sector = &file->sat->secids[shortsec_container_id];

    while(ssecid >= max_shortsec)
    {
        //if(sector->next == NULL) { critical error }
        secid = sector->value;
        sector = sector->next;
        //max_shortsec += CALC_SECTOR_SIZE(file->hdr->ssz) / CALC_SHORT_SECTOR_SIZE(file->hdr->sssz);
        ssecid -= max_shortsec;
    }

    offset = sector_position(file->hdr, secid);
    offset += ssecid * CALC_SHORT_SECTOR_SIZE(file->hdr->sssz);

    return offset;
}
    
/* 
 * Returns the absolute offset, from the beginning of the file,
 * of a sector with ID `secid'.
 */

off_t
sector_position(comp_doc_header_t *header, uint32_t secid)
{
    return COMP_DOC_HEADER_SIZE + (secid * CALC_SECTOR_SIZE(header->ssz));
}

/*
 *  Reads exactly `size' bytes from the file descriptor into buffer.
 *  Otherwise, the file is closed and returns error.
 */
ssize_t
read_exactly(int fd, void *buffer, ssize_t size)
{
    ssize_t bytes_read;

    bytes_read = read(fd, buffer, size);
    if(bytes_read < size)
    {
        close(fd);
        return COMP_DOC_READ_ERR;
    }

    return bytes_read;
}

inline void
free_header(comp_doc_header_t *hdr)
{
    if(hdr)
        free(hdr);
}

inline void
free_msat(comp_doc_msat_t *msat)
{
    if(msat)
    {
        if(msat->secids)
            free(msat->secids);
        free(msat);
    }
}

inline void
free_sat(comp_doc_sat_t *sat)
{
    if(sat)
    {
        if(sat->secids)
            free(sat->secids);
        free(sat);
    }
}

inline void
free_directory(comp_doc_directory_t *dirs)
{
    if(dirs)
        free(dirs);
}

static int
parse_msat_from_sectors(int fd, comp_doc_header_t *hdr, comp_doc_msat_t *msat)
{

    int err;
    uint8_t *buffer;
    ssize_t bytes_read;
    uint32_t *p, msat_sector;
    uint32_t msat_per_sector;

    err = COMP_DOC_SUCCESS;
    buffer = NULL;

    buffer = malloc(CALC_SECTOR_SIZE(hdr->ssz) * sizeof(uint8_t));
    if(buffer == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }
    
    // The whole sector - except the last four bytes - is filled up
    // with MSAT IDs. Each MSAT SecID is an integer, meaning it is four bytes.
    msat_per_sector = (CALC_SECTOR_SIZE(hdr->ssz) - 4) / 4;
    
    msat->secids = realloc(msat->secids, (COMP_DOC_HEADER_MSAT_SLOTS + msat_per_sector * hdr->nmsat_sectors) * sizeof(uint32_t));

    if(msat->secids == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    msat_sector = hdr->msat_first_sector;

    /* 
     * According to OO-specification, the indication of a sector that it is the last
     * that contains the MSAT, is the last four bytes of the sector are equal
     * to SECID_END_OF_CHAIN. However, I encountered some files that the last four
     * bytes were SECID_FREE.
     */
    while(msat_sector != SECID_END_OF_CHAIN && msat_sector != SECID_FREE)
    {
        //write the whole sector into the buffer

        lseek(fd, sector_position(hdr, msat_sector), SEEK_SET);
        //printf("POSITION: %x\n", sector_position(hdr, hdr->msat_first_sector));
        if((bytes_read = read_exactly(fd, buffer, CALC_SECTOR_SIZE(hdr->ssz))) < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }

        p = (uint32_t *)buffer;

        // parse the whole sector except from the last slot
        while(p < (uint32_t *)(buffer + bytes_read - 4))
        {
            if(*p != SECID_FREE)
            {
                *(msat->secids + msat->slots) = *p;
                msat->slots++;
            }
            p++;
        }

        // p poinst to next secid if exists. otherwise it's SECID_END_OF_CHAIN 
        msat_sector = *p;
    }

    ///XXX: maybe realloc msat->secids here.

_error:
    if(buffer)
        free(buffer);

    return err;
}


int
parse_msat(int fd, comp_doc_header_t *header, comp_doc_msat_t **ret_msat)
{
    uint8_t *buffer;
    uint32_t *p, err;
    ssize_t bytes_read;
    unsigned int msat_slots;
    comp_doc_msat_t *msat;
    
    *ret_msat = NULL;
    msat = NULL;
    buffer = NULL;
    msat_slots = 0;

    err = COMP_DOC_SUCCESS;

    msat = malloc(sizeof(comp_doc_msat_t));

    if(msat == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    msat->slots = 0;
    msat->secids = NULL;


    // although the sector size may be small, the buffer should be
    // big enough to hold the MSAT that have not been read yet 
    if(CALC_SECTOR_SIZE(header->ssz) > (COMP_DOC_HEADER_SIZE - sizeof(comp_doc_header_t)))
        buffer = malloc(CALC_SECTOR_SIZE(header->ssz) * sizeof(int8_t));
    else
        buffer = malloc((COMP_DOC_HEADER_SIZE - sizeof(comp_doc_header_t)) * sizeof(int8_t));

    if(buffer == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    if((bytes_read = read_exactly(fd, buffer, 
        (COMP_DOC_HEADER_SIZE - sizeof(comp_doc_header_t)))) < 0)
            return COMP_DOC_READ_ERR;

    p = (uint32_t *)buffer;

    //TODO: for security purposes we should apply strict parsing.
    while(p < (uint32_t *)(buffer + bytes_read))
    {
        if(*p != SECID_FREE)
        {
            msat_slots++;   
        }
        p++;
    }

    if(msat_slots > 0)
    {
        msat->slots = msat_slots;
        msat->secids = malloc(msat_slots * sizeof(uint32_t));
        memcpy(msat->secids, buffer, msat->slots * sizeof(uint32_t));
    }
    else
    {
        msat_slots = 0;
        msat->secids = NULL;
    }

    //XXX: Make it stricter.
    if(header->nmsat_sectors > 0 && header->msat_first_sector != SECID_FREE)
    {
        // if a new sector is used for MSAT, then the MSAT region
        // in the header must be full.
        if(msat_slots != 109)
        {
            err = COMP_DOC_INVALID_MSAT;
            goto _error;
        }
        else
        {
            parse_msat_from_sectors(fd, header, msat);
        }
    }

    *ret_msat = msat;

_error:
    if(buffer)
        free(buffer);

    if(err != COMP_DOC_SUCCESS)
    {
        free_msat(msat);
        *ret_msat = NULL;
    }

    return err;
}


int
parse_ssat(int fd, comp_doc_header_t *hdr, comp_doc_sat_t *sat, comp_doc_ssat_t **ret_ssat)
{
    comp_doc_ssat_t *ssat;
    int err;    
    unsigned int slots_per_sector;
    uint32_t current_sector, tmp, *p;
    uint8_t *buffer;
    ssize_t bytes_read;

    *ret_ssat = NULL;
    err = COMP_DOC_SUCCESS;

    // TODO: sanity check here
    if(hdr->first_ssat_sector == SECID_END_OF_CHAIN && hdr->nssat_sectors == 0)
        return COMP_DOC_NO_SSAT;

    buffer = malloc(CALC_SECTOR_SIZE(hdr->ssz));
    if(buffer == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }
    
    ssat = malloc(sizeof(comp_doc_ssat_t));
    if(ssat == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    slots_per_sector = CALC_SECTOR_SIZE(hdr->ssz) / 4;
    ssat->secids = calloc(slots_per_sector * hdr->nssat_sectors, sizeof(comp_doc_ssat_t));

    if(ssat->secids == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    ssat->slots = 0;

    current_sector = hdr->first_ssat_sector;
    
    while(current_sector != SECID_END_OF_CHAIN)
    {
        if(lseek(fd, sector_position(hdr, current_sector), SEEK_SET) < 0)
        {
            err = COMP_DOC_SEEK_ERR;
            goto _error;
        }

        if((bytes_read = read_exactly(fd, buffer, CALC_SECTOR_SIZE(hdr->ssz))) < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }

        p = (uint32_t *)buffer;

        while(p < (uint32_t *)(buffer + bytes_read))
        {
            tmp = *p;
            ssat->secids[ssat->slots].value = tmp;

            if(tmp < SECID_MSAT)
            {
                // XXX: if tmp is not checked, this can lead to a serious vulnerability
                ssat->secids[ssat->slots].next = &ssat->secids[tmp];
            }
            else
            {
                ssat->secids[ssat->slots].next = NULL;
            }

            p++;
            ssat->slots++;
        }

        current_sector = sat->secids[current_sector].value;
    }

    *ret_ssat = ssat;

_error:
    if(buffer)
        free(buffer);

    if(err != COMP_DOC_SUCCESS)
    {
        free_ssat(ssat);
        *ret_ssat = NULL;
    }

    return err; 
}

int 
parse_sat(int fd, comp_doc_header_t *hdr, comp_doc_msat_t *msat, comp_doc_sat_t **ret_sat)
{
    int i, err;
    uint8_t *buffer;
    ssize_t bytes_read;
    comp_doc_secid_value_t *p, tmp;
    comp_doc_secid_value_t sector_index;
    comp_doc_sat_t *sat;

    *ret_sat = NULL;
    err = COMP_DOC_SUCCESS;
    sector_index = 0;
    buffer = NULL;

    sat = malloc(sizeof(comp_doc_sat_t));

    if(sat == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    // The whole sector is used as SAT.
    sat->slots = (CALC_SECTOR_SIZE(hdr->ssz) * hdr->nsat_sectors) / 4;
    // XXX: may overflow here
    sat->secids = malloc(sat->slots * sizeof(comp_doc_sector_id_t));

    if(sat->secids == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    buffer = malloc(CALC_SECTOR_SIZE(hdr->ssz));

    if(buffer == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }
    
    for(i = 0; i < msat->slots; i++)
    {

        lseek(fd, sector_position(hdr, msat->secids[i]), SEEK_SET);
        /* Read the sector that is indicated by MSAT */
        bytes_read = read_exactly(fd, buffer, CALC_SECTOR_SIZE(hdr->ssz));
        
        if(bytes_read < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }

        p = (uint32_t *)buffer;

        while(p < (uint32_t *)(buffer + bytes_read))
        {
            tmp = *p;
            sat->secids[sector_index].value = tmp;
            sat->secids[sector_index].offset = sector_position(hdr, msat->secids[i]);
            
            if(tmp < SECID_MSAT)
            {
                // if tmp is a just regular sector

                // XXX: if tmp is not checked, this can lead to a serious vulnerability
                if(tmp >= sat->slots)
                {
                    err = COMP_DOC_INVALID_SAT;
                    goto _error;
                }
                sat->secids[sector_index].next = &sat->secids[tmp];
            }
            else
            {
                sat->secids[sector_index].next = NULL;
            }

            p++;
            sector_index++;
        }
    }

    *ret_sat = sat;

_error:    
    if(buffer)
        free(buffer);

    if (err != COMP_DOC_SUCCESS)
    {
        free_sat(sat);
    }

    return err;
}

int
parse_directories(int fd, comp_doc_header_t *hdr, comp_doc_sat_t *sat, comp_doc_directory_t **ret_dirs, unsigned int *ndirs)
{
    int err;
    unsigned int ndir_sectors, dirs_per_sector, i;
    comp_doc_sector_id_t *root_secid, *cur_secid;
    comp_doc_directory_t *dirs;

    err = COMP_DOC_SUCCESS;
    dirs = NULL;
    *ret_dirs = NULL;
    *ndirs = 0;
    cur_secid = root_secid = &sat->secids[hdr->first_dir_sector];

    ndir_sectors = 1;

    /* count the sectors in which are contained the directories */
    while(cur_secid->value != SECID_END_OF_CHAIN)
    {
        ndir_sectors++;
        cur_secid = cur_secid->next;
    }

    /* there should be at least one directory entry */
    if(!ndir_sectors)
    {
        err = COMP_DOC_NO_DIRS;
        goto _error;
    }

    dirs_per_sector = CALC_SECTOR_SIZE(hdr->ssz) / COMP_DOC_DIRECTORY_SZ;

    dirs = malloc(sizeof(comp_doc_directory_t) * dirs_per_sector * ndir_sectors);

    if(dirs == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    cur_secid = root_secid;

    // parse all (ndir_sectors) sectors that contain directories
    for(i = 0; i < ndir_sectors; i++)
    {
        if(i == 0)
        {
            //printf("%x\n", sector_position(hdr, hdr->first_dir_sector));
            if(lseek(fd, sector_position(hdr, hdr->first_dir_sector), SEEK_SET) < 0)
            {
                err = COMP_DOC_SEEK_ERR;
                goto _error;
            }
        }
        else
        {
            if(lseek(fd, sector_position(hdr, cur_secid->value), SEEK_SET) < 0)
            {
                err = COMP_DOC_SEEK_ERR;
                goto _error;
            }
        }

        if(read_exactly(fd, (dirs + (i * dirs_per_sector)), CALC_SECTOR_SIZE(hdr->ssz)) < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }

        if(i != 0)
            cur_secid = cur_secid->next;
    }


    *ret_dirs = dirs;
    *ndirs = dirs_per_sector * ndir_sectors;

_error:
    if(err != COMP_DOC_SUCCESS)
    {
        *ndirs = 0;
        free_directory(dirs);
    }

    return err;
}


/* 
 * This function contains some basic checks to ensure that the header 
 * is not corrupted or malformed.
 */
int
check_header_sanity(comp_doc_header_t *hdr)
{
    // Probably I could include all comparison that exist in this function
    // in a single if statement. However, I chose to do like this for readability reasons.
    // Hopefully the compiler will produce highly optimized code :)
    int err = COMP_DOC_SUCCESS;

    if(memcmp(hdr->magic, COMP_DOC_MAGIC, 0x8))
        err = COMP_DOC_INSANE_HEADER;

    #ifdef COMP_DOC_SUPPORT_ONLY_LITTLE_ENDIAN
    if(hdr->byte_order != COMP_DOC_LITTLE_ENDIAN)
        err = COMP_DOC_INSANE_HEADER;
    #endif /* COMP_DOC_SUPPORT_ONLY_LITTLE_ENDIAN */

    if(hdr->ssz < 7 || hdr->ssz > COMP_DOC_SSZ_MAX)
        err = COMP_DOC_INSANE_HEADER;

    // XXX: is the second condition required ?
    if(hdr->sssz > hdr->ssz || hdr->sssz == 0)
        err = COMP_DOC_INSANE_HEADER;

    if(hdr->stream_min_size < 0x1000)
        err = COMP_DOC_INSANE_HEADER;

    if(hdr->nssat_sectors == 0 && hdr->first_ssat_sector != SECID_END_OF_CHAIN)
        err = COMP_DOC_INSANE_HEADER;

    if(hdr->nmsat_sectors == 0 && hdr->msat_first_sector != SECID_END_OF_CHAIN)
        err = COMP_DOC_INSANE_HEADER;

    return err;
}

int 
parse_header(int fd, comp_doc_header_t **ret_hdr)
{
    int err;
    comp_doc_header_t *hdr;

    *ret_hdr = NULL;
    hdr = NULL;
    err = COMP_DOC_SUCCESS;

    hdr = calloc(1, sizeof(comp_doc_header_t));

    if(hdr == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    if(read_exactly(fd, hdr, sizeof(comp_doc_header_t)) < 0)
    {
            err = COMP_DOC_READ_ERR;
            goto _error;
    }

    *ret_hdr = hdr;

    err = check_header_sanity(hdr);    

_error:
    if(err != COMP_DOC_SUCCESS)
    {
        *ret_hdr = NULL;
        free_header(hdr);
    }

    return err;
}