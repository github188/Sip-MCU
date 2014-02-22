/*
 *  np_list.c
 *  NetPhone
 *
 *  Created by DuanWei on 10-12-23.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "np_list.h"
#include <sys/types.h>
#include <stdlib.h>
#include <memory.h>

void* np_malloc(size_t sz)
{
	return malloc(sz);
}

void np_free(void *ptr)
{
	free(ptr);
}

void* np_realloc(void *ptr, size_t sz)
{
	return realloc(ptr,sz);
}

void* np_malloc0(size_t sz)
{
	void *ptr=np_malloc(sz);
	memset(ptr,0,sz);
	return ptr;
}


NPList *np_list_new(void *data){
	NPList *new_elem=(NPList *)np_new(NPList,1);
	new_elem->prev=new_elem->next=NULL;
	new_elem->data=data;
	return new_elem;
}

NPList * np_list_append(NPList *elem, void * data){
	NPList *new_elem=np_list_new(data);
	NPList *it=elem;
	if (elem==NULL) return new_elem;
	while (it->next!=NULL) it=np_list_next(it);
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

NPList * np_list_prepend(NPList *elem, void *data){
	NPList *new_elem=np_list_new(data);
	if (elem!=NULL) {
		new_elem->next=elem;
		elem->prev=new_elem;
	}
	return new_elem;
}


NPList * np_list_concat(NPList *first, NPList *second){
	NPList *it=first;
	if (it==NULL) return second;
	while(it->next!=NULL) it=np_list_next(it);
	it->next=second;
	second->prev=it;
	return first;
}

NPList * np_list_free(NPList *list){
	NPList *elem = list;
	NPList *tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		np_free(tmp);
	}
	np_free(elem);
	return NULL;
}

NPList * np_list_remove(NPList *first, void *data){
	NPList *it;
	it=np_list_find(first,data);
	if (it) return np_list_remove_link(first,it);
	else {
		np_warning("np_list_remove: no element with %p data was in the list", data);
		return first;
	}
}

int np_list_size(const NPList *first){
	int n=0;
	while(first!=NULL){
		++n;
		first=first->next;
	}
	return n;
}

void np_list_for_each(const NPList *list, void (*func)(void *)){
	for(;list!=NULL;list=list->next){
		func(list->data);
	}
}

void np_list_for_each2(const NPList *list, void (*func)(void *, void *), void *user_data){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data);
	}
}

NPList *np_list_remove_link(NPList *list, NPList *elem){
	NPList *ret;
	if (elem==list){
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		np_free(elem);
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) elem->next->prev=elem->prev;
	elem->next=NULL;
	elem->prev=NULL;
	np_free(elem);
	return list;
}

NPList *np_list_find(NPList *list, void *data){
	for(;list!=NULL;list=list->next){
		if (list->data==data) return list;
	}
	return NULL;
}

NPList *np_list_find_custom(NPList *list, int (*compare_func)(const void *, const void*), void *user_data){
	for(;list!=NULL;list=list->next){
		if (compare_func(list->data,user_data)==0) return list;
	}
	return NULL;
}

void * np_list_nth_data(const NPList *list, int index){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (i==index) return list->data;
	}
	np_error("np_list_nth_data: no such index in list.\n");
	return NULL;
}

int np_list_position(const NPList *list, NPList *elem){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (elem==list) return i;
	}
	np_error("np_list_position: no such element in list.\n");
	return -1;
}

int np_list_index(const NPList *list, void *data){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (data==list->data) return i;
	}
	np_error("np_list_index: no such element in list.\n");
	return -1;
}

NPList *np_list_insert_sorted(NPList *list, void *data, int (*compare_func)(const void *, const void*)){
	NPList *it,*previt=NULL;
	NPList *nelem;
	NPList *ret=list;
	if (list==NULL) return np_list_append(list,data);
	else{
		nelem=np_list_new(data);
		for(it=list;it!=NULL;it=it->next){
			previt=it;
			if (compare_func(data,it->data)<=0){
				nelem->prev=it->prev;
				nelem->next=it;
				if (it->prev!=NULL)
					it->prev->next=nelem;
				else{
					ret=nelem;
				}
				it->prev=nelem;
				return ret;
			}
		}
		previt->next=nelem;
		nelem->prev=previt;
	}
	return ret;
}

NPList *np_list_insert(NPList *list, NPList *before, void *data){
	NPList *elem;
	if (list==NULL || before==NULL) return np_list_append(list,data);
	for(elem=list;elem!=NULL;elem=np_list_next(elem)){
		if (elem==before){
			if (elem->prev==NULL)
				return np_list_prepend(list,data);
			else{
				NPList *nelem=np_list_new(data);
				nelem->prev=elem->prev;
				nelem->next=elem;
				elem->prev->next=nelem;
				elem->prev=nelem;
			}
		}
	}
	return list;
}

NPList *np_list_copy(const NPList *list){
	NPList *copy=NULL;
	const NPList *iter;
	for(iter=list;iter!=NULL;iter=np_list_next(iter)){
		copy=np_list_append(copy,iter->data);
	}
	return copy;
}
