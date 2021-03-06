// Distributed under the BSD License, see accompanying LICENSE.txt
// (C) Copyright 2010 John-John Tedro et al.
#ifndef _NBT_H_
#define _NBT_H_

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <zlib.h>
#include <setjmp.h>

#include <iostream>
#include <string.h>
#include <exception>
#include <string>
#include <stack>
#include <list>

#include <boost/ptr_container/ptr_vector.hpp>

namespace nbt {
  class bad_grammar : std::exception {};
  #define NBT_STACK_SIZE 100

  #define nbt_assert_error(exc_env, zfile, cond, why)                \
  do {                                                  \
  if (!(cond)) {                                           \
    size_t where = file == NULL ? 0 : gztell(file);     \
    error_handler(context, where, why);                 \
    longjmp(exc_env, 1);                                \
  }                                                     \
  } while(0)
  
  #define assert_error_c(exc_env, zfile, cond, why, cleanup)     \
  do {                                                  \
  if (!(cond)) {                                           \
    size_t where = file == NULL ? 0 : gztell(file);     \
    error_handler(context, where, why);                 \
    cleanup;                                            \
    longjmp(exc_env, 1);                                \
  }                                                     \
  } while(0)
  
  typedef int8_t Byte;
  typedef int16_t Short;
  typedef int32_t Int;
  typedef int64_t Long;
  typedef std::string String;
  typedef float Float;
  typedef double Double;
  
  struct ByteArray {
    Int length;
    Byte *values;
    ~ByteArray() {
      delete [] values;
    }
  };

  struct stack_entry {
    Byte type;
    String name;
    Int list_count, list_read;
    Byte list_type;
  };
  
  const Byte TAG_End = 0x0;
  const Byte TAG_Byte = 0x1;
  const Byte TAG_Short = 0x2;
  const Byte TAG_Int = 0x3;
  const Byte TAG_Long = 0x4;
  const Byte TAG_Float = 0x5;
  const Byte TAG_Double = 0x6;
  const Byte TAG_Byte_Array = 0x7;
  const Byte TAG_String = 0x8;
  const Byte TAG_List = 0x9;
  const Byte TAG_Compound = 0xa;
  
  const std::string TAG_End_str("TAG_End");
  const std::string TAG_Byte_str("TAG_Byte");
  const std::string TAG_Short_str("TAG_Short");
  const std::string TAG_Int_str("TAG_Int");
  const std::string TAG_Long_str("TAG_Long");
  const std::string TAG_Float_str("TAG_Float");
  const std::string TAG_Double_str("TAG_Double");
  const std::string TAG_Byte_Array_str("TAG_Byte_Array");
  const std::string TAG_String_str("TAG_String");
  const std::string TAG_List_str("TAG_List");
  const std::string TAG_Compound_str("TAG_Compound");
  
  const std::string tag_string_map[] = {
    TAG_End_str,
    TAG_Byte_str,
    TAG_Short_str,
    TAG_Int_str,
    TAG_Long_str,
    TAG_Float_str,
    TAG_Double_str,
    TAG_Byte_Array_str,
    TAG_String_str,
    TAG_List_str,
    TAG_Compound_str
  };
  
  bool is_big_endian();
  
  template <class C>
  void default_begin_compound(C* context, nbt::String name) {
    //std::cout << "TAG_Compound('" << name << "') BEGIN" << std::endl;
  }

  template <class C>
  void default_end_compound(C* context, String name) {
    //std::cout << "TAG_Compound END" << std::endl;
  }

  template <class C>
  void default_begin_list(C* context, nbt::String name, nbt::Byte type, nbt::Int length) {
    //std::cout << "TAG_List('" << name << "'): " << length << " items" << std::endl;
  }

  template <class C>
  void default_end_list(C* context, nbt::String name) {
    //std::cout << "TAG_List END" << std::endl;
  }

  template <class C>
  void default_error_handler(C* context, size_t where, const char *why) {
    std::cerr << "Unhandled nbt parser error at byte " << where << ": " << why << std::endl;
    exit(1);
  }
  
