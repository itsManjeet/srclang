#include <getopt.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "srclang.h"

FILE *cout = NULL;

struct List {
    char **data;
    int size;
    int capacity;
};

static void append(struct List *list, char *value) {
    if (list->capacity <= list->size + 1) {
        list->capacity = (list->capacity + 1) * 2;
        list->data = realloc(list->data, list->capacity * sizeof(char *));
    }
    list->data[list->size++] = value;
}

static bool is_exists(const char *path) {
    struct stat st;
    return !stat(path, &st);
}

static const char *gcc_path() {
    char *path = NULL;

    glob_t buf = {};
    glob("/usr/lib/gcc/*-linux-gnu/*/crtbegin.o", 0, NULL, &buf);
    if (buf.gl_pathc > 0) {
        path = dirname(strdup(buf.gl_pathv[buf.gl_pathc - 1]));
    }
    globfree(&buf);
    return path;

    return path;
}

char *read_file(char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp)
        error("cannot open %s: %s", path, strerror(errno));

    fseek(fp, 0, SEEK_END);
    size_t filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc(filesize + 2);
    int size = fread(buf, 1, filesize, fp);
    if (size != filesize)
        error("file too large %lld != %lld", filesize, size);

    buf[filesize] = '\n';
    buf[filesize + 1] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *aout = "a.out";
    const char *linker = "ld";
    const char *assembler = "as";
    bool static_linked = true;
    bool verbose = false;

    int opt;
    char command[10240];
    int status = 0;
    struct List object_files = {};

    while ((opt = getopt(argc, argv, "o:A:L:dv")) != -1) {
        switch (opt) {
            case 'o':
                aout = optarg;
                break;
            case 'A':
                assembler = optarg;
                break;
            case 'L':
                linker = optarg;
                break;
            case 'd':
                static_linked = false;
                break;
            case 'v':
                verbose = true;
                break;
            case ':':
                fprintf(stderr, "option needs a value\n");
                return 1;
            default:
                fprintf(stderr, "unknown option: %c\n", optopt);
                return 1;
        }
    }

    for (; optind < argc; optind++) {
        filename = argv[optind];
        user_input = read_file(filename);
        if (user_input == NULL) {
            goto cleanup;
        }

        token = tokenize();
        Program *prog = program();
        add_type(prog);
        for (Function *fn = prog->fns; fn; fn = fn->next) {
            int offset = 0;
            for (VarList *vl = fn->locals; vl; vl = vl->next) {
                Var *var = vl->var;
                offset = align_to(offset, var->ty->align);
                offset += size_of(var->ty, var->tok);
                var->offset = offset;
            }
            fn->stack_size = align_to(offset, 8);
        }

        char objectfile[1024];
        sprintf(objectfile, "/tmp/srclang-XXXXXX");
        mkstemp(objectfile);

        if ((cout = fopen(objectfile, "w")) == NULL) {
            fprintf(stderr, "failed to open %s: %s", objectfile, strerror(errno));
            goto cleanup;
        }
        codegen(prog);
        fclose(cout);
        free(user_input);

        sprintf(command, "%s -o %s.o %s", "as", objectfile, objectfile);
        status = system(command);
        remove(objectfile);
        if (status != 0) goto cleanup;

        strcat(objectfile, ".o");
        append(&object_files, strdup(objectfile));
    }

    if (object_files.size == 0) {
        fprintf(stderr, "no input file provided\n");
        return 1;
    }

    sprintf(command,
            "%s -o %s -m elf_x86_64 "
            "-L%s "
            "-L/usr/lib64/x86_64-linux-gnu "
            "-L/usr/lib64 "
            "-L/lib64 "
            "-L/usr/lib/x86_64-linux-gnu "
            "-L/usr/lib "
            "-L/lib "
            "-l :crt1.o "
            "-l :crti.o "
            "--dynamic-linker /lib64/ld-linux-x86-64.so.2 ",

            "ld",
            aout,
            gcc_path());

    for (int i = 0; i < object_files.size; i++) {
        strcat(command, object_files.data[i]);
        strcat(command, " ");
    }

    strcat(command, "-lc -l :crtend.o -l :crtn.o ");

    status = system(command);
    if (verbose) fprintf(stdout, "COMMAND: %s\n", command);

cleanup:
    for (int i = 0; i < object_files.size; i++) {
        remove(object_files.data[i]);
    }
    return status;
}