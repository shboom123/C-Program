#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

#define MAX_FIELDS     20
#define MAX_LEN        256
#define MAX_RECORDS    1000
#define MAX_FOLDERS    10
#define MAX_FNAME      512

typedef struct {
    char header[MAX_LEN];
    char value[MAX_LEN];
} Field;

typedef struct {
    Field fields[MAX_FIELDS];
    int   field_count;
    char  filename[MAX_FNAME];
} Record;

typedef struct {
    Record *records;
    int     count;
    int     capacity;
    char    folder[MAX_FNAME];
} Table;

Table* table_create(const char *folder) {
    Table t = (Table)malloc(sizeof(Table));
    t->records = (Record*)malloc(sizeof(Record) * MAX_RECORDS);
    t->count    = 0;
    t->capacity = MAX_RECORDS;
    strncpy(t->folder, folder, MAX_FNAME - 1);
    return t;
}

void table_free(Table *t) {
    if (t) { free(t->records); free(t); }
}

int parse_file(const char *filepath, Record *rec) {
    FILE *f = fopen(filepath, "r");
    if (!f) { printf("Cannot open: %s\n", filepath); return 0; }
    rec->field_count = 0;
    strncpy(rec->filename, filepath, MAX_FNAME - 1);
    char line[MAX_LEN * 2];
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' ||
                            line[len-1] == ','  || line[len-1] == ' ')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *key = line;
        char *val = colon + 1;
        while (*val == ' ') val++;
        if (rec->field_count < MAX_FIELDS) {
            strncpy(rec->fields[rec->field_count].header, key, MAX_LEN - 1);
            strncpy(rec->fields[rec->field_count].value,  val, MAX_LEN - 1);
            rec->field_count++;
        }
    }
    fclose(f);
    return rec->field_count > 0;
}

int load_folder(Table *t, const char *folder) {
    DIR *dir = opendir(folder);
    if (!dir) { printf("Cannot open folder: %s\n", folder); return 0; }
    struct dirent *entry;
    int loaded = 0;
    while ((entry = readdir(dir)) != NULL) {
        char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".txt") != 0) continue;
        char filepath[MAX_FNAME];
        snprintf(filepath, MAX_FNAME, "%s/%s", folder, entry->d_name);
        if (t->count < t->capacity) {
            if (parse_file(filepath, &t->records[t->count])) {
                t->count++;
                loaded++;
            }
        }
    }
    closedir(dir);
    printf("Loaded %d records from '%s'\n", loaded, folder);
    return loaded;
}

void save_record(Record *rec) {
    FILE *f = fopen(rec->filename, "w");
    if (!f) { printf("Cannot save: %s\n", rec->filename); return; }
    for (int i = 0; i < rec->field_count; i++)
        fprintf(f, "%s:%s,\n", rec->fields[i].header, rec->fields[i].value);
    fclose(f);
}

void save_all(Table *t) {
    for (int i = 0; i < t->count; i++) save_record(&t->records[i]);
    printf("Saved %d records to disk.\n", t->count);
}

void save_result(Table *t, const char *outfile) {
    FILE *f = fopen(outfile, "w");
    if (!f) { printf("Cannot create: %s\n", outfile); return; }
    if (t->count > 0) {
        for (int j = 0; j < t->records[0].field_count; j++)
            fprintf(f, "%-20s", t->records[0].fields[j].header);
        fprintf(f, "\n");
    }
    for (int i = 0; i < t->count; i++) {
        for (int j = 0; j < t->records[i].field_count; j++)
            fprintf(f, "%-20s", t->records[i].fields[j].value);
        fprintf(f, "\n");
    }
    fclose(f);
    printf("Result saved to '%s'\n", outfile);
}

void print_table(Table *t) {
    if (t->count == 0) { printf("  (no records)\n"); return; }
    for (int j = 0; j < t->records[0].field_count; j++)
        printf("%-20s", t->records[0].fields[j].header);
    printf("\n");
    for (int j = 0; j < t->records[0].field_count; j++)
        printf("--------------------");
    printf("\n");
    for (int i = 0; i < t->count; i++) {
        for (int j = 0; j < t->records[i].field_count; j++)
            printf("%-20s", t->records[i].fields[j].value);
        printf("\n");
    }
    printf("\nTotal: %d records\n", t->count);
}

int get_field_index(Record *rec, const char *header) {
    for (int i = 0; i < rec->field_count; i++)
        if (strcasecmp(rec->fields[i].header, header) == 0) return i;
    return -1;
}

const char* get_value(Record *rec, const char *header) {
    int idx = get_field_index(rec, header);
    return (idx >= 0) ? rec->fields[idx].value : "";
}

