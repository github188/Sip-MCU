/***************************************************************************
*            npconfig.c
*			 配置参数的 读取/保存
****************************************************************************/

#define MAX_LEN 1024
#include "np_list.h"
#include "np_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <arpa/inet.h>
#endif // WIN32

#ifdef WIN32
#define snprintf _snprintf
#endif

#ifdef HAVE_SQLITE3
#define SQLITE3_KEY "dww1982"
#define SQLITE3_KEY_LEN strlen(SQLITE3_KEY)
#include "sqlite3.h"
#endif // HAVE_SQLITE3

#define LISTNODE(_struct_)	\
	struct _struct_ *_prev;\
	struct _struct_ *_next;

typedef struct _ListNode{
	LISTNODE(_ListNode)
} ListNode;

typedef void (*ListNodeForEachFunc)(ListNode *);

ListNode * list_node_append(ListNode *head,ListNode *elem){
	ListNode *e=head;
	while(e->_next!=NULL) e=e->_next;
	e->_next=elem;
	elem->_prev=e;
	return head;
}

ListNode * list_node_remove(ListNode *head, ListNode *elem){
	ListNode *before,*after;
	before=elem->_prev;
	after=elem->_next;
	if (before!=NULL) before->_next=after;
	if (after!=NULL) after->_prev=before;
	elem->_prev=NULL;
	elem->_next=NULL;
	if (head==elem) return after;
	return head;
}

void list_node_foreach(ListNode *head, ListNodeForEachFunc func){
	for (;head!=NULL;head=head->_next){
		func(head);
	}
}


#define LIST_PREPEND(e1,e2) (  (e2)->_prev=NULL,(e2)->_next=(e1),(e1)->_prev=(e2),(e2) )
#define LIST_APPEND(head,elem) ((head)==0 ? (elem) : (list_node_append((ListNode*)(head),(ListNode*)(elem)), (head)) ) 
#define LIST_REMOVE(head,elem) 

/* returns void */
#define LIST_FOREACH(head) list_node_foreach((ListNode*)head)

typedef struct _LpItem{
	char *key;
	char *value;
	char *old_value;
	bool_t modified;
} LpItem;

typedef struct _LpSection{
	char *name;
	NPList *items;
} LpSection;

struct _NetPhoneConfig{
	char *filename;
#ifdef HAVE_SQLITE3
	sqlite3 *db;
#else
	FILE *file;
#endif // HAVE_SQLITE3
	NPList *sections;
	int modified;
};

#ifdef HAVE_SQLITE3
void optimize_database(sqlite3 *db)
{
	sqlite3_exec(db,"PRAGMA synchronous = OFF",0,0,0);//如果有定期备份的机制，而且少量数据丢失可接受，用OFF
	sqlite3_exec(db,"PRAGMA cache_size = 8000",0,0,0); //建议改为8000
	sqlite3_exec(db,"PRAGMA case_sensitive_like=1",0,0,0);//搜索中文字串
}

// TRUE: create new table   FALSE: table Exists
bool_t sqlite3_table_new(sqlite3 *db)
{
	char szSQL[] ="CREATE TABLE [ua_config] ( [section] CHAR(128), [var_name] CHAR(256), [var_value] CHAR(256));";
	int res = sqlite3_exec(db,szSQL,0,0, 0);

	if (res == SQLITE_OK) optimize_database(db);

	return (res == SQLITE_OK)? TRUE : FALSE;
}

bool_t sqlite3_table_empty(sqlite3 *db)
{
	char szSQL[] ="DELETE FROM  ua_config;";
	int res = sqlite3_exec(db,szSQL,0,0, 0);
	return (res == SQLITE_OK)? TRUE : FALSE;
}
#endif
LpItem * np_item_new(const char *key, const char *value,bool_t changed){
	LpItem *item=np_new0(LpItem,1);
	item->key=strdup(key);
	item->value=strdup(value);
	item->old_value=strdup(value);
	item->modified = changed;
	return item;
}

