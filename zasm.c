#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char    *name;
    unsigned char opcode;
} MnemonicEntry;

#define MAX_LINE_LEN      256
#define MAX_WORDS         4
#define MAX_WORD_LEN      32
#define INSTRUCTION_BYTES 3

static const MnemonicEntry MNEMONIC_TABLE[] = {
    { "ADD",  0  },
    { "SUB",  1  },
    { "MUL",  2  },
    { "DIV",  3  },
    { "MOD",  4  },
    { "STP",  5  },
    { "LDI",  6  },
    { "ADR",  7  },
    { "SUR",  8  },
    { "INC",  9  },
    { "DEC",  10 },
    { "JMP",  11 },
    { "CMP",  12 },
    { "JE",   13 },
    { "JZ",   13 }, /* alias */
    { "STI",  14 },
    { "LDM",  15 },
    { "PUSH", 16 },
    { "POP",  17 },
};

#define MNEMONIC_COUNT (sizeof(MNEMONIC_TABLE) / sizeof(MNEMONIC_TABLE[0]))

typedef struct {
    unsigned char *data;
    int   length;
    int   capacity;
} ByteBuffer;

static void buffer_init(ByteBuffer *buf) {
    buf->data     = NULL;
    buf->length   = 0;
    buf->capacity = 0;
}

static void buffer_push(ByteBuffer *buf, unsigned char value) {
    if (buf->length >= buf->capacity) {
        if (buf->capacity == 0) {
            buf->capacity = 64;
        } else {
            buf->capacity = buf->capacity * 2;
        }
        buf->data = realloc(buf->data, buf->capacity);
    }
    buf->data[buf->length++] = value;
}

static void buffer_free(ByteBuffer *buf) {
    free(buf->data);
    buf->data     = NULL;
    buf->length   = 0;
    buf->capacity = 0;
}

static int find_opcode(const char *name, unsigned char *opcode) {
    for (int i = 0; i < (int)MNEMONIC_COUNT; i++) {
        if (strcmp(MNEMONIC_TABLE[i].name, name) == 0) {
            *opcode = MNEMONIC_TABLE[i].opcode;
            return 1;
        }
    }
    return 0;
}

static int parse_operand(const char *raw) {
    char word[MAX_WORD_LEN];
    strncpy(word, raw, sizeof(word) - 1);
    word[sizeof(word) - 1] = '\0';

    int len = strlen(word);
    if (len > 0 && word[len - 1] == ',')
        word[--len] = '\0';

    if ((word[0] == 'R' || word[0] == 'r') && len == 2 && isdigit((unsigned char)word[1])) {
        int reg = atoi(&word[1]);
        if (reg >= 0 && reg <= 3) return reg;
    }

    if (word[0] == '[' && word[len - 1] == ']') {
        word[len - 1] = '\0';
        return atoi(word + 1);
    }

    if (isdigit((unsigned char)word[0])) {
    return atoi(word);
    }

    return -1;
}

static int split_line(char *line, char words[][MAX_WORD_LEN], int max_words) {
    char *comment = strchr(line, ';');
    if (comment) *comment = '\0';

    line[strcspn(line, "\r\n")] = '\0';

    int count = 0;
    char *token = strtok(line, " \t");

    while (token && count < max_words) {
        strncpy(words[count], token, MAX_WORD_LEN - 1);
        words[count][MAX_WORD_LEN - 1] = '\0';
        count++;
        token = strtok(NULL, " \t");
    }

    return count;
}

static void str_toupper(char *s) {
    for (; *s; s++) *s = toupper((unsigned char)*s);
}

static int assemble_file(FILE *input, ByteBuffer *output) {
    char raw_line[MAX_LINE_LEN];
    char words[MAX_WORDS][MAX_WORD_LEN];
    int  line_number = 0;
    int  error_count = 0;

    while (fgets(raw_line, sizeof(raw_line), input)) {
        line_number++;

        int word_count = split_line(raw_line, words, MAX_WORDS);
        if (word_count == 0) continue;

        str_toupper(words[0]);

        unsigned char opcode;
        if (find_opcode(words[0], &opcode) == 0) {
            fprintf(stderr, "line %d: unknown mnemonic '%s'\n", line_number, words[0]);
            error_count++;
            continue;
        }

        int op0 = 0;
        int op1 = 0;

        if (word_count >= 2) {
            op0 = parse_operand(words[1]);
            if (op0 < 0) {
                fprintf(stderr, "line %d: bad operand '%s'\n", line_number, words[1]);
                error_count++;
                continue;
            }
        }

        if (word_count >= 3) {
            op1 = parse_operand(words[2]);
            if (op1 < 0) {
                fprintf(stderr, "line %d: bad operand '%s'\n", line_number, words[2]);
                error_count++;
                continue;
            }
        }

        buffer_push(output, opcode);
        buffer_push(output, (unsigned char)op0);
        buffer_push(output, (unsigned char)op1);
    }

    return error_count;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: zasm <input.zasm> <output.zo>\n");
        return 1;
    }

    const char *input_path  = argv[1];
    const char *output_path = argv[2];

    FILE *input = fopen(input_path, "r");
    if (input == NULL) {
        perror(input_path);
        return 1;
    }

    ByteBuffer output;
    buffer_init(&output);

    int errors = assemble_file(input, &output);
    fclose(input);

    if (errors > 0) {
        fprintf(stderr, "%d error(s) — no output written.\n", errors);
        buffer_free(&output);
        return 1;
    }

    FILE *out_file = fopen(output_path, "wb");
    if (out_file == NULL) {
        perror(output_path);
        buffer_free(&output);
        return 1;
    }

    fwrite(output.data, 1, output.length, out_file);
    fclose(out_file);

    int instruction_count = output.length / INSTRUCTION_BYTES;
    printf("ok: %d instructions -> %d bytes -> %s\n",
           instruction_count, output.length, output_path);

    buffer_free(&output);
    return 0;
}
