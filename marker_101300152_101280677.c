#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef USE_SEMAPHORES
#include <semaphore.h>
#endif

#define SHM_NAME "/sysc4001_a3p2_shm"

#define MAX_QUESTIONS 5
#define MAX_RUBRIC_LEN 256
#define MAX_EXAMS 50
#define MAX_FILENAME_LEN 64

typedef struct {
    char rubric_lines[MAX_QUESTIONS][MAX_RUBRIC_LEN];

    int student_id;
    int question_status[MAX_QUESTIONS];
    int questions_done;

    int exam_count;
    int current_exam;
    char exam_files[MAX_EXAMS][MAX_FILENAME_LEN];

    int terminate;
} SharedData;

#ifdef USE_SEMAPHORES
sem_t *sem_rubric = NULL;       // protect rubric updates + file writes
sem_t *sem_question = NULL;     // protect question selection + questions_done
sem_t *sem_exam_load = NULL;    // ensure one loader at a time
#endif

// Wrapper macros so code compiles without semaphores for Part 2.a
#ifdef USE_SEMAPHORES
#define LOCK(sem)   sem_wait(sem)
#define UNLOCK(sem) sem_post(sem)
#else
#define LOCK(sem)   ((void)0)
#define UNLOCK(sem) ((void)0)
#endif

// Forward declarations
void load_rubric(SharedData *shm, const char *rubric_path);
void write_rubric_to_file(SharedData *shm, const char *rubric_path);
void load_exam(SharedData *shm, int exam_index);
void ta_process(SharedData *shm, int ta_id,
                const char *rubric_path);

void load_exam_list(SharedData *shm, const char *exam_list_path) {
    FILE *f = fopen(exam_list_path, "r");
    if (!f) {
        perror("fopen exam_list");
        exit(1);
    }

    char line[256];
    shm->exam_count = 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (shm->exam_count >= MAX_EXAMS) break;
        // strip newline
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) == 0) continue;
        strncpy(shm->exam_files[shm->exam_count], line, MAX_FILENAME_LEN - 1);
        shm->exam_files[shm->exam_count][MAX_FILENAME_LEN - 1] = '\0';
        shm->exam_count++;
    }
    fclose(f);

    if (shm->exam_count == 0) {
        fprintf(stderr, "No exams listed in %s\n", exam_list_path);
        exit(1);
    }
}

void load_rubric(SharedData *shm, const char *rubric_path) {
    FILE *f = fopen(rubric_path, "r");
    if (!f) {
        perror("fopen rubric");
        exit(1);
    }
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        if (!fgets(shm->rubric_lines[i], MAX_RUBRIC_LEN, f)) {
            // If fewer than 5 lines, fill the rest
            snprintf(shm->rubric_lines[i], MAX_RUBRIC_LEN, "%d, X\n", i+1);
        }
    }
    fclose(f);
}

void write_rubric_to_file(SharedData *shm, const char *rubric_path) {
    FILE *f = fopen(rubric_path, "w");
    if (!f) {
        perror("fopen rubric for write");
        return;
    }
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        fputs(shm->rubric_lines[i], f);
        // ensure newline
        size_t len = strlen(shm->rubric_lines[i]);
        if (len == 0 || shm->rubric_lines[i][len-1] != '\n') {
            fputc('\n', f);
        }
    }
    fclose(f);
}

void load_exam(SharedData *shm, int exam_index) {
    if (exam_index < 0 || exam_index >= shm->exam_count) {
        shm->terminate = 1;
        return;
    }

    const char *filename = shm->exam_files[exam_index];
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen exam");
        shm->terminate = 1;
        return;
    }

    char line[256];
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "Exam file %s is empty\n", filename);
        shm->student_id = 9999; // force termination
    } else {
        shm->student_id = atoi(line);
    }
    fclose(f);

    // Reset marking state
    shm->questions_done = 0;
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        shm->question_status[i] = 0;
    }

    printf("Loaded exam %s, student_id=%d\n", filename, shm->student_id);
    fflush(stdout);

    if (shm->student_id == 9999) {
        shm->terminate = 1;
    }
}

void random_sleep_range(int min_us, int max_us) {
    int delta = max_us - min_us;
    int extra = (delta > 0) ? rand() % (delta + 1) : 0;
    usleep(min_us + extra);
}

