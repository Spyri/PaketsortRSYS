//
// Created by Felix Bronnhuber on 13.05.24.
//
#include <stdio.h>
#include <stdlib.h>

#define MAX_SIZE 100

// Define the structure for a queue node
typedef struct {
    int data;
} QueueNode;

// Define the structure for the queue
typedef struct {
    QueueNode items[MAX_SIZE];
    int front;
    int rear;
    int size;
} Queue;

// Function to initialize the queue
void initQueue(Queue *queue) {
    queue->front = 0;
    queue->rear = -1;
    queue->size = 0;
}

// Function to check if the queue is empty
int isEmpty(Queue *queue) {
    return queue->size == 0;
}

// Function to check if the queue is full
int isFull(Queue *queue) {
    return queue->size == MAX_SIZE;
}

// Function to enqueue an element into the queue
void enqueue(Queue *queue, int data) {
    if (isFull(queue)) {
        printf("Queue is full. Cannot enqueue.\n");
        return;
    }
    queue->rear = (queue->rear + 1) % MAX_SIZE;
    queue->items[queue->rear].data = data;
    queue->size++;
}

// Function to dequeue an element from the queue
int dequeue(Queue *queue) {
    if (isEmpty(queue)) {
        printf("Queue is empty. Cannot dequeue.\n");
        return -1;
    }
    int data = queue->items[queue->front].data;
    queue->front = (queue->front + 1) % MAX_SIZE;
    queue->size--;
    return data;
}

// Function to display the elements of the queue
void display(Queue *queue) {
    if (isEmpty(queue)) {
        printf("Queue is empty.\n");
        return;
    }
    printf("Queue elements: ");
    int i = queue->front;
    int count = 0;
    while (count < queue->size) {
        printf("%d ", queue->items[i].data);
        i = (i + 1) % MAX_SIZE;
        count++;
    }
    printf("\n");
}

int main() {
    Queue queue;
    initQueue(&queue);

    enqueue(&queue, 10);
    enqueue(&queue, 20);
    enqueue(&queue, 30);
    enqueue(&queue, 40);

    display(&queue);

    printf("Dequeued element: %d\n", dequeue(&queue));
    printf("Dequeued element: %d\n", dequeue(&queue));

    display(&queue);

    return 0;
}