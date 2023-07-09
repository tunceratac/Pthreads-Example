#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

pthread_mutex_t *mutexes;

typedef struct node {
 struct node* r_node;
 unsigned value;
}node;

typedef struct hash_table{
 node** list;
 unsigned nof_element;
}hash_table;

typedef struct parameterPass{
 hash_table* table;
 unsigned* numbers;
 unsigned offset;
 unsigned last_ele;
 unsigned index;
}parameterPass;


void* insertionFunction(void *parameters) {
    parameterPass* pass = (parameterPass*)parameters;
    unsigned int idx;

    for (unsigned int i = pass->offset; i < pass->last_ele; i++) {
        unsigned int val = pass->numbers[i];
        idx = val % pass->table->nof_element;

        node *new_node = (node*)malloc(sizeof(node));
        new_node->value = val;
        new_node->r_node = NULL;

        pthread_mutex_lock(&mutexes[idx]);

        new_node->r_node = pass->table->list[idx];
        pass->table->list[idx] = new_node;

        pthread_mutex_unlock(&mutexes[idx]);
    }
    pthread_exit(NULL);
}

void swap(node *a, node *b) {
 int temp = a->value;
 a->value = b->value;
 b->value = temp;
}

void bubbleSort(node *start) {
    int swapped;
    node *ptr;
    node *lptr = NULL;

    if (start == NULL)
        return;

    do {
        swapped = 0;
        ptr = start;

        while (ptr->r_node != lptr) {
            if (ptr->value > ptr->r_node->value) { 
                swap(ptr, ptr->r_node);
                swapped = 1;
            }
            ptr = ptr->r_node;
        }
        lptr = ptr;
    } while (swapped);
}

hash_table* initializeHashTable(unsigned numOfThread, unsigned numOfElements) {
    hash_table *new_table = (hash_table*)malloc(sizeof(hash_table));
    new_table->nof_element = numOfElements;

    new_table->list = (node**)malloc(numOfElements * sizeof(node*));
    for (unsigned i = 0; i < numOfElements; i++) {
        new_table->list[i] = NULL;
    }
    
    mutexes = (pthread_mutex_t*)malloc(numOfElements * sizeof(pthread_mutex_t));
    for (unsigned i = 0; i < numOfElements; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
    }

    return new_table;
}

unsigned countNumOfElements(char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open file %s\n", filename);
        return 0;
    }

    char line[1024];
    unsigned count = 0;
    while (fgets(line, 1024, file)) {
        count++;
    }

    fclose(file);
    return count - 1;
}

unsigned* readNumbers(char* filename, unsigned num_element) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open file %s\n", filename);
        return NULL;
    }
    unsigned *numbers = (unsigned*)malloc(num_element * sizeof(unsigned));
    char line[1024];
    unsigned count = 0;
    fgets(line, 1024, file);

    while (fgets(line, 1024, file) && count < num_element) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        numbers[count] = atoi(token);
        count++;
    }    
    fclose(file);
    return numbers;
}

void* bubbleSortThread(void *parameters) {
    parameterPass* pass = (parameterPass*)parameters;
    bubbleSort(pass->table->list[pass->index]);
    pthread_exit(NULL);
}

parameterPass* prepareParameters(unsigned numOfThreads, unsigned numElements, unsigned* numbers, hash_table* table) {
    parameterPass* params = (parameterPass*) malloc(numOfThreads * sizeof(parameterPass));
    unsigned elementsPerThread = numElements / numOfThreads;
    unsigned remainder = numElements % numOfThreads;
    unsigned current = 0;

    for (unsigned i = 0; i < numOfThreads; i++) {
        params[i].table = table;
        params[i].numbers = numbers;
        params[i].offset = current;
        if (i < remainder) {
            params[i].last_ele = current + elementsPerThread + 1;
            current = params[i].last_ele;
        } else {
            params[i].last_ele = current + elementsPerThread;
            current = params[i].last_ele;
        }
    }

    return params;
}

parameterPass* prepareParametersPhaseTwo(unsigned numOfThreadsPhaseTwo, hash_table* table) {
    parameterPass* params = (parameterPass*) malloc(numOfThreadsPhaseTwo * sizeof(parameterPass));
    
    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        params[i].table = table;
        params[i].index = i;
    }

    return params;
}


void freeHashTable(hash_table* table) {
    for (unsigned i = 0; i < table->nof_element; i++) {
        node* current = table->list[i];

        while (current != NULL) {
            node* next = current->r_node;
            free(current);  
            current = next;  
        }
        
        pthread_mutex_destroy(&mutexes[i]);
    }

    free(table->list);
    free(table);
    free(mutexes);
}


int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <numbers.csv> <numOfThreads>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];
    unsigned numOfThreads = atoi(argv[2]);

    unsigned numElements = countNumOfElements(filename);
    unsigned* numbers = readNumbers(filename, numElements);

    hash_table* table = initializeHashTable(numOfThreads, numElements);

    parameterPass* params = prepareParameters(numOfThreads, numElements, numbers, table);

    pthread_t* threads = malloc(sizeof(pthread_t) * numOfThreads);
    for (unsigned i = 0; i < numOfThreads; i++) {
        pthread_create(&threads[i], NULL, insertionFunction, &params[i]);
    }

    for (unsigned i = 0; i < numOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(params);

    unsigned numOfThreadsPhaseTwo = (numOfThreads * (numOfThreads + 1)) / 2;
    pthread_t* threadsPhaseTwo = malloc(sizeof(pthread_t) * numOfThreadsPhaseTwo);
    
    parameterPass* paramsPhaseTwo = prepareParametersPhaseTwo(numOfThreadsPhaseTwo, table);

    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        pthread_create(&threadsPhaseTwo[i], NULL, bubbleSortThread, &paramsPhaseTwo[i]);
    }

    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        pthread_join(threadsPhaseTwo[i], NULL);
    }

    free(threadsPhaseTwo);
    free(paramsPhaseTwo);

    free(numbers);
    freeHashTable(table);

    return 0;
}
