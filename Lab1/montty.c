#include <stdio.h>
#include <stdlib.h>
#include <terminals.h>
#include <hardware.h>
#include <threads.h>

#define MAX_LEN 1024

typedef struct {
    char *items;
    int front;
    int rear;
    int size;
    int capacity;
    int lines;
} Buffer;

static void initBuf(Buffer *b, int capacity);
static int isEmpty(Buffer *b);
static int isFull(Buffer *b);
static void addBuf(Buffer *b, char item);
static char popBuf(Buffer *b);
static void deleteLast(Buffer *b);
static char cutIn(Buffer *b, char target);
static int getLines(Buffer *b);

// buffers
static Buffer* input_buffers[NUM_TERMINALS];
static Buffer* echo_buffers[NUM_TERMINALS];
static Buffer* output_buffers[NUM_TERMINALS];
static Buffer* pri_buffers[NUM_TERMINALS];

// condition variables
static cond_id_t all_transmitted[NUM_TERMINALS];
static cond_id_t writingTerminal[NUM_TERMINALS];
static cond_id_t output_full[NUM_TERMINALS];
static cond_id_t line[NUM_TERMINALS];
static cond_id_t readingTerminal[NUM_TERMINALS];

// states
static int outputRegister_states[NUM_TERMINALS];
static int writeTerminal_states[NUM_TERMINALS];
static int readTerminal_states[NUM_TERMINALS];

// term states
static struct termstat term_states[NUM_TERMINALS];

#define BUSY 1
#define FREE 0

extern int InitTerminal(int term) {
    InitHardware(term);

    input_buffers[term] = (Buffer*)malloc(sizeof(Buffer));
    echo_buffers[term] = (Buffer*)malloc(sizeof(Buffer));
    output_buffers[term] = (Buffer*)malloc(sizeof(Buffer));
    pri_buffers[term] = (Buffer*)malloc(sizeof(Buffer));

    writingTerminal[term] = CondCreate();
    readingTerminal[term] = CondCreate();
    all_transmitted[term] =  CondCreate();
    output_full[term] = CondCreate();
    line[term] = CondCreate();

    outputRegister_states[term] = FREE;
    writeTerminal_states[term] = FREE;

    initBuf(input_buffers[term], MAX_LEN);
    initBuf(echo_buffers[term], MAX_LEN);
    initBuf(output_buffers[term], MAX_LEN);
    initBuf(pri_buffers[term], MAX_LEN);

    term_states[term].tty_in = term_states[term].tty_out = term_states[term].user_in = term_states[term].user_out = 0;
    return 1;
}

extern int InitTerminalDriver() {
    for (int i = 0; i < NUM_TERMINALS; i++) {
        // InitTerminal(i);
        term_states[i].tty_in = term_states[i].tty_out = term_states[i].user_in = term_states[i].user_out = -1;
    }
        
    return 1;
}

extern void ReceiveInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    char data = ReadDataRegister(term);
    term_states[term].tty_in++;

    if (data == '\b' || data == '\177') {
        if (!isEmpty(input_buffers[term]))
            deleteLast(input_buffers[term]);
        else {
            return;
        }
        
        if (outputRegister_states[term] == BUSY) {
            addBuf(pri_buffers[term], '\b');
            addBuf(pri_buffers[term], ' ');
            addBuf(pri_buffers[term], '\b');
        }
        else {
            addBuf(pri_buffers[term], ' ');
            addBuf(pri_buffers[term], '\b');
            outputRegister_states[term] = BUSY;
            WriteDataRegister(term, '\b');
            term_states[term].tty_out++;
        }
        
        return;
    }

    if (data == '\r') {
        data = '\n';
        CondSignal(line[term]);
    }

    if (!isFull(input_buffers[term])) {
        addBuf(input_buffers[term], data);
    }
    else if (data == '\n') {
        cutIn(input_buffers[term], data);
    }

    if (!isFull(echo_buffers[term])) {
        // if the write terminal cycle is not running, call the cycle manually
        if (outputRegister_states[term] == FREE) {
            if (data == '\n') {
                data = '\r';
                addBuf(pri_buffers[term], '\n');
            }
            outputRegister_states[term] = BUSY;
            WriteDataRegister(term, data);
            term_states[term].tty_out++;
        }
        else
            addBuf(echo_buffers[term], data);
    }
}

