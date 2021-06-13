#include "hash.h"
#include "hash/md5.h"
#include "hash/sha1.h"
#include "hash/sha256.h"
#include "hash/sha512.h"
#include "hash/fnv.h"
#include "zlib.h"
#include "pathinfo.h"

DECLARE_MODULE_METHOD(hash__crc32) {
  ENFORCE_ARG_RANGE(crc32, 1, 2);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("crc32() expects string or bytes");
  }

  uint32_t crc = 0;
  if(!IS_NIL(args[1])){
    ENFORCE_ARG_TYPE(crc32, 1, IS_NUMBER);
    crc = (uint32_t) AS_NUMBER(args[1]);
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    RETURN_NUMBER(crc32(crc, (unsigned char *)string->chars, string->length));
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    RETURN_NUMBER(crc32(crc, bytes->bytes.bytes, bytes->bytes.count));
  }
}

DECLARE_MODULE_METHOD(hash__adler32) {
  ENFORCE_ARG_RANGE(adler32, 1, 2);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("adler32() expects string or bytes");
  }

  unsigned int adler = 1;
  if(!IS_NIL(args[1])){
    ENFORCE_ARG_TYPE(adler32, 1, IS_NUMBER);
    adler = (unsigned int) AS_NUMBER(args[1]);
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    RETURN_NUMBER(adler32(adler, (unsigned char *)string->chars, string->length));
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    RETURN_NUMBER(adler32(adler, bytes->bytes.bytes, bytes->bytes.count));
  }
}

DECLARE_MODULE_METHOD(hash__md5) {
  ENFORCE_ARG_COUNT(md5, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("md5() expects string or bytes");
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    char *result = MD5String(string->chars, string->length);
    RETURN_T_STRING(result, 32);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    char *result = MD5String((char *)bytes->bytes.bytes, bytes->bytes.count);
    RETURN_T_STRING(result, 32);
  }
}

DECLARE_MODULE_METHOD(hash__md5_file) {
  ENFORCE_ARG_COUNT(md5_file, 1);
  ENFORCE_ARG_TYPE(md5_file, 0, IS_FILE);

  b_obj_file *file = AS_FILE(args[0]);

  if(file_exists(file->path->chars)) {
    char *result = MD5File(file->path->chars);
    if(result == NULL) {
      RETURN_ERROR("md5_file() could not open file");
    }

    RETURN_T_STRING(result, 32);
  }

  RETURN_ERROR("md5_file() file not found");
}

DECLARE_MODULE_METHOD(hash__sha1) {
  ENFORCE_ARG_COUNT(sha1, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("sha1() expects string or bytes");
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    char *result = SHA1String((unsigned char *)string->chars, string->length);
    RETURN_T_STRING(result, 40);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    char *result = SHA1String(bytes->bytes.bytes, bytes->bytes.count);
    RETURN_T_STRING(result, 40);
  }
}

DECLARE_MODULE_METHOD(hash__sha256) {
  ENFORCE_ARG_COUNT(sha256, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("sha256() expects string or bytes");
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    char *result = sha256_string((unsigned char *)string->chars, string->length);
    RETURN_T_STRING(result, 64);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    char *result = sha256_string(bytes->bytes.bytes, bytes->bytes.count);
    RETURN_T_STRING(result, 64);
  }
}

DECLARE_MODULE_METHOD(hash__sha512) {
  ENFORCE_ARG_COUNT(sha512, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("sha512() expects string or bytes");
  }

  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    char *result = SHA512String((unsigned char *) string->chars, string->length);
    RETURN_T_STRING(result, 128);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    char *result = SHA512String(bytes->bytes.bytes, bytes->bytes.count);
    RETURN_T_STRING(result, 128);
  }
}

DECLARE_MODULE_METHOD(hash__fnv1) {
  ENFORCE_ARG_COUNT(fnv1, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("fnv1() expects string or bytes");
  }

  char *result;
  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    result = FNV1((unsigned char *)string->chars, string->length);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    result = FNV1(bytes->bytes.bytes, bytes->bytes.count);
  }

  RETURN_TT_STRING(result);
}

DECLARE_MODULE_METHOD(hash__fnv1a) {
  ENFORCE_ARG_COUNT(fnv1a, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("fnv1a() expects string or bytes");
  }

  char *result;
  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    result = FNV1a((unsigned char *)string->chars, string->length);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    result = FNV1a(bytes->bytes.bytes, bytes->bytes.count);
  }

  RETURN_TT_STRING(result);
}

DECLARE_MODULE_METHOD(hash__fnv1_64) {
  ENFORCE_ARG_COUNT(fnv1_64, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("fnv1_64() expects string or bytes");
  }

  char *result;
  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    result = FNV164((unsigned char *)string->chars, string->length);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    result = FNV164(bytes->bytes.bytes, bytes->bytes.count);
  }

  RETURN_TT_STRING(result);
}

DECLARE_MODULE_METHOD(hash__fnv1a_64) {
  ENFORCE_ARG_COUNT(fnv1a64, 1);

  if(!IS_STRING(args[0]) && !IS_BYTES(args[0])){
    RETURN_ERROR("fnv1a_64() expects string or bytes");
  }

  char *result;
  if(IS_STRING(args[0])){
    b_obj_string *string = AS_STRING(args[0]);
    result = FNV1a64((unsigned char *)string->chars, string->length);
  } else {
    b_obj_bytes *bytes = AS_BYTES(args[0]);
    result = FNV1a64(bytes->bytes.bytes, bytes->bytes.count);
  }

  RETURN_TT_STRING(result);
}

CREATE_MODULE_LOADER(hash) {
  static b_func_reg class_functions[] = {
      {"_adler32", true, GET_MODULE_METHOD(hash__adler32)},
      {"_crc32", true, GET_MODULE_METHOD(hash__crc32)},
      {"md5", true, GET_MODULE_METHOD(hash__md5)},
      {"md5_file", true, GET_MODULE_METHOD(hash__md5_file)},
      {"sha1", true, GET_MODULE_METHOD(hash__sha1)},
      {"sha256", true, GET_MODULE_METHOD(hash__sha256)},
      {"sha512", true, GET_MODULE_METHOD(hash__sha512)},
      {"fnv1", true, GET_MODULE_METHOD(hash__fnv1)},
      {"fnv1a", true, GET_MODULE_METHOD(hash__fnv1a)},
      {"fnv1_64", true, GET_MODULE_METHOD(hash__fnv1_64)},
      {"fnv1a_64", true, GET_MODULE_METHOD(hash__fnv1a_64)},
      {NULL,      false, NULL},
  };

  static b_class_reg classes[] = {
      {"Hash", NULL, class_functions},
      {NULL,     NULL, NULL},
  };

  static b_module_reg module = {NULL, classes};

  return module;
}