#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <setjmp.h>
#include <errno.h>
#include <xcleanup.h>

/*
* Return codes
*/
#define YARCH_SUCCESS 0
#define YARCH_EARGS 1
#define YARCH_EOPEN 2
#define YARCH_EFOPS 3

/*
* argv indexes
*/
#define OPTION_CMD_ARGV_INDEX 1
#define ARCHIEVE_NAME_ARGV_INDEX 2
#define FILES_ARGV_INDEX 3
#define EXTRACT_PATH_ARGV_INDEX 3

/*
* Useful macro for iterating through files
*/
#define for_each_file for (int32_t file_argv_index = FILES_ARGV_INDEX; file_argv_index < argc; ++file_argv_index)

/*
* Get files count to compress
*/
#define FILES_COUNT (argc - FILES_ARGV_INDEX)

/*
* argv selection
*/
#define OPTION_CMD argv[OPTION_CMD_ARGV_INDEX]
#define ARCHIEVE_NAME argv[ARCHIEVE_NAME_ARGV_INDEX]
#define FILE_NAME argv[file_argv_index]
#define EXTRACT_PATH argv[EXTRACT_PATH_ARGV_INDEX]
#define MAX_EXTRACT_PATH 256

/*
* try-catch defining
*/
static jmp_buf save_state;
static int EXCEPTION_VALUE = 0;
#define TRY if ((EXCEPTION_VALUE = setjmp(save_state)) == 0)
#define CATCH(value) else if (EXCEPTION_VALUE == value)
#define CATCHALL else
#define THROW(value) longjmp(save_state, value)

/*
* Offsets
*
* |--------------|-
* | HEADER_SIZE  | \
* | FILES_COUNT  |  \
* |--------------|   \
* | PAYLOAD_LEN  |    | HEADER SECTION
* | FILENAME_LEN |   /
* | FILENAME     |  /
* |     ...      | /
* |--------------|-
* | PAYLOAD_0    | \  DATA
* |     ...      | /  SECTION
* |--------------|-
*
*/
typedef long int size_type;
static const size_type files_count_offset = (2 * sizeof(size_type));

/*
* Payload write chunk size
*/
#define PAYLOAD_CHUNK_SIZE 512

/*
* Common variables
*/
static int ret = YARCH_SUCCESS;
static size_type size = 0;
static size_t filename_len = 0;
static char *filename_pure_pos = NULL;

int check_fopen_error(FILE *fd)
{
    if (fd == NULL)
    {
        THROW(-YARCH_EOPEN);
    }

    return YARCH_SUCCESS;
}

int check_fops_error(int code, bool by_true, int ret_code)
{
    if (by_true == true && code == ret_code || by_true == false && code != ret_code)
    {
        THROW(-YARCH_EFOPS);
    }

    return YARCH_SUCCESS;
}