static char g_sort_header[MAX_LEN];
static int  g_sort_numeric = 0;

int compare_records(const void *a, const void *b) {
    const char va = get_value((Record)a, g_sort_header);
    const char vb = get_value((Record)b, g_sort_header);
    if (g_sort_numeric) { double da=atof(va),db=atof(vb); return (da>db)-(da<db); }
    return strcmp(va, vb);
}

double sort_table(Table *t, const char *header, int numeric) {
    strncpy(g_sort_header, header, MAX_LEN - 1);
    g_sort_numeric = numeric;
    clock_t start = clock();
    qsort(t->records, t->count, sizeof(Record), compare_records);
    return (double)(clock()-start)/CLOCKS_PER_SEC;
}

double insert_record(Table *t) {
    if (t->count >= t->capacity) { printf("Table full!\n"); return 0; }
    clock_t start = clock();
    Record *rec = &t->records[t->count];
    rec->field_count = 0;
    if (t->count > 0) {
        printf("Enter values:\n");
        for (int i = 0; i < t->records[0].field_count; i++) {
            char *hdr = t->records[0].fields[i].header;
            printf("  %s: ", hdr);
            char val[MAX_LEN]; fgets(val, MAX_LEN, stdin);
            val[strcspn(val,"\n")] = '\0';
            strncpy(rec->fields[i].header, hdr, MAX_LEN-1);
            strncpy(rec->fields[i].value,  val, MAX_LEN-1);
            rec->field_count++;
        }
    }
    snprintf(rec->filename, MAX_FNAME, "%s/new_%d.txt", t->folder, t->count+1);
    t->count++;
    printf("Record inserted.\n");
    return (double)(clock()-start)/CLOCKS_PER_SEC;
}

double delete_record(Table *t, const char *header, const char *value) {
    clock_t start = clock();
    int deleted = 0;
    for (int i = 0; i < t->count; ) {
        if (strcasecmp(get_value(&t->records[i], header), value) == 0) {
            for (int j = i; j < t->count-1; j++) t->records[j] = t->records[j+1];
            t->count--; deleted++;
        } else i++;
    }
    printf("Deleted %d record(s).\n", deleted);
    return (double)(clock()-start)/CLOCKS_PER_SEC;
}

double update_record(Table *t, const char *sh, const char *sv,
                     const char *uh, const char *uv) {
    clock_t start = clock();
    int updated = 0;
    for (int i = 0; i < t->count; i++) {
        if (strcasecmp(get_value(&t->records[i], sh), sv) == 0) {
            int idx = get_field_index(&t->records[i], uh);
            if (idx >= 0) { strncpy(t->records[i].fields[idx].value, uv, MAX_LEN-1); updated++; }
        }
    }
    printf("Updated %d record(s).\n", updated);
    return (double)(clock()-start)/CLOCKS_PER_SEC;
}

Table* inner_join(Table *t1, Table *t2, const char *key) {
    Table *r = table_create("join_result");
    for (int i = 0; i < t1->count; i++) {
        for (int j = 0; j < t2->count; j++) {
            if (strcasecmp(get_value(&t1->records[i],key), get_value(&t2->records[j],key))==0) {
                Record *nr = &r->records[r->count];
                nr->field_count = 0;
                strcpy(nr->filename, "join_result.txt");
                for (int k=0;k<t1->records[i].field_count;k++) nr->fields[nr->field_count++]=t1->records[i].fields[k];
                for (int k=0;k<t2->records[j].field_count;k++)
                    if (strcasecmp(t2->records[j].fields[k].header,key)!=0 && nr->field_count<MAX_FIELDS)
                        nr->fields[nr->field_count++]=t2->records[j].fields[k];
                r->count++;
                if (r->count>=r->capacity) goto done;
            }
        }
    }
    done: return r;
}

Table* left_join(Table *t1, Table *t2, const char *key) {
    Table *r = table_create("join_result");
    for (int i = 0; i < t1->count; i++) {
        int matched = 0;
        for (int j = 0; j < t2->count; j++) {
            if (strcasecmp(get_value(&t1->records[i],key), get_value(&t2->records[j],key))==0) {
                Record *nr = &r->records[r->count];
                nr->field_count = 0;
                strcpy(nr->filename,"join_result.txt");
                for (int k=0;k<t1->records[i].field_count;k++) nr->fields[nr->field_count++]=t1->records[i].fields[k];
                for (int k=0;k<t2->records[j].field_count;k++)
                    if (strcasecmp(t2->records[j].fields[k].header,key)!=0 && nr->field_count<MAX_FIELDS)
                        nr->fields[nr->field_count++]=t2->records[j].fields[k];
                r->count++; matched=1;
            }
        }
        if (!matched && r->count<r->capacity) { r->records[r->count]=t1->records[i]; r->count++; }
    }
    return r;
}

