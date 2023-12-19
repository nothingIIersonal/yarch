#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <setjmp.h>
#include <errno.h>

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
// #define HEADER_SIZE_TYPE long int
// #define HEADER_FILES_COUNT_TYPE long int
// #define HEADER_SIZE_OFF sizeof(HEADER_SIZE_TYPE)
// #define FILES_COUNT_OFF HEADER_SIZE_OFF(HEADER_SIZE_TYPE) + sizeof(HEADER_FILES_COUNT_TYPE)
#define SIZE_TYPE long int
#define HEADER_SIZE_OFF sizeof(SIZE_TYPE)
#define FILES_COUNT_OFF (2 * sizeof(SIZE_TYPE))

/*
* Payload write chunk size
*/
#define PAYLOAD_CHUNK_SIZE 512

/*
* Common variables
*/
static int ret = YARCH_SUCCESS;
static SIZE_TYPE size = 0;
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

SIZE_TYPE get_file_size(FILE *fd)
{
    SIZE_TYPE size = 0, cur_pos = ftell(fd);

    TRY
    {
        check_fops_error(fseek(fd, 0, SEEK_END), false, YARCH_SUCCESS);
        size = ftell(fd);
        check_fops_error(size, true, -1L);
        check_fops_error(fseek(fd, cur_pos, SEEK_SET), false, YARCH_SUCCESS);
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
            check_fopen_error(fdr = fopen(FILE_NAME, "rb"));
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            exit(EXIT_FAILURE);
        }
    }

    TRY
    {
        check_fopen_error(fdw = fopen(ARCHIEVE_NAME, "wb"));
    }
    CATCH(-YARCH_EOPEN)
    {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, FILES_COUNT_OFF, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        fclose(fdw);
        exit(EXIT_FAILURE);
    }

    for_each_file
    {
        printf("Archiving '%s'...\n", FILE_NAME);

        TRY
        {
            check_fopen_error(fdr = fopen(FILE_NAME, "rb"));
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            fclose(fdw);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            size = get_file_size(fdr);
        }
        CATCH(-YARCH_EFOPS)
        {
            fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
            fclose(fdw);
            fclose(fdr);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fwrite(&size, sizeof(size), 1, fdw) != 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fwrite()");
            fclose(fdw);
            fclose(fdr);
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
            fclose(fdw);
            fclose(fdr);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fwrite(filename_pure_pos, sizeof(*filename_pure_pos), filename_len, fdw) != filename_len, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fwrite()");
            fclose(fdw);
            fclose(fdr);
            exit(EXIT_FAILURE);
        }

        fclose(fdr);
    }

    TRY
    {
        size = get_file_size(fdw);
    }
    CATCH(-YARCH_EFOPS)
    {
        fprintf(stderr, "Can't get file '%s' size\n", ARCHIEVE_NAME);
        fclose(fdw);
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, 0, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        fclose(fdw);
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fwrite(&size, sizeof(size), 1, fdw) != 1, false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fwrite()");
        fclose(fdw);
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
        fclose(fdw);
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdw, 0, SEEK_END), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        fclose(fdw);
        exit(EXIT_FAILURE);
    }

    char read_c = '\0';
    char prev_c = '\0';
    SIZE_TYPE pattern_size = 0;
    for_each_file
    {
        TRY
        {
            check_fopen_error(fdr = fopen(FILE_NAME, "rb"));
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            fclose(fdw);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            size = get_file_size(fdr);
        }
        CATCH(-YARCH_EFOPS)
        {
            fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
            fclose(fdw);
            fclose(fdr);
            exit(EXIT_FAILURE);
        }

        for (SIZE_TYPE i = 0; i < size; ++i)
        {
            read_c = fgetc(fdr);
            TRY
            {
                if (argv[OPTION_CMD_ARGV_INDEX][1] == 'c')
                {
                    if (i == 0)
                    {
                        prev_c = read_c;
                        continue;
                    }

                    ++pattern_size;

                    if (read_c != prev_c)
                    {
                        check_fops_error(fwrite(&prev_c, sizeof(prev_c), 1, fdw) != 1, false, YARCH_SUCCESS);
                        check_fops_error(fwrite(&pattern_size, sizeof(pattern_size), 1, fdw) != 1, false, YARCH_SUCCESS);
                        pattern_size = 0;
                    }

                    prev_c = read_c;
                }
                else
                {
                    check_fops_error(fwrite(&read_c, sizeof(read_c), 1, fdw) != 1, false, YARCH_SUCCESS);
                }
            }
            CATCH(-YARCH_EFOPS)
            {
                perror("fwrite()");
                fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
                fclose(fdw);
                fclose(fdr);
                exit(EXIT_FAILURE);
            }
        }

        if (argv[OPTION_CMD_ARGV_INDEX][1] == 'c')
        {
            ++pattern_size;
            TRY
            {
                check_fops_error(fwrite(&prev_c, sizeof(prev_c), 1, fdw) != 1, false, YARCH_SUCCESS);
                check_fops_error(fwrite(&pattern_size, sizeof(pattern_size), 1, fdw) != 1, false, YARCH_SUCCESS);
            }
            CATCH(-YARCH_EFOPS)
            {
                perror("fwrite()");
                fprintf(stderr, "Can't get file '%s' size\n", FILE_NAME);
                fclose(fdw);
                fclose(fdr);
                exit(EXIT_FAILURE);
            }
        }

        fclose(fdr);
    }

    fclose(fdw);

    printf("The '%s' archive is ready!\n", ARCHIEVE_NAME);
}