  template <class C>
  class Parser {
    private:
      jmp_buf exc_env;
      bool running;
      C *context;
      
      inline Byte read_byte(gzFile file) {
        Byte b;
        nbt_assert_error(exc_env, file, gzread(file, &b, sizeof(Byte)) == sizeof(Byte), "Buffer too short to read Byte");
        return b;
      }
      
      inline Short read_short(gzFile file) {
        uint8_t b[2];
        nbt_assert_error(exc_env, file, gzread(file, b, sizeof(b)) == sizeof(b), "Buffer to short to read Short");
        Short s = (b[0] << 8) + b[1];
        return s;
      }

      inline Int read_int(gzFile file) {
        Int i;
        
#ifdef BOOST_BIG_ENDIAN
        nbt_assert_error(exc_env, file, gzread(file, &i, sizeof(b)) == sizeof(b), "Buffer to short to read Int");
#else
        Byte b[sizeof(i)];
        nbt_assert_error(exc_env, file, gzread(file, b, sizeof(b)) == sizeof(b), "Buffer to short to read Int");
        Int *ip = &i;
        *((Byte*)ip) = b[3];
        *((Byte*)ip + 1) = b[2];
        *((Byte*)ip + 2) = b[1];
        *((Byte*)ip + 3) = b[0];
#endif
        
        return i;
      }
      
      inline String read_string(gzFile file) {
        Short s = read_short(file);
        nbt_assert_error(exc_env, file, s >= 0, "String specified with invalid length < 0");
        uint8_t *str = new uint8_t[s + 1];
        assert_error_c(exc_env, file, gzread(file, str, s) == s, "Buffer to short to read String", delete str);
        String so((const char*)str, s);
        delete [] str;
        return so;
      }
      
      inline void flush_string(gzFile file) {
        Short s = read_short(file);
        nbt_assert_error(exc_env, file, gzseek(file, s, SEEK_CUR) != -1, "Buffer to short to flush String");
      }
      
      inline Float read_float(gzFile file)
      {
        Float f;
        
#ifdef BOOST_BIG_ENDIAN
        nbt_assert_error(exc_env, file, gzread(file, &f, sizeof(f)) == sizeof(f), "Buffer to short to read Float");
#else
        Byte b[sizeof(f)];
        nbt_assert_error(exc_env, file, gzread(file, b, sizeof(f)) == sizeof(f), "Buffer to short to read Float");
        Float *fp = &f;
        *((Byte*)fp) = b[3];
        *((Byte*)fp + 1) = b[2];
        *((Byte*)fp + 2) = b[1];
        *((Byte*)fp + 3) = b[0];
#endif
        
        return f;
      }
      
      inline Long read_long(gzFile file) {
        Long l;
        
#ifdef BOOST_BIG_ENDIAN
        nbt_assert_error(exc_env, file, gzread(file, &l, sizeof(b)) == sizeof(b), "Buffer to short to read Long");
#else
        Byte b[sizeof(l)];
        nbt_assert_error(exc_env, file, gzread(file, b, sizeof(b)) == sizeof(b), "Buffer to short to read Long");
        Long *lp = &l;
        *((Byte*)lp) = b[7];
        *((Byte*)lp + 1) = b[6];
        *((Byte*)lp + 2) = b[5];
        *((Byte*)lp + 3) = b[4];
        *((Byte*)lp + 4) = b[3];
        *((Byte*)lp + 5) = b[2];
        *((Byte*)lp + 6) = b[1];
        *((Byte*)lp + 7) = b[0];
#endif
        
        return l;
      }
      
