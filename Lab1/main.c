#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME_LEN 31
#define MAX_STATE_LEN 3
#define MAX_LINE_LEN 256

#define DEBUG 0

typedef struct NamedPlace {
    int code;
    char state[MAX_STATE_LEN];
    char name[MAX_NAME_LEN];
    int population;
    double area;
    double latitude;
    double longitude;
    int road_intersection_code;
    double distance_to_intersection;
    struct NamedPlace* next;
} NamedPlace;

void trim_spaces(char* str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}
NamedPlace* read_data(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open file");
        return NULL;
    }

    NamedPlace* head = NULL;
    NamedPlace* tail = NULL;
    char line[MAX_LINE_LEN];
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        NamedPlace* place = (NamedPlace*)malloc(sizeof(NamedPlace));
        if (!place) {
            perror("Failed to allocate memory");
            fclose(file);
            return NULL;
        }
        line_num++;

        // Extract fields based on fixed-width positions
        char code_str[9], state_str[3], name_str[31], population_str[11], area_str[14], latitude_str[10], longitude_str[11], road_code_str[6], distance_str[8];

        strncpy(code_str, line, 8); code_str[8] = '\0';
        strncpy(state_str, line + 8, 2); state_str[2] = '\0';
        strncpy(name_str, line + 10, 30); name_str[30] = '\0';
        strncpy(population_str, line + 57, 10); population_str[10] = '\0';
        strncpy(area_str, line + 68, 13); area_str[13] = '\0';
        strncpy(latitude_str, line + 81, 9); latitude_str[9] = '\0';
        strncpy(longitude_str, line + 91, 10); longitude_str[10] = '\0';
        strncpy(road_code_str, line + 101, 5); road_code_str[5] = '\0';
        strncpy(distance_str, line + 108, 7); distance_str[7] = '\0';

        place->code = strtol(code_str, NULL, 10);
        strncpy(place->state, state_str, MAX_STATE_LEN - 1);
        place->state[MAX_STATE_LEN - 1] = '\0';
        strncpy(place->name, name_str, MAX_NAME_LEN - 1);
        place->name[MAX_NAME_LEN - 1] = '\0';
        trim_spaces(place->name);
        place->population = strtol(population_str, NULL, 10);
        place->area = strtod(area_str, NULL);
        place->latitude = strtod(latitude_str, NULL);
        place->longitude = strtod(longitude_str, NULL);
        place->road_intersection_code = strtol(road_code_str, NULL, 10);
        place->distance_to_intersection = strtod(distance_str, NULL);

        place->next = NULL;

        if (tail) {
            tail->next = place;
        } else {
            head = place;
        }
        tail = place;

        if (DEBUG) {
            printf("%s\n", place->name);
        }

        if (DEBUG && line_num >= 10) {
            break;
        }
    }

    fclose(file);
    return head;
}

void print_place(NamedPlace* place) {
    printf("Code: %d\n", place->code);
    printf("State: %s\n", place->state);
    printf("Name: %s\n", place->name);
    printf("Population: %d\n", place->population);
    printf("Area: %.2lf\n", place->area);
    printf("Latitude: %.2lf\n", place->latitude);
    printf("Longitude: %.2lf\n", place->longitude);
    printf("Road Intersection Code: %d\n", place->road_intersection_code);
    printf("Distance to Intersection: %.2lf\n", place->distance_to_intersection);
}

void list_states(NamedPlace* head, const char* name) {
    NamedPlace* current = head;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            printf("%s\n", current->state);
        }
        current = current->next;
    }
}

void print_place_info(NamedPlace* head, const char* name, const char* state) {
    NamedPlace* current = head;
    while (current) {
        if (strcmp(current->name, name) == 0 && strcmp(current->state, state) == 0) {
            printf("Code: %d\n", current->code);
            printf("State: %s\n", current->state);
            printf("Name: %s\n", current->name);
            printf("Population: %d\n", current->population);
            printf("Area: %.2lf\n", current->area);
            printf("Latitude: %.2lf\n", current->latitude);
            printf("Longitude: %.2lf\n", current->longitude);
            printf("Road Intersection Code: %d\n", current->road_intersection_code);
            printf("Distance to Intersection: %.2lf\n", current->distance_to_intersection);
            return;
        }
        current = current->next;
    }
    printf("Place not found.\n");
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    NamedPlace* places = read_data("../Lab1/named-places.txt");
    if (!places) {
        return 1;
    }

    while (1) {
        char name[MAX_NAME_LEN];
        char state[MAX_STATE_LEN];

        printf("Enter place name (or 'exit' to quit): ");
        scanf("%30s", name);
        trim_spaces(name);
        if (strcmp(name, "exit") == 0) {
            break;
        }

        printf("States with place name '%s':\n", name);
        list_states(places, name);

        printf("Enter state abbreviation: ");
        scanf("%2s", state);
        trim_spaces(state);

        print_place_info(places, name, state);
    }

    // Free the linked list
    NamedPlace* current = places;
    while (current) {
        NamedPlace* next = current->next;
        free(current);
        current = next;
    }

    return 0;
}