Table* full_join(Table *t1, Table *t2, const char *key) {
    Table *r = left_join(t1, t2, key);
    for (int j = 0; j < t2->count; j++) {
        int found = 0;
        for (int i = 0; i < t1->count; i++)
            if (strcasecmp(get_value(&t1->records[i],key), get_value(&t2->records[j],key))==0) { found=1; break; }
        if (!found && r->count<r->capacity) { r->records[r->count]=t2->records[j]; r->count++; }
    }
    return r;
}

void run_query(char *query, Table **tables, int table_count) {
    clock_t start = clock();
    char buf[1024]; strncpy(buf, query, 1023);
    char *tokens[30]; int tc=0;
    char *tok = strtok(buf," \t\n");
    while(tok&&tc<30){tokens[tc++]=tok;tok=strtok(NULL," \t\n");}
    if(tc<4){printf("Invalid query.\n");return;}
    if(strcasecmp(tokens[0],"SELECT")!=0){printf("Start with SELECT.\n");return;}
    char *cols=tokens[1];
    if(strcasecmp(tokens[2],"FROM")!=0){printf("Expected FROM.\n");return;}
    int t1=atoi(tokens[3]);
    if(t1<0||t1>=table_count){printf("Invalid table.\n");return;}
    Table *src=tables[t1], *jr=NULL; int free_jr=0;
    if(tc>=7&&(strcasecmp(tokens[4],"INNER_JOIN")==0||strcasecmp(tokens[4],"LEFT_JOIN")==0||strcasecmp(tokens[4],"FULL_JOIN")==0)){
        int t2=atoi(tokens[5]);
        if(tc<8||strcasecmp(tokens[6],"ON")!=0){printf("Expected ON.\n");return;}
        if(t2<0||t2>=table_count){printf("Invalid table 2.\n");return;}
        if(strcasecmp(tokens[4],"INNER_JOIN")==0) jr=inner_join(src,tables[t2],tokens[7]);
        else if(strcasecmp(tokens[4],"LEFT_JOIN")==0) jr=left_join(src,tables[t2],tokens[7]);
        else jr=full_join(src,tables[t2],tokens[7]);
        src=jr; free_jr=1;
    }
    char *wh=NULL,*wv=NULL;
    for(int i=4;i<tc-2;i++) if(strcasecmp(tokens[i],"WHERE")==0){wh=tokens[i+1];wv=(i+3<tc)?tokens[i+3]:tokens[i+2];break;}
    int select_all=(strcmp(cols,"*")==0);
    char proj[MAX_FIELDS][MAX_LEN]; int pc=0;
    if(!select_all){char tmp[MAX_LEN*MAX_FIELDS];strncpy(tmp,cols,sizeof(tmp)-1);char *c=strtok(tmp,",");while(c&&pc<MAX_FIELDS){strncpy(proj[pc++],c,MAX_LEN-1);c=strtok(NULL,",");}}
    printf("\n--- Query Result ---\n");
    int rows=0;
    for(int i=0;i<src->count;i++){
        Record *rec=&src->records[i];
        if(wh&&strcasecmp(get_value(rec,wh),wv)!=0) continue;
        if(select_all) for(int j=0;j<rec->field_count;j++) printf("%-15s: %s\n",rec->fields[j].header,rec->fields[j].value);
        else for(int p=0;p<pc;p++) printf("%-15s: %s\n",proj[p],get_value(rec,proj[p]));
        printf("\n"); rows++;
    }
    printf("Rows: %d | Time: %.6f sec\n\n",rows,(double)(clock()-start)/CLOCKS_PER_SEC);
    if(free_jr&&jr) table_free(jr);
}

void print_menu() {
    printf("\n========================================\n");
    printf("  Generic Data Management System\n");
    printf("========================================\n");
    printf(" 1. Load folder\n 2. Display table\n 3. Sort\n");
    printf(" 4. Insert record\n 5. Delete record\n 6. Update record\n");
    printf(" 7. Join tables\n 8. Run query\n 9. Save\n 0. Exit\n");
    printf("========================================\nChoice: ");
}

