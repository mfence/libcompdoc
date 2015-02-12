#include "compdoc.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc != 2)
    {
        printf("wrong arguments\n");
        return -1;
    }

    comp_doc_file_t *file;
    unsigned char *buffer;
    uint32_t dirid = 0;

    if((dirid = comp_doc_open(argv[1], COMP_DOC_PERM_READ_WRITE, &file)) != 0)
        return -1;

    comp_doc_directory_t *directory;
    
    for(dirid = 0; dirid < file->ndirs; dirid++)
    {
        directory = comp_doc_get_directory(file, dirid);
        if(IS_DIR_STREAM(directory))
            break;
    }

    comp_doc_read_stream(file, directory, &buffer);

    FILE *fp;
    fp = fopen("/tmp/stream.bin", "w");
    fwrite(buffer, directory->size, 1, fp);
    fclose(fp);

    comp_doc_close(file);
    
	return 0;
}
	
