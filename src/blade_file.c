#include "blade_file.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#include "blade_unistd.h"
#endif /* ifdef HAVE_UNISTD_H */
#include "pathinfo.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#include <sys/utime.h>
#else
#include <utime.h>
#endif /* ifdef _MSC_VER */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* ifdef HAVE_SYS_TIME_H */
#include <time.h>

#ifdef _WIN32

/* Symbolic links aren't really a 'thing' on Windows, so just use plain-old
 * stat() instead of lstat(). */
#define lstat stat

#endif /* ifdef _WIN32 */

#define FILE_ERROR(type, message)                                              \
  file_close(file);                                                            \
  RETURN_ERROR(#type " -> %s", message, file->path->chars);

#define RETURN_STATUS(status)                                                  \
  if ((status) == 0) {                                                         \
    RETURN_TRUE;                                                               \
  } else {                                                                     \
    FILE_ERROR(, strerror(errno));                                             \
  }

#define DENY_STD()                                                             \
  if (file->mode->length == 0)                                                 \
    RETURN_ERROR("method not supported for std files");

#define SET_DICT_STRING(d, n, l, v) dict_add_entry(vm, d, GC_L_STRING(n, l), v)

bool is_std_file(b_obj_file *file) { return file->mode->length == 0; }

static void file_close(b_obj_file *file) {
  if (file->file != NULL && !is_std_file(file)) {
    fflush(file->file);
    fclose(file->file);
    file->file = NULL;
    file->is_open = false;
  }
}

static void file_open(b_obj_file *file) {
  if ((file->file == NULL || !file->is_open) && !is_std_file(file)) {
    char *mode = file->mode->chars;
    if (strstr(file->mode->chars, "w") != NULL &&
        strstr(file->mode->chars, "+") != NULL) {
      mode = (char *) "a+";
    }
    file->file = fopen(file->path->chars, mode);
    file->is_open = true;
  }
}

DECLARE_NATIVE(file) {
  ENFORCE_ARG_RANGE(file, 1, 2);
  ENFORCE_ARG_TYPE(file, 0, IS_STRING);
  b_obj_string *path = AS_STRING(args[0]);

  if (path->length == 0) {
    RETURN_ERROR("file path cannot be empty");
  }

  b_obj_string *mode = NULL;

  if (arg_count == 2) {
    ENFORCE_ARG_TYPE(file, 1, IS_STRING);
    mode = AS_STRING(args[1]);
  } else {
    mode = (b_obj_string *) GC(copy_string(vm, "r", 1));
  }

  b_obj_file *file = new_file(vm, path, mode);
  file_open(file);

  RETURN_OBJ(file);
}

DECLARE_FILE_METHOD(exists) {
  ENFORCE_ARG_COUNT(exists, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();
  RETURN_BOOL(file_exists(file->path->chars));
}

DECLARE_FILE_METHOD(close) {
  ENFORCE_ARG_COUNT(close, 0);
  file_close(AS_FILE(METHOD_OBJECT));
  RETURN;
}

DECLARE_FILE_METHOD(open) {
  ENFORCE_ARG_COUNT(open, 0);
  file_open(AS_FILE(METHOD_OBJECT));
  RETURN;
}

DECLARE_FILE_METHOD(is_open) {
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  RETURN_BOOL(is_std_file(file) || (file->is_open && file->file != NULL));
}

DECLARE_FILE_METHOD(is_closed) {
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  RETURN_BOOL(!is_std_file(file) && !file->is_open && file->file == NULL);
}

DECLARE_FILE_METHOD(read) {
  ENFORCE_ARG_RANGE(read, 0, 1);
  size_t file_size = -1;
  size_t file_size_real = -1;
  if (arg_count == 1) {
    ENFORCE_ARG_TYPE(read, 0, IS_NUMBER);
    file_size = (size_t) AS_NUMBER(args[0]);
  }

  b_obj_file *file = AS_FILE(METHOD_OBJECT);

  bool in_binary_mode = strstr(file->mode->chars, "b") != NULL;

  if (!is_std_file(file)) {
    // file is in read mode and file does not exist
    if (strstr(file->mode->chars, "r") != NULL &&
        !file_exists(file->path->chars)) {
      FILE_ERROR(NotFound, "no such file or directory");
    }
    // file is in write only mode
    else if (strstr(file->mode->chars, "w") != NULL &&
             strstr(file->mode->chars, "+") == NULL) {
      FILE_ERROR(Unsupported, "cannot read file in write mode");
    }

    if (!file->is_open) { // open the file if it isn't open
      file_open(file);
    }

    if (file->file == NULL) {
      FILE_ERROR(Read, "could not read file");
    }

    // Get file size
    struct stat stats; // stats is super faster on large files
    if (lstat(file->path->chars, &stats) == 0) {
      file_size_real = (size_t) stats.st_size;
    } else {
      // fallback
      fseek(file->file, 0L, SEEK_END);
      file_size_real = ftell(file->file);
      rewind(file->file);
    }

    if (file_size == (size_t) -1 || file_size > file_size_real) {
      file_size = file_size_real;
    }
  } else {
    // stdout should not read
    if (fileno(stdout) == fileno(file->file) ||
        fileno(stderr) == fileno(file->file)) {
      FILE_ERROR(Unsupported, "cannot read from output file");
    }

    // for non-file objects such as stdin
    // minimum read bytes should be 1
    if (file_size == (size_t) -1) {
      file_size = 1;
    }
  }

  char *buffer =
      (char *) ALLOCATE(char, file_size + 1); // +1 for terminator '\0'

  if (buffer == NULL && file_size != 0) {
    FILE_ERROR(Buffer, "not enough memory to read file");
  }

  size_t bytes_read = fread(buffer, sizeof(char), file_size, file->file);

  if (bytes_read < file_size && file_size == file_size_real) {
    FILE_ERROR(Read, "could not read file contents");
  }

  // we made use of +1 so we can terminate the string.
  if (buffer != NULL)
    buffer[file_size] = '\0';

  // close file
  if (bytes_read == file_size) {
    file_close(file);
  }

  if (!in_binary_mode) {
    RETURN_T_STRING(buffer, bytes_read);
  }

  RETURN_OBJ(take_bytes(vm, (unsigned char *) buffer, bytes_read));
}

DECLARE_FILE_METHOD(write) {
  ENFORCE_ARG_COUNT(write, 1);

  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  b_obj_string *string = NULL;
  b_obj_bytes *bytes = NULL;

  bool in_binary_mode = strstr(file->mode->chars, "b") != NULL;

  if (!in_binary_mode) {
    ENFORCE_ARG_TYPE(write, 0, IS_STRING);
    string = AS_STRING(args[0]);
  } else {
    ENFORCE_ARG_TYPE(write, 0, IS_BYTES);
    bytes = AS_BYTES(args[0]);
  }

  // file is in read only mode
  if (!is_std_file(file)) {
    if (strstr(file->mode->chars, "r") != NULL &&
        strstr(file->mode->chars, "+") == NULL) {
      FILE_ERROR(Unsupported, "cannot read file in write mode");
    }

    if ((!in_binary_mode ? string->length : bytes->bytes.count) == 0) {
      FILE_ERROR(Write, "cannot write empty buffer to file");
    }

    if (!file->is_open) { // open the file if it isn't open
      file_open(file);
    }

    if (file->file == NULL) {
      FILE_ERROR(Write, "could not write to file");
    }
  } else {
    // stdin should not write
    if (fileno(stdin) == fileno(file->file)) {
      FILE_ERROR(Unsupported, "cannot write to input file");
    }
  }

  size_t count;

  if (!in_binary_mode) {
    count = fwrite(string->chars, sizeof(char), string->length, file->file);
  } else {
    count = fwrite(bytes->bytes.bytes, sizeof(unsigned char),
                   bytes->bytes.count, file->file);
  }

  // close file
  if (count ==
      (size_t) (in_binary_mode ? bytes->bytes.count : string->length)) {
    file_close(file);
  }

  if (count > (size_t) 0) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

DECLARE_FILE_METHOD(number) {
  ENFORCE_ARG_COUNT(number, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  if (file->file == NULL) {
    RETURN_NUMBER(-1);
  } else {
    RETURN_NUMBER(fileno(file->file));
  }
}

DECLARE_FILE_METHOD(is_tty) {
  ENFORCE_ARG_COUNT(is_tty, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  if (is_std_file(file)) {
    RETURN_BOOL(isatty(fileno(file->file)) &&
                fileno(file->file) == fileno(stdout));
  }
  RETURN_FALSE;
}

DECLARE_FILE_METHOD(flush) {
  ENFORCE_ARG_COUNT(flush, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);

  if (!file->is_open) {
    FILE_ERROR(Unsupported, "i/o operation on closed file");
  }

#if defined(IS_UNIX)
  // using fflush on stdin have undesired effect on unix environments
  if (fileno(stdin) == fileno(file->file)) {
    while ((getchar()) != '\n')
      ;
  } else {
    fflush(file->file);
  }
#else
  fflush(file->file);
#endif
  RETURN;
}

DECLARE_FILE_METHOD(stats) {
  ENFORCE_ARG_COUNT(stats, 0);

  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  b_obj_dict *dict = (b_obj_dict *) GC(new_dict(vm));

  if (!is_std_file(file)) {
    if (file_exists(file->path->chars)) {
      struct stat stats;
      if (lstat(file->path->chars, &stats) == 0) {

#ifndef _WIN32
        // read mode
        SET_DICT_STRING(dict, "is_readable", 11,
                        BOOL_VAL(((stats.st_mode & S_IRUSR) != 0)));

        // write mode
        SET_DICT_STRING(dict, "is_writable", 11,
                        BOOL_VAL(((stats.st_mode & S_IWUSR) != 0)));

        // execute mode
        SET_DICT_STRING(dict, "is_executable", 13,
                        BOOL_VAL(((stats.st_mode & S_IXUSR) != 0)));

        // is symbolic link
        SET_DICT_STRING(dict, "is_symbolic", 11,
                        BOOL_VAL((S_ISLNK(stats.st_mode) != 0)));
#else
        // read mode
        SET_DICT_STRING(dict, "is_readable", 11,
                        BOOL_VAL(((stats.st_mode & S_IREAD) != 0)));

        // write mode
        SET_DICT_STRING(dict, "is_writable", 11,
                        BOOL_VAL(((stats.st_mode & S_IWRITE) != 0)));

        // execute mode
        SET_DICT_STRING(dict, "is_executable", 13,
                        BOOL_VAL(((stats.st_mode & S_IEXEC) != 0)));

        // is symbolic link
        SET_DICT_STRING(dict, "is_symbolic", 11, BOOL_VAL(false));
#endif /* ifndef _WIN32 */

        // file details
        SET_DICT_STRING(dict, "size", 4, NUMBER_VAL(stats.st_size));
        SET_DICT_STRING(dict, "mode", 4, NUMBER_VAL(stats.st_mode));
        SET_DICT_STRING(dict, "dev", 3, NUMBER_VAL(stats.st_dev));
        SET_DICT_STRING(dict, "ino", 3, NUMBER_VAL(stats.st_ino));
        SET_DICT_STRING(dict, "nlink", 5, NUMBER_VAL(stats.st_nlink));
        SET_DICT_STRING(dict, "uid", 3, NUMBER_VAL(stats.st_uid));
        SET_DICT_STRING(dict, "gid", 3, NUMBER_VAL(stats.st_gid));

#if !defined(_WIN32) && (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))

        // last modified time in milliseconds
        SET_DICT_STRING(dict, "mtime", 5,
                        NUMBER_VAL(stats.st_mtimespec.tv_sec));

        // last accessed time in milliseconds
        SET_DICT_STRING(dict, "atime", 5,
                        NUMBER_VAL(stats.st_atimespec.tv_sec));

        // last c time in milliseconds
        SET_DICT_STRING(dict, "ctime", 5,
                        NUMBER_VAL(stats.st_ctimespec.tv_sec));

        // blocks
        SET_DICT_STRING(dict, "blocks", 6, NUMBER_VAL(stats.st_blocks));
        SET_DICT_STRING(dict, "blksize", 7, NUMBER_VAL(stats.st_blksize));

#else

        // last modified time in milliseconds
        SET_DICT_STRING(dict, "mtime", 5, NUMBER_VAL(stats.st_mtime));

        // last accessed time in milliseconds
        SET_DICT_STRING(dict, "mtime", 5, NUMBER_VAL(stats.st_mtime));

        // last c time in milliseconds
        SET_DICT_STRING(dict, "ctime", 5, NUMBER_VAL(stats.st_ctime));

#endif
      }
    } else {
      RETURN_ERROR("cannot get stats for non-existing file");
    }
  } else {
    // we are dealing with an std
    if (fileno(stdin) == fileno(file->file)) {
      SET_DICT_STRING(dict, "is_readable", 11, TRUE_VAL);
      SET_DICT_STRING(dict, "is_writable", 11, FALSE_VAL);
    } else if (fileno(stdout) == fileno(file->file) ||
               fileno(stderr) == fileno(file->file)) {
      SET_DICT_STRING(dict, "is_readable", 11, FALSE_VAL);
      SET_DICT_STRING(dict, "is_writable", 11, TRUE_VAL);
    }
    SET_DICT_STRING(dict, "is_executable", 13, FALSE_VAL);
    SET_DICT_STRING(dict, "size", 4, NUMBER_VAL(1));
  }
  RETURN_OBJ(dict);
}

DECLARE_FILE_METHOD(symlink) {
  ENFORCE_ARG_COUNT(symlink, 1);
  ENFORCE_ARG_TYPE(symlink, 0, IS_STRING);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

#ifdef _WIN32
  RETURN_ERROR("symlink not supported in windows");
#else
  if (file_exists(file->path->chars)) {
    b_obj_string *path = AS_STRING(args[0]);
    RETURN_BOOL(symlink(file->path->chars, path->chars) == 0);
  } else {
    RETURN_ERROR("symlink to file not found");
  }
#endif /* ifdef _WIN32 */
}

DECLARE_FILE_METHOD(delete) {
  ENFORCE_ARG_COUNT(delete, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  file_close(file);
  RETURN_STATUS(unlink(file->path->chars));
}

DECLARE_FILE_METHOD(rename) {
  ENFORCE_ARG_COUNT(rename, 1);
  ENFORCE_ARG_TYPE(rename, 0, IS_STRING);

  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  if (file_exists(file->path->chars)) {
    b_obj_string *new_name = AS_STRING(args[0]);
    if (new_name->length == 0) {
      FILE_ERROR(Operation, "file name cannot be empty");
    }
    file_close(file);
    RETURN_STATUS(rename(file->path->chars, new_name->chars));
  } else {
    RETURN_ERROR("file not found");
  }
}

DECLARE_FILE_METHOD(path) {
  ENFORCE_ARG_COUNT(path, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();
  RETURN_OBJ(file->path);
}

DECLARE_FILE_METHOD(mode) {
  ENFORCE_ARG_COUNT(mode, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();
  RETURN_OBJ(file->mode);
}

DECLARE_FILE_METHOD(name) {
  ENFORCE_ARG_COUNT(name, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();
  char *name = get_real_file_name(file->path->chars);
  RETURN_STRING(name);
}

DECLARE_FILE_METHOD(abs_path) {
  ENFORCE_ARG_COUNT(abs_path, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  char *abs_path = realpath(file->path->chars, NULL);
  if (abs_path != NULL)
    RETURN_STRING(abs_path);
  RETURN_STRING("");
}

DECLARE_FILE_METHOD(copy) {
  ENFORCE_ARG_COUNT(copy, 1);
  ENFORCE_ARG_TYPE(copy, 0, IS_STRING);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  if (file_exists(file->path->chars)) {
    b_obj_string *name = AS_STRING(args[0]);

    if (strstr(file->mode->chars, "r") == NULL) {
      FILE_ERROR(Unsupported, "file not open for reading");
    }

    char *mode = "w";
    // if we are dealing with a binary file
    if (strstr(file->mode->chars, "b") != NULL) {
      mode = "wb";
    }

    FILE *fp = fopen(name->chars, mode);
    if (fp == NULL) {
      FILE_ERROR(Permission, "unable to create new file");
    }

    size_t n_read, n_write;
    unsigned char buffer[8192];
    do {
      n_read = fread(buffer, 1, sizeof(buffer), file->file);
      if (n_read > 0) {
        n_write = fwrite(buffer, 1, n_read, fp);
      } else {
        n_write = 0;
      }
    } while ((n_read > 0) && (n_read == n_write));

    if (n_write > 0) {
      FILE_ERROR(Operation, "error copying file");
    }

    fflush(fp);
    fclose(fp);
    file_close(file);

    RETURN_BOOL(n_read == n_write);
  } else {
    RETURN_ERROR("file not found");
  }
}

#ifdef _WIN32
#define truncate Truncate
#endif /* ifdef _WIN32 */

DECLARE_FILE_METHOD(truncate) {
  ENFORCE_ARG_RANGE(truncate, 0, 1);

  off_t final_size = 0;
  if (arg_count == 1) {
    ENFORCE_ARG_TYPE(truncate, 0, IS_NUMBER);
    final_size = (off_t) AS_NUMBER(args[0]);
  }
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

#ifndef _WIN32
  RETURN_STATUS(truncate(file->path->chars, final_size));
#else
  RETURN_STATUS(_chsize_s(fileno(file->file), final_size));
#endif /* ifndef _WIN32 */
}

DECLARE_FILE_METHOD(chmod) {
  ENFORCE_ARG_COUNT(chmod, 1);
  ENFORCE_ARG_TYPE(chmod, 0, IS_NUMBER);

  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  if (file_exists(file->path->chars)) {
    int mode = AS_NUMBER(args[0]);

#ifndef _WIN32
    RETURN_STATUS(chmod(file->path->chars, (mode_t) mode));
#else
    RETURN_STATUS(_chmod(file->path->chars, mode));
#endif // !_WIN32
  } else {
    RETURN_ERROR("file not found");
  }
}

DECLARE_FILE_METHOD(set_times) {
  ENFORCE_ARG_COUNT(set_times, 2);
  ENFORCE_ARG_TYPE(set_times, 0, IS_NUMBER);
  ENFORCE_ARG_TYPE(set_times, 1, IS_NUMBER);

#ifdef HAVE_UTIME
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  if (file_exists(file->path->chars)) {

    time_t atime = (time_t) AS_NUMBER(args[0]);
    time_t mtime = (time_t) AS_NUMBER(args[1]);

    struct stat stats;
    int status = lstat(file->path->chars, &stats);
    if (status == 0) {
      struct utimbuf new_times;

#if !defined(_WIN32) && (!defined(_POSIX_C_SOURCE) || defined(_DARWIN_C_SOURCE))
      if (atime == (time_t) -1)
        new_times.actime = stats.st_atimespec.tv_sec;
      else
        new_times.actime = atime;

      if (mtime == (time_t) -1)
        new_times.modtime = stats.st_mtimespec.tv_sec;
      else
        new_times.modtime = mtime;
#else
      if (atime == (time_t) -1)
        new_times.actime = stats.st_atime;
      else
        new_times.actime = atime;

      if (mtime == (time_t) -1)
        new_times.modtime = stats.st_mtime;
      else
        new_times.modtime = mtime;
#endif

      RETURN_STATUS(utime(file->path->chars, &new_times));
    } else {
      RETURN_STATUS(status);
    }
  } else {
    FILE_ERROR(Access, "file not found");
  }
#else
  RETURN_ERROR("not available: OS does not support utime");
#endif /* ifdef HAVE_UTIME */
}

DECLARE_FILE_METHOD(seek) {
  ENFORCE_ARG_COUNT(seek, 2);
  ENFORCE_ARG_TYPE(seek, 0, IS_NUMBER);
  ENFORCE_ARG_TYPE(seek, 1, IS_NUMBER);

  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();

  long position = (long) AS_NUMBER(args[0]);
  int seek_type = AS_NUMBER(args[1]);
  RETURN_STATUS(fseek(file->file, position, seek_type));
}

DECLARE_FILE_METHOD(tell) {
  ENFORCE_ARG_COUNT(tell, 0);
  b_obj_file *file = AS_FILE(METHOD_OBJECT);
  DENY_STD();
  RETURN_NUMBER(ftell(file->file));
}

#undef FILE_ERROR
#undef RETURN_STATUS
#undef SET_DICT_STRING
#undef DENY_STD