int main() {
    Table *tables[MAX_FOLDERS]; int table_count=0;
    char input[MAX_LEN]; double et;
    printf("\nWelcome to Generic Data Management System!\n");
    while(1) {
        print_menu();
        int ch; scanf("%d",&ch); getchar();
        if(ch==1){
            if(table_count>=MAX_FOLDERS){printf("Max tables.\n");continue;}
            printf("Folder path: "); fgets(input,MAX_LEN,stdin); input[strcspn(input,"\n")]='\0';
            tables[table_count]=table_create(input);
            if(load_folder(tables[table_count],input)>0){printf("Table %d loaded.\n",table_count);table_count++;}
            else table_free(tables[table_count]);
        }
        else if(ch==2){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx>=0&&idx<table_count) print_table(tables[idx]); else printf("Invalid.\n");
        }
        else if(ch==3){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx<0||idx>=table_count){printf("Invalid.\n");continue;}
            printf("Sort by header: "); fgets(input,MAX_LEN,stdin); input[strcspn(input,"\n")]='\0';
            printf("Numeric? (1=yes 0=no): "); int n; scanf("%d",&n); getchar();
            et=sort_table(tables[idx],input,n);
            printf("Sorted. Time: %.6f sec\n",et); print_table(tables[idx]);
        }
        else if(ch==4){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx<0||idx>=table_count){printf("Invalid.\n");continue;}
            et=insert_record(tables[idx]); printf("Time: %.6f sec\n",et);
        }
        else if(ch==5){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx<0||idx>=table_count){printf("Invalid.\n");continue;}
            printf("Header: "); char h[MAX_LEN]; fgets(h,MAX_LEN,stdin); h[strcspn(h,"\n")]='\0';
            printf("Value : "); char v[MAX_LEN]; fgets(v,MAX_LEN,stdin); v[strcspn(v,"\n")]='\0';
            et=delete_record(tables[idx],h,v); printf("Time: %.6f sec\n",et);
        }
        else if(ch==6){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx<0||idx>=table_count){printf("Invalid.\n");continue;}
            char sh[MAX_LEN],sv[MAX_LEN],uh[MAX_LEN],uv[MAX_LEN];
            printf("Find header: "); fgets(sh,MAX_LEN,stdin); sh[strcspn(sh,"\n")]='\0';
            printf("Equals    : "); fgets(sv,MAX_LEN,stdin); sv[strcspn(sv,"\n")]='\0';
            printf("Update hdr: "); fgets(uh,MAX_LEN,stdin); uh[strcspn(uh,"\n")]='\0';
            printf("New value : "); fgets(uv,MAX_LEN,stdin); uv[strcspn(uv,"\n")]='\0';
            et=update_record(tables[idx],sh,sv,uh,uv); printf("Time: %.6f sec\n",et);
        }
        else if(ch==7){
            printf("Table 1: "); int i1; scanf("%d",&i1); getchar();
            printf("Table 2: "); int i2; scanf("%d",&i2); getchar();
            if(i1<0||i1>=table_count||i2<0||i2>=table_count){printf("Invalid.\n");continue;}
            printf("1=inner 2=left 3=full: "); int jt; scanf("%d",&jt); getchar();
            printf("Join key: "); char key[MAX_LEN]; fgets(key,MAX_LEN,stdin); key[strcspn(key,"\n")]='\0';
            clock_t s=clock(); Table *jr;
            if(jt==1) jr=inner_join(tables[i1],tables[i2],key);
            else if(jt==2) jr=left_join(tables[i1],tables[i2],key);
            else jr=full_join(tables[i1],tables[i2],key);
            printf("Result: %d rows | Time: %.6f sec\n",jr->count,(double)(clock()-s)/CLOCKS_PER_SEC);
            print_table(jr);
            printf("Save? (1=yes): "); int sv2; scanf("%d",&sv2); getchar();
            if(sv2==1){printf("Filename: ");char fn[MAX_FNAME];fgets(fn,MAX_FNAME,stdin);fn[strcspn(fn,"\n")]='\0';save_result(jr,fn);}
            table_free(jr);
        }
        else if(ch==8){
            printf("Examples:\n  SELECT * FROM 0\n  SELECT Name,Maths FROM 0 WHERE Hindi = 90\n  SELECT * FROM 0 INNER_JOIN 1 ON RollNo\n");
            printf("Query: "); char q[1024]; fgets(q,1024,stdin); q[strcspn(q,"\n")]='\0';
            run_query(q,tables,table_count);
        }
        else if(ch==9){
            printf("Table index: "); int idx; scanf("%d",&idx); getchar();
            if(idx<0||idx>=table_count){printf("Invalid.\n");continue;}
            printf("1=original files  2=new file: "); int sv3; scanf("%d",&sv3); getchar();
            if(sv3==1) save_all(tables[idx]);
            else{printf("Filename: ");char fn[MAX_FNAME];fgets(fn,MAX_FNAME,stdin);fn[strcspn(fn,"\n")]='\0';save_result(tables[idx],fn);}
        }
        else if(ch==0){printf("Goodbye!\n");for(int i=0;i<table_count;i++)table_free(tables[i]);break;}
        else printf("Invalid choice.\n");
    }
    return 0;
}
