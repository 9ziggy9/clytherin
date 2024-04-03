#include <sqlite3.h>
#include <stdint.h>

#define MAX_PATH_LEN 260

void open_db_or_die(sqlite3 **);
void close_db(sqlite3 *);
void create_table_users(sqlite3 *);
void create_table_messages(sqlite3 *);
sqlite3_int64 insert_user(sqlite3 *, const char *);
sqlite3_int64 insert_message(sqlite3 *, const char *, int, int);
void commit_from_memory(sqlite3 *, const char *);
