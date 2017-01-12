//
//  orbit_value.h
//  OrbitGC
//
//  Created by Cesar Parent on 2016-12-26.
//  Copyright © 2016 cesarparent. All rights reserved.
//
#ifndef orbit_value_h
#define orbit_value_h

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "orbit_string.h"
#include "orbit_platforms.h"

typedef enum _ValueType     ValueType;
typedef enum _GCFnType      GCFnType;
typedef enum _GCObjType     GCObjType;
typedef struct _GCValue     GCValue;
typedef struct _GCClass     GCClass;
typedef struct _GCObject    GCObject;
typedef struct _GCInstance  GCInstance;
typedef struct _GCString    GCString;
typedef struct _GCMap       GCMap;
typedef struct _GCArray     GCArray;
typedef struct _VMFunction  VMFunction;
typedef struct _VMCallFrame VMCallFrame;
typedef struct _VMModule    VMModule;
typedef struct _VMTask      VMTask;
typedef GCValue (*GCForeignFn)(GCValue*);


// The type tag of a GCValue tagged union. NIL, True and False are singletons
// to simplify dealing with them often.
//
// All numbers are double to simplify the standard library and allow
// non-programmers to not have to worry about number types. 1.8x10^308 should
// be large enough to avoid problems in most places.
//
// A value can also hold a function reference for potential closures in the
// future.
enum _ValueType {
    TYPE_NIL,
    TYPE_TRUE,
    TYPE_FALSE,
    TYPE_NUM,
    TYPE_OBJECT
};


// Orbit's value type, used for the GC's stack and the language's variables.
//
// TODO: add support for future GCArray and GCMap types
struct _GCValue {
    ValueType       type;
    union {
        double      numValue;
        void*       objectValue;
    };
};

// The type of a garbage-collected object. This is used to decide how to collect
// the object, and wether it has fields pointing to other objects in the graph.
enum _GCObjType {
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_STRING,
    OBJ_MAP,
    OBJ_ARRAY,
    OBJ_FUNCTION,
    OBJ_MODULE,
    OBJ_TASK,
};


// The base struct for any object that must be kept track of by the GC's garbage
// collector.
struct _GCObject {
    GCClass*        class;
    GCObjType       type;
    bool            mark;
    GCObject*       next;
};


// Orbit's class/user type representation. Even though Orbit 1 will probably
// not support inheritance (if it even supports OOP at all), we keep some space
// for a pointer to the parent class.
struct _GCClass {
    GCObject        base;
    String          name;
    GCClass*        super;
    uint16_t        fieldCount;
};


// Orbit's representation of an allocated instance of a language-defined class.
// 
// Half-classes like the language's primitives string, array and map do not
// require [fields] and are implemented mostly in C.
struct _GCInstance {
    GCObject        base;
    GCValue         fields[ORBIT_FLEXIBLE_ARRAY_MEMB];
};


// Orbit's primitive String type.
//
// Strings are immutable, which allows a bunch of optimisiations like storing
// length and hash, computed only once when the string is created.
struct _GCString {
    GCObject        base;
    uint64_t        length;
    uint32_t        hash;
    char            data[ORBIT_FLEXIBLE_ARRAY_MEMB];
};

// Orbit's primitive map's entry type. Key can be any primitive value (string
// or number).
typedef struct {
    GCValue         key;
    GCValue         value;
} GCMapEntry;

// The default capacity of a hash map. Must be a power of two to allow for
// AND modulo hack.
#define GCMAP_DEFAULT_CAPACITY 32

// Orbit's associative array type, implemented as an open-addressed, linear
// probed hash map.
struct _GCMap {
    GCObject        base;
    uint32_t        mask;
    uint32_t        size;
    uint32_t        capacity;
    GCMapEntry*     data;
};

#define GCARRAY_DEFAULT_CAPACITY 32

// Orbit's dynamic array type.
struct _GCArray {
    GCObject        base;
    uint32_t        size;
    uint32_t        capacity;
    GCValue*        data;
};

// The type fo a GC function.
enum _GCFnType {
    FN_NATIVE,
    FN_FOREIGN,
};

// Orbit's native function type, used for bytecode-compiled functions.
typedef struct _GCNativeFn {
    uint8_t     constantCount;
    uint16_t    byteCodeLength;
    GCValue*    constants;
    uint8_t*    byteCode;
} GCNativeFn;

// Orbit's Function type.
//
// Function objects can hold either bytecode for functions compiled from an
// Orbit script file, or a pointer to their native implementation for functions
// declared through the C API.
struct _VMFunction {
    GCObject        base;
    GCFnType        type;
    VMModule*       module;
    uint8_t         parameterCount;
    union {
        GCForeignFn foreign;
        GCNativeFn  native;
    };
};

