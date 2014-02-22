/*
 *  np_list.h
 *  NetPhone
 *
 *  Created by DuanWei on 10-12-23.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */
#ifndef NP_LIST_H
#define NP_LIST_H

#include <sys/types.h>
#include <stdio.h>

#ifndef NULL
#define NULL 0
#endif

#ifndef bool_t
#define bool_t unsigned char
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#define np_message printf
#define np_warning printf
#define np_error printf


struct _NPList {
	struct _NPList *next;
	struct _NPList *prev;
	void *data;
};

typedef struct _NPList NPList;


#define np_list_next(elem) ((elem)->next)

#ifdef __cplusplus
extern "C"{
#endif


	void* np_malloc(size_t sz);
	void np_free(void *ptr);
	void* np_realloc(void *ptr, size_t sz);
	void* np_malloc0(size_t sz);
	
	
#define np_new(type,count)	(type*)np_malloc(sizeof(type)*(count))
#define np_new0(type,count)	(type*)np_malloc0(sizeof(type)*(count))
	
	
	NPList * np_list_append(NPList *elem, void * data);
	NPList * np_list_prepend(NPList *elem, void * data);
	NPList * np_list_free(NPList *elem);
	NPList * np_list_concat(NPList *first, NPList *second);
	NPList * np_list_remove(NPList *first, void *data);
	int np_list_size(const NPList *first);
	void np_list_for_each(const NPList *list, void (*func)(void *));
	void np_list_for_each2(const NPList *list, void (*func)(void *, void *), void *user_data);
	NPList *np_list_remove_link(NPList *list, NPList *elem);
	NPList *np_list_find(NPList *list, void *data);
	NPList *np_list_find_custom(NPList *list, int (*compare_func)(const void *, const void*), void *user_data);
	void * np_list_nth_data(const NPList *list, int index);
	int np_list_position(const NPList *list, NPList *elem);
	int np_list_index(const NPList *list, void *data);
	NPList *np_list_insert_sorted(NPList *list, void *data, int (*compare_func)(const void *, const void*));
	NPList *np_list_insert(NPList *list, NPList *before, void *data);
	NPList *np_list_copy(const NPList *list);

#ifdef __cplusplus
}
#endif

#endif