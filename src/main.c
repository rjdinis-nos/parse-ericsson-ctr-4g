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

#define UTILS_IMPLEMENTATION
#include "utils.h"

#include "uthash.h"

void usage(const char *program);

#define MAX_RECORDS 1000 * 1000

// Global variables
int max_records = MAX_RECORDS;
int list_records_flag = false;
int dump_records_flag = false;
int verbose_flag = false;

const char *input_dir = {0};
const char *output_dir = {0};

const char PmEventParams_filepath[] = "config/PmEventParams.cfg";

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
    int file_id;
    int record_id;
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

typedef struct ParamsList
{
    int id;
    char name[256];
    bool unavailable_flag;
    struct EventParams *next;
} ParamsList;

typedef struct EventConfig
{
    int id; /* key */
    char name[128];
    char type[128];
    struct ParamsList params;
    struct EventConfig *next;
    UT_hash_handle hh; /* makes this structure hashable */
} EventConfig;

EventConfig *event_hash = NULL;

EventConfig *add_pm_Event(int event_id, const char *event_name, const char *event_type)
{
    struct EventConfig *s;

    s = malloc(sizeof *s);
    memset(s, 0, sizeof *s);
    s->id = event_id;
    strcpy(s->name, event_name);
    strcpy(s->type, event_type);
    HASH_ADD_INT(event_hash, id, s); /* id: name of key field */

    return s;
}