      inline Double read_double(gzFile file) {
        Double d;
        
#ifdef BOOST_BIG_ENDIAN
        nbt_assert_error(exc_env, file, gzread(file, &d, sizeof(d)) == sizeof(d), "Buffer to short to read Double");
#else
        Byte b[sizeof(d)];
        nbt_assert_error(exc_env, file, gzread(file, b, sizeof(d)) == sizeof(d), "Buffer to short to read Double");
        Double *dp = &d;
        *((Byte*)dp) = b[7];
        *((Byte*)dp + 1) = b[6];
        *((Byte*)dp + 2) = b[5];
        *((Byte*)dp + 3) = b[4];
        *((Byte*)dp + 4) = b[3];
        *((Byte*)dp + 5) = b[2];
        *((Byte*)dp + 6) = b[1];
        *((Byte*)dp + 7) = b[0];
#endif
        
        return d;
      }
      
      inline Byte read_tagType(gzFile file) {
        Byte type = read_byte(file);
        nbt_assert_error(exc_env, file, type >= 0 && type <= TAG_Compound, "Not a valid tag type");
        return type;
      }
      
      inline void flush_byte_array(gzFile file) {
        Int length = read_int(file);
        nbt_assert_error(exc_env, file, gzseek(file, length, SEEK_CUR) != -1,
          "Buffer to short to flush ByteArray");
      }
      
      inline void handle_byte_array(String name, gzFile file) {
        Int length = read_int(file);
        Byte *values = new Byte[length];
        nbt_assert_error(exc_env, file, gzread(file, values, length) == length, "Buffer to short to read ByteArray");
        ByteArray *array = new ByteArray();
        array->values = values;
        array->length = length;
        register_byte_array(context, name, array);
      }
    public:
      typedef void (*begin_compound_t)(C*, String name);
      typedef void (*end_compound_t)(C*, String name);
      
      typedef void (*begin_list_t)(C*, String name, Byte type, Int length);
      typedef void (*end_list_t)(C*, String name);
      
      typedef void (*register_long_t)(C*, String name, Long l);
      typedef void (*register_short_t)(C*, String name, Short l);
      typedef void (*register_string_t)(C*, String name, String l);
      typedef void (*register_float_t)(C*, String name, Float l);
      typedef void (*register_double_t)(C*, String name, Double l);
      typedef void (*register_int_t)(C*, String name, Int l);
      typedef void (*register_byte_t)(C*, String name, Byte b);
      typedef void (*register_byte_array_t)(C*, String name, ByteArray* array);
      typedef void (*error_handler_t)(C*, size_t where, const char *why);
      
      register_long_t register_long;
      register_short_t register_short;
      register_string_t register_string;
      register_float_t register_float;
      register_double_t register_double;
      register_int_t register_int;
      register_byte_t register_byte;
      register_byte_array_t register_byte_array;

      begin_compound_t begin_compound;
      end_compound_t end_compound;
      begin_list_t begin_list;
      end_list_t end_list;
      error_handler_t error_handler;
      
      Parser() :
        context(NULL),
        register_long(NULL),
        register_short(NULL),
        register_string(NULL),
        register_float(NULL),
        register_double(NULL),
        register_int(NULL),
        register_byte(NULL),
        register_byte_array(NULL),
        begin_compound(&default_begin_compound<C>),
        end_compound(&default_end_compound<C>),
        begin_list(&default_begin_list<C>),
        end_list(&default_end_list<C>),
        error_handler(&default_error_handler<C>)
      {
      }
      
      Parser(C *context) :
        context(context),
        register_long(NULL),
        register_short(NULL),
        register_string(NULL),
        register_float(NULL),
        register_double(NULL),
        register_int(NULL),
        register_byte(NULL),
        register_byte_array(NULL),
        begin_compound(&default_begin_compound<C>),
        end_compound(&default_end_compound<C>),
        begin_list(&default_begin_list<C>),
        end_list(&default_end_list<C>),
        error_handler(&default_error_handler<C>)
      {
        this->context = context;
      }
      
      void stop() {
        running = false;
      }
      
