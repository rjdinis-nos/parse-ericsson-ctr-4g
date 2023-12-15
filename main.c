#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

const char *filepath = "A20231207.1715+0000-1730+0000_SubNetwork=ONRM_ROOT_MO,SubNetwork=RAN,SubNetwork=NODE,SubNetwork=AV_AVEIRO,MeContext=NAV002B2,ManagedElement=NAV002B2_celltracefile_DUL1_1.bin";

enum RecordType
{
    HEADER,
    SCANNER,
    EVENT,
    FOOTER
};

typedef struct RecordLenType
{
    uint16_t length;
    uint16_t type;
} RecordLenType;

typedef struct CTRHeader
{
    uint16_t length;
    uint16_t type;
    uint8_t version[5];
    uint8_t pm_version[13];
    uint8_t pm_revision[5];
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t ne_user_label[128];
    uint8_t ne_logical_label[255];
    void *next;
} CTRHeader;

typedef struct CTRScanner
{
    uint16_t length;
    uint16_t type;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint16_t milisecond;
    uint8_t scannerid[3];
    uint8_t status[1];
    uint8_t padding[3];
    void *next;
} CTRScanner;

typedef struct CTREvent
{
    uint16_t length;
    uint16_t type;
    int id;
    uint8_t parameters[255];
    void *next;
} CTREvent;

typedef struct CTRFooter
{
    uint16_t length;
    uint16_t type;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t padding[1];
    void *next;
} CTRFooter;

/* CHAR_BIT == 8 assumed */
uint16_t le16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[0]) | (((uint16_t)buf[1]) << 8);
}
uint16_t be16_to_cpu(const uint8_t *buf)
{
    return ((uint16_t)buf[1]) | (((uint16_t)buf[0]) << 8);
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

int read_header(CTRHeader *hdr, uint16_t len, FILE *fp)
{
    hdr->length = len;
    hdr->type = 0;

    uint8_t buf[len - 4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    for (int i = 0; i <= 4; i++)
    {
        hdr->version[0 + i] = buf[i];
    }

    for (int i = 0; i <= 12; i++)
    {
        hdr->pm_version[0 + i] = buf[5 + i];
    }

    for (int i = 0; i <= 4; i++)
    {
        hdr->pm_revision[0 + i] = buf[18 + i];
    }

    hdr->year = be16_to_cpu(buf + 23);
    hdr->month = (uint8_t)buf[25];
    hdr->day = (uint8_t)buf[26];
    hdr->hour = (uint8_t)buf[27];
    hdr->minute = (uint8_t)buf[28];
    hdr->second = (uint8_t)buf[29];

    for (int i = 0; i <= 128; i++)
    {
        hdr->ne_user_label[0 + i] = buf[30 + i];
    }

    for (int i = 0; i <= 255; i++)
    {
        hdr->ne_logical_label[0 + i] = buf[158 + i];
    }

    return 0;
}

int read_scanner(CTRScanner *scan, uint16_t len, FILE *fp)
{
    scan->length = len;
    scan->type = 3;

    uint8_t buf[len - 4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    scan->hour = (uint8_t)buf[0];
    scan->minute = (uint8_t)buf[1];
    scan->second = (uint8_t)buf[2];
    scan->milisecond = (uint16_t)buf[3];

    for (int i = 0; i <= 2; i++)
    {
        scan->scannerid[0 + i] = buf[5 + i];
    }

    scan->status[0] = buf[8];

    for (int i = 0; i <= 2; i++)
    {
        scan->padding[0 + i] = buf[9 + i];
    }

    return 0;
}

int read_event(CTREvent *event, uint16_t len, FILE *fp)
{
    event->length = len;
    event->type = 4;

    uint8_t buf[len - 4];

    if (fread(buf, 1, sizeof buf, fp) != sizeof buf)
        return -1;

    event->id = buf[2] | buf[1] << 8 | buf[0] << 16;

    for (int i = 0; i <= len - 4 - 3; i++)
    {
        event->parameters[0 + i] = buf[3 + i];
    }

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

void print_header(CTRHeader *hdr)
{
    printf("Header\n");
    printf("-----------------------\n");
    printf("Record Lenght: %d\n", hdr->length);
    printf("Record Type: %d\n", hdr->type);
    printf("File Format Version: %s\n", hdr->version);
    printf("PM recording version: %s\n", hdr->pm_version);
    printf("PM recording revision: %s\n", hdr->pm_revision);
    printf("Year: %d\n", hdr->year);
    printf("Month: %d\n", hdr->month);
    printf("Day: %d\n", hdr->day);
    printf("Hour: %d\n", hdr->hour);
    printf("Minute: %d\n", hdr->minute);
    printf("Second: %d\n", hdr->second);
    printf("NE user label: %s\n", hdr->ne_user_label);
    printf("NE logical label: %s\n", hdr->ne_logical_label);
}

void print_scanner(CTRScanner *scan)
{
    printf("\n");
    printf("Scanner\n");
    printf("-----------------------\n");
    printf("Record Lenght: %d\n", scan->length);
    printf("Record Type: %d\n", scan->type);
    printf("Hour: %d\n", scan->hour);
    printf("Minute: %d\n", scan->minute);
    printf("Second: %d\n", scan->second);
    printf("Milisecond: %d\n", scan->milisecond);
    printf("Scannerid: 0x%x 0x%x 0x%x\n", scan->scannerid[0], scan->scannerid[1], scan->scannerid[2]);
    printf("Status: 0x%x\n", scan->status[0]);
    printf("Padding Bytes: 0x%x 0x%x 0x%x\n", scan->padding[0], scan->padding[1], scan->padding[2]);
}

void print_event(CTREvent *event)
{
    printf("\n");
    printf("Event\n");
    printf("-----------------------\n");
    printf("Record Lenght: %d\n", event->length);
    printf("Record Type: %d\n", event->type);
    printf("Event ID: %d\n", event->id);
    printf("Event parameter part: ");
    for (int i = 0; i <= event->length - 4 - 3; i++)
    {
        printf("0x%x ", event->parameters[i]);
    }
    printf("\n");
}

int main(void)
{
    FILE *file;
    file = fopen(filepath, "rb");
    if (file == NULL)
    {
        printf("Error while opening the file.\n");
        exit(0);
    }
    fseek(file, 0L, SEEK_END);
    int file_len = ftell(file);
    fseek(file, 0L, SEEK_SET);

    CTRHeader header = {0};
    CTRScanner scanner = {0};
    CTREvent event = {0};
    CTRFooter footer = {0};

    int num_records = 0;
    while (file_len > 0 && num_records < 10)
    {
        uint16_t record_lenght = 0;
        uint16_t record_type = 255;
        read_record_len_type(&record_lenght, &record_type, file);

        switch (record_type)
        {
        case 0:
            read_header(&header, record_lenght, file);
            print_header(&header);
            break;
        case 3:
            read_scanner(&scanner, record_lenght, file);
            print_scanner(&scanner);
            break;
        case 4:
            read_event(&event, record_lenght, file);
            print_event(&event);
            break;
        case 5:
            /* read_footer(&footer, record_lenght, file);
            print_footer(&footer); */
            break;
        default:
            printf("Record type not known");
            break;
        }

        file_len = file_len - record_lenght;
        num_records++;
    }

    fclose(file);

    return 0;
}