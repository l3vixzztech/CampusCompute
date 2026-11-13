#include "banker.h"

#include "common.h"

#define BANKER_PROCESSES 5
#define BANKER_RESOURCES 3

typedef struct {
    const char *name;
    int available[BANKER_RESOURCES];
    int maximum[BANKER_PROCESSES][BANKER_RESOURCES];
    int allocation[BANKER_PROCESSES][BANKER_RESOURCES];
    int expect_safe;
} BankerCase;

static const BankerCase safe_case = {
    "SAFE state (classic Banker's example)",
    {3, 3, 2},
    {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}},
    {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}},
    1};

static const BankerCase unsafe_case = {
    "UNSAFE state (no complete safe sequence)",
    {0, 1, 0},
    {{2, 0, 0}, {1, 1, 0}, {1, 0, 1}, {2, 1, 0}, {0, 2, 0}},
    {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 0, 0}, {0, 1, 0}},
    0};

static void print_matrices(const BankerCase *c,
                           const int need[BANKER_PROCESSES][BANKER_RESOURCES])
{
    int i;

    printf("\nAvailable (A B C): [%d %d %d]\n", c->available[0],
           c->available[1], c->available[2]);
    printf("\n%-9s %-18s %-18s %s\n", "Process", "Allocation (A B C)",
           "Maximum (A B C)", "Need (A B C)");
    for (i = 0; i < BANKER_PROCESSES; i++) {
        char alloc[32], maximum[32], need_text[32];

        snprintf(alloc, sizeof(alloc), "[%d %d %d]", c->allocation[i][0],
                 c->allocation[i][1], c->allocation[i][2]);
        snprintf(maximum, sizeof(maximum), "[%d %d %d]", c->maximum[i][0],
                 c->maximum[i][1], c->maximum[i][2]);
        snprintf(need_text, sizeof(need_text), "[%d %d %d]", need[i][0],
                 need[i][1], need[i][2]);
        printf("P%-8d %-18s %-18s %s\n", i, alloc, maximum, need_text);
    }
}

static int run_case(const BankerCase *c)
{
    int need[BANKER_PROCESSES][BANKER_RESOURCES];
    int work[BANKER_RESOURCES];
    int finish[BANKER_PROCESSES] = {0};
    int sequence[BANKER_PROCESSES];
    int completed = 0;
    int i, j;

    print_rule("Banker's resource-safety algorithm");
    printf("Case: %s\n", c->name);
    printf("Need = Maximum - Allocation; safety check uses Work and Finish.\n");

    for (i = 0; i < BANKER_PROCESSES; i++) {
        for (j = 0; j < BANKER_RESOURCES; j++) {
            need[i][j] = c->maximum[i][j] - c->allocation[i][j];
            if (need[i][j] < 0) {
                fprintf(stderr,
                        "error: Maximum < Allocation for P%d resource %d\n",
                        i, j);
                return 1;
            }
        }
    }

    print_matrices(c, need);
    for (j = 0; j < BANKER_RESOURCES; j++)
        work[j] = c->available[j];

    printf("\nSafety trace:\n");
    while (completed < BANKER_PROCESSES) {
        int selected = -1;

        for (i = 0; i < BANKER_PROCESSES; i++) {
            int fits = 1;

            if (finish[i])
                continue;
            for (j = 0; j < BANKER_RESOURCES; j++) {
                if (need[i][j] > work[j]) {
                    fits = 0;
                    break;
                }
            }
            if (fits) {
                selected = i;
                break;
            }
        }

        if (selected < 0)
            break;

        printf("  P%d: Need(%d %d %d) <= Work(%d %d %d) -> grant, ",
               selected, need[selected][0], need[selected][1],
               need[selected][2], work[0], work[1], work[2]);
        for (j = 0; j < BANKER_RESOURCES; j++)
            work[j] += c->allocation[selected][j];
        finish[selected] = 1;
        sequence[completed++] = selected;
        printf("Work becomes (%d %d %d)\n", work[0], work[1], work[2]);
    }

    if (completed == BANKER_PROCESSES) {
        printf("\nSafe sequence: ");
        for (i = 0; i < completed; i++)
            printf("%sP%d", i == 0 ? "" : " -> ", sequence[i]);
        printf("\nResult: SAFE - every process can finish without deadlock.\n");
        return c->expect_safe ? 0 : 1;
    }

    printf("\nNo remaining process can satisfy Need <= Work.\n");
    printf("Result: UNSAFE - no safe sequence exists; the state could deadlock.\n");
    return c->expect_safe ? 1 : 0;
}

int command_banker(const char *mode)
{
    int rc = 0;

    if (mode == NULL || strcmp(mode, "both") == 0) {
        if (run_case(&safe_case) != 0)
            rc = 1;
        if (run_case(&unsafe_case) != 0)
            rc = 1;
    } else if (strcmp(mode, "safe") == 0) {
        rc = run_case(&safe_case);
    } else if (strcmp(mode, "unsafe") == 0) {
        rc = run_case(&unsafe_case);
    } else {
        fprintf(stderr, "error: banker mode must be safe, unsafe, or both\n");
        return 2;
    }

    return rc;
}
