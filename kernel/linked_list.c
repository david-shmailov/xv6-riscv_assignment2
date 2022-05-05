//
// Created by David on 05/05/2022.
//
#include "spinlock.h"
#include "linked_list.h"
 struct node init_node(uint proc_index){
    struct node node;
    node.proc_index =proc_index;
    node.next = NULL;
    return node;
}


struct list init_linked_list(struct spinlock lock){
    struct list ls;
    struct node first_node;
    first_node.proc_index=100;
    first_node.next =NULL;
    ls.head = first_node;
    ls.first_lock =lock;
    return ls;
}

void add(struct list ls, struct node node){
    struct node curr = ls.head;
    while(curr.next != NULL){
        curr = curr.next;
    }
    acquire(&porc[curr.proc_index]->lock_linked_list);
    curr.next = node;
    release(&porc[curr.proc_index]->lock_linked_list);
}

uint pop(struct list ls){
    struct node pred = ls.head;
    acquire(ls.first_lock);
    struct node curr = pred.next;
    acquire(&porc[curr.proc_index]->lock_linked_list);
    ls.head = curr.next;
    release(ls.first_lock);
    release(&porc[curr.proc_index]->lock_linked_list);
    return curr.proc_index;
}
void remove(struct list ls, struct node node){
    struct node pred = ls.head;
    int flag = 0;
    acquire(ls.first_lock);
    struct node curr = pred.next;
    acquire(&porc[curr.proc_index]->lock_linked_list);
    while(curr.proc_index != node.proc_index){
        if(flag == 0){
            release(ls.first_lock);
            flag++;
        }
        else
            release(&porc[pred.proc_index]->lock_linked_list);
        pred = curr;
        curr = curr.next;
        acquire(&porc[curr.proc_index]->lock_linked_list);
    }
    pred.next = curr.next;
    if(flag == 0){
        release(ls.first_lock);
        flag++;
    }
    else
        release(&porc[pred.proc_index]->lock_linked_list);
    release(&porc[curr.proc_index]->lock_linked_list);
}

