#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <errno.h>

#define NOB_IMPLEMENTATION
#include "nob.h"

#include "uthash.h"

void usage(const char *program);

#define MAX_RECORDS 1000 * 1000

// Global variables
const char *directory;
const char *filepath;
int n_files;
struct dirent **fileList;

int max_records = MAX_RECORDS;
int dump_flag = false;
int verbose_flag = false;
const char *output_dir = {0};

enum RecordType
{
    HEADER = 0,
    SCANNER = 3,
    EVENT = 4,
    FOOTER = 5
};

typedef struct RecordLenType
{
    uint16_t length;
    uint16_t type;
} RecordLenType;

typedef struct CTRHeader
{
    uint16_t length;
    uint8_t file_name[256];
    uint8_t file_version[6];       //  5 bytes + termination char
    uint8_t pm_version[14];        // 13 bytes + termination char
    uint8_t pm_revision[6];        //  5 bytes + termination char
    uint8_t date_time[20];         //  7 bytes + termination char (yyyy-mm-dd hh:mm:ss)
    uint8_t ne_user_label[129];    // 128 bytes + termination char
    uint8_t ne_logical_label[256]; // 255 bytes + termination char
} CTRHeader;

typedef struct CTRScanner
{
    uint16_t length;
    uint8_t timestamp[13];
    uint8_t scannerid[3];
    uint8_t status[1];
    uint8_t padding[3];
} CTRScanner;

typedef struct CTREvent
{
    uint16_t length;
    int id;
    char name[128];
    uint8_t *parameters;
} CTREvent;

typedef struct CTRFooter
{
    uint16_t length;
    uint8_t date_time[20]; //  7 bytes + termination char (yyyy-mm-dd hh:mm:ss)
    uint8_t padding[1];
} CTRFooter;

typedef struct CTRStruct
{
    struct CTRStruct *next; // Next structure in the linked list
    enum RecordType type;   // Indicates which of the union fields is valid
    union
    {
        struct CTRHeader header;
        struct CTRScanner scanner;
        struct CTREvent event;
        struct CTRFooter footer;
    };
} CTRStruct;

typedef struct PMEvent
{
    int id; /* key */
    char name[128];
    UT_hash_handle hh; /* makes this structure hashable */
} PMEvent;

PMEvent *event_hash = NULL;

void add_pm_Event(int event_id, char *event_name)
{
    struct PMEvent *s;

    s = malloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->id = event_id;
    strcpy(s->name, event_name);
    HASH_ADD_INT(event_hash, id, s); /* id: name of key field */
}

const char *find_pm_event(int id)
{
    struct PMEvent *event = {0};
    char *event_name;

    HASH_FIND_INT(event_hash, &id, event); /* s: output pointer */
    if (event != NULL)
    {
        event_name = event->name;
    }
    else
    {
        event_name = "";
    }
    return event_name;
}

int RecordTypeValid(uint16_t type)
{
    int valid = 0;

    switch (type)
    {
    case HEADER:
    case SCANNER:
    case EVENT:
    case FOOTER:
        valid = 1;
    };

    return valid;
}

static char *shift_args(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

/* when return 1, scandir will put this dirent to the list */
static int parse_ext_bin(const struct dirent *dir)
{
    if (!dir)
        return 0;

    if (dir->d_type == DT_REG) /* only deal with regular file */
    {
        const char *ext = strrchr(dir->d_name, '.');
        if ((!ext) || (ext == dir->d_name))
            return 0;
        else
        {
            if (strcmp(ext, ".bin") == 0)
                return 1;
        }
    }

    return 0;
}

/* CHAR_BIT == 8 assumed */
uint16_t le16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[0]) | (((uint16_t)buf[1]) << 8);
}

uint16_t be16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[1]) | (((uint16_t)buf[0]) << 8);
}

uint32_t be32_to_cpu(const uint8_t *buf)
{
    return ((uint32_t)buf[2] | (uint32_t)buf[1] << 8 | (uint32_t)buf[0] << 16);
}

void cpu_to_le16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val & 0x00FF);
    buf[1] = (val & 0xFF00) >> 8;
}

void cpu_to_be16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val & 0xFF00) >> 8;
    buf[1] = (val & 0x00FF);
}

void scan_string_from_buf(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos, uint16_t size)
{
    for (int i = 0; i < size; i++)
    {
        target_var[i] = buf[*buf_pos + i];
    }
    target_var[size + 1] = '\0';
    *buf_pos = *buf_pos + size;
}