LpSection *np_section_new(const char *name){
	LpSection *sec=np_new0(LpSection,1);
	sec->name=strdup(name);
	return sec;
}

bool_t np_item_modified(LpItem * item){
	if (strcmp(item->value,item->old_value) || item->modified)
	    return TRUE;
	else
		return FALSE;
}


void np_item_destroy(void *pitem){
	LpItem *item=(LpItem*)pitem;
	np_free(item->key);
	np_free(item->value);
	np_free(item->old_value);
	np_free(item);
}

void np_section_destroy(LpSection *sec){
	np_free(sec->name);
	np_list_for_each(sec->items,np_item_destroy);
	np_list_free(sec->items);
	np_free(sec);
}

void np_section_add_item(LpSection *sec,LpItem *item){
	sec->items=np_list_append(sec->items,(void *)item);
}

void np_config_add_section(NetPhoneConfig *npconfig, LpSection *section){
	npconfig->sections=np_list_append(npconfig->sections,(void *)section);
}

void np_config_remove_section(NetPhoneConfig *npconfig, LpSection *section){
#ifdef HAVE_SQLITE3
	char szSQL[128];
	sprintf(szSQL,"DELETE FROM ua_config WHERE  section = '%s';",section->name);

	if(!npconfig->db)
	{
		if (sqlite3_open(npconfig->filename,&npconfig->db) == SQLITE_OK)
		{
			sqlite3_key(npconfig->db,SQLITE3_KEY,SQLITE3_KEY_LEN);
			sqlite3_exec(npconfig->db,szSQL,0,0, 0);
			np_message("np_config_remove_section: sqlite3_exec %s\n", szSQL);
			sqlite3_close(npconfig->db);
		}
	  npconfig->db = NULL;	
	}else
	{
		sqlite3_exec(npconfig->db,szSQL,0,0, 0);
		np_message("np_config_remove_section: sqlite3_exec %s\n", szSQL);
	}
	
#endif
	npconfig->sections=np_list_remove(npconfig->sections,(void *)section);
	np_section_destroy(section);
}

static bool_t is_first_char(const char *start, const char *pos){
	const char *p;
	for(p=start;p<pos;p++){
		if (*p!=' ') return FALSE;
	}
	return TRUE;
}

#ifdef HAVE_SQLITE3

/************************************************************************/
/*
第一步 查询 section 总条目
SELECT DISTINCT section FROM ua_config;
第二步根据每个 section 查询出 var_name, var_value 
SELECT var_name, var_value FROM ua_config WHERE section = 'sound';*/
/************************************************************************/
void np_config_parse(NetPhoneConfig *npconfig){
	int nrow = 0, ncolumn = 0;
	char **sectionResult; //二维数组存放结果
//	int nIndex=0;
	LpSection *cur=NULL;

	char szSQL[] = "SELECT DISTINCT section FROM ua_config;";
	if(sqlite3_get_table(npconfig->db, szSQL ,&sectionResult ,&nrow ,&ncolumn ,0) == SQLITE_OK)
	{
		int index = ncolumn;
		int i,j;
		for(i=0;i<nrow;i++){
			for(j=0;j<ncolumn;j++)
			{			  
				if ( strcmp(sectionResult[j],"section") == 0)
				{
					int itemnrow = 0, itemncolumn = 0;
					char **itemResult; //二维数组存放结果
//					int itemIndex=0;
					char szSQLitem[128];
					
					cur = np_section_new(sectionResult[index]);
					np_config_add_section(npconfig,cur);

					sprintf(szSQLitem,"SELECT var_name, var_value FROM ua_config WHERE section = '%s';",sectionResult[index]);

					if(sqlite3_get_table(npconfig->db, szSQLitem ,&itemResult ,&itemnrow ,&itemncolumn ,0) == SQLITE_OK)
					{
						 int idx = itemncolumn;
						 int k,l;
						 char *val_name;
						 char *val_value;
						 val_name = val_value = NULL;
						 
						 for(k=0;k<itemnrow;k++){
							 for(l=0;l<itemncolumn;l++)
							 {	
								 if ( strcmp(itemResult[l],"var_name") == 0)
								 {
									 val_name = itemResult[idx];
								 }
								 
								 if ( strcmp(itemResult[l],"var_value") == 0)
								 {
									 val_value = itemResult[idx];
								 }

								 ++idx;
							 }
							 np_message("np_config_parse section: %s, var_name: %s, var_value: %s\n",cur->name,val_name,val_value);
							 np_section_add_item(cur,np_item_new(val_name,val_value,FALSE));

						 }

						 sqlite3_free_table(itemResult);
					}
					
				}
				++index;
			}
		}

		sqlite3_free_table(sectionResult);
	}


}

