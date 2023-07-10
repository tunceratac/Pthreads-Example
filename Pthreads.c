#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Array used to hold the list of each mutex
pthread_mutex_t *mutexes;

// Structure representing the node structure
typedef struct node {
 struct node* r_node;
 unsigned value;
}node;

// Structure representing the hash table
typedef struct hash_table{
 node** list;
 unsigned nof_element;
}hash_table;

// Structure representing the data to be passed between threads
typedef struct parameterPass{
 hash_table* table;
 unsigned* numbers;
 unsigned offset;
 unsigned last_ele;
 unsigned index;
}parameterPass;

// Function used to insert items into the hash table
void* insertionFunction(void *parameters) {
    // Obtain the parameters by casting to the correct structure
    parameterPass* pass = (parameterPass*)parameters;
    unsigned int idx;

    for (unsigned int i = pass->offset; i < pass->last_ele; i++) {
        // Calculate the number in the array and the hash index
        unsigned int val = pass->numbers[i];
        idx = val % pass->table->nof_element;

        // Create a new node and set its value
        node *new_node = (node*)malloc(sizeof(node));
        new_node->value = val;
        new_node->r_node = NULL;

        // Lock the insertion operation on the hash table with a mutex
        pthread_mutex_lock(&mutexes[idx]);

        // Insert the new node into the hash table
        new_node->r_node = pass->table->list[idx];
        pass->table->list[idx] = new_node;

        // Unlock the mutex
        pthread_mutex_unlock(&mutexes[idx]);
    }
    pthread_exit(NULL);
}

// Helper function to swap the values of two nodes
void swap(node *a, node *b) {
 int temp = a->value;
 a->value = b->value;
 b->value = temp;
}

// Bubble Sort algorithm that sorts a one-dimensional linked list
void bubbleSort(node *start) {
    int swapped;
    node *ptr;
    node *lptr = NULL;
    
    // If the list is empty, return
    if (start == NULL)
        return;

    // Traverse the list and swap nodes as necessary
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

// Function to initialize the hash table
hash_table* initializeHashTable(unsigned numOfThread, unsigned numOfElements) {
    // Create a new hash table
    hash_table *new_table = (hash_table*)malloc(sizeof(hash_table));
    new_table->nof_element = numOfElements;

    // Create the linked list
    new_table->list = (node**)malloc(numOfElements * sizeof(node*));
    for (unsigned i = 0; i < numOfElements; i++) {
        new_table->list[i] = NULL;
    }
    
    // Create the mutexes
    mutexes = (pthread_mutex_t*)malloc(numOfElements * sizeof(pthread_mutex_t));
    for (unsigned i = 0; i < numOfElements; i++) {
        pthread_mutex_init(&mutexes[i], NULL);
    }

    return new_table;
}

// Function that counts the number of items in the CSV file
unsigned countNumOfElements(char* filename) {
    // Open the file
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open file %s\n", filename);
        return 0;
    }

    char line[1024];
    unsigned count = 0;
    // Read the file line by line and count
    while (fgets(line, 1024, file)) {
        count++;
    }
    // Close the file
    fclose(file);
    return count - 1;
}

// Function that reads the numbers from the CSV file and assigns them to an array
unsigned* readNumbers(char* filename, unsigned num_element) {
    // Open the file
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Cannot open file %s\n", filename);
        return NULL;
    }

    // Allocate an array to hold the numbers
    unsigned *numbers = (unsigned*)malloc(num_element * sizeof(unsigned));
    char line[1024];
    unsigned count = 0;
    fgets(line, 1024, file);

    // Read the file line by line and get the numbers
    while (fgets(line, 1024, file) && count < num_element) {
        char* token = strtok(line, ",");
        token = strtok(NULL, ",");
        numbers[count] = atoi(token);
        count++;
    }    
    // Close the file
    fclose(file);
    return numbers;
}

// Thread function that runs the bubble sort algorithm
void* bubbleSortThread(void *parameters) {
    parameterPass* pass = (parameterPass*)parameters;
    bubbleSort(pass->table->list[pass->index]);
    pthread_exit(NULL);
}

// Function that prepares the parameters to be passed to the threads
parameterPass* prepareParameters(unsigned numOfThreads, unsigned numElements, unsigned* numbers, hash_table* table) {
    // Allocate a parameter structure for each thread
    parameterPass* params = (parameterPass*) malloc(numOfThreads * sizeof(parameterPass));
    unsigned elementsPerThread = numElements / numOfThreads;
    unsigned remainder = numElements % numOfThreads;
    unsigned current = 0;

    // Set up the parameters for each thread
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

// Function that prepares the parameters for the second phase
parameterPass* prepareParametersPhaseTwo(unsigned numOfThreadsPhaseTwo, hash_table* table) {
    parameterPass* params = (parameterPass*) malloc(numOfThreadsPhaseTwo * sizeof(parameterPass));
    
    // Set up the parameters for each thread
    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        params[i].table = table;
        params[i].index = i;
    }

    return params;
}

// Function that frees the hash table and the mutexes
void freeHashTable(hash_table* table) {
    for (unsigned i = 0; i < table->nof_element; i++) {
        node* current = table->list[i];

        // Traverse the list and free the nodes
        while (current != NULL) {
            node* next = current->r_node;
            free(current);  
            current = next;  
        }
        // Destroy the mutex
        pthread_mutex_destroy(&mutexes[i]);
    }
    // Free the list and the table
    free(table->list);
    free(table);
    // Free the mutex list
    free(mutexes);
}

// Main function
int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <numbers.csv> <numOfThreads>\n", argv[0]);
        return 1;
    }

    char* filename = argv[1];
    unsigned numOfThreads = atoi(argv[2]);

    // Calculate the number of items in the CSV file
    unsigned numElements = countNumOfElements(filename);
    // Read the numbers from the CSV file
    unsigned* numbers = readNumbers(filename, numElements);

    // Initialize the hash table
    hash_table* table = initializeHashTable(numOfThreads, numElements);

    // Prepare the parameters for the threads
    parameterPass* params = prepareParameters(numOfThreads, numElements, numbers, table);

    pthread_t* threads = malloc(sizeof(pthread_t) * numOfThreads);
    // Create and start the threads
    for (unsigned i = 0; i < numOfThreads; i++) {
        pthread_create(&threads[i], NULL, insertionFunction, &params[i]);
    }

    // Wait for the threads to finish
    for (unsigned i = 0; i < numOfThreads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(params);

    // Calculate the number of threads for the second phase
    unsigned numOfThreadsPhaseTwo = (numOfThreads * (numOfThreads + 1)) / 2;
    pthread_t* threadsPhaseTwo = malloc(sizeof(pthread_t) * numOfThreadsPhaseTwo);
    
    // Prepare the parameters for the second phase
    parameterPass* paramsPhaseTwo = prepareParametersPhaseTwo(numOfThreadsPhaseTwo, table);

    // Create and start the threads for the second phase
    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        pthread_create(&threadsPhaseTwo[i], NULL, bubbleSortThread, &paramsPhaseTwo[i]);
    }

    // Wait for the second phase threads to finish
    for (unsigned i = 0; i < numOfThreadsPhaseTwo; i++) {
        pthread_join(threadsPhaseTwo[i], NULL);
    }

    free(threadsPhaseTwo);
    free(paramsPhaseTwo);

    // Free the numbers and the hash table
    free(numbers);
    freeHashTable(table);

    return 0;
}
