#include <stdio.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mkdir

void open_db_or_die(sqlite3 **db) {
  if(sqlite3_open(":memory:", db) != SQLITE_OK) {
    fprintf(stderr,
            "%s() :: cannot open database: %s\n",
            __func__,
            sqlite3_errmsg(*db));
    exit(EXIT_FAILURE);
  }
  fprintf(stdout,
          "%s() :: successfully opened database.\n",
          __func__);
}

void close_db(sqlite3 *db) {
  sqlite3_close(db);
  fprintf(stdout, "%s() :: closed database.\n", __func__);
  exit(EXIT_SUCCESS);
}

void new_message_table(sqlite3 *db) {
  char *err = 0;
  const char *sql = "CREATE TABLE IF NOT EXISTS msgs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "message TEXT NOT NULL, "
                    "user TEXT NOT NULL, "
                    "date DATETIME NOT NULL);";

  if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
    fprintf(stderr, "%s() :: SQL error: %s\n", __func__, err);
    sqlite3_free(err);
  } else {
    printf("%s() :: `%s` table created successfully.\n",
           __func__, "msgs");
  }
}

void insert_message(sqlite3 *db, const char *msg, const char *user) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO msgs (message, user, date) "
                    "VALUES (?, ?, datetime('now'));";
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, msg,  -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_DONE) {
      fprintf(stdout, "%s() :: %s\n", __func__, sql);
      fprintf(stdout, "%s() :: user `%s` inserted msg `%s` successfully.\n",
              __func__, user, msg);
    } else {
    fprintf(stderr, "%s() :: failed to insert message: %s\n",
            __func__, sqlite3_errmsg(db));
    }
    sqlite3_finalize(stmt);
    return;
  }
  fprintf(stderr, "%s() :: failed to prepare sql statement: %s\n",
          __func__, sqlite3_errmsg(db));
  sqlite3_finalize(stmt);
  return;
}

void message_table_log(sqlite3 *db) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, message, user, date "
                      "FROM msgs ORDER BY date ASC;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        printf("\n%-5s | %-20s | %-10s | %-20s\n",
               "ID", "Message", "User", "Date");
        printf("--------------------------------"
               "-------------------------------\n");
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const unsigned char *msg  = sqlite3_column_text(stmt, 1);
            const unsigned char *user = sqlite3_column_text(stmt, 2);
            const unsigned char *date = sqlite3_column_text(stmt, 3);
            printf("%-5d | %-20s | %-10s | %-20s\n", id, msg, user, date);
        }
        printf("\n");
        sqlite3_finalize(stmt);
    } else {
        fprintf(stderr, "%s() :: failed to prepare statement: %s\n",
                __func__, sqlite3_errmsg(db));
    }
}

void ensure_directory(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    mkdir(path, 0700);
  }
}

#define MAX_PATH_LEN 260
void commit_from_memory(sqlite3 *source_db, const char *path) {
  sqlite3 *target_db;
  sqlite3_backup *backup;
  time_t now = time(NULL);

  // TODO: ensure the relative path ends with a slash ('/')
  size_t path_len = strlen(path);
  char final_path[MAX_PATH_LEN];

  if (path[path_len - 1] == '/') strcpy(final_path, path);
  else snprintf(final_path, sizeof(final_path), "%s/", path);

  ensure_directory(final_path);

  char filename[MAX_PATH_LEN + 20]; // extra space for filename
  snprintf(filename, sizeof(filename),
            "%sconv-%ld.sqlite3", final_path, now);

  if (sqlite3_open(filename, &target_db) != SQLITE_OK) {
    fprintf(stderr,
            "%s() :: can't open database file %s: %s\n",
            __func__, filename, sqlite3_errmsg(target_db));
    sqlite3_close(target_db);
    return;
  }

  backup = sqlite3_backup_init(target_db, "main", source_db, "main");
  if (backup) {
    sqlite3_backup_step(backup, -1);
    sqlite3_backup_finish(backup);
  }

  if (sqlite3_errcode(target_db) != SQLITE_OK) {
    fprintf(stderr, "%s() :: backup failed: %s\n",
            __func__, sqlite3_errmsg(target_db));
  } else {
    fprintf(stdout, "%s() :: backup successful to %s\n",
            __func__, filename);
  }

  sqlite3_close(target_db);
}

int main(void) {
  sqlite3 *db = NULL;
  open_db_or_die(&db);
  new_message_table(db);
  insert_message(db, "hello!" , "ziggy");
  insert_message(db, "nopers" , "jack");
  insert_message(db, "yeppers", "david");
  message_table_log(db);
  commit_from_memory(db, "./saves/");
  close_db(db);
}