#else // else HAVE_SQLITE3
void np_config_parse(NetPhoneConfig *npconfig){
	char tmp[MAX_LEN];
	LpSection *cur=NULL;
	if (npconfig->file==NULL) return;
	while(fgets(tmp,MAX_LEN,npconfig->file)!=NULL){
		char *pos1,*pos2;
		pos1=strchr(tmp,'[');
		if (pos1!=NULL && is_first_char(tmp,pos1) ){
			pos2=strchr(pos1,']');
			if (pos2!=NULL){
				int nbs;
				char secname[MAX_LEN];
				secname[0]='\0';
				/* found section */
				*pos2='\0';
				nbs = sscanf(pos1+1,"%s",secname);
				if (nbs == 1 ){
					if (strlen(secname)>0){
						cur=np_section_new(secname);
						np_config_add_section(npconfig,cur);
					}
				}else{
					np_warning("parse error!\n");
				}
			}
		}else {
			pos1=strchr(tmp,'=');
			if (pos1!=NULL){
				char key[MAX_LEN];
				key[0]='\0';
				
				*pos1='\0';
				if (sscanf(tmp,"%s",key)>0){
					
					pos1++;
					pos2=strchr(pos1,'\n');
					if (pos2==NULL) pos2=pos1+strlen(pos1);
					else {
						*pos2='\0'; /*replace the '\n' */
						pos2--;
					}
					/* remove ending white spaces */
					for (; pos2>pos1 && *pos2==' ';pos2--) *pos2='\0';
					if (pos2-pos1>=0){
						/* found a pair key,value */
						if (cur!=NULL){
							np_section_add_item(cur,np_item_new(key,pos1,FALSE));
							/*printf("Found %s %s=%s\n",cur->name,key,pos1);*/
						}else{
							np_warning("found key,item but no sections\n");
						}
					}
				}
			}
		}
	}
}
#endif // HAVE_SQLITE3


NetPhoneConfig * np_config_new(const char *filename){
	NetPhoneConfig *npconfig=np_new0(NetPhoneConfig,1);
	if (filename!=NULL){
		npconfig->filename=strdup(filename);
#ifdef HAVE_SQLITE3
		npconfig->db = NULL;
		if (sqlite3_open(filename,&npconfig->db) == SQLITE_OK){
			sqlite3_key(npconfig->db,SQLITE3_KEY,SQLITE3_KEY_LEN);

			if(!sqlite3_table_new(npconfig->db));
				np_config_parse(npconfig);

			sqlite3_close(npconfig->db);
		}else{
			//np_warning("Can't open database: %s\n", sqlite3_errmsg(npconfig->db));
		}
		npconfig->db = NULL;
#else
#ifdef _MSC_VER
		npconfig->file=fopen(npconfig->filename,"a+b");
#else
		npconfig->file=fopen(npconfig->filename,"rw");
#endif // _MSC_VER
		if (npconfig->file!=NULL){
			np_config_parse(npconfig);
			fclose(npconfig->file);

			/* make existing configuration files non-group/world-accessible */
			//if (chmod(filename, S_IRUSR | S_IWUSR) == -1)
			//	np_warning("unable to correct permissions on "
			//	  	  "configuration file: %s",
			//		   strerror(errno));
			npconfig->file=NULL;
			npconfig->modified=0;
		}
#endif // HAVE_SQLITE3

	}
	return npconfig;
}

