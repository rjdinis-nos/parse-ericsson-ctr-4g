#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

const char *filepath = "sample_files/A20231207.1715+0000-1730+0000_SubNetwork=ONRM_ROOT_MO,SubNetwork=RAN,SubNetwork=NODE,SubNetwork=AV_AVEIRO,MeContext=NAV002B2,ManagedElement=NAV002B2_celltracefile_DUL1_1.bin";

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
    uint8_t file_version[6];       //  5 bytes + termination char
    uint8_t pm_version[14];        // 13 bytes + termination char
    uint8_t pm_revision[6];        //  5 bytes + termination char
    uint8_t date_time[20];         //  7 bytes + termination char (yyyy-mm-dd hh:mm:ss)
    uint8_t ne_user_label[129];    // 128 bytes + termination char
    uint8_t ne_logical_label[256]; // 255 bytes + termination char
    void *next;
} CTRHeader;

typedef struct CTRScanner
{
    uint16_t length;
    uint8_t timestamp[13];
    uint8_t scannerid[3];
    uint8_t status[1];
    uint8_t padding[3];
    void *next;
} CTRScanner;

typedef struct CTREvent
{
    uint16_t length;
    int id;
    uint8_t parameters[255];
    void *next;
} CTREvent;

typedef struct CTRFooter
{
    uint16_t length;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t padding[1];
    void *next;
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

int RecordTypeValid(int type)
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
    snprintf(target_var, 20,
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
    snprintf(target_var, 13,
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

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_string_from_buf(ptr->header.file_version, buf, &buf_pos, 5);
    scan_string_from_buf(ptr->header.pm_version, buf, &buf_pos, 13);
    scan_string_from_buf(ptr->header.pm_revision, buf, &buf_pos, 5);
    scan_date_time(ptr->header.date_time, buf, &buf_pos);
    scan_string_from_buf(ptr->header.ne_user_label, buf, &buf_pos, 128);
    scan_string_from_buf(ptr->header.ne_logical_label, buf, &buf_pos, 255);

    return 0;
}

int read_scanner(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = SCANNER;
    ptr->scanner.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan_timestamp(ptr->scanner.timestamp, buf, &buf_pos);
    scan_bytes_from_buf(ptr->scanner.scannerid, buf, &buf_pos, 3);
    scan_bytes_from_buf(ptr->scanner.status, buf, &buf_pos, 1);
    scan_bytes_from_buf(ptr->scanner.padding, buf, &buf_pos, 2);

    return 0;
}

int read_event(CTRStruct *ptr, uint16_t len, FILE *fp)
{
    ptr->type = EVENT;
    ptr->event.length = len;

    uint16_t buf_pos = 0;
    uint8_t buf[len - 4]; // first 4 bytes of buf (type+length) already read from file

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    // ptr->event.id = buf[2] | buf[1] << 8 | buf[0] << 16;
    ptr->event.id = be32_to_cpu(buf);
    buf_pos = buf_pos + 3;

    scan_bytes_from_buf(ptr->event.parameters, buf, &buf_pos, len - buf_pos - 4);

    return 0;
}

int read_record_len_type(uint16_t *len, uint16_t *type, FILE *fp)
{
    uint8_t buf[4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    *len = be16_to_cpu(buf);
    *type = be16_to_cpu(buf + 2);

    return 0;
}

CTRStruct *add_record(uint16_t type, uint16_t lenght, FILE *fp)
{
    CTRStruct *node = malloc(sizeof(CTRStruct));
    switch (type)
    {
    case HEADER:
        read_header(node, lenght, fp);
        return node;
    case SCANNER:
        read_scanner(node, lenght, fp);
        return node;
    case EVENT:
        read_event(node, lenght, fp);
        return node;
    case FOOTER:
        // read_footer(&footer, record_lenght, file);
        // return node;
    default:
        printf("Record type not known");
        return NULL;
    }

    return node;
}

void print_header(CTRStruct *ptr)
{
    printf("\nHeader (%d bytes):\n", ptr->header.length);
    printf("{\n");
    printf("timestamp: %s\n", ptr->header.date_time);
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
    printf("Record Lenght: %d\n", ptr->event.length);
    printf("Event ID: %d\n", ptr->event.id);
    printf("Event parameter part: ");
    for (int i = 0; i <= ptr->event.length - 4 - 3; i = i + 2)
    {
        printf("%02x%02x ", ptr->event.parameters[i], ptr->event.parameters[i + 1]);
    }
    printf("}\n");
}

void dump_records(CTRStruct *node)
{
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
            // print_footer(node);
            break;
        }
    } while ((node = node->next) != NULL);
}

int main(void)
{
    FILE *file;
    file = fopen(filepath, "rb");
    if (file == NULL)
    {
        printf("ERROR: Opening the file.\n");
        exit(1);
    }

    int file_lenght = get_file_lenght(file);
    if (file_lenght != 0)
    {
        printf("DEBUG: Processing file with %d bytes\n", file_lenght);
    }
    else
    {
        printf("ERROR: File is empty");
    }

    CTRStruct *head = NULL;
    CTRStruct *tail = NULL;
    CTRStruct *node = NULL;

    int num_records = 0;
    while (file_lenght > 0 && num_records < 5)
    {
        uint16_t record_lenght = 0;
        uint16_t record_type = 255;
        read_record_len_type(&record_lenght, &record_type, file);
        if (record_lenght <= 0 || record_lenght > file_lenght)
        {
            printf("ERROR: Record lenght '%d' not valid\n", record_lenght);
            exit(1);
        }
        if (RecordTypeValid(record_type) != 1)
        {
            printf("ERROR: Record type '%d' not known\n", record_type);
            exit(1);
        }
        printf("DEBUG: Record type %d found with %d bytes\n", record_type, record_lenght);

        node = add_record(record_type, record_lenght, file);
        if (node == NULL)
        {
            printf("ERROR: Adding record to linked list.\n");
            exit(1);
        }
        // printf("DEBUG: Current file position, %d\n", get_file_pos(file));

        if (record_type == HEADER)
        {
            head = tail = node;
        }
        else
        {
            tail->next = node;
            tail = node;
        }
        num_records++;
        file_lenght = file_lenght - record_lenght;
    }
    printf("DEBUG: Total %d records processed\n", num_records);

    fclose(file);

    dump_records(head);

    return 0;
}