extern void TransmitInterrupt(int term) {
    Declare_Monitor_Entry_Procedure();
    // allow to transmit to output reg
    outputRegister_states[term] = FREE;

    char data = '\0';
    if (!isEmpty(pri_buffers[term])) {
        data = popBuf(pri_buffers[term]);
    }

    else if (!isEmpty(echo_buffers[term])) {
        data = popBuf(echo_buffers[term]);
        if (data == '\n') {
            data = '\r';
            addBuf(pri_buffers[term], '\n');
        }
    }

    else if (!isEmpty(output_buffers[term])) {
        data = popBuf(output_buffers[term]);
        if (data == '\n') {
            data = '\r';
            addBuf(pri_buffers[term], '\n');
        }
        CondSignal(output_full[term]);
    }
    if (data == '\0') {
        CondSignal(all_transmitted[term]);
        return;
    }

    // TODO: echo ‘\b \b’, 难点：不知道条件是什么，因为'\b'不会加入buffer
    outputRegister_states[term] = BUSY;
    WriteDataRegister(term, data);
    term_states[term].tty_out++;
}

extern int WriteTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    
    // if the current term is occupied for an ongoing WriteTerminal call, block the current call
    while (writeTerminal_states[term] == BUSY)
        CondWait(writingTerminal[term]);

    if (buflen == 0)
        return 0;

    // block other WriteTerminal call
    writeTerminal_states[term] = BUSY;
    
    int i;
    for (i = 0; i < buflen; i++) {
        char data = buf[i];
        
        // if the output buffer is full, block and release them to terminal first
        if (isFull(output_buffers[term])) {
            CondWait(output_full[term]);
        }

        addBuf(output_buffers[term], data);

        // make the first WriteDataRegister call to make the cycle running
        if (outputRegister_states[term] == FREE) {
            outputRegister_states[term] = BUSY;
            WriteDataRegister(term, popBuf(output_buffers[term]));
            term_states[term].tty_out++;
        }
    }

    // make sure all the characters from buf are transmitted successfully 
    while (!isEmpty(output_buffers[term]))
        CondWait(all_transmitted[term]);
    
    // give right to other WriteTerminal call to this terminal, signal them to compete if waiting
    writeTerminal_states[term] = FREE;
    CondSignal(writingTerminal[term]);
    term_states[term].user_in = term_states[term].user_in + i;
    return i;
}

extern int ReadTerminal(int term, char *buf, int buflen) {
    Declare_Monitor_Entry_Procedure();
    
    while (readTerminal_states[term] == BUSY)
        CondWait(readingTerminal[term]);

    if (buflen == 0)
        return 0;
    
    // block other ReadTerminal call
    readTerminal_states[term] = BUSY;

    // if the input buffer does not have a line, wait until it is
    while (getLines(input_buffers[term]) == 0)
        CondWait(line[term]);

    int i;
    for (i = 0; i < buflen; i++) {
        buf[i] = popBuf(input_buffers[term]);
        if (buf[i] == '\n') {
            i++;
            break;
        }
    }

    readTerminal_states[term] = FREE;
    CondSignal(readingTerminal[term]);
    term_states[term].user_out = term_states[term].user_out + i;
    return i;
}

extern int TerminalDriverStatistics(struct termstat *stats) {
    Declare_Monitor_Entry_Procedure();
    if (stats == NULL) {
        return -1;
    }
    
    for (int i = 0; i < NUM_TERMINALS; i++) {
        stats[i] = term_states[i];
    }
    
    return 0;
}

// buffer stuct 

static void initBuf(Buffer *b, int capacity) {
    b->items = (char *)malloc(capacity * sizeof(char));
    b->front = 0;
    b->rear = 0;
    b->size = 0;
    b->lines = 0;
    b->capacity = capacity;
}

static int isEmpty(Buffer *b) {
    return b->size == 0;
}

static int isFull(Buffer *b) {
    return b->size == b->capacity;
}

static void addBuf(Buffer *b, char item) {
    if (isFull(b)) {
        printf("Buffer is full\n");
        return;
    }
    b->items[b->rear] = item;
    b->rear = (b->rear + 1) % b->capacity;
    b->size++;
    if (item == '\n')
        b->lines++;
}

static char popBuf(Buffer *b) {
    if (isEmpty(b)) {
        printf("Buffer is empty\n");
        return -1;
    }
    char item = b->items[b->front];
    b->front = (b->front + 1) % b->capacity;
    b->size--;
    if (item == '\n')
        b->lines--;
    return item;
}

static void deleteLast(Buffer *b) {
    if (isEmpty(b)) {
        printf("Buffer is empty\n");
    }
    b->rear = b->rear - 1 < 0 ? b->capacity - 1 : b->rear - 1;
    b->size--;
}

static char cutIn(Buffer *b, char target) {
    if (isEmpty(b)) {
        printf("Buffer is empty\n");
        return 0;
    }
    int last = b->rear - 1 < 0 ? b->capacity - 1 : b->rear - 1;
    char item = b->items[last];
    b->items[last] = target;
    if (target == '\n')
        b->lines++;
    return item;
}

static int getLines(Buffer *b) {
    return b->lines;
}

// static char peek(Buffer *b) {
//     return b->items[b->front];
// }

// static void destroyBuffer(Buffer *b) {
//     free(b->items);
// }

