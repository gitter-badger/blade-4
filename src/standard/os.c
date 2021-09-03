#include "os.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#include "blade_unistd.h"
#endif /* HAVE_UNISTD_H */

#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif /* HAVE_SYS_UTSNAME_H */

#include <ctype.h>
#include <stdio.h>
#include <pathinfo.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose

#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#ifndef sleep
#define sleep(s) Sleep((DWORD)s)
#endif /* ifndef sleep */

#endif /* ifdef _WIN32 */

#ifndef HAVE_DIRENT_H
#include "dirent/dirent.h"
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/errno.h>
#endif /* HAVE_DIRENT_H */

DECLARE_MODULE_METHOD(os_exec) {
  ENFORCE_ARG_COUNT(exec, 1);
  ENFORCE_ARG_TYPE(exec, 0, IS_STRING);
  b_obj_string *string = AS_STRING(args[0]);
  if (string->length == 0) {
    RETURN;
  }

  FILE *fd = popen(string->chars, "r");
  if (!fd) RETURN;

  char buffer[256];
  size_t n_read;
  size_t output_size = 256;
  int length = 0;
  char *output = ALLOCATE(char, output_size);

  if (output != NULL) {
    while ((n_read = fread(buffer, 1, sizeof(buffer), fd)) != 0) {
      if (length + n_read >= output_size) {
        output_size *= 2;
        void *temp = realloc(output, output_size);
        if (temp == NULL) {
          RETURN_ERROR("device out of memory");
        } else {
          output = temp;
        }
      }
      if ((output + length) != NULL) {
        strncat(output + length, buffer, n_read);
      }
      length += (int) n_read;
    }

    if (length == 0) {
      pclose(fd);
      RETURN;
    }

    output[length - 1] = '\0';

    pclose(fd);
    RETURN_T_STRING(output, length);
  }

  pclose(fd);
  RETURN_STRING("");
}

DECLARE_MODULE_METHOD(os_info) {
  ENFORCE_ARG_COUNT(info, 0);

#ifdef HAVE_SYS_UTSNAME_H
  struct utsname os;
  if (uname(&os) != 0) {
    RETURN_ERROR("could not access os information");
  }

  b_obj_dict *dict = new_dict(vm);
  push(vm, OBJ_VAL(dict));

  dict_add_entry(vm, dict, OBJ_VAL(copy_string(vm, "sysname", 7)),
                 OBJ_VAL(copy_string(vm, os.sysname, strlen(os.sysname))));
  dict_add_entry(vm, dict, OBJ_VAL(copy_string(vm, "nodename", 8)),
                 OBJ_VAL(copy_string(vm, os.nodename, strlen(os.nodename))));
  dict_add_entry(vm, dict, OBJ_VAL(copy_string(vm, "version", 7)),
                 OBJ_VAL(copy_string(vm, os.version, strlen(os.version))));
  dict_add_entry(vm, dict, OBJ_VAL(copy_string(vm, "release", 7)),
                 OBJ_VAL(copy_string(vm, os.release, strlen(os.release))));
  dict_add_entry(vm, dict, OBJ_VAL(copy_string(vm, "machine", 7)),
                 OBJ_VAL(copy_string(vm, os.machine, strlen(os.machine))));

  pop(vm);
  RETURN_OBJ(dict);
#else
  RETURN_ERROR("not available: OS does not have uname()")
#endif /* HAVE_SYS_UTSNAME_H */
}

DECLARE_MODULE_METHOD(os_sleep) {
  ENFORCE_ARG_COUNT(sleep, 1);
  ENFORCE_ARG_TYPE(sleep, 0, IS_NUMBER);
  sleep((int) AS_NUMBER(args[0]));
  RETURN;
}