void np_item_set_value(LpItem *item, const char *value){
	np_free(item->value);
	item->value=strdup(value);
	item->modified = np_item_modified(item);
}


void np_config_destroy(NetPhoneConfig *npconfig){
	if (npconfig->filename!=NULL) np_free(npconfig->filename);
	np_list_for_each(npconfig->sections,(void (*)(void*))np_section_destroy);
	np_list_free(npconfig->sections);
	np_free(npconfig);
}

LpSection *np_config_find_section(NetPhoneConfig *npconfig, const char *name){
	LpSection *sec;
	NPList *elem;
	/*printf("Looking for section %s\n",name);*/
	for (elem=npconfig->sections;elem!=NULL;elem=np_list_next(elem)){
		sec=(LpSection*)elem->data;
		if (strcmp(sec->name,name)==0){
			/*printf("Section %s found\n",name);*/
			return sec;
		}
	}
	return NULL;
}

LpItem *np_section_find_item(LpSection *sec, const char *name){
	NPList *elem;
	LpItem *item;
	/*printf("Looking for item %s\n",name);*/
	for (elem=sec->items;elem!=NULL;elem=np_list_next(elem)){
		item=(LpItem*)elem->data;
		if (strcmp(item->key,name)==0) {
			/*printf("Item %s found\n",name);*/
			return item;
		}
	}
	return NULL;
}

void np_section_remove_item(LpSection *sec, LpItem *item){
	sec->items=np_list_remove(sec->items,(void *)item);
	np_item_destroy(item);
}

const char *np_config_get_string(NetPhoneConfig *npconfig, const char *section, const char *key, const char *default_string){
	LpSection *sec;
	LpItem *item;
	sec=np_config_find_section(npconfig,section);
	if (sec!=NULL){
		item=np_section_find_item(sec,key);
		if (item!=NULL) return item->value;
	}
	return default_string;
}

int np_config_get_int(NetPhoneConfig *npconfig,const char *section, const char *key, int default_value){
	const char *str=np_config_get_string(npconfig,section,key,NULL);
	if (str!=NULL) return atoi(str);
	else return default_value;
}

float np_config_get_float(NetPhoneConfig *npconfig,const char *section, const char *key, float default_value){
	const char *str=np_config_get_string(npconfig,section,key,NULL);
	float ret=default_value;
	if (str==NULL) return default_value;
	sscanf(str,"%f",&ret);
	return ret;
}

void np_config_set_string(NetPhoneConfig *npconfig,const char *section, const char *key, const char *value){
	LpItem *item;
	LpSection *sec=np_config_find_section(npconfig,section);
	if (sec!=NULL){
		item=np_section_find_item(sec,key);
		if (item!=NULL){
			if (value!=NULL)
				np_item_set_value(item,value);
			else np_section_remove_item(sec,item);
		}else{
			if (value!=NULL)
				np_section_add_item(sec,np_item_new(key,value,TRUE));
		}
	}else if (value!=NULL){
		sec=np_section_new(section);
		np_config_add_section(npconfig,sec);
		np_section_add_item(sec,np_item_new(key,value,TRUE));
	}
	npconfig->modified++;
}

void np_config_set_int(NetPhoneConfig *npconfig,const char *section, const char *key, int value){
	char tmp[30];
	snprintf(tmp,30,"%i",value);
	np_config_set_string(npconfig,section,key,tmp);
	npconfig->modified++;
}

