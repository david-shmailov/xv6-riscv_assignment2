//
// Created by David on 05/05/2022.
//

#include "linked_list.h"
 struct node init_node(uint proc_index){
    struct node node;
    node.proc_index =proc_index;
    node.next = NULL;
    return node;
}


struct list init_linked_list(){
    struct list ls;
    ls.head = NULL;
    return ls;
}

void add(struct list ls, struct node node){
    struct node curr = ls.head;
    while(curr.next != NULL){
        curr = curr.next;
    }
    curr.next = node;
}

uint pop(struct list ls){
    struct node curr = ls.head;
    ls.head = curr.next;
    return curr.proc_index;
}
uint remove(struct list ls, struct node node){

}