const char *find_pm_event(int id)
{
    struct EventConfig *event = {0};
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

CTRStruct *add_record(int id, uint16_t type, uint16_t lenght, FILE *fp, int file_id)
{
    CTRStruct *new_node = malloc(sizeof(CTRStruct));
    memset(new_node, 0, sizeof(CTRStruct));

    new_node->file_id = file_id;
    new_node->record_id = id;

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
    printf("file-id: %d\n", ptr->file_id);
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
    printf("file-id: %d\n", ptr->file_id);
    printf("Scannerid: 0x%02x%02x%02x\n", ptr->scanner.scannerid[0], ptr->scanner.scannerid[1], ptr->scanner.scannerid[2]);
    printf("Status: 0x%x\n", ptr->scanner.status[0]);
    printf("Padding Bytes: 0x%02x%02x%02x\n", ptr->scanner.padding[0], ptr->scanner.padding[1], ptr->scanner.padding[2]);
    printf("}\n");
}

void print_event(CTRStruct *ptr)
{
    printf("\nEvent (%d bytes):\n", ptr->event.length);
    printf("{\n");
    printf("file-id: %d\n", ptr->file_id);
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
    printf("file-id: %d\n", ptr->file_id);
    printf("Padding Bytes: 0x%02x\n", ptr->scanner.padding[0]);
    printf("}\n");
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

int list_records(CTRStruct *node)
{
    if (node == NULL)
        return EXIT_SUCCESS;
    printf("\nRecords:\n");
    printf("------------------------------------------------------------------------\n");
    printf("%3s %3s %6s %5s           %9s\n", "File_Id", "Id", "Bytes", "Type", "Event(Id)");

    do
    {
        switch (node->type)
        {
        case HEADER:
            printf("%6d  %3d  %5d  %-7s\n", node->file_id, node->record_id, node->header.length, "HEADER");
            break;
        case SCANNER:
            printf("%6d  %3d  %5d  %-7s\n", node->file_id, node->record_id, node->header.length, "SCANNER");
            break;
        case EVENT:
            printf("%6d  %3d  %5d  %-7s   %s (%d)\n", node->file_id, node->record_id, node->header.length, "EVENT", node->event.name, node->event.id);
            break;
        case FOOTER:
            printf("%6d  %3d  %5d  %-7s\n", node->file_id, node->record_id, node->header.length, "FOOTER");
            break;
        }
    } while ((node = node->next) != NULL);

    return EXIT_SUCCESS;
}

int print_records_csv(CTRStruct *node, const char *path, char *mode)
{
    bool result = true;

    FILE *f = fopen(path, mode);
    if (f == NULL)
    {
        printf("[ ERR ]: Could not open file %s for writing: %s\n", path, strerror(errno));
        nob_return_defer(false);
    }

    if (node == NULL)
        nob_return_defer(false);

    if (strcmp(mode, "w") == 0)
        fprintf(f, "File_Id,Event_Name,Event_Size_bytes,Event_Id,Event_name\n");

    do
    {
        switch (node->type)
        {
        case HEADER:
            fprintf(f, "%d,%s,%d\n", node->file_id, "HEADER", node->header.length);
            break;
        case SCANNER:
            fprintf(f, "%d,%s,%d\n", node->file_id, "SCANNER", node->scanner.length);
            break;
        case EVENT:
            fprintf(f, "%d,%s,%d,%d,%s\n", node->file_id, "EVENT", node->event.length, node->event.id, node->event.name);
            break;
        case FOOTER:
            fprintf(f, "%d,%s,%d\n", node->file_id, "FOOTER", node->footer.length);
            break;
        }
    } while ((node = node->next) != NULL);

defer:
    if (f)
        fclose(f);
    return result;
}

int print_files_csv(CTRStruct *node, const char *path, char *mode)
{
    bool result = true;

    FILE *f = fopen(path, mode);
    if (f == NULL)
    {
        printf("[ ERR ]: Could not open file %s for writing: %s\n", path, strerror(errno));
        nob_return_defer(false);
    }

    if (node == NULL)
        nob_return_defer(false);

    if (strcmp(mode, "w") == 0)
        fprintf(f, "id, filename\n");

    fprintf(f, "%d,%s\n", node->file_id, node->header.file_name);

defer:
    if (f)
        fclose(f);
    return result;
}

CTRStruct *parse_events()
{
    CTRStruct *head = NULL;
    CTRStruct *node = NULL;
    CTRStruct *tail = NULL;

    struct dirent **fileList;

    int n_files = scandir(input_dir, &fileList, parse_ext_bin, alphasort);
    if (n_files == -1)
    {
        perror("[ DBG ]");
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("\nParsing: (%d files)\n", n_files);
        printf("------------------------------------------------------------------------\n");
    }

    int files_processed = 0;
    while (n_files--)
    {
        FILE *file;
        char *fullpath = malloc(strlen(input_dir) + strlen(fileList[n_files]->d_name) + 2); // + 2 because of the '/' and the terminating 0
        sprintf(fullpath, "%s/%s", input_dir, fileList[n_files]->d_name);
        file = fopen(fullpath, "rb");
        if (file == NULL)
        {
            printf("[ ERR ]: Opening the file %s\n", fullpath);
            exit(EXIT_FAILURE);
        }
        files_processed++;

        int file_lenght = get_file_lenght(file);
        if (file_lenght > 0)
        {
            printf("[ INF ]: File #%03d:  %s\n", files_processed, fullpath);
            printf("[ INF ]: File #%03d:  Size - %s\n", files_processed, calculateSize(file_lenght));
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

            node = add_record(num_records, record_type, record_lenght, file, files_processed);

            if (record_type == HEADER)
            {
                head = tail = node;
                strcpy((char *)node->header.file_name, fileList[n_files]->d_name);
            }
            else
            {
                tail->next = node;
                tail = node;
            }

            file_lenght = file_lenght - record_lenght;
        }
        printf("[ INF ]: File #%03d:  Records - %d processed\n", files_processed, num_records);

        if (list_records_flag == true)
        {
            list_records(head);
        }

        char *mode = (files_processed == 1) ? "w" : "a";

        char *files_path = malloc(strlen(output_dir) + strlen("files.csv") + 2); // + 2 because of the '/' and the terminating 0
        sprintf(files_path, "%s/%s", output_dir, "files.csv");
        print_files_csv(head, files_path, mode);

        char *reports_filepath = malloc(strlen(output_dir) + strlen("records.csv") + 2); // + 2 because of the '/' and the terminating 0
        sprintf(reports_filepath, "%s/%s", output_dir, "records.csv");
        print_records_csv(head, reports_filepath, mode);

        if (dump_records_flag == true)
            dump_records(head);

        free(fullpath);
        free(fileList[n_files]);
        fclose(file);
    }

    free(fileList);

    return head;
}

EventConfig *load_event_format_config(const char *path, EventConfig *head)
{
    bool result = true;
    Nob_String_Builder sb = {0};

    printf("[ CFG ]: Loading configuration from %s\n", path);

    if (!nob_read_entire_file(path, &sb))
        nob_return_defer(false);

    Nob_String_View content = {
        .data = sb.items,
        .count = sb.count,
    };

    int row;
    for (row = 0; content.count > 0; ++row)
    {
        Nob_String_View line = nob_sv_trim(nob_sv_chop_by_delim(&content, '\n'));
        if (line.count == 0)
            continue;

        const char *name = nob_temp_sv_to_cstr(nob_sv_trim(nob_sv_chop_by_delim(&line, ' ')));
        int id = nob_temp_sv_to_int(nob_sv_trim(nob_sv_chop_by_delim(&line, ' ')));
        const char *param = nob_temp_sv_to_cstr(nob_sv_trim(nob_sv_chop_by_delim(&line, ' ')));
        const char *flag = nob_temp_sv_to_cstr(nob_sv_trim(line));

        if (verbose_flag)
        {
            printf("[ CFG ]: Add parameter %s (%s)\n", param, flag);
        }
    }

defer:
    nob_sb_free(sb);
    return head;
}

EventConfig *load_event_config(const char *path)
{
    bool result = true;
    Nob_String_Builder sb = {0};

    EventConfig *head = NULL;
    EventConfig *node = NULL;
    EventConfig *tail = NULL;

    printf("[ CFG ]: Loading configuration from %s\n", path);

    if (!nob_read_entire_file(path, &sb))
        nob_return_defer(false);

    Nob_String_View content = {
        .data = sb.items,
        .count = sb.count,
    };

    int row;
    for (row = 0; content.count > 0; ++row)
    {
        Nob_String_View line = nob_sv_trim(nob_sv_chop_by_delim(&content, '\n'));
        if (line.count == 0)
            continue;

        const char *name = nob_temp_sv_to_cstr(nob_sv_trim(nob_sv_chop_by_delim(&line, ' ')));
        int id = nob_temp_sv_to_int(nob_sv_trim(nob_sv_chop_by_delim(&line, ' ')));
        const char *type = nob_temp_sv_to_cstr(nob_sv_trim(line));
        if (verbose_flag)
        {
            printf("[ CFG ]: Add event %s (%d)\n", name, id);
        }

        node = add_pm_Event(id, name, type);
        if (row == 0)
        {
            head = tail = node;
        }
        else
        {
            tail->next = node;
            tail = node;
        }
    }
    printf("[ CFG ]: Total %d event ids added to config table\n", row);

defer:
    nob_sb_free(sb);
    return head;
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
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            int max_record_arg = atoi(shift_args(&argc, &argv));
            if (max_record_arg > 0)
                max_records = max_record_arg;

            printf("[ CFG ]: Max records set to %d\n", max_records);
        }
        else if (strcmp(flag, "-l") == 0)
        {
            list_records_flag = true;
            printf("[ CFG ]: list records flag on\n");
        }
        else if (strcmp(flag, "-c") == 0)
        {
            dump_records_flag = true;
            printf("[ CFG ]: Dump record contents flag on\n");
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
            input_dir = shift_args(&argc, &argv);
            printf("[ CFG ]: Set input directory to '%s'\n", input_dir);
        }
        else if (strcmp(flag, "-o") == 0)
        {
            if (argc <= 0)
            {
                fprintf(stderr, "[ ERR ]: no value is provided for %s\n", flag);
                exit(EXIT_FAILURE);
            }
            output_dir = shift_args(&argc, &argv);
            printf("[ CFG ]: Set output directory to '%s'\n", output_dir);
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

    if (!input_dir || !output_dir)
    {
        if (!input_dir)
        {
            printf("\[ ERR ]: argument -i is mandatory\n");
        }
        else
        {
            printf("\n[ ERR ]: argument -o is mandatory\n");
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
    fprintf(stderr, "    -l            print record content to stdout (default off)\n");
    fprintf(stderr, "    -c            print record content to stdout (default off)\n");
    fprintf(stderr, "    -v            set verbose\n");
    fprintf(stderr, "    -h            print usage and exit\n");
    fprintf(stderr, "Example:\n");
    fprintf(stderr, "    $ %s -r 0 -l -i ./input -o ./output\n", program);
    fprintf(stderr, "    list records to stdout.\n");
    fprintf(stderr, "    Set max records to unlimited.\n");
    fprintf(stderr, "    Set input directory to ./input.\n");
    fprintf(stderr, "    Set output directory to ./output.\n");
}

int main(int argc, char **argv)
{
    CTRStruct *ctr_head = NULL;
    EventConfig *config_head = NULL;

    printf("Config:\n");
    printf("------------------------------------------------------------------------\n");

    parse_args(argc, argv);

    config_head = load_event_config(PmEventParams_filepath);
    if (!config_head)
        exit(EXIT_FAILURE);

    // load_event_format_config(PmEventParams_filepath, config_head);

    ctr_head = parse_events();

    return EXIT_SUCCESS;
}