      void parse_file(const char *path)
      {
        if (setjmp(exc_env) == 1) return;
        
        gzFile file = gzopen(path, "rb");
        nbt_assert_error(exc_env, file, file != NULL, strerror(errno));
        
        running = true;
        stack_entry *stack = new stack_entry[NBT_STACK_SIZE];
        int stack_p = 0;
        stack_entry *root = stack + 0;
        
        if (setjmp(exc_env) == 1) {
          goto exit_error;
        }
        
        root->type = read_tagType(file);
        nbt_assert_error(exc_env, file, root->type == TAG_Compound, "Expected TAG_Compound at root");
        root->name = read_string(file);
        
        begin_compound(context, root->name);
        
        while(running && stack_p >= 0) {
          nbt_assert_error(exc_env, file, stack_p < NBT_STACK_SIZE, "Stack cannot be larger than NBT_STACK_SIZE");
          
          stack_entry* top = stack + stack_p;
          
          Byte type;
          String name("");
          
          // if top is of Compound type, we must read the item name first
          if (top->type == TAG_Compound) {
            type = read_tagType(file);
            
            if (type == TAG_End) {
              end_compound(context, top->name);
              --stack_p;
              continue;
            }
            
            name = read_string(file);
          }
          
          // if top of stack is of type list, the type must be inferred, name is assumed to be "" (empty string)
          else if (top->type == TAG_List) {
            if (top->list_read >= top->list_count) {
              end_list(context, top->name);
              --stack_p;
              continue;
            }
            
            type = top->list_type;
            top->list_read++;
          }
          
          else {
            nbt_assert_error(exc_env, file, 1, "Unknown stack type");
            continue;
          }
          
          switch(type) {
          case TAG_Long:
            if (register_long == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Long), SEEK_CUR) != -1,
                "Buffer too short to flush long");
            } else {
              register_long(context, name, read_long(file));
            }
            break;
          case TAG_Short:
            if (register_short == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Short), SEEK_CUR) != -1,
                "Buffer too short to flush short");
            } else {
              register_short(context, name, read_short(file));
            }
            break;
          case TAG_String:
            if (register_string == NULL) {
              flush_string(file);
            } else {
              register_string(context, name, read_string(file));
            }
            break;
          case TAG_Float:
            if (register_float == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Float), SEEK_CUR) != -1,
                "Buffer too short to flush float");
            } else {
              register_float(context, name, read_float(file));
            }
            break;
          case TAG_Double:
            if (register_double == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Double), SEEK_CUR) != -1,
                "Buffer too short to flush double");
            } else {
              register_double(context, name, read_double(file));
            }
            break;
          case TAG_Int:
            if (register_int == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Int), SEEK_CUR) != -1,
                "Buffer too short to flush int");
            } else {
              register_int(context, name, read_int(file));
            }
            break;
          case TAG_Byte:
            if (register_byte == NULL) {
              nbt_assert_error(exc_env, file, gzseek(file, sizeof(nbt::Byte), SEEK_CUR) != -1,
                "Buffer too short to flush byte");
            } else {
              register_byte(context, name, read_byte(file));
            }
            break;
          case TAG_List:
            {
              stack_entry* c = stack + ++stack_p;
              
              c->list_read = 0;
              c->list_type = read_tagType(file);
              c->list_count = read_int(file);
              c->name = name;
              c->type = TAG_List;
              
              begin_list(context, name, c->list_type, c->list_count);
            }
            break;
          case TAG_Compound:
            {
              begin_compound(context, name);
              stack_entry* c = stack + ++stack_p;
              
              c->list_read = 0;
              c->list_count = 0;
              c->list_type = -1;
              c->name = name;
              c->type = TAG_Compound;
            }
            break;
          case TAG_Byte_Array:
            if (register_byte_array == NULL) {
              flush_byte_array(file);
            } else {
              handle_byte_array(name, file);
            }
            break;
          default:
            nbt_assert_error(exc_env, file, 0, "Encountered unknown type");
            break;
          }
        }
        
exit_error:
        gzclose(file);
        delete [] stack;
      }
  };
}

#endif /* _NBT_H_ */