size_type get_file_size(FILE *fd)
{
    size_type size = 0;

    TRY
    {
        check_fops_error(fseek(fd, 0, SEEK_END), false, YARCH_SUCCESS);
        size = ftell(fd);
        check_fops_error(size, true, -1L);
        check_fops_error(fseek(fd, 0, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        THROW(-YARCH_EFOPS);
    }

    return size;
}

void compress(int argc, char *argv[])
{
    FILE *fdr = NULL, *fdw = NULL;

    // lets check all files be able to be opened
    for_each_file
    {
        TRY
        {
            XFOPEN(fdr, FILE_NAME, "rb");
            check_fopen_error(fdr);
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }
    }

    TRY
    {
        XFOPEN(fdw, ARCHIEVE_NAME, "wb");
        check_fopen_error(fdw);
    }
    CATCH(-YARCH_EOPEN)
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, files_count_offset, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        exit(EXIT_FAILURE);
    }

    for_each_file
    {
        printf("Archiving '%s'...\n", FILE_NAME);

        TRY
        {
            XFOPEN(fdr, FILE_NAME, "rb");
            check_fopen_error(fdr);
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }

        TRY
        {
            size = get_file_size(fdr);
        }
        CATCH(-YARCH_EFOPS)
        {
            fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fwrite(&size, sizeof(size), 1, fdw) != 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fwrite()");
            exit(EXIT_FAILURE);
        }

        filename_len = strlen(FILE_NAME);

        for (filename_pure_pos = &FILE_NAME[filename_len - 1]; filename_pure_pos != FILE_NAME; --filename_pure_pos)
        {
            if (*filename_pure_pos == '\\' || *filename_pure_pos == '/')
            {
                ++filename_pure_pos;
                filename_len = strlen(filename_pure_pos);
                break;
            } 
        }

        TRY
        {
            check_fops_error(fwrite(&filename_len, sizeof(filename_len), 1, fdw) != 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fwrite()");
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fwrite(filename_pure_pos, sizeof(*filename_pure_pos), filename_len, fdw) != filename_len, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fwrite()");
            exit(EXIT_FAILURE);
        }

        XFCLOSE(fdr);
    }

    TRY
    {
        size = get_file_size(fdw);
    }
    CATCH(-YARCH_EFOPS)
    {
        fprintf(stderr, "Can't get file '%s' size\n", ARCHIEVE_NAME);
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, 0, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fwrite(&size, sizeof(size), 1, fdw) != 1, false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fwrite()");
        exit(EXIT_FAILURE);
    }

    size = FILES_COUNT;

    TRY
    {
        check_fops_error(fwrite(&size, sizeof(size), 1, fdw) != 1, false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fwrite()");
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, 0, SEEK_END), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        exit(EXIT_FAILURE);
    }

    char read_c = '\0';
    for_each_file
    {
        TRY
        {
            XFOPEN(fdr, FILE_NAME, "rb");
            check_fopen_error(fdr);
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }

        TRY
        {
            size = get_file_size(fdr);
        }
        CATCH(-YARCH_EFOPS)
        {
            fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < size; ++i)
        {
            read_c = fgetc(fdr);
            TRY
            {
                check_fops_error(fwrite(&read_c, sizeof(read_c), 1, fdw) != 1, false, YARCH_SUCCESS);
            }
            CATCH(-YARCH_EFOPS)
            {
                perror("fwrite()");
                fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
                exit(EXIT_FAILURE);
            }
        }

        XFCLOSE(fdr);
    }

    XFCLOSE(fdw);

    printf("The '%s' archive is ready!\n", ARCHIEVE_NAME);
}

void extract(int argc, char *argv[])
{
    char *filename = NULL;

    FILE *fdr_header = NULL, *fdr_payload = NULL, *fdw = NULL;

    XFOPEN(fdr_header, ARCHIEVE_NAME, "rb");
    XFOPEN(fdr_payload, ARCHIEVE_NAME, "rb");

    TRY
    {
        check_fopen_error(fdr_header);
        check_fopen_error(fdr_payload);
    }
    CATCH(-YARCH_EOPEN)
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    size_type header_size = 0;
    size_type files_count = 0;
    size_type payload_size = 0;
    size_t chunk_size = PAYLOAD_CHUNK_SIZE;
    char *payload = NULL;
    char *full_filename = NULL;

    TRY
    {
        check_fops_error(fread(&header_size, sizeof(header_size), 1, fdr_header) != 1, false, YARCH_SUCCESS);
        check_fops_error(fread(&files_count, sizeof(files_count), 1, fdr_header) != 1, false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fread()");
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdr_payload, header_size, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        exit(EXIT_FAILURE);
    }

    size_t extract_path_len = 0;
    char *extract_path = NULL;

    for (size_type i = 0; i < files_count; ++i)
    {
        TRY
        {
            check_fops_error(fread(&payload_size, sizeof(payload_size), 1, fdr_header) != 1, false, YARCH_SUCCESS);
            check_fops_error(fread(&filename_len, sizeof(filename_len), 1, fdr_header) != 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fread()");
            exit(EXIT_FAILURE);
        }
        ++filename_len;

        XMALLOC(filename, filename_len);

        if (filename == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fread(filename, sizeof(*filename), filename_len - 1, fdr_header) != filename_len - 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fread()");
            exit(EXIT_FAILURE);
        }

        filename[filename_len - 1] = '\0';

        extract_path_len = strnlen(EXTRACT_PATH, MAX_EXTRACT_PATH);
        if (extract_path_len == 0 || extract_path_len == MAX_EXTRACT_PATH)
        {
            perror("strnlen()");
            exit(EXIT_FAILURE);
        }

        XMALLOC(extract_path, extract_path_len + 1);

        if (extract_path == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }

        if (!memcpy(extract_path, EXTRACT_PATH, extract_path_len))
        {
            perror("memcpy()");
            exit(EXIT_FAILURE);
        }

        if (extract_path[extract_path_len - 1] != '\\' && extract_path[extract_path_len - 1] != '/')
        {
#ifdef _WIN32
            extract_path[extract_path_len] = '\\';
#else
            extract_path[extract_path_len] = '/';
#endif
            ++extract_path_len;
        }

        filename_len = filename_len + extract_path_len;

        XMALLOC(full_filename, filename_len);

        if (full_filename == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }

        if (!memcpy(full_filename, extract_path, extract_path_len) || !memcpy(full_filename + extract_path_len, filename, filename_len - extract_path_len))
        {
            perror("memcpy()");
            exit(EXIT_FAILURE);
        }

        XFREE(filename);
        XFREE(extract_path);

        printf("Extracting '%s'...\n", full_filename);

        XFOPEN(fdw, full_filename, "wb");

        TRY
        {
            check_fopen_error(fdw);
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }

        chunk_size = PAYLOAD_CHUNK_SIZE;

        XMALLOC(payload, chunk_size);

        if (payload == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }

        for (size_type i = 1; i <= payload_size / PAYLOAD_CHUNK_SIZE + 1; ++i)
        {
            if (i * chunk_size > payload_size)
            {
                chunk_size = payload_size - (i - 1) * chunk_size;

                if (chunk_size == 0)
                {
                    break;
                }

                XREALLOC(payload, chunk_size);

                if (payload == NULL)
                {
                    perror("realloc()");
                    exit(EXIT_FAILURE);
                }
            }

            TRY
            {
                check_fops_error(fread(payload, sizeof(*payload), chunk_size, fdr_payload) != chunk_size, false, YARCH_SUCCESS);
            }
            CATCH(-YARCH_EFOPS)
            {
                perror("fread()");
                exit(EXIT_FAILURE);
            }

            TRY
            {
                check_fops_error(fwrite(payload, sizeof(*payload), chunk_size, fdw) != chunk_size, false, YARCH_SUCCESS);
            }
            CATCH(-YARCH_EFOPS)
            {
                perror("fwrite()");
                exit(EXIT_FAILURE);
            }
        }

        XFCLOSE(fdw);
        XFREE(full_filename);
    }

    XFCLOSE(fdr_header);
    XFCLOSE(fdr_payload);

    printf("The files from the '%s' archive have been extracted!\n", ARCHIEVE_NAME);
}

int main(int argc, char *argv[])
{
    CLEANUP_INIT();

    if (argc < 4)
    {
        printf("Usage:\n");
        printf("  --compress-- %s c <archieve_name> <file_1> <file_2> ...\n", argv[0]);
        printf("  --extract-- %s x <archieve_name> <path>\n", argv[0]);
        exit(YARCH_EARGS);
    }

    if (argv[1][0] == 'c')
    {
        compress(argc, argv);
    }
    else if (argv[1][0] == 'x')
    {
        extract(argc, argv);
    }
    else
    {
        printf("Unknown option '%c'\n", argv[1][0]);
    }

    return EXIT_SUCCESS;
}