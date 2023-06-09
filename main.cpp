#include "disasm.h"

enum DMODE {
    DARM,
    DTHUMB
};

enum DARGS_STATUS {
    DARGS_INVALID,
    DARGS_SINGLE,
    DARGS_STDOUT,
    DARGS_FILEOUT
};

struct FILERANGE {
    int start; //starting address
    int end;   //end address
};

struct DARGS {
    u8 *fname_in;
    u8 *fname_out;
    FILERANGE frange;
    DMODE dmode;
    ARMARCH arch;
    u32 code;
};

static int GetFileSize_mine(FILE *fp) {
    /* Return the size of an opened file */
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    rewind(fp);
    return size;
}

static int DisassembleFile(FILE *in, FILE *out, DARGS *dargs) {
    /* Disassemble from a binary file, print to another file */
    int size = GetFileSize_mine(in);
    if (dargs->frange.start > size || dargs->frange.end > size) return 0; //out of range
    if (dargs->frange.end == 0) dargs->frange.end = size;
    size = dargs->frange.end - dargs->frange.start;
    fprintf(out, "Disassembly of %u (0x%X) bytes:\n\n", size, size);

    fseek(in, dargs->frange.start, SEEK_SET);

    if (dargs->dmode == DARM) {
        for (int i = 0; i < size / 4; i++) {
            u8 str[STRING_LENGTH] = {0};
            u32 code = 0;
            fread(&code, 4, 1, in); //read 32 bits
            Disassemble_arm(code, str, dargs->arch);
            fprintf(out, "%08X: %08X %s\n", dargs->frange.start + i * 4, code, str);
        }
    } else //DTHUMB
    {
        for (int i = 0; i < size / 2; i++) {
            u8 str[STRING_LENGTH] = {0};
            u32 code = 0;
            fread(&code, 4, 1, in); //prefetch 32 bits
            if (Disassemble_thumb(code, str, dargs->arch) == SIZE_32) //32-bit
            {
                fprintf(out, "%08X: %08X %s\n", dargs->frange.start + i * 2, code, str);
                i++;
            } else //16-bit
            {
                fseek(in, -2, SEEK_CUR); //go back 2 bytes
                fprintf(out, "%08X: %04X     %s\n", dargs->frange.start + i * 2, code & 0xffff, str);
            }
        }
    }

    fprintf(out, "\n%u unknown instructions.", debug_na_count);
    return 1; //success
}

static void DisassembleSingle(DARGS *dargs) {
    /* Disassemble a single code and prints it to stdout */
    u8 str[STRING_LENGTH] = {0};
    if (dargs->dmode == DARM) {
        Disassemble_arm(dargs->code, str, dargs->arch);
        printf("%08X %s\n", dargs->code, str);
    } else {
        if (Disassemble_thumb(dargs->code, str, dargs->arch) == SIZE_32) //32-bit
        {
            printf("%08X %s\n", dargs->code, str);
        } else //16-bit
        {
            printf("%04X     %s\n", dargs->code & 0xffff, str);
        }
    }
}

static int IsValidPath(u8 *path) {
    /* Check if length of path/filename is */
    u32 hasDotAndEom = 0;
    if (!path || !path[0]) return 0; //needs to be at least one char
    for (u32 i = 1; i < PATH_LENGTH; i++) {
        if (path[i] == '.') hasDotAndEom = 1;
        if (!path[i]) {
            hasDotAndEom++;
            break; //found EOM
        }
    }
    if (hasDotAndEom == 2) return 1;
    return 0;
}

static int IfValidCodeSet(u32 *code, u8 *str) {
    /* Convert string to 32-bit hex value */
    if (strlen(str) > 8) return 0;
    *code = (u32) strtoll(str, NULL, 16);
    return 1;
}

static int IfValidRangeSet(FILERANGE *range, u8 *r) {
    /* Check to see if the range string yields a valid FILERANGE */
    //Acceptable formats:
    //"X-X" //start to end
    //"--X" //unspecified start (default to beginning of file, 0) to end
    //"X--" //start to unspecified end (default to end of file)

    //todo: new acceptable format: <start>:<size>

    u32 start = 0;
    u32 end = 0;
    u32 valid = 0;

    if (!r || !r[0] || r[0] == '/') return 0; //needs to be at least one char, +smart abort for detecting "/a"

    /* Init buffer (memcpy with two limit conditions) */
    u8 str[RANGE_LENGTH] = {0};
    u32 i = 0; //count chars
    while (r[i] && (i < RANGE_LENGTH - 1)) //must be null terminated
    {
        str[i] = r[i];
        i++;
    }

    switch (*(u16 *) str) {
        case 0x2d2d: //-- detect double dash
        case 0x3a2d: //-: detect size
        {
            end = strtol(&str[2], NULL, 16);
            valid = 1;
            break;
        }
        default: //go through the string
        {
            for (u32 j = 0; j < i; j++) {
                if (str[j] == '-') //first dash
                {
                    start = strtol(str, NULL, 16);
                    end = strtol(&str[j + 1], NULL, 16);
                    valid = 1;
                    break;
                } else if (str[j] == ':') //start + size
                {
                    start = strtol(str, NULL, 16);
                    end = start + strtol(&str[j + 1], NULL, 16);
                    valid = 1;
                    break;
                }
            }
        }
    }

    if (!valid) return 0;

    if (end && (start > end)) {
        printf("WARNING: If END is specified and non-zero, it cannot be greater than START. The whole file will be disassembled.\n");
        return 0; //start can't be greater than end if end is non-zero
    }

    /* Success, set */
    range->start = start;
    range->end = end;
    return 1;
}

