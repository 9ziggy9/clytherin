#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h> // mkdir
#include <sys/types.h> // mkdir
#include "db.h"

static const char *SQL_RAW_TABLE_USER =
  "CREATE TABLE IF NOT EXISTS users ("
  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
  "name TEXT NOT NULL, "
  "created_at DATETIME DEFAULT CURRENT_TIMESTAMP);";

static const char *SQL_RAW_TABLE_MSG =
  "CREATE TABLE IF NOT EXISTS messages ("
  "id INTEGER PRIMARY KEY AUTOINCREMENT, "
  "content TEXT NOT NULL, "
  "sender_id INTEGER NOT NULL, "
  "recipient_id INTEGER NOT NULL, "
  "date DATETIME DEFAULT CURRENT_TIMESTAMP, "
  "FOREIGN KEY(sender_id) REFERENCES users(id), "
  "FOREIGN KEY(recipient_id) REFERENCES users(id));";

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

void create_table_users(sqlite3 *db) {
  char *err = 0;
  if (sqlite3_exec(db, SQL_RAW_TABLE_USER, 0, 0, &err) != SQLITE_OK) {
    fprintf(stderr, "%s() :: error creating users table: %s\n",
            __func__, err);
    sqlite3_free(err);
  } else {
    fprintf(stdout, "%s() :: users table created successfully.\n", __func__);
  }
}

void create_table_messages(sqlite3 *db) {
  char *err = 0;
  if (sqlite3_exec(db, SQL_RAW_TABLE_MSG, 0, 0, &err) != SQLITE_OK) {
      fprintf(stderr, "%s() :: error creating messages table: %s\n",
              __func__, err);
      sqlite3_free(err);
  } else {
    fprintf(stdout, "%s() :: messages table created successfully.\n",
            __func__);
  }
}

sqlite3_int64 insert_user(sqlite3 *db, const char *name) {
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO users (name) VALUES (?);";
  sqlite3_int64 last_id = -1;

  fprintf(stdout, "%s() :: try( %s )\n", __func__, sql);

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      fprintf(stderr, "%s() :: failed to insert user: %s\n",
              __func__,
              sqlite3_errmsg(db));
    } else {
      fprintf(stdout, "%s() :: user `%s` inserted successfully.\n",
              __func__, name);
      last_id = sqlite3_last_insert_rowid(db);
    }
    sqlite3_finalize(stmt);
    return last_id;
  }
  fprintf(stderr, "%s() :: failed to prepare statement: %s\n",
          __func__,
          sqlite3_errmsg(db));
  return last_id;
}

sqlite3_int64 insert_message(sqlite3 *db,
                       const char *content,
                       int sender_id,
                       int recipient_id)
{
  sqlite3_stmt *stmt;
  const char *sql = "INSERT INTO messages (content, sender_id, "
                    "recipient_id) VALUES (?, ?, ?);";
  sqlite3_int64 last_id = -1;

  fprintf(stdout, "%s() :: try( %s )\n", __func__, sql);

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, sender_id);
    sqlite3_bind_int(stmt, 3, recipient_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
      fprintf(stderr, "%s() :: failed to insert message: %s\n",
              __func__,
              sqlite3_errmsg(db));
    } else {
      fprintf(stdout, "%s() :: message inserted successfully.\n", __func__);
      last_id = sqlite3_last_insert_rowid(db);
    }
    sqlite3_finalize(stmt);
    return last_id;
  }
  fprintf(stderr, "%s() :: failed to prepare statement: %s\n",
          __func__, sqlite3_errmsg(db));
  return last_id;
}

void ensure_directory(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) mkdir(path, 0700);
}

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
  snprintf(filename, sizeof(filename), "%sconv-%ld.sqlite3", final_path, now);

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
    fprintf(stdout, "%s() :: backup successful to %s\n", __func__, filename);
  }

  sqlite3_close(target_db);
}

#ifdef __RUN_AS_STANDALONE__
int main(void) {
  sqlite3 *db = NULL;
  open_db_or_die(&db);
  create_table_messages(db);
  create_table_users(db);
  sqlite3_int64 uid1 = insert_user(db, "Ziggy");
  sqlite3_int64 uid2 = insert_user(db, "Jack");
  insert_message(db, "hello world!",   uid1, uid2);
  insert_message(db, "goodbye moon!",  uid1, uid2);
  insert_message(db, "ola sun!",       uid2, uid1);
  insert_message(db, "sayonara mars!", uid2, uid1);
  commit_from_memory(db, "./saves/");
  close_db(db);
}
#endif