void scan_bytes_from_buf(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos, uint16_t size)
{
    for (int i = 0; i < size; i++)
    {
        target_var[i] = buf[*buf_pos + i];
    }
    *buf_pos = *buf_pos + size;
}

void scan_date_time(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos)
{
    snprintf((char *)target_var, 20,
             "%04d-%02d-%02d %02d:%02d:%02d",
             be16_to_cpu(buf + *buf_pos),
             (uint8_t)buf[*buf_pos + 2],
             (uint8_t)buf[*buf_pos + 3],
             (uint8_t)buf[*buf_pos + 4],
             (uint8_t)buf[*buf_pos + 5],
             (uint8_t)buf[*buf_pos + 6]);
    *buf_pos = *buf_pos + 7;
}

void scan_timestamp(uint8_t *target_var, uint8_t *buf, uint16_t *buf_pos)
{
    snprintf((char *)target_var, 13,
             "%02d:%02d:%02d:%03d",
             (uint8_t)buf[*buf_pos],
             (uint8_t)buf[*buf_pos + 1],
             (uint8_t)buf[*buf_pos + 2],
             be16_to_cpu(buf + *buf_pos + 3));
    *buf_pos = *buf_pos + 5;
}

int get_file_lenght(FILE *fp)
{
    int lenght = 0;
    fseek(fp, 0L, SEEK_END);
    lenght = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    return lenght;
}

int get_file_pos(FILE *fp)
{
    return ftell(fp);
}

int read_header(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = HEADER;
    ptr->header.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_string_from_buf(ptr->header.file_version, buf, &buf_pos, 5);
    scan_string_from_buf(ptr->header.pm_version, buf, &buf_pos, 13);
    scan_string_from_buf(ptr->header.pm_revision, buf, &buf_pos, 5);
    scan_date_time(ptr->header.date_time, buf, &buf_pos);
    scan_string_from_buf(ptr->header.ne_user_label, buf, &buf_pos, 128);
    scan_string_from_buf(ptr->header.ne_logical_label, buf, &buf_pos, 255);

    return EXIT_SUCCESS;
}

int read_scanner(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = SCANNER;
    ptr->scanner.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_timestamp(ptr->scanner.timestamp, buf, &buf_pos);
    scan_bytes_from_buf(ptr->scanner.scannerid, buf, &buf_pos, 3);
    scan_bytes_from_buf(ptr->scanner.status, buf, &buf_pos, 1);
    scan_bytes_from_buf(ptr->scanner.padding, buf, &buf_pos, 2);

    return EXIT_SUCCESS;
}

int read_event(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = EVENT;
    ptr->event.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    ptr->event.id = be32_to_cpu(buf);
    buf_pos = buf_pos + 3;

    const char *event_name = find_pm_event(ptr->event.id);
    strcpy(ptr->event.name, event_name);

    int event_parameter_size = len - buf_pos - 4;
    uint8_t *event_parameters = malloc(event_parameter_size);
    scan_bytes_from_buf(event_parameters, buf, &buf_pos, len - buf_pos - 4);
    ptr->event.parameters = event_parameters;

    return EXIT_SUCCESS;
}

int read_footer(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = FOOTER;
    ptr->event.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file
    memset(buf, 0, len - 4);

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_date_time(ptr->footer.date_time, buf, &buf_pos);
    scan_bytes_from_buf(ptr->footer.padding, buf, &buf_pos, 1);

    return EXIT_SUCCESS;
}

