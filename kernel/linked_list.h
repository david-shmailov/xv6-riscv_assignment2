//
// Created by David on 05/05/2022.
//

#ifndef XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H
#define XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H

#endif //XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H

struct node {
    struct node next; //instead of pointers, we can use index of the node in the proc[] array
    uint proc_index;
};
struct node init_node(uint proc_index);

struct list {
    struct node head;
    struct spinlock first_lock;
};
struct list init_linked_list();
void add(struct list ls, struct node node);
void remove(struct list ls, struct node node);
uint pop(struct list ls);