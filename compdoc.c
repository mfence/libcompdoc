#include "compdoc.h"
#include "parse.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

//#include <stdio.h>
//#include <ctype.h>

comp_doc_directory_t **
comp_doc_list_dir(comp_doc_file_t *file, comp_doc_directory_t *dir)
{
    comp_doc_directory_t **list, **cur_read, **cur_write;
    comp_doc_directory_t *d;
    uint32_t written;

    if(!IS_DIR_STORAGE(dir) && !IS_DIR_ROOT_ENTRY(dir))
        return NULL;

    list = calloc((file->ndirs + 1), sizeof(comp_doc_directory_t *));

    if(list == NULL)
        return NULL;
    
    //XXX: sanity check: dir->root_dirid
    *list = &file->dirs[dir->root_dirid];

    cur_write = list + 1;
    cur_read = list;
    
    // XXX: check that cur_write is not writing out of bounds as well
    while(*cur_read != NULL)
    {
        d = *cur_read;

        if(d->left_child_dirid != COMP_DOC_DIRECTORY_NO_NODE)
        {
            // XXX: Should check if the dirid is bigger than file->ndirs
            *cur_write = &file->dirs[d->left_child_dirid];
            cur_write++;
        }

        if(d->right_child_dirid != COMP_DOC_DIRECTORY_NO_NODE)
        {
            // XXX: also here
            *cur_write = &file->dirs[d->right_child_dirid];
            cur_write++;
        }

        cur_read++;
    }

    written = cur_write - list;
    //if(written > file->ndirs)
    //  corruption

    return realloc(list, (written + 1) * sizeof(comp_doc_directory_t));
}

comp_doc_directory_t *
comp_doc_get_root_storage(comp_doc_file_t *file)
{
    if(file->ndirs > 0)
        return file->dirs;
    else
        return NULL;
}

comp_doc_directory_t *
comp_doc_get_directory(comp_doc_file_t *file, uint32_t dirid)
{
    comp_doc_directory_t *dir = NULL;
    if(dirid < file->ndirs)
    {
        dir = file->dirs + dirid;
    }
    return dir;
}

void 
comp_doc_close(comp_doc_file_t *file)
{
    free_header(file->hdr);
    free_msat(file->msat);
    free_sat(file->sat);
    free_ssat(file->ssat);
    free_directory(file->dirs);
    close(file->fd);
    free(file->path);
    free(file);
}

int
comp_doc_open(char *path, int perm, comp_doc_file_t **ret_file)
{
    int fd, open_flags, retval, err;
    comp_doc_file_t *file;
    
    *ret_file = NULL;
    err = COMP_DOC_SUCCESS;

    file = calloc(1, sizeof(comp_doc_file_t));

    if(file == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    if(!strlen(path))
    {
        err = COMP_DOC_NO_SUCH_FILE;
        goto _error;
    }

    file->path = malloc(strlen(path)+1);

    if(file->path == NULL)
    {
        err = COMP_DOC_NO_MEM;
        goto _error;
    }

    strcpy(file->path, path);

    if(file->perm != COMP_DOC_PERM_READ && file->perm != COMP_DOC_PERM_WRITE)
    {
        err = COMP_DOC_PERM_UNK;
        goto _error;
    }
    else
    {
        if(file->perm == COMP_DOC_PERM_READ)
            open_flags = O_RDONLY;
        else if(file->perm == COMP_DOC_PERM_WRITE)
            open_flags = O_WRONLY;
        else if(file->perm == COMP_DOC_PERM_READ_WRITE)
            open_flags = O_RDWR;
    }
    
    fd = open(file->path, open_flags);

    if(fd == -1)
    {
        err = fd;
        goto _error;
    }
    file->fd = fd;

    if((retval = parse_header(fd, &file->hdr)) != COMP_DOC_SUCCESS)
    {
        err = retval;
        goto _error;
    }

    if((retval = parse_msat(file->fd, file->hdr, &file->msat)) != COMP_DOC_SUCCESS)
    {
            err = retval;
            goto _error;
    }

    if((retval = parse_sat(file->fd, file->hdr, file->msat, &file->sat)) != COMP_DOC_SUCCESS)
    {
        err = retval;
        goto _error;
    }

    if((retval = parse_ssat(file->fd, file->hdr, file->sat, &file->ssat)) < COMP_DOC_SUCCESS)
    {
        err = retval;
        goto _error;
    }

    if((retval = parse_directories(file->fd, file->hdr, file->sat, &file->dirs, &file->ndirs)) != COMP_DOC_SUCCESS)
    {
        err = retval;
        goto _error;
    }


    *ret_file = file;

_error:
    if(err != COMP_DOC_SUCCESS)
    {
        free_header(file->hdr);
        free_msat(file->msat);
        free_sat(file->sat);
        free_ssat(file->ssat);
        free_directory(file->dirs);
    }

    return err;
}

