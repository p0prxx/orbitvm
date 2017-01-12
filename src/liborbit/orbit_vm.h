//
//  orbit_vm.h
//  OrbitVM
//
//  Created by Cesar Parent on 2017-01-03.
//  Copyright © 2017 cesarparent. All rights reserved.
//
#ifndef orbit_vm_h
#define orbit_vm_h

#include <stdint.h>
#include "orbit_value.h"

#define ORBIT_GCSTACK_SIZE 16

typedef struct _OrbitVM {
    VMTask*     task;
    GCObject*   gcHead;
    size_t      allocated;
    
    GCObject*   gcStack[ORBIT_GCSTACK_SIZE];
    size_t      gcStackSize;
} OrbitVM;

static inline void orbit_gcRetain(OrbitVM* vm, GCObject* object) {
    OASSERT(vm->gcStackSize < ORBIT_GCSTACK_SIZE-1, "stack overflow");
    vm->gcStack[vm->gcStackSize++] = object;
}

static inline void orbit_gcRelease(OrbitVM* vm) {
    OASSERT(vm->gcStackSize > 0, "stack underflow");
    vm->gcStackSize--;
}

void orbit_vmInit(OrbitVM* vm);

#endif /* orbit_vm_h */
