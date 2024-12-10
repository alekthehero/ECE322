#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>
#include <stdlib.h>

#define MAX_PATH 1024
#define MAX_MATCHES 100
#define MAX_ERROR 1024

void enterFile(const char *file_path, const regex_t *regex) {
    FILE *file = fopen(file_path, "r");
    int reg;
    regmatch_t match[MAX_MATCHES];
    if (!file) {
        perror("fopen");
        return;
    }

    char line[MAX_PATH];
    int i = 0;
    while (fgets(line, sizeof(line), file)) {
        i++;
        reg = regexec(regex, line, MAX_MATCHES, match, 0);
        if (reg == REG_NOMATCH) {
            continue;
        }
        if (reg != 0) {
            char errline[MAX_ERROR];
            regerror(reg, regex, errline, MAX_ERROR);
            fprintf(stderr, "%s\n", errline);
            exit(1);
        }
        if (match[0].rm_so != -1) {
            printf("%s:%d: %s", file_path, i, line);
        }
    }

    fclose(file);
}

int isCompatFile(char *name) {
    char *ext = strrchr(name, '.');
    if (!ext) return 0;
    return (strcmp(ext, ".txt") == 0 || strcmp(ext, ".c") == 0 ||
            strcmp(ext, ".h") == 0 || strcmp(ext, ".cpp") == 0);
}

int searchDirectory(char *dirPath, const regex_t *regex) {
    DIR *dir = opendir(dirPath);
    if (!dir) {
        perror("opendir");
        return 1;
    }
    struct dirent *file;
    while ((file = readdir(dir)) != NULL) {
        if (strcmp(file->d_name, ".") == 0 || strcmp(file->d_name, "..") == 0) {
            continue;
        }
        char newPath[MAX_PATH];
        snprintf(newPath, sizeof(newPath), "%s/%s", dirPath, file->d_name);

        struct stat structstat;
        if (stat(newPath, &structstat) == -1) {
            perror("stat");
            continue;
        }
        if (S_ISDIR(structstat.st_mode)) {
            searchDirectory(newPath, regex);
        } else if (S_ISREG(structstat.st_mode) && isCompatFile(file->d_name)) {
            enterFile(newPath, regex);
        }
    }

    closedir(dir);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Correct: %s <regular expression> <directory>\n", argv[0]);
        return 1;
    }
    regex_t reg;
    int errcode = regcomp(&reg, argv[1], REG_EXTENDED);
    if (errcode) {
        char errbuf[MAX_ERROR];
        regerror(errcode, &reg, errbuf, sizeof(errbuf));
        fprintf(stderr, "Incorrect reg entered: %s\n", errbuf);
        return 1;
    }
    searchDirectory(argv[2], &reg);
    regfree(&reg);
    return 0;
}