b_value get_os_platform(b_vm *vm) {

#if defined(_WIN32)
#define PLATFORM_NAME "windows" // Windows
#elif defined(_WIN64)
#define PLATFORM_NAME "windows" // Windows
#elif defined(__CYGWIN__) && !defined(_WIN32)
#define PLATFORM_NAME "windows" // Windows (Cygwin POSIX under Microsoft Window)
#elif defined(__ANDROID__)
#define PLATFORM_NAME                                                          \
  "android" // Android (implies Linux, so it must come first)
#elif defined(__linux__)
#define PLATFORM_NAME                                                          \
  "linux" // Debian, Ubuntu, Gentoo, Fedora, openSUSE, RedHat, Centos and other
#elif defined(__unix__) || !defined(__APPLE__) && defined(__MACH__)
#include <sys/param.h>
#if defined(BSD)
#define PLATFORM_NAME "bsd" // FreeBSD, NetBSD, OpenBSD, DragonFly BSD
#endif
#elif defined(__hpux)
#define PLATFORM_NAME "hp-ux" // HP-UX
#elif defined(_AIX)
#define PLATFORM_NAME "aix"                   // IBM AIX
#elif defined(__APPLE__) && defined(__MACH__) // Apple OSX and iOS (Darwin)

#include <TargetConditionals.h>

#if TARGET_IPHONE_SIMULATOR == 1
#define PLATFORM_NAME "ios" // Apple iOS
#elif TARGET_OS_IPHONE == 1
#define PLATFORM_NAME "ios" // Apple iOS
#elif TARGET_OS_MAC == 1
#define PLATFORM_NAME "osx" // Apple OSX
#endif
#elif defined(__sun) && defined(__SVR4)
#define PLATFORM_NAME "solaris" // Oracle Solaris, Open Indiana
#elif defined(__OS400__)
#define PLATFORM_NAME "ibm" // IBM OS/400
#elif defined(AMIGA) || defined(__MORPHOS__)
#define PLATFORM_NAME "amiga"
#else
#define PLATFORM_NAME "unknown"
#endif

  return OBJ_VAL(copy_string(vm, PLATFORM_NAME, (int) strlen(PLATFORM_NAME)));

#undef PLATFORM_NAME
}

b_value get_blade_os_args(b_vm *vm) {
  b_obj_list *list = (b_obj_list*)GC(new_list(vm));
  if(vm->std_args != NULL) {
    for(int i = 0; i < vm->std_args_count; i++) {
      write_list(vm, list, STRING_VAL(vm->std_args[i]));
    }
  }
  CLEAR_GC();
  return OBJ_VAL(list);
}

b_value get_blade_os_path_separator(b_vm *vm) {
  return STRING_L_VAL(BLADE_PATH_SEPARATOR, 1);
}

DECLARE_MODULE_METHOD(os_getenv) {
  ENFORCE_ARG_COUNT(getenv, 1);
  ENFORCE_ARG_TYPE(getenv, 0, IS_STRING);

  char *env = getenv(AS_C_STRING(args[0]));
  if (env != NULL) {
    RETURN_STRING(env);
  } else {
    RETURN;
  }
}

DECLARE_MODULE_METHOD(os_setenv) {
  ENFORCE_ARG_RANGE(setenv, 2, 3);
  ENFORCE_ARG_TYPE(setenv, 0, IS_STRING);
  ENFORCE_ARG_TYPE(setenv, 1, IS_STRING);

  int overwrite = 1;
  if (arg_count == 3) {
    ENFORCE_ARG_TYPE(setenv, 2, IS_BOOL);
    overwrite = AS_BOOL(args[2]) ? 1 : 0;
  }

#ifdef _WIN32
#define setenv(e, v, i) _putenv_s(e, v)
#endif

  if (setenv(AS_C_STRING(args[0]), AS_C_STRING(args[1]), overwrite) == 0) {
    RETURN_TRUE;
  }
  RETURN_FALSE;
}

DECLARE_MODULE_METHOD(os_read_dir) {
  RETURN;
}