void read_record_len_type(uint16_t *len, uint16_t *type, FILE *fp)
{
    uint8_t buf[4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
    {
        printf("ERROR: Reading from file\n");
        exit(EXIT_FAILURE);
    }

    *len = be16_to_cpu(buf);
    if (len <= 0)
    {
        printf("ERROR: Record lenght '%hn' not valid\n", len);
        exit(EXIT_FAILURE);
    }

    *type = be16_to_cpu(buf + 2);
    if (RecordTypeValid(*type) != 1)
    {
        printf("ERROR: Record type '%hn' not known\n", type);
        exit(EXIT_FAILURE);
    }
}

CTRStruct *add_record(uint16_t type, uint16_t lenght, FILE *fp)
{
    CTRStruct *new_node = malloc(sizeof(CTRStruct));
    memset(new_node, 0, sizeof(CTRStruct));

    switch (type)
    {
    case HEADER:
        read_header(new_node, lenght, fp);
        return new_node;
    case SCANNER:
        read_scanner(new_node, lenght, fp);
        return new_node;
    case EVENT:
        read_event(new_node, lenght, fp);
        return new_node;
    case FOOTER:
        read_footer(new_node, lenght, fp);
        return new_node;
    default:
        printf("Record type not known");
        return NULL;
    }
}

void print_header(CTRStruct *ptr)
{
    printf("\nHeader (%d bytes):\n", ptr->header.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->header.date_time);
    printf("file-name: %s\n", ptr->header.file_name);
    printf("file-format-version: %s\n", ptr->header.file_version);
    printf("pm-recording-version: %s\n", ptr->header.pm_version);
    printf("pm-recording-revision: %s\n", ptr->header.pm_revision);
    printf("ne-user-label: %s\n", ptr->header.ne_user_label);
    printf("ne-logical-name: %s\n", ptr->header.ne_logical_label);
    printf("}\n");
}

void print_scanner(CTRStruct *ptr)
{
    printf("\nScanner (%d bytes):\n", ptr->scanner.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->scanner.timestamp);
    printf("Scannerid: 0x%02x%02x%02x\n", ptr->scanner.scannerid[0], ptr->scanner.scannerid[1], ptr->scanner.scannerid[2]);
    printf("Status: 0x%x\n", ptr->scanner.status[0]);
    printf("Padding Bytes: 0x%02x%02x%02x\n", ptr->scanner.padding[0], ptr->scanner.padding[1], ptr->scanner.padding[2]);
    printf("}\n");
}

void print_event(CTRStruct *ptr)
{
    printf("\nEvent (%d bytes):\n", ptr->event.length);
    printf("{\n");
    if (ptr->event.name)
    {
        printf("Event: %s (%d)\n", ptr->event.name, ptr->event.id);
    }
    else
    {
        printf("Event: %d\n", ptr->event.id);
    }
    printf("Event parameters: ");

    for (int i = 0; i <= ptr->event.length - 4 - 3; i = i + 2)
    {
        printf("%02x%02x ", ptr->event.parameters[i], ptr->event.parameters[i + 1]);
    }
    printf("}\n");
}

void print_footer(CTRStruct *ptr)
{
    printf("\nFooter (%d bytes):\n", ptr->footer.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->footer.date_time);
    printf("Padding Bytes: 0x%02x\n", ptr->scanner.padding[0]);
    printf("}\n");
}

void print_record_info(CTRStruct *ptr, int num_records, uint16_t type, uint16_t lenght)
{
    switch (type)
    {
    case HEADER:
        printf("#%03d %5d bytes HEADER\n", num_records, lenght);
        break;
    case SCANNER:
        printf("#%03d %5d bytes SCANNER\n", num_records, lenght);
        break;
    case EVENT:
        printf("#%03d %5d bytes EVENT -> %s (%d)\n", num_records, lenght, ptr->event.name, ptr->event.id);
        break;
    case FOOTER:
        printf("#%03d %5d bytes FOOTER\n", num_records, lenght);
        break;
    }
}

int dump_records(CTRStruct *node)
{
    if (node == NULL)
        return EXIT_SUCCESS;
    do
    {
        switch (node->type)
        {
        case HEADER:
            print_header(node);
            break;
        case SCANNER:
            print_scanner(node);
            break;
        case EVENT:
            print_event(node);
            break;
        case FOOTER:
            print_footer(node);
            break;
        }
    } while ((node = node->next) != NULL);

    return EXIT_SUCCESS;
}

CTRStruct *parse_events()
{
    CTRStruct *head = NULL;
    CTRStruct *node = NULL;
    CTRStruct *tail = NULL;

    while (n_files--)
    {
        FILE *file;
        char *fullpath = malloc(strlen(directory) + strlen(fileList[n_files]->d_name) + 2); // + 2 because of the '/' and the terminating 0
        sprintf(fullpath, "%s/%s", directory, fileList[n_files]->d_name);
        file = fopen(fullpath, "rb");
        if (file == NULL)
        {
            printf("[ ERR ]: Opening the file %s\n", fullpath);
            exit(EXIT_FAILURE);
        }

        int file_lenght = get_file_lenght(file);
        if (file_lenght > 0)
        {
            if (verbose_flag)
            {
                printf("[ DBG ]: Input file -> %s\n", fullpath);
            }
            printf("[ DBG ]: Processing file with %d bytes\n", file_lenght);
        }
        else
        {
            printf("[ ERR ]: File is empty");
        }

        int num_records = 0;
        while (file_lenght > 0 && num_records < max_records)
        {
            uint16_t record_lenght = 0;
            uint16_t record_type = 255;
            read_record_len_type(&record_lenght, &record_type, file);
            num_records++;

            node = add_record(record_type, record_lenght, file);
            print_record_info(node, num_records, record_type, record_lenght);

            if (record_type == HEADER)
            {
                strcpy(node->header.file_name, fileList[n_files]->d_name);
                head = tail = node;
            }
            else
            {
                tail->next = node;
                tail = node;
            }

            file_lenght = file_lenght - record_lenght;
        }
        printf("DEBUG: Total %d records processed\n", num_records);

        free(fullpath);
        free(fileList[n_files]);
        fclose(file);
    }

    free(fileList);

    return head;
}

bool load_config_from_file(const char *path)
{
    bool result = true;
    String_Builder sb = {0};

    printf("[ CFG ]: Loading configuration from %s\n", path);

    if (!read_entire_file(path, &sb))
        return_defer(false);

    String_View content = {
        .data = sb.items,
        .count = sb.count,
    };

    for (size_t row = 0; content.count > 0; ++row)
    {
        String_View line = sv_trim(sv_chop_by_delim(&content, '\n'));
        if (line.count == 0)
            continue;

        const char *key = temp_sv_to_cstr(sv_trim(sv_chop_by_delim(&line, '=')));
        int value = temp_sv_to_int(sv_trim(line));
        if (verbose_flag)
        {
            printf("[ CFG ]: Add event %s (%d) to hash table\n", key, value);
        }
        add_pm_Event(value, key);
    }

defer:
    sb_free(sb);
    return result;
}

int parse_args(int argc, char **argv)
{
    const char *program = shift_args(&argc, &argv);
    if (argc == 0)
    {
        usage(program);
        exit(EXIT_FAILURE);
    }

    while (argc > 0)
    {
        const char *flag = shift_args(&argc, &argv);
        if (strcmp(flag, "-r") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "ERROR: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            max_records = atoi(shift_args(&argc, &argv));
            printf("[ CFG ]: Max records set to %d\n", max_records);
        }
        else if (strcmp(flag, "-p") == 0)
        {
            dump_flag = true;
            printf("[ CFG ]: Print flag on\n");
        }
        else if (strcmp(flag, "-v") == 0)
        {
            verbose_flag = true;
            printf("[ CFG ]: Verbose flag on\n");
        }
        else if (strcmp(flag, "-i") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            directory = shift_args(&argc, &argv);
            printf("[ CFG ]: Set input directory to '%s'\n", directory);
            n_files = scandir(directory, &fileList, parse_ext_bin, alphasort);
            if (n_files == -1)
            {
                perror("[ DBG ]");
                exit(EXIT_FAILURE);
            }
        }
        else if (strcmp(flag, "-o") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            output_dir = shift_args(&argc, &argv);
        }
        else if (strcmp(flag, "-h") == 0)
        {
            usage(program);
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("[ WRN ]: Unkown flag %s\n", flag);
        }
    }

    if (!fileList || !output_dir)
    {
        if (!fileList)
        {
            printf("\nError: argument -d is mandatory\n");
        }
        else
        {
            printf("\nError: argument -o is mandatory\n");
        }
        usage(program);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

void usage(const char *program)
{
    fprintf(stderr, "Usage: %s [OPTIONS...] [FILES...]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -i <path>     set input directory (mandatory argument)\n");
    fprintf(stderr, "    -o <path>     set output directory (mandatory argument)\n");
    fprintf(stderr, "    -r <int>      set max number of records to be parsed (0 - unlimited; 10 - default)\n");
    fprintf(stderr, "    -p            print record content to stdout (default off)\n");
    fprintf(stderr, "    -v            set verbose\n");
    fprintf(stderr, "    -h            print usage and exit\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "    $ %s -r 0 -d sample_files -o output\n", program);
    fprintf(stderr, "    Parse file1.bin.\n");
    fprintf(stderr, "    Set max records to unlimited.\n");
    fprintf(stderr, "    Print record contents to stdout.\n");
}

int main(int argc, char **argv)
{
    CTRStruct *head = NULL;

    if (!load_config_from_file("config/event_ids.ini"))
        exit(EXIT_FAILURE);

    parse_args(argc, argv);
    head = parse_events();

    if (dump_flag == true)
        dump_records(head);

    return EXIT_SUCCESS;
}