// Orbit's call stack frame structure.
struct _VMCallFrame {
    VMTask*         task;
    VMFunction*     function;
    uint8_t*        ip;
    GCValue*        stackBase;
};

// Tasks hold the data required to execute bytecode: an operand stack for
// temporary results, as well as a call stack for function invocation and
// return.
struct _VMTask {
    GCObject        base;
    
    uint32_t        stackCapacity;
    GCValue*        sp;
    GCValue*        stack;
    
    uint32_t        frameCount;
    uint32_t        frameCapacity;
    VMCallFrame*    frames;
};

// VMModule holds all that is needed for a bytecode file to be executed.
// A module is created when a bytecode file is loaded into the VM, and can be
// used to hold state in between C API function calls.
struct _VMModule {
    GCObject        base;
    
    GCMap*          globals;
    GCMap*          classes;
    GCMap*          dispatchTable;
};

// Macros used to check the type of an orbit GCValue tagged union.

#define MAKE_NUM(num)   ((GCValue){TYPE_NUM, {.numValue=(num)}})
#define MAKE_OBJECT(obj)((GCValue){TYPE_OBJECT, {.objectValue=(obj)}})

#define VAL_NIL         ((GCValue){TYPE_NIL})
#define VAL_TRUE        ((GCValue){TYPE_TRUE})
#define VAL_FALSE       ((GCValue){TYPE_FALSE})

#define IS_BOOL(val)    ((val).type == TYPE_TRUE || (val).type == TYPE_FALSE)
#define IS_TRUE(val)    ((val).type != TYPE_FALSE)
#define IS_FALSE(val)   ((val).type == TYPE_FALSE)
#define IS_NIL(val)     ((val).type == TYPE_NIL)
#define IS_NUM(val)     ((val).type == TYPE_NUM)
#define IS_OBJECT(val)  ((val).type == TYPE_OBJECT)
#define IS_INSTANCE(val)(IS_OBJECT(val) && AS_OBJECT(val)->type == OBJ_INSTANCE)
#define IS_STRING(val)  (IS_OBJECT(val) && AS_OBJECT(val)->type == OBJ_STRING)

// Macros used to cast [val] to a given GC type.

#define AS_BOOL(val)    ((val).type == TYPE_TRUE)
#define AS_NUM(val)     ((double)(val).numValue)
#define AS_OBJECT(val)  ((GCObject*)(val).objectValue)
#define AS_INST(val)    ((GCInstance*)AS_OBJECT(val))
#define AS_STRING(val)  ((GCString*)AS_OBJECT(val))

// Creates a garbage collected string in [vm] from the bytes in [string].
GCString* orbit_gcStringNew(OrbitVM* vm, const char* string);

// Creates a garbage collected instance of [class] in [vm].
GCInstance* orbit_gcInstanceNew(OrbitVM* vm, GCClass* class);

// Creates a new class meta-object in [vm] named [className].
GCClass* orbit_gcClassNew(OrbitVM* vm, const char* name, uint16_t fieldCount);

// Creates a new hash map object in [vm];
GCMap* orbit_gcMapNew(OrbitVM* vm);

// Add a the [key] ==> [value] pair to [map]. [map] is grown if necessary.
void orbit_gcMapAdd(OrbitVM* vm, GCMap* map, GCValue key, GCValue value);

// Fetch the value for [key] in [map] into [value]. If [key] does not exist in
// [map], returns false.
bool orbit_gcMapGet(GCMap* map, GCValue key, GCValue* value);

// Remove the value for [key] in [map] if it exists.
void orbit_gcMapRemove(OrbitVM* vm, GCMap* map, GCValue key);

// Creates a new array in [vm].
GCArray* orbit_gcArrayNew(OrbitVM* vm);

// Add [value] to [array].
void orbit_gcArrayAdd(OrbitVM* vm, GCArray* array, GCValue value);

// Fetch the value at [index] in [array] into [value]. If [index] is out of
// bounds, returns false.
bool orbit_gcArrayGet(GCArray* array, uint32_t index, GCValue* value);

// Remove the value at [index] in [array]. If [index] is out of bounds, returns
// false. Shrink [array] if necessary.
bool orbit_gcArrayRemove(OrbitVM* vm, GCArray* array, uint32_t index);

// Creates a native bytecode function.
VMFunction* orbit_gcFunctionNew(OrbitVM* vm, uint8_t* byteCode,
                                uint16_t byteCodeLength, uint8_t constantCount);

// Creates a module that can be populated with the contents of a bytecode file.
VMModule* orbit_gcModuleNew(OrbitVM* vm);

// Creates a new task in [vm] and push [function] on the call stack;
VMTask* orbit_gcTaskNew(OrbitVM* vm, VMFunction* function);

// Deallocates [object].
void orbit_gcDeallocate(OrbitVM* vm, GCObject* object);

#endif /* orbit_value_h */
