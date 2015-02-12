#include "io.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>


/*
 * Reads from the compound document the stream that corresponds to the given directory
 * entry. It allocates and returns the data of the stream in the `buffer'.
 */
int
comp_doc_read_stream(comp_doc_file_t *file, comp_doc_directory_t *dir, unsigned char **buffer)
{
    if(!IS_DIR_STREAM(dir))
        return COMP_DOC_NO_STREAM;

    unsigned char *buf, *pos;
    uint32_t total, count;
    ssize_t bytes_read;
    int err;

    err = COMP_DOC_SUCCESS;
    total = 0;

    buf = malloc(dir->size);

    if(buf == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    pos = buf;

    if(dir->size >= file->hdr->stream_min_size)
    {
        //then the stream is divided in regular sectors
        //use SAT to construct it
        comp_doc_sector_id_t *sector = &file->sat->secids[dir->first_sector];

        if(lseek(file->fd, sector_position(file->hdr, dir->first_sector), SEEK_SET) == ((off_t)-1))
        {
            err = COMP_DOC_SEEK_ERR;
            goto _error;
        }

        if(CALC_SECTOR_SIZE(file->hdr->ssz) > dir->size)
            count = dir->size;
        else
            count = CALC_SECTOR_SIZE(file->hdr->ssz);

        if((bytes_read = read_exactly(file->fd, buf, count)) < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }

        total += bytes_read;
        pos += bytes_read;

        while(sector->next != NULL)
        {
            if(lseek(file->fd, sector_position(file->hdr, sector->value), SEEK_SET) == (off_t)-1)
            {
                err = COMP_DOC_SEEK_ERR;
                goto _error;
            }

            if(CALC_SECTOR_SIZE(file->hdr->ssz) > dir->size - total)
                count = dir->size - total;
            else
                count = CALC_SECTOR_SIZE(file->hdr->ssz);

            //printf("Copying: %x\n", count);

            if((bytes_read = read_exactly(file->fd, pos, count)) < 0)
            {
                err = COMP_DOC_READ_ERR;
                goto _error;
            }

            total += bytes_read;
            pos += bytes_read;
            sector = sector->next;
        }       
    }
    else
    {
        // the stream consists of short sectors
        comp_doc_sector_id_t *sector = &file->ssat->secids[dir->first_sector];

        if(lseek(file->fd, short_sector_position(file, dir->first_sector), SEEK_SET) < 0)
        {
            err = COMP_DOC_SEEK_ERR;
            goto _error;
        }

        if(CALC_SHORT_SECTOR_SIZE(file->hdr->sssz) > dir->size)
            count = dir->size;
        else
            count = CALC_SHORT_SECTOR_SIZE(file->hdr->sssz);

        if((bytes_read = read_exactly(file->fd, buf, count)) < 0)
        {
            err = COMP_DOC_READ_ERR;
            goto _error;
        }
        total += bytes_read;
        pos += bytes_read;

        while(sector->next != NULL)
        {
            if(lseek(file->fd, short_sector_position(file, sector->value), SEEK_SET) < 0)
            {
                err = COMP_DOC_SEEK_ERR;
                goto _error;
            }

            if(CALC_SHORT_SECTOR_SIZE(file->hdr->sssz) > dir->size - total)
                count = dir->size - total;
            else
                count = CALC_SHORT_SECTOR_SIZE(file->hdr->sssz);

            if((bytes_read = read_exactly(file->fd, pos, count)) < 0)
            {
                err = COMP_DOC_READ_ERR;
                goto _error;
            }

            total += bytes_read;
            pos += bytes_read;
            sector = sector->next;       
        }
    }

    *buffer = buf;
_error:
    if(err != COMP_DOC_SUCCESS)
    {
        if(buf)
            free(buf);
    }

    return err;
}