void extract(int argc, char *argv[])
{
    char *filename = NULL;

    FILE *fdr_header = fopen(ARCHIEVE_NAME, "rb"), *fdr_payload = fopen(ARCHIEVE_NAME, "rb"), *fdw = NULL;
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

    SIZE_TYPE header_size = 0;
    SIZE_TYPE files_count = 0;
    SIZE_TYPE payload_size = 0;
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
        fclose(fdr_header);
        fclose(fdr_payload);
        exit(EXIT_FAILURE);
    }

    TRY
    {
        check_fops_error(fseek(fdr_payload, header_size, SEEK_SET), false, YARCH_SUCCESS);
    }
    CATCH(-YARCH_EFOPS)
    {
        perror("fseek()");
        fclose(fdr_header);
        fclose(fdr_payload);
        exit(EXIT_FAILURE);
    }

    size_t extract_path_len = 0;
    char *extract_path = NULL;

    char read_data = '\0';
    SIZE_TYPE pattern_size = 0;

    for (SIZE_TYPE i = 0; i < files_count; ++i)
    {
        TRY
        {
            check_fops_error(fread(&payload_size, sizeof(payload_size), 1, fdr_header) != 1, false, YARCH_SUCCESS);
            check_fops_error(fread(&filename_len, sizeof(filename_len), 1, fdr_header) != 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fread()");
            fclose(fdr_header);
            fclose(fdr_payload);
            exit(EXIT_FAILURE);
        }
        ++filename_len;

        filename = (char *)malloc(filename_len * sizeof(*filename));
        if (filename == NULL)
        {
            perror("malloc()");
            fclose(fdr_header);
            fclose(fdr_payload);
            exit(EXIT_FAILURE);
        }

        TRY
        {
            check_fops_error(fread(filename, sizeof(*filename), filename_len - 1, fdr_header) != filename_len - 1, false, YARCH_SUCCESS);
        }
        CATCH(-YARCH_EFOPS)
        {
            perror("fread()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            exit(EXIT_FAILURE);
        }

        filename[filename_len - 1] = '\0';

        extract_path_len = strnlen(EXTRACT_PATH, MAX_EXTRACT_PATH);
        if (extract_path_len == 0 || extract_path_len == MAX_EXTRACT_PATH)
        {
            perror("strnlen()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            exit(EXIT_FAILURE);
        }

        extract_path = (char *)malloc((extract_path_len + 1) * sizeof(*extract_path));
        if (extract_path == NULL)
        {
            perror("malloc()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            exit(EXIT_FAILURE);
        }

        if (!memcpy(extract_path, EXTRACT_PATH, extract_path_len))
        {
            perror("memcpy()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            free(extract_path);
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

        full_filename = (char *)malloc(filename_len * sizeof(*full_filename));
        if (full_filename == NULL)
        {
            perror("malloc()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            free(extract_path);
            exit(EXIT_FAILURE);
        }

        if (!memcpy(full_filename, extract_path, extract_path_len) || !memcpy(full_filename + extract_path_len, filename, filename_len - extract_path_len))
        {
            perror("memcpy()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(filename);
            free(extract_path);
            free(full_filename);
            exit(EXIT_FAILURE);
        }
        free(filename);
        free(extract_path);

        printf("Extracting '%s'...\n", full_filename);

        TRY
        {
            check_fopen_error(fdw = fopen(full_filename, "wb"));
        }
        CATCH(-YARCH_EOPEN)
        {
            perror("fopen()");
            fclose(fdr_header);
            fclose(fdr_payload);
            free(full_filename);
            exit(EXIT_FAILURE);
        }

        if (argv[OPTION_CMD_ARGV_INDEX][1] == 'c')
        {
            for (SIZE_TYPE size_i = 0; size_i < payload_size; ++size_i)
            {
                TRY
                {
                    check_fops_error(fread(&read_data, sizeof(read_data), 1, fdr_payload) != 1, false, YARCH_SUCCESS);
                    check_fops_error(fread(&pattern_size, sizeof(pattern_size), 1, fdr_payload) != 1, false, YARCH_SUCCESS);
                    printf("%c|%ld|%lu\n", read_data, pattern_size, ftell(fdr_payload));
                }
                CATCH(-YARCH_EFOPS)
                {
                    perror("fread()");
                    fclose(fdr_header);
                    fclose(fdr_payload);
                    free(full_filename);
                    fclose(fdw);
                    exit(EXIT_FAILURE);
                }

                TRY
                {
                    for (SIZE_TYPE pattern_i = 0; pattern_i < pattern_size; ++pattern_i)
                    {
                        check_fops_error(fwrite(&read_data, sizeof(read_data), 1, fdw) != 1, false, YARCH_SUCCESS);
                    }
                }
                CATCH(-YARCH_EFOPS)
                {
                    perror("fwrite()");
                    fclose(fdr_header);
                    fclose(fdr_payload);
                    free(full_filename);
                    fclose(fdw);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else
        {
            chunk_size = PAYLOAD_CHUNK_SIZE;

            payload = (char *)malloc(chunk_size * sizeof(*payload));
            if (payload == NULL)
            {
                perror("malloc()");
                fclose(fdr_header);
                fclose(fdr_payload);
                free(full_filename);
                fclose(fdw);
                exit(EXIT_FAILURE);
            }

            for (SIZE_TYPE i = 1; i <= payload_size / PAYLOAD_CHUNK_SIZE + 1; ++i)
            {
                if (i * chunk_size > payload_size)
                {
                    chunk_size = payload_size - (i - 1) * chunk_size;

                    if (chunk_size == 0)
                    {
                        break;
                    }

                    payload = (char *)realloc(payload, chunk_size * sizeof(*payload));
                    if (payload == NULL)
                    {
                        perror("realloc()");
                        fclose(fdr_header);
                        fclose(fdr_payload);
                        free(full_filename);
                        fclose(fdw);
                        free(payload);
                        exit(EXIT_FAILURE);
                    }

                    TRY
                    {
                        check_fops_error(fread(payload, sizeof(*payload), chunk_size, fdr_payload) != chunk_size, false, YARCH_SUCCESS);
                    }
                    CATCH(-YARCH_EFOPS)
                    {
                        perror("fread()");
                        fclose(fdr_header);
                        fclose(fdr_payload);
                        free(full_filename);
                        fclose(fdw);
                        free(payload);
                        exit(EXIT_FAILURE);
                    }

                    TRY
                    {
                        check_fops_error(fwrite(payload, sizeof(*payload), chunk_size, fdw) != chunk_size, false, YARCH_SUCCESS);
                    }
                    CATCH(-YARCH_EFOPS)
                    {
                        perror("fwrite()");
                        fclose(fdr_header);
                        fclose(fdr_payload);
                        free(full_filename);
                        fclose(fdw);
                        free(payload);
                        exit(EXIT_FAILURE);
                    }
                }
            }

            free(payload);
        }

        fclose(fdw);
        free(full_filename);
    }

    fclose(fdr_header);
    fclose(fdr_payload);

    printf("The files from the '%s' archive have been extracted!\n", ARCHIEVE_NAME);
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage:\n");
        printf("  --compress-- %s c<c> <archieve_name> <file_1> <file_2> ...\n", argv[0]);
        printf("  --extract-- %s x<c> <archieve_name> <path>\n", argv[0]);
        exit(YARCH_EARGS);
    }

    if (argv[OPTION_CMD_ARGV_INDEX][0] == 'c')
    {
        compress(argc, argv);
    }
    else if (argv[OPTION_CMD_ARGV_INDEX][0] == 'x')
    {
        extract(argc, argv);
    }
    else
    {
        printf("Unknown option '%c'\n", argv[OPTION_CMD_ARGV_INDEX][0]);
    }

    return EXIT_SUCCESS;
}