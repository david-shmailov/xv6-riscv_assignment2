//
// Created by David on 05/05/2022.
//

#include "linked_list.h"

sleeping_ls = init_linked_list()


 struct node init_node(uint proc_index){
    struct node node;
    node.proc_index =proc_index;
    node.next = NULL;
    return node;
}


struct list init_linked_list(){
    struct list ls;
    //ls.head = NULL;
    struct node dummy = init_node(0); // dummy
    ls.head = dummy;
    return ls;
}

void add(struct list ls, struct node node){
    struct node curr = ls.head;
    while(curr.next != NULL){
        curr = curr.next;
    }
    // todo pseudo code
    proc[node.proc_index].acquire()
    curr.next = node;
    proc[node.proc_index].release()
}

uint pop(struct list ls){
    struct node curr = ls.head;
    if curr.next == NULL{
        //some exception?
    }else {
        curr = curr.next; // skip dummy
    }
    // todo pseudo code
    proc[curr.proc_index].acquire()
    ls.head = curr.next;
    proc[curr.proc_index].release()

    return curr.proc_index;
}
uint remove(struct list ls, struct node node){

}