DECLARE_MODULE_METHOD(os__mkdir) {
  ENFORCE_ARG_COUNT(create, 3);
  ENFORCE_ARG_TYPE(create, 0, IS_STRING);
  ENFORCE_ARG_TYPE(create, 1, IS_NUMBER);
  ENFORCE_ARG_TYPE(create, 2, IS_BOOL);

  b_obj_string *path = AS_STRING(args[0]);
  int mode = AS_NUMBER(args[1]);
  bool is_recursive = AS_BOOL(args[2]);

  char sep = BLADE_PATH_SEPARATOR[0];
  bool exists = false;

  if(is_recursive) {

    for (char* p = strchr(path->chars + 1, sep); p; p = strchr(p + 1, sep)) {
      *p = '\0';
#ifdef _WIN32
      if (CreateDirectory(path->chars, NULL) == FALSE) {
        if (errno != ERROR_ALREADY_EXISTS) {
#else
      if (mkdir(path->chars, mode) == -1) {
        if (errno != EEXIST) {
#endif /* _WIN32 */
          *p = sep;
          RETURN_ERROR(strerror(errno));
        } else {
          exists = true;
        }
      } else {
        exists = false;
      }
//      chmod(path->chars, (mode_t) mode);
      *p = sep;
    }

  } else {

#ifdef _WIN32
    if (CreateDirectory(path->chars, NULL) == FALSE) {
      if (errno != ERROR_ALREADY_EXISTS) {
#else
    if (mkdir(path->chars, mode) == -1) {
      if (errno != EEXIST) {
#endif /* _WIN32 */
        RETURN_ERROR(strerror(errno));
      } else {
        exists = true;
      }
    }
//    chmod(path->chars, (mode_t) mode);

  }

  RETURN_BOOL(!exists);
}

DECLARE_MODULE_METHOD(os__readdir) {
  ENFORCE_ARG_COUNT(read, 1);
  ENFORCE_ARG_TYPE(read, 0, IS_STRING);
  b_obj_string *path = AS_STRING(args[0]);

  DIR *dir;
  if((dir = opendir(path->chars)) != NULL) {
    b_obj_list *list = (b_obj_list *)GC(new_list(vm));
    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) {
      b_obj_list *d_list = (b_obj_list *)GC(new_list(vm));
      write_list(vm, d_list, STRING_L_VAL(ent->d_name, ent->d_namlen));
      write_list(vm, d_list, NUMBER_VAL(ent->d_type));
      write_list(vm, list, OBJ_VAL(d_list));
    }
    closedir(dir);
    RETURN_OBJ(list);
  }
  RETURN_ERROR(strerror(errno));
}

/** DIR TYPES BEGIN */

b_value __os_dir_DT_UNKNOWN(b_vm *vm){
  return NUMBER_VAL(DT_UNKNOWN);
}

b_value __os_dir_DT_REG(b_vm *vm){
  return NUMBER_VAL(DT_REG);
}

b_value __os_dir_DT_DIR(b_vm *vm){
  return NUMBER_VAL(DT_DIR);
}

b_value __os_dir_DT_FIFO(b_vm *vm){
  return NUMBER_VAL(DT_FIFO);
}

b_value __os_dir_DT_SOCK(b_vm *vm){
  return NUMBER_VAL(DT_SOCK);
}

b_value __os_dir_DT_CHR(b_vm *vm){
  return NUMBER_VAL(DT_CHR);
}

b_value __os_dir_DT_BLK(b_vm *vm) {
  return NUMBER_VAL(DT_BLK);
}

b_value __os_dir_DT_LNK(b_vm *vm) {
  return NUMBER_VAL(DT_LNK);
}

b_value __os_dir_DT_WHT(b_vm *vm) {
#ifdef DT_WHT
  return NUMBER_VAL(DT_WHT);
#else
  return NUMBER_VAL(-1);
#endif
}

/** DIR TYPES ENDS */

CREATE_MODULE_LOADER(os) {
  static b_func_reg os_module_functions[] = {
      {"info",   true,  GET_MODULE_METHOD(os_info)},
      {"exec",   true,  GET_MODULE_METHOD(os_exec)},
      {"sleep",  true,  GET_MODULE_METHOD(os_sleep)},
      {"getenv", true,  GET_MODULE_METHOD(os_getenv)},
      {"setenv", true,  GET_MODULE_METHOD(os_setenv)},
      {"_mkdir", true,  GET_MODULE_METHOD(os__mkdir)},
      {"_readdir", true,  GET_MODULE_METHOD(os__readdir)},
      {NULL,     false, NULL},
  };

  static b_field_reg os_module_fields[] = {
      {"platform", true, get_os_platform},
      {"args", true, get_blade_os_args},
      {"path_separator", true, get_blade_os_path_separator},
      {"DT_UNKNOWN", true, __os_dir_DT_UNKNOWN},
      {"DT_BLK", true, __os_dir_DT_BLK},
      {"DT_CHR", true, __os_dir_DT_CHR},
      {"DT_DIR", true, __os_dir_DT_DIR},
      {"DT_FIFO", true, __os_dir_DT_FIFO},
      {"DT_LNK", true, __os_dir_DT_LNK},
      {"DT_REG", true, __os_dir_DT_REG},
      {"DT_SOCK", true, __os_dir_DT_SOCK},
      {"DT_WHT", true, __os_dir_DT_WHT},
      {NULL,       false, NULL},
  };

  static b_module_reg module = {
      .name = "_os",
      .fields = os_module_fields,
      .functions = os_module_functions,
      .classes = NULL,
      .preloader= NULL,
      .unloader = NULL
  };

  return &module;
}
