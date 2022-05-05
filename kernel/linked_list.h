//
// Created by David on 05/05/2022.
//

#ifndef XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H
#define XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H

#endif //XV6_RISCV_ASSIGNMENT2_LINKED_LIST_H

struct node {
    uint32 next_node_index; //instead of pointers, we can use index of the node in the proc[] array
    struct proc *proc;
};

struct list {
    struct node *head;
};
