//
// Created by David on 05/05/2022.
//
#include "spinlock.h"
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
void remove(struct list ls, struct node node){
    struct node pred = ls.head;
    acquire(&porc[pred.proc_index]->lock_linked_list);
    struct node curr = pred.next;
    acquire(&porc[curr.proc_index]->lock_linked_list);
    while(curr.proc_index != node.proc_index){
        release(&porc[pred.proc_index]->lock_linked_list);
        pred = curr;
        curr = curr.next;
        acquire(&porc[curr.proc_index]->lock_linked_list);
    }
    pred.next = curr.next;
    release(&porc[pred.proc_index]->lock_linked_list);
    release(&porc[curr.proc_index]->lock_linked_list);
}

