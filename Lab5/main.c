#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <stdlib.h>

int is_text_file(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    return (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 ||
            strcmp(ext, ".h") == 0 || strcmp(ext, ".cpp") == 0);
}

void search_file(const char *file_path, const regex_t *regex) {
    FILE *file = fopen(file_path, "r");
    int reg;
    regmatch_t match[100];
    if (!file) {
        perror("fopen");
        return;
    }

    char line[1024];
    int line_number = 0;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        reg = regexec(regex, line, 100, match, 0);
        if (reg == REG_NOMATCH) {
            continue;
        }
        if (reg != 0) {
            char errline[1000];
            regerror(reg, regex, errline, 1000);
            fprintf(stderr, "%s\n", errline);
            exit(1);
        }
        if (match[0].rm_so != -1) {
            printf("%s:%d: %s", file_path, line_number, line);
        }
    }

    fclose(file);
}

int search_directory(const char *dir_path, const regex_t *regex) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == -1) {
            perror("stat");
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
            search_directory(full_path, regex);
        } else if (S_ISREG(statbuf.st_mode) && is_text_file(entry->d_name)) {
            search_file(full_path, regex);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <regular expression> <directory>\n", argv[0]);
        return 1;
    }
    regex_t reg;
    int errcode = regcomp(&reg, argv[1], REG_EXTENDED);
    if (errcode) {
        char errbuf[100];
        regerror(errcode, &reg, errbuf, sizeof(errbuf));
        fprintf(stderr, "Could not compile reg: %s\n", errbuf);
        return 1;
    }

    search_directory(argv[2], &reg);

    regfree(&reg);

    return 0;
}