//此处替换成sqlite3 insert
#ifdef HAVE_SQLITE3
void np_item_write(LpItem *item, sqlite3 *db,const char *section){
	char szSQL[128];
	if (np_item_modified(item))
	{
		sprintf(szSQL,
			"DELETE FROM ua_config WHERE  section = '%s' AND var_name = '%s' AND var_value = '%s';",
			section,item->key,item->old_value);

		if (sqlite3_exec(db,szSQL,0,0, 0) != SQLITE_OK){
			np_warning("np_item_write,write section to database erro: %s\n", sqlite3_errmsg(db));
		}else {
			np_message("np_item_write del old value: %s\n",szSQL);
		}


		sprintf(szSQL,
			"INSERT INTO [ua_config] values('%s', '%s', '%s');",
			section,item->key,item->value);

		np_message("np_item_write section: %s\n",szSQL);

		if (sqlite3_exec(db,szSQL,0,0, 0) != SQLITE_OK){
			np_warning("np_item_write,write section to database erro: %s\n", sqlite3_errmsg(db));
		}else{
			np_free(item->old_value);
			item->old_value = strdup(item->value);
			item->modified = FALSE;
		}
		
	}else
	{
		np_message("np_item_write section: %s, name: %s, value: %s no changed, skip to update!\n",section,item->key,item->value);
	}
}
#else
void np_item_write(LpItem *item, FILE *file){
	fprintf(file,"%s=%s\n",item->key,item->value);
}
#endif // HAVE_SQLITE3

//此处替换成更新和写入sqlite3设置
#ifdef HAVE_SQLITE3
void np_list_for_each3(const NPList *list, void (*func)(void *, void *, void *), void *user_data,void *user_data2){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data,user_data2);
	}
}
void np_section_write(LpSection *sec, sqlite3 *db){
	np_list_for_each3(sec->items,(void (*)(void*, void*, void *))np_item_write,(void *)db,(void *)sec->name);
}
#else
void np_section_write(LpSection *sec, FILE *file){
	fprintf(file,"[%s]\n",sec->name);
	np_list_for_each2(sec->items,(void (*)(void*, void*))np_item_write,(void *)file);
	fprintf(file,"\n");
}
#endif // HAVE_SQLITE

int np_config_sync(NetPhoneConfig *npconfig){
#ifdef HAVE_SQLITE3
	npconfig->db = NULL;
	if (npconfig->filename==NULL) return -1;
	
	if (sqlite3_open(npconfig->filename,&npconfig->db) == SQLITE_OK)
	{
		sqlite3_key(npconfig->db,SQLITE3_KEY,SQLITE3_KEY_LEN);

		if (!sqlite3_table_new(npconfig->db))
		{
			
		}

		np_list_for_each2(npconfig->sections,(void (*)(void *,void*))np_section_write,(void *)npconfig->db);
		sqlite3_close(npconfig->db);
	}else{
		np_warning("Can't open database: %s\n", sqlite3_errmsg(npconfig->db));
	}
	npconfig->db = NULL;
#else
	FILE *file;
	if (npconfig->filename==NULL) return -1;
#ifndef WIN32
	/* don't create group/world-accessible files */
	(void) umask(S_IRWXG | S_IRWXO);
#endif

#ifdef _MSC_VER
	file=fopen(npconfig->filename,"w+b");
#else
	file=fopen(npconfig->filename,"w");
#endif // _MSC_VER

	if (file==NULL){
		np_warning("Could not write %s !\n",npconfig->filename);
		return -1;
	}
	np_list_for_each2(npconfig->sections,(void (*)(void *,void*))np_section_write,(void *)file);
	fclose(file);
#endif // HAVE_SQLITE3
	npconfig->modified=0;
	return 0;
}

int np_config_has_section(NetPhoneConfig *npconfig, const char *section){
	if (np_config_find_section(npconfig,section)!=NULL) return 1;
	return 0;
}

void np_config_clean_section(NetPhoneConfig *npconfig, const char *section){
	LpSection *sec=np_config_find_section(npconfig,section);
	if (sec!=NULL){
		np_config_remove_section(npconfig,sec);
	}
	npconfig->modified++;
}

int np_config_needs_commit(const NetPhoneConfig *npconfig){
	return npconfig->modified>0;
}
