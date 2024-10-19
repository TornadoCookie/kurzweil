#include "cfbf.h"
#include <endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <string.h>

struct __attribute__((packed)) StructuredStorageHeader { // [offset from start (bytes), length (bytes)]
    unsigned char _abSig[8];             // [00H,08] {0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1,
                                // 0x1a, 0xe1} for current version
    unsigned char _clsid[16];               // [08H,16] reserved must be zero (WriteClassStg/
                                // GetClassFile uses root directory class id)
    unsigned short _uMinorVersion;      // [18H,02] minor version of the format: 33 is
                                // written by reference implementation
    unsigned short _uDllVersion;        // [1AH,02] major version of the dll/format: 3 for
                                // 512-byte sectors, 4 for 4 KB sectors
    unsigned short _uByteOrder;         // [1CH,02] 0xFFFE: indicates Intel byte-ordering
    unsigned short _uSectorShift;       // [1EH,02] size of sectors in power-of-two;
                                // typically 9 indicating 512-byte sectors
    unsigned short _uMiniSectorShift;   // [20H,02] size of mini-sectors in power-of-two;
                                // typically 6 indicating 64-byte mini-sectors
    unsigned short _usReserved;         // [22H,02] reserved, must be zero
    unsigned int _ulReserved1;         // [24H,04] reserved, must be zero
    unsigned int _csectDir;          // [28H,04] must be zero for 512-byte sectors,
                                // number of SECTs in directory chain for 4 KB
                                // sectors
    unsigned int _csectFat;          // [2CH,04] number of SECTs in the FAT chain
    unsigned int _sectDirStart;         // [30H,04] first SECT in the directory chain
    unsigned int _signature;     // [34H,04] signature used for transactions; must
                                // be zero. The reference implementation
                                // does not support transactions
    unsigned int _ulMiniSectorCutoff;  // [38H,04] maximum size for a mini stream;
                                // typically 4096 bytes
    unsigned int _sectMiniFatStart;     // [3CH,04] first SECT in the MiniFAT chain
    unsigned int _csectMiniFat;      // [40H,04] number of SECTs in the MiniFAT chain
    unsigned int _sectDifStart;         // [44H,04] first SECT in the DIFAT chain
    unsigned int _csectDif;          // [48H,04] number of SECTs in the DIFAT chain
    unsigned int _sectFat[109];         // [4CH,436] the SECTs of first 109 FAT sectors
};

struct __attribute__((packed)) DirectoryEntry {
    uint16_t entry_name[32]; // 64 bytes
    uint16_t entry_name_length;
    uint8_t object_type;
    uint8_t color_flag;
    uint32_t left_sibling;
    uint32_t right_sibling;
    uint32_t child;
    char clsid[16];
    uint32_t state_bits;
    uint64_t creation_time;
    uint64_t modified_time;
    uint32_t starting_sector_location;
    uint64_t stream_size;
};

// <RAYLIB>

#include <stdbool.h>
#include <stdarg.h>

#define SUPPORT_STANDARD_FILEIO
#define SUPPORT_TRACELOG
#define TRACELOG TraceLog
#define MAX_TRACELOG_MSG_LENGTH     256         // Max length of one trace-log message

// Trace log level
// NOTE: Organized by priority level
typedef enum {
    LOG_ALL = 0,        // Display all logs
    LOG_TRACE,          // Trace logging, intended for internal use only
    LOG_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    LOG_INFO,           // Info logging, used for program execution info
    LOG_WARNING,        // Warning logging, used on recoverable failures
    LOG_ERROR,          // Error logging, used on unrecoverable failures
    LOG_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    LOG_NONE            // Disable logging
} TraceLogLevel;

static int logTypeLevel = LOG_INFO;                 // Minimum log type level

// Show trace log messages (LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_DEBUG)
void TraceLog(int logType, const char *text, ...)
{
#if defined(SUPPORT_TRACELOG)
    // Message has level below current threshold, don't emit
    if (logType < logTypeLevel) return;

    va_list args;
    va_start(args, text);

    //if (traceLog)
    //{
    //    traceLog(logType, text, args);
    //    va_end(args);
    //    return;
    //}

#if defined(PLATFORM_ANDROID)
    switch (logType)
    {
        case LOG_TRACE: __android_log_vprint(ANDROID_LOG_VERBOSE, "raylib", text, args); break;
        case LOG_DEBUG: __android_log_vprint(ANDROID_LOG_DEBUG, "raylib", text, args); break;
        case LOG_INFO: __android_log_vprint(ANDROID_LOG_INFO, "raylib", text, args); break;
        case LOG_WARNING: __android_log_vprint(ANDROID_LOG_WARN, "raylib", text, args); break;
        case LOG_ERROR: __android_log_vprint(ANDROID_LOG_ERROR, "raylib", text, args); break;
        case LOG_FATAL: __android_log_vprint(ANDROID_LOG_FATAL, "raylib", text, args); break;
        default: break;
    }
#else
    char buffer[MAX_TRACELOG_MSG_LENGTH] = { 0 };

    switch (logType)
    {
        case LOG_TRACE: strcpy(buffer, "TRACE: "); break;
        case LOG_DEBUG: strcpy(buffer, "DEBUG: "); break;
        case LOG_INFO: strcpy(buffer, "INFO: "); break;
        case LOG_WARNING: strcpy(buffer, "WARNING: "); break;
        case LOG_ERROR: strcpy(buffer, "ERROR: "); break;
        case LOG_FATAL: strcpy(buffer, "FATAL: "); break;
        default: break;
    }

    unsigned int textSize = (unsigned int)strlen(text);
    memcpy(buffer + strlen(buffer), text, (textSize < (MAX_TRACELOG_MSG_LENGTH - 12))? textSize : (MAX_TRACELOG_MSG_LENGTH - 12));
    strcat(buffer, "\n");
    vprintf(buffer, args);
    fflush(stdout);
#endif

    va_end(args);

    if (logType == LOG_FATAL) exit(EXIT_FAILURE);  // If fatal logging, exit program

#endif  // SUPPORT_TRACELOG
}

// Save data to file from buffer
bool SaveFileData(const char *fileName, void *data, int dataSize)
{
    bool success = false;

    if (fileName != NULL)
    {
        //if (saveFileData)
        //{
        //    return saveFileData(fileName, data, dataSize);
        //}
#if defined(SUPPORT_STANDARD_FILEIO)
        FILE *file = fopen(fileName, "wb");

        if (file != NULL)
        {
            // WARNING: fwrite() returns a size_t value, usually 'unsigned int' (32bit compilation) and 'unsigned long long' (64bit compilation)
            // and expects a size_t input value but as dataSize is limited to INT_MAX (2147483647 bytes), there shouldn't be a problem
            int count = (int)fwrite(data, sizeof(unsigned char), dataSize, file);

            if (count == 0) TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to write file", fileName);
            else if (count != dataSize) TRACELOG(LOG_WARNING, "FILEIO: [%s] File partially written", fileName);
            else TRACELOG(LOG_INFO, "FILEIO: [%s] File saved successfully", fileName);

            int result = fclose(file);
            if (result == 0) success = true;
        }
        else TRACELOG(LOG_WARNING, "FILEIO: [%s] Failed to open file", fileName);
#else
    TRACELOG(LOG_WARNING, "FILEIO: Standard file io not supported, use custom file callback");
#endif
    }
    else TRACELOG(LOG_WARNING, "FILEIO: File name provided is not valid");

    return success;
}

#ifndef MAX_TEXT_BUFFER_LENGTH
    #define MAX_TEXT_BUFFER_LENGTH              1024        // Size of internal static buffers used on some functions:
                                                            // TextFormat(), TextSubtext(), TextToUpper(), TextToLower(), TextToPascal(), TextSplit()
#endif

// Formatting of text with variables to 'embed'
// WARNING: String returned will expire after this function is called MAX_TEXTFORMAT_BUFFERS times
const char *TextFormat(const char *text, ...)
{
#ifndef MAX_TEXTFORMAT_BUFFERS
    #define MAX_TEXTFORMAT_BUFFERS 4        // Maximum number of static buffers for text formatting
#endif

    // We create an array of buffers so strings don't expire until MAX_TEXTFORMAT_BUFFERS invocations
    static char buffers[MAX_TEXTFORMAT_BUFFERS][MAX_TEXT_BUFFER_LENGTH] = { 0 };
    static int index = 0;

    char *currentBuffer = buffers[index];
    memset(currentBuffer, 0, MAX_TEXT_BUFFER_LENGTH);   // Clear buffer before using

    va_list args;
    va_start(args, text);
    int requiredByteCount = vsnprintf(currentBuffer, MAX_TEXT_BUFFER_LENGTH, text, args);
    va_end(args);

    // If requiredByteCount is larger than the MAX_TEXT_BUFFER_LENGTH, then overflow occured
    if (requiredByteCount >= MAX_TEXT_BUFFER_LENGTH)
    {
        // Inserting "..." at the end of the string to mark as truncated
        char *truncBuffer = buffers[index] + MAX_TEXT_BUFFER_LENGTH - 4; // Adding 4 bytes = "...\0"
        sprintf(truncBuffer, "...");
    }

    index += 1;     // Move to next buffer for next function call
    if (index >= MAX_TEXTFORMAT_BUFFERS) index = 0;

    return currentBuffer;
}

// </RAYLIB>

void set_parent(int *parents, struct DirectoryEntry *dir, int i, int parent, bool fromLeft, bool fromRight)
{
    if (dir[i].left_sibling != 0xFFFFFFFF && !fromLeft)
    {
        parents[dir[i].left_sibling] = parent;
        set_parent(parents, dir, dir[i].left_sibling, parent, false, false);
    }
    if (dir[i].right_sibling != 0xFFFFFFFF && !fromRight)
    {
        parents[dir[i].right_sibling] = parent;
        set_parent(parents, dir, dir[i].right_sibling, parent, false, false);
    }
}

