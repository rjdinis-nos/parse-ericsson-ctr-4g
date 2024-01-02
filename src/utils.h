#ifndef UTILS_H_
#define UTILS_H_

#endif // UTILS_IMPLEMENTATION

#ifdef UTILS_IMPLEMENTATION

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

char *calculateSize(off_t size)
{
    char *result = (char *)malloc(sizeof(char) * 20);
    static int GB = 1024 * 1024 * 1024;
    static int MB = 1024 * 1024;
    static int KB = 1024;
    if (size >= GB)
    {
        if (size % GB == 0)
            sprintf(result, "%d GiB", size / GB);
        else
            sprintf(result, "%.1f GiB", (float)size / GB);
    }
    else if (size >= MB)
    {
        if (size % MB == 0)
            sprintf(result, "%d MiB", size / MB);
        else
            sprintf(result, "%.1f MiB", (float)size / MB);
    }
    else
    {
        if (size == 0)
        {
            result[0] = '0';
            result[1] = '\0';
        }
        else
        {
            if (size % KB == 0)
                sprintf(result, "%d KiB", size / KB);
            else
                sprintf(result, "%.1f KiB", (float)size / KB);
        }
    }
    return result;
}

static char *shift_args(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

#endif // UTILS_IMPLEMENTATION