void review_rubric(SharedData *shm, const char *rubric_path, int ta_id) {
    for (int i = 0; i < MAX_QUESTIONS; i++) {
        // 0.5 - 1.0 seconds
        random_sleep_range(500000, 1000000);

        // Random decision: 50% chance to modify
        int change = rand() % 2;

        if (change) {
            LOCK(sem_rubric);

            // Find first char after comma
            char *line = shm->rubric_lines[i];
            char *comma = strchr(line, ',');
            if (comma && *(comma + 1) != '\0') {
                char *ch = comma + 1;
                // Skip spaces
                while (*ch == ' ') ch++;
                if (*ch != '\0' && *ch != '\n') {
                    (*ch) = (*ch) + 1; // next ASCII
                    printf("TA %d changed rubric line %d to: %s",
                           ta_id, i+1, line);
                }
            }

            // Write rubric back to file
            write_rubric_to_file(shm, rubric_path);

            UNLOCK(sem_rubric);
        }
    }
}

int select_question(SharedData *shm) {
    int q = -1;

    LOCK(sem_question);

    for (int i = 0; i < MAX_QUESTIONS; i++) {
        if (shm->question_status[i] == 0) {
            shm->question_status[i] = 1; // mark as taken
            q = i;
            break;
        }
    }

    UNLOCK(sem_question);
    return q;
}

int increment_questions_done_and_check_loader(SharedData *shm) {
    int become_loader = 0;

    LOCK(sem_question);
    shm->questions_done++;
    if (shm->questions_done == MAX_QUESTIONS) {
        become_loader = 1;
    }
    UNLOCK(sem_question);

    return become_loader;
}

void load_next_exam_by_ta(SharedData *shm) {
    LOCK(sem_exam_load);

    if (shm->terminate) {
        UNLOCK(sem_exam_load);
        return;
    }

    shm->current_exam++;
    if (shm->current_exam >= shm->exam_count) {
        shm->terminate = 1;
        UNLOCK(sem_exam_load);
        return;
    }

    load_exam(shm, shm->current_exam);

    UNLOCK(sem_exam_load);
}

void ta_process(SharedData *shm, int ta_id, const char *rubric_path) {
    srand(time(NULL) ^ (getpid() << 16));

    while (!shm->terminate) {
        // 1. Review rubric for current exam
        review_rubric(shm, rubric_path, ta_id);

        // 2. Mark questions
        while (!shm->terminate && shm->student_id != 9999) {
            int q = select_question(shm);
            if (q == -1) {
                // No more questions on this exam
                break;
            }

            // Marking takes 1.0 - 2.0 seconds
            random_sleep_range(1000000, 2000000);

            printf("TA %d marked student %d question %d\n",
                   ta_id, shm->student_id, q+1);
            fflush(stdout);

            int is_loader = increment_questions_done_and_check_loader(shm);
            if (is_loader) {
                // This TA loads the next exam
                load_next_exam_by_ta(shm);
            }
        }

        if (shm->student_id == 9999 || shm->terminate) {
            break;
        }
    }

    printf("TA %d exiting.\n", ta_id);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr,
            "Usage: %s <num_TAs> <rubric_file> <exam_list_file>\n",
            argv[0]);
        exit(1);
    }

    int num_tas = atoi(argv[1]);
    if (num_tas < 2) {
        fprintf(stderr, "num_TAs must be >= 2\n");
        exit(1);
    }

    const char *rubric_path = argv[2];
    const char *exam_list_path = argv[3];

    // Create POSIX shared memory
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
        perror("ftruncate");
        exit(1);
    }

    SharedData *shm = mmap(NULL, sizeof(SharedData),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize shared data
    memset(shm, 0, sizeof(SharedData));
    shm->terminate = 0;

    load_exam_list(shm, exam_list_path);
    load_rubric(shm, rubric_path);
    shm->current_exam = 0;
    load_exam(shm, shm->current_exam);

#ifdef USE_SEMAPHORES
    // Create named semaphores
    sem_rubric = sem_open("/rubric_sem", O_CREAT, 0666, 1);
    sem_question = sem_open("/question_sem", O_CREAT, 0666, 1);
    sem_exam_load = sem_open("/examload_sem", O_CREAT, 0666, 1);

    if (sem_rubric == SEM_FAILED || sem_question == SEM_FAILED ||
        sem_exam_load == SEM_FAILED) {
        perror("sem_open");
        exit(1);
    }
#endif

    // Fork TA processes
    for (int i = 0; i < num_tas; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child
            ta_process(shm, i+1, rubric_path);
            // Detach and exit
            munmap(shm, sizeof(SharedData));
            close(shm_fd);
            exit(0);
        }
    }

    // Parent waits for children
    for (int i = 0; i < num_tas; i++) {
        wait(NULL);
    }

    // Cleanup
#ifdef USE_SEMAPHORES
    sem_close(sem_rubric);
    sem_close(sem_question);
    sem_close(sem_exam_load);
    sem_unlink("/rubric_sem");
    sem_unlink("/question_sem");
    sem_unlink("/examload_sem");
#endif

    munmap(shm, sizeof(SharedData));
    close(shm_fd);
    shm_unlink(SHM_NAME);

    printf("All TAs finished. Exiting main.\n");
    return 0;
}
