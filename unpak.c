#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>

#define PAK_HEADER_MAGIC (0xBAC04AC0)
#define PATH_BUFFER_SIZE (1024)
#define OUTPUT_BUFFER_SIZE (1024)

typedef struct pak_header
{
    unsigned int magic;
    unsigned int version;
} pak_header_t;

typedef struct pak_file_info
{
    unsigned char path_len;
    char *path;
    unsigned int file_size;
    unsigned long long timestamp;
    struct pak_file_info *next;
} pak_file_info_t;

void pak_xor(void *buf, size_t size)
{
    unsigned char *p = (unsigned char *)buf;
    while (size--)
        *p++ ^= 0xF7;
}

void pak_read_header(FILE *pf, pak_header_t *pheader)
{
    fread(pheader, sizeof(pak_header_t), 1, pf);
    pak_xor(pheader, sizeof(pak_header_t));
}

pak_file_info_t *pak_read_file_info(FILE *pf)
{
    static char path[256];
    unsigned char flag;
    unsigned char path_len;
    unsigned int file_size;
    unsigned long long timestamp;

    pak_file_info_t *first = NULL;
    pak_file_info_t *p = NULL;
    while (1)
    {
        fread(&flag, 1, 1, pf);
        pak_xor(&flag, 1);
        if (flag)
            break;

        if (p == NULL)
            p = first = (pak_file_info_t *)malloc(sizeof(pak_file_info_t));
        else
        {
            p->next = (pak_file_info_t *)malloc(sizeof(pak_file_info_t));
            p = p->next;
        }

        fread(&path_len, 1, 1, pf);
        pak_xor(&path_len, 1);

        fread(path, 1, path_len, pf);
        pak_xor(path, path_len);
        path[path_len] = '\0';

        fread(&file_size, 4, 1, pf);
        pak_xor(&file_size, 4);

        fread(&timestamp, 8, 1, pf);
        pak_xor(&timestamp, 8);

        p->path_len = path_len;
        p->path = (char *)malloc(path_len + 1);
        strcpy(p->path, path);
        p->file_size = file_size;
        p->timestamp = timestamp;
        p->next = NULL;
    }
    return first;
}

void free_file_info(pak_file_info_t *p)
{
    pak_file_info_t *next = NULL;
    while (p)
    {
        next = p->next;
        free(p->path);
        free(p);
        p = next;
    }
}

void create_folder_ifneed(const char *file_path)
{
    static char buf[PATH_BUFFER_SIZE];
    int i, c;
    for (i = 0; file_path[i]; ++i)
    {
        c = file_path[i];
        if (c == '\\' || c == '/')
        {
            buf[i] = '\0';
            if (access(buf, 0) == -1)
            {
                mkdir(buf);
                // printf("created folder: %s\n", buf);
            }
        }
        buf[i] = c;
    }
}

int pak_release_files(FILE *pf, pak_file_info_t *files, const char *path_out)
{
    static char path_full[PATH_BUFFER_SIZE];
    static unsigned char buf[OUTPUT_BUFFER_SIZE];

    size_t path_out_len = strlen(path_out);
    strcpy(path_full, path_out);
    if (path_full[path_out_len - 1] != '\\' && path_full[path_out_len - 1] != '/')
    {
        path_full[path_out_len] = '\\';
        path_full[++path_out_len] = '\0';
    }

    unsigned int file_size;
    pak_file_info_t *p = files;
    FILE *pfout = NULL;
    while (p)
    {
        file_size = p->file_size;
        strcpy(path_full + path_out_len, p->path);
        create_folder_ifneed(path_full);
        pfout = fopen(path_full, "wb");
        if (pfout == NULL)
        {
            printf("error: unable to open file %s\n", path_full);
            return 0;
        }
        while (file_size >= OUTPUT_BUFFER_SIZE)
        {
            fread(buf, OUTPUT_BUFFER_SIZE, 1, pf);
            pak_xor(buf, OUTPUT_BUFFER_SIZE);
            fwrite(buf, OUTPUT_BUFFER_SIZE, 1, pfout);
            file_size -= OUTPUT_BUFFER_SIZE;
        }
        if (file_size)
        {
            fread(buf, file_size, 1, pf);
            pak_xor(buf, file_size);
            fwrite(buf, file_size, 1, pfout);
        }
        // printf("output file: %s\n", path_full);
        fclose(pfout);
        p = p->next;
    }
    return 1;
}

int unpak(const char *path_pak, const char *path_out)
{
    FILE *pf = fopen(path_pak, "rb");
    if (pf == NULL)
    {
        printf("error: unable to open file %s\n", path_pak);
        return 0;
    }

    pak_header_t header;
    pak_read_header(pf, &header);
    if (header.magic != PAK_HEADER_MAGIC)
    {
        puts("error: input file is not a legal pak file");
        fclose(pf);
        return 0;
    }
    // else
    // {
    //     printf("pak version: %u\n", header.version);
    // }

    pak_file_info_t *files = pak_read_file_info(pf);
    int ret;
    if (files == NULL)
    {
        ret = 0;
        puts("error: fail to read file info");
    }
    else
    {
        ret = pak_release_files(pf, files, path_out);
        free_file_info(files);
    }

    fclose(pf);
    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        puts("unpak [pak] [output]");
        puts("");
        puts("  pak:    path of pak file");
        puts("  output: path to output");
        puts("");
        return 0;
    }

    char *path_pak = argv[1];
    char *path_out = argv[2];

    if (unpak(path_pak, path_out))
    {
        puts("done");
    }

    return 0;
}