CFBF parse_cfbf(FILE *f)
{
    struct StructuredStorageHeader header;
    CFBF cfbf;

    fread(&header, sizeof(header), 1, f);

    printf("sizeof ul %ld us %ld header %ld\n", sizeof(unsigned long), sizeof(unsigned short), sizeof(header));

    int sectorSize = 1<<header._uSectorShift;
    int mSectorSize = 1<<header._uMiniSectorShift;

    printf("DllVersion: %d\n", header._uDllVersion);
    printf("MinorVersion: %d\n", header._uMinorVersion);
    printf("Byte order: %#X\n", header._uByteOrder);
    printf("Sector Size: %d\n", sectorSize);
    printf("MiniSector Size: %d\n", mSectorSize);
    printf("number of SECTS in the FAT chain: %d\n", header._csectFat);
    printf("sectDirStart: %d\n", header._sectDirStart);
    printf("miniSectorCutoff: %d\n", header._ulMiniSectorCutoff);
    printf("MiniFAT chain first SECT: %d\n", header._sectMiniFatStart);
    printf("MiniFAT chain SECT count: %d\n", header._csectMiniFat);
    printf("DIFAT chain first SECT: %d\n", header._sectDifStart);
    printf("DIFAT chain SECT count: %d\n", header._csectDif);
    
    printf("first 109 SECTS:");
    for (int i = 0; i < 109; i++) printf(" %d", header._sectFat[i]);
    printf("\n");

    uint32_t *fat = malloc(sectorSize * header._csectFat);
    void *fatCur = fat;
    for (int i = 0; i < header._csectFat; i++)
    {
        fseek(f, 512 + header._sectFat[i] * sectorSize, SEEK_SET);
        fread(fatCur, sectorSize, 1, f);
        fatCur += sectorSize;
    }

    printf("FAT sector:\n");
    for (int i = 0; i < (sectorSize / 4) * header._csectFat; i++)
    {
        if (i % (sectorSize/4) == 0) printf("SECTOR BREAK\n");
        printf("%#x: %#x", i, fat[i]);
        switch (fat[i])
        {
            case 0xFFFFFFFF:
            {
                printf(" FREESECT\n");
            } break;
            case 0xFFFFFFFE:
            {
                printf(" ENDOFCHAIN\n");
            } break;
            case 0xFFFFFFFD:
            {
                printf(" FATSECT\n");
            } break;
            case 0xFFFFFFFC:
            {
                printf(" DIFSECT\n");
            } break;
            case 0xFFFFFFFA:
            {
                printf(" MAXREGSECT\n");
            } break;
            default:
            {
                printf("\n");
            } break;
        } 
    }
    printf("\n");

    uint32_t *mfat = malloc(sectorSize * header._csectMiniFat);
    void *mfatCur = mfat;
    int mfat_fat_index = header._sectMiniFatStart;
    int mfat_fat_offset = 512 + mfat_fat_index * sectorSize;
    for (int i = 0; i < header._csectMiniFat; i++)
    {
        printf("mfat off %#x\n", mfat_fat_offset);
        fseek(f, mfat_fat_offset, SEEK_SET);
        fread(mfatCur, sectorSize, 1, f);
        mfatCur += sectorSize;
        mfat_fat_index = fat[mfat_fat_index];
        mfat_fat_offset = 512 + mfat_fat_index * sectorSize;
    }

    printf("MiniFAT sector:\n");
    for (int i = 0; i < (sectorSize / 4) * header._csectMiniFat; i++)
    {
        if (i % (sectorSize/4) == 0) printf("SECTOR BREAK\n");
        printf("%#x: %#x", i, mfat[i]);
        switch (mfat[i])
        {
            case 0xFFFFFFFF:
            {
                printf(" FREESECT\n");
            } break;
            case 0xFFFFFFFE:
            {
                printf(" ENDOFCHAIN\n");
            } break;
            case 0xFFFFFFFD:
            {
                printf(" FATSECT\n");
            } break;
            case 0xFFFFFFFC:
            {
                printf(" DIFSECT\n");
            } break;
            case 0xFFFFFFFA:
            {
                printf(" MAXREGSECT\n");
            } break;
            default:
            {
                printf("\n");
            } break;
        } 
    }
    printf("\n");

    struct DirectoryEntry *dir = NULL;
    int dirCountPerSector = sectorSize / sizeof(struct DirectoryEntry);
    int dirCount = 0;
    int dirSize = 0;
    int dir_fat_index = header._sectDirStart;
    int dir_fat_offset = 512 + dir_fat_index * sectorSize;
    while (dir_fat_index != 0xFFFFFFFE)
    {
        dirCount += dirCountPerSector;
        dirSize += sectorSize;
        dir = realloc(dir, dirSize);
        fseek(f, dir_fat_offset, SEEK_SET);
        printf("sector off %#x\n", dir_fat_offset);
        fread(((void*)dir) + dirSize - sectorSize, sectorSize, 1, f);
        dir_fat_index = fat[dir_fat_index];
        dir_fat_offset = 512 + dir_fat_index * sectorSize;
    }

    char **entryData = malloc(sizeof(char *) * dirCount);
    int *parents = malloc(sizeof(int) * dirCount);
    memset(parents, 0xFF, sizeof(int) * dirCount);
    char **goodNames = malloc(sizeof(char *) * dirCount);

    for (int i = 0; i < dirCount; i++)
    {
        char *name = malloc(dir[i].entry_name_length / 2);
        for (int j = 0; j < dir[i].entry_name_length / 2 - 1; j++)
        {
            name[j] = dir[i].entry_name[j];
        }
        goodNames[i] = name;
    }

    for (int i = 0; i < dirCount; i++)
    {
        if (dir[i].child != -1)
        {
            parents[dir[i].child] = i;
            set_parent(parents, dir, dir[i].child, i, false, false);
        }

        if (dir[i].object_type == 1)
        {
            mkdir(TextFormat("output/%s", goodNames[i]), 0777);
        }
    }
    
    printf("Directory count: %d\n", dirCount);
    for (int i = 0; i < dirCount; i++)
    {
        printf("\nDirectory %d:\n", i);
        printf("name length: %d\n", dir[i].entry_name_length);
        printf("Name: ");
        for (int j = 0; j < dir[i].entry_name_length / 2 - 1; j++)
        {
            printf("%c", dir[i].entry_name[j]);
        }
        printf("\n");
        printf("Object type: %d ", dir[i].object_type);
        switch (dir[i].object_type)
        {
            case 0:
            {
                printf("(Unknown or unallocated)\n");
            } break;
            case 1:
            {
                printf("(Storage Object)\n");
            } break;
            case 2:
            {
                printf("(Stream Object)\n");
            } break;
            case 5:
            {
                printf("(Root Storage Object)\n");
            } break;
        }
        printf("Color flag: %d ", dir[i].color_flag);
        switch (dir[i].color_flag)
        {
            case 0:
            {
                printf("(Red)\n");
            } break;
            case 1:
            {
                printf("(Black)\n");
            } break;
        }
        printf("Left Sibling: %#x\n", dir[i].left_sibling);
        printf("Right sibling: %#x\n", dir[i].right_sibling);
        printf("Child: %#x\n", dir[i].child);
        printf("State Bits: %d\n", dir[i].state_bits);
        printf("Creation time: %ld\n", dir[i].creation_time);
        printf("Modified time: %ld\n", dir[i].modified_time);
        printf("Starting sector: %d\n", dir[i].starting_sector_location);
        printf("Stream size: %ld\n", dir[i].stream_size);

        if (dir[i].object_type == 2)
        {
            if (dir[i].stream_size >= header._ulMiniSectorCutoff)
            {
                printf("We have a stream object stored in FAT.\n");

                char *data = malloc(dir[i].stream_size);
                void *dataCur = data;
                int fat_index = dir[i].starting_sector_location;
                int fat_offset = 512 + fat_index * sectorSize;
                int filled = 0;

                while (filled < dir[i].stream_size)
                {
                    int diff = dir[i].stream_size - filled;
                    printf("sector off %#x, index %#x\n", fat_offset, fat_index);
                    fseek(f, fat_offset, SEEK_SET);
                    fread(dataCur, fmin(sectorSize, diff), 1, f);
                    dataCur += sectorSize;
                    filled += sectorSize;
                    fat_index = fat[fat_index];
                    fat_offset = 512 + fat_index * sectorSize;
                }

                //SaveFileData(TextFormat("output/%d", i), data, filled);
                entryData[i] = data;
            }
            else
            {
                printf("We have a stream object stored in MiniFAT.\n");

                char *data = malloc(dir[i].stream_size);
                void *dataCur = data;
                int fat_index = dir[i].starting_sector_location;
                int fat_offset = 512 + (dir[0].starting_sector_location) * sectorSize + fat_index * mSectorSize;
                int filled = 0;

                while (filled < dir[i].stream_size)
                {
                    int diff = dir[i].stream_size - filled;
                    printf("sector off %#x, index %#x\n", fat_offset, fat_index);
                    fseek(f, fat_offset, SEEK_SET);
                    fread(dataCur, fmin(mSectorSize, diff), 1, f);
                    dataCur += sectorSize;
                    filled += sectorSize;
                    fat_index = mfat[fat_index];
                    fat_offset = 512 + header._sectMiniFatStart * sectorSize + fat_index * mSectorSize;
                }

                entryData[i] = data;
            }
            
            if (parents[i] == -1)
            {
                SaveFileData(TextFormat("output/%s", goodNames[i]), entryData[i], dir[i].stream_size);
            }
            else
            {
                SaveFileData(TextFormat("output/%s/%s", goodNames[parents[i]], goodNames[i]), entryData[i], dir[i].stream_size);
            }
            
        }
    }

    return cfbf;
}