static int IfValidModeSet(DARGS *dargs, u8 *m) {
    /* Only check first two chars to change from default DTHUMB to DARM mode */
    //valid inputs: (/a, /a4, /a5), (/t, /t4, /t5, /4, /5) -> (/a, /a4), (/4)

    if (!m || !m[0] || m[0] != '/') return 0; //needs to be at least one char, beggining with "/"
    if (strlen(m) > 3) return 0; //can't have more than 3 printable characters

    switch (*(u16 *) (&m[1])) //treat as a 16-bit value to decode faster
    {
        case 0x0061: //a
        case 0x3561: //a5
        {
            dargs->dmode = DARM;
            dargs->arch = ARMv5TE; //redundant
            return 1;
        }
        case 0x3461: //a4
        {
            dargs->dmode = DARM;
            dargs->arch = ARMv4T;
            return 1;
        }
        case 0x3474: //t4
        case 0x0034: //4
        {
            dargs->dmode = DTHUMB; //redundant
            dargs->arch = ARMv4T;
            return 1;
        }
        case 0x0074: //t
        case 0x0035: //5
        case 0x3574: //t5
        {
            dargs->dmode = DTHUMB; //redundant
            dargs->arch = ARMv5TE; //redundant
            return 1;
        }
        default: {
            return 0; //invalid input
        }
    }
}

static int ParseCommandLineArguments(DARGS *dargs, int argc, char *argv[]) {
    /* You can pass arguments in any order, but they need to be valid */
    /* fname_in has to be valid, else return 0 (failed) */
    /* The other arguments can be invalid, default behavior is handled */

    //todo: make better use of argc to know which argv you can read from (careful with invalid pointers, oob reads)

    if (argc < 2 || argc > 5) return DARGS_INVALID;

    if (IsValidPath(argv[1])) //filein
    {
        dargs->fname_in = argv[1];
        if (IsValidPath(argv[2])) //fileout
        {
            dargs->fname_out = argv[2];
            if (IfValidRangeSet(&dargs->frange, argv[3])) {
                IfValidModeSet(dargs, argv[4]);
            } else if (IfValidModeSet(dargs, argv[3])) {
                IfValidRangeSet(&dargs->frange, argv[4]);
            }
            return DARGS_FILEOUT;
        } else //stdout
        {
            if (IfValidRangeSet(&dargs->frange, argv[2])) {
                IfValidModeSet(dargs, argv[3]);
            } else if (IfValidModeSet(dargs, argv[2])) {
                IfValidRangeSet(&dargs->frange, argv[3]);
            }
            return DARGS_STDOUT;
        }
    } else if (IfValidCodeSet(&dargs->code, argv[1])) //single code
    {
        IfValidModeSet(dargs, argv[2]);
        return DARGS_SINGLE;
    }
    return DARGS_INVALID;
}

//#define DEBUG //comment out to disable debug ifdefs

int main(int argc, char *argv[]) {

    clock_t start = clock();

    DARGS dargs = {NULL, NULL, {0}, DTHUMB, ARMv5TE, 0};
    DARGS_STATUS ds = ParseCommandLineArguments(&dargs, argc, argv);

    switch (ds) {
        case DARGS_INVALID: {
            printf("Nothing was done\n");
            return 0; //terminate
        }
        case DARGS_SINGLE: {
            DisassembleSingle(&dargs);
            break;
        }
        case DARGS_STDOUT: {
            FILE *file_in = fopen(dargs.fname_in, "rb"); //ignore warning, IsValidPath makes sure it isn't NULL
            if (file_in == NULL) {
                printf("[ERROR]: The file \"%s\" doesn't exist. Aborting.\n", dargs.fname_in);
                return 0; //terminate
            }
            printf("[INFO]: Starting disassembly of \"%s\".\n", dargs.fname_in);
            if (DisassembleFile(file_in, stdout, &dargs)) {
                printf("\n[INFO]: Completed successfully \"%s\".\n", dargs.fname_in);
            } else {
                printf("[ERROR]: DisassembleFile failed.\n");
            }
            fclose(file_in);
            break;
        }
        case DARGS_FILEOUT: {
            FILE *file_in = fopen(dargs.fname_in, "rb"); //ignore warning, IsValidPath makes sure it isn't NULL
            if (file_in == NULL) {
                printf("[ERROR]: The file \"%s\" doesn't exist. Aborting.\n", dargs.fname_in);
                return 0; //terminate
            }
            FILE *file_out = fopen(dargs.fname_out, "w+"); //ignore warning, IsValidPath makes sure it isn't NULL
            if (file_out == NULL) {
                printf("[ERROR]: The file \"%s\" could not be created. Aborting.\n", dargs.fname_out);
                return 0; //terminate
            }

            printf("Starting disassembly of \"%s\".\n", dargs.fname_in);
            if (DisassembleFile(file_in, file_out, &dargs)) {
                printf("Successfully disassembled \"%s\" to \"%s\".\n", dargs.fname_in, dargs.fname_out);
            } else {
                printf("[ERROR]: DisassembleFile failed.\n");
            }
            fclose(file_in);
            fclose(file_out);
            break;
        }
    }

    printf("[INFO]: Completed in: %.0f ms\n", (double) clock() - (double) start);
    return